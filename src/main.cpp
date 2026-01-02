// =============================================================
// WebCan Terminal + SLCAN (Compatibility Mode)
// Baud: 115200 (Standard)
// =============================================================

#include <Arduino.h>

// RTOS & System
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h" 

// Hardware Drivers
#include <SPI.h>
#include <Wire.h>
#include <ACAN2515.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Network
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <FS.h>
#include <SPIFFS.h>

// Project Files
#include "webInterface.h" 

// ================== CONFIGURATION ==================
#define SERIAL_BAUD_RATE 115200 

// ================== DATA STRUCTURES ==================
struct FrameLite {
  uint32_t id;
  uint8_t  len;    
  uint8_t  flags;  // Bit 0: EXT, Bit 1: RTR
  uint8_t  data[8];
};
inline bool fl_ext(const FrameLite& f){ return (f.flags & 0x01) != 0; }
inline bool fl_rtr(const FrameLite& f){ return (f.flags & 0x02) != 0; }

// ================== GLOBALS ==================
static QueueHandle_t qRx = nullptr;   
static QueueHandle_t qTx = nullptr;   
static EventGroupHandle_t eg = nullptr;
static const EventBits_t BIT_WS_HAS_CLIENT = (1<<0);
static TaskHandle_t hCanTask = nullptr;
static TaskHandle_t hNetTask = nullptr;

// OLED
#ifndef OLED_SDA
  #define OLED_SDA 21
#endif
#ifndef OLED_SCL
  #define OLED_SCL 22
#endif
#define OLED_ADDR  0x3C
Adafruit_SSD1306 display(128, 64, &Wire, -1);
static bool gOledOK = false;

// OLED Thread Safety
static volatile bool gOledWakeReq = false;       
static volatile bool gOledRedrawReq = false;     
static volatile bool gOledMsgReq = false;        
static char gOledMsgBuf[22] = {0};               
static portMUX_TYPE gOledMux = portMUX_INITIALIZER_UNLOCKED; 

// Web
WebServer http(80);
WebSocketsServer ws(81); 
#define WS_BATCH_BYTES 8192
#define WS_FLUSH_EVERY_MS 5
static char gWsBuf[WS_BATCH_BYTES];
static size_t gWsLen = 0;
static uint32_t gWsLastFlush = 0;

// CAN Hardware
static const byte MCP2515_SCK  = 14;
static const byte MCP2515_MOSI = 13;
static const byte MCP2515_MISO = 12;
static const byte MCP2515_CS   = 26;
static const byte MCP2515_INT  = 25;

ACAN2515 can(MCP2515_CS, SPI, MCP2515_INT);
static const uint32_t QUARTZ_FREQUENCY = 8UL * 1000UL * 1000UL; 

// State
static bool canReady = false;
static bool ackMode = true;        
static uint16_t currentKbps = 500; 

// Forward Decls
static bool reconfigure(uint16_t kbps);
static void oledShowIP(const char *extraLine = nullptr);
static void oledRenderBase(); 
static void oledBump(const char* transient = nullptr);
static void slcanHandle(const char *cmd);

// ================== CONFIG STORAGE ==================
struct ApConfig { char ssid[33]; char pass[65]; };
static ApConfig gApCfg;
const char *AP_CFG_PATH = "/apcfg.txt";

struct StaConfig { char ssid[33]; char pass[65]; bool enabled; };
static StaConfig gStaCfg;
const char *STA_CFG_PATH = "/stacfg.txt";

const char *DEF_AP_SSID = "WebCan";
const char *DEF_AP_PASS = "12345678";

static void loadApConfig() {
  if (SPIFFS.begin(true) && SPIFFS.exists(AP_CFG_PATH)) {
    File f = SPIFFS.open(AP_CFG_PATH, "r");
    String s = f.readStringUntil('\n'); s.trim();
    String p = f.readStringUntil('\n'); p.trim();
    strncpy(gApCfg.ssid, s.length()?s.c_str():DEF_AP_SSID, 32);
    strncpy(gApCfg.pass, p.length()?p.c_str():DEF_AP_PASS, 64);
    f.close();
  } else {
    strncpy(gApCfg.ssid, DEF_AP_SSID, 32);
    strncpy(gApCfg.pass, DEF_AP_PASS, 64);
  }
}
static void loadStaConfig() {
  if (SPIFFS.begin(true) && SPIFFS.exists(STA_CFG_PATH)) {
    File f = SPIFFS.open(STA_CFG_PATH, "r");
    String s = f.readStringUntil('\n'); s.trim();
    String p = f.readStringUntil('\n'); p.trim();
    String e = f.readStringUntil('\n'); e.trim();
    strncpy(gStaCfg.ssid, s.c_str(), 32);
    strncpy(gStaCfg.pass, p.c_str(), 64);
    gStaCfg.enabled = (e == "1");
    f.close();
  } else {
    gStaCfg.ssid[0]=0; gStaCfg.pass[0]=0; gStaCfg.enabled=true;
  }
}
static void saveApConfig(String s, String p) {
  File f = SPIFFS.open(AP_CFG_PATH, "w");
  f.println(s); f.println(p); f.close();
  strncpy(gApCfg.ssid, s.c_str(), 32);
  strncpy(gApCfg.pass, p.c_str(), 64);
}
static void saveStaConfig(String s, String p, bool en) {
  File f = SPIFFS.open(STA_CFG_PATH, "w");
  f.println(s); f.println(p); f.println(en?"1":"0"); f.close();
  strncpy(gStaCfg.ssid, s.c_str(), 32);
  strncpy(gStaCfg.pass, p.c_str(), 64);
  gStaCfg.enabled = en;
}

// ================== UTILS ==================
static inline int8_t hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}
static inline uint8_t hex2(const char *p, bool &ok) {
    int8_t hi = hexNibble(p[0]);
    int8_t lo = hexNibble(p[1]);
    if (hi < 0 || lo < 0) ok = false;
    return (uint8_t)((hi << 4) | lo);
}
static inline char hexDigit(uint8_t v) { return (v < 10) ? char('0' + v) : char('A' + (v - 10)); }
static inline void slcanOK() { Serial.write('\r'); }
static inline void slcanERR() { Serial.write('\a'); }

void mcp2515HardReset() {
    SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    digitalWrite(MCP2515_CS, LOW);
    SPI.transfer(0xC0); 
    digitalWrite(MCP2515_CS, HIGH);
    SPI.endTransaction();
}

// ================== OLED ==================
static void oledInit() {
  Wire.begin(OLED_SDA, OLED_SCL);
  gOledOK = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (!gOledOK) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("WebCan Pro");
  display.println("Booting...");
  display.display();
}

static void oledBump(const char* transient) {
  gOledWakeReq = true; 
  if (transient && transient[0]) {
    portENTER_CRITICAL(&gOledMux);
    strncpy(gOledMsgBuf, transient, sizeof(gOledMsgBuf)-1);
    gOledMsgBuf[sizeof(gOledMsgBuf)-1] = 0;
    gOledMsgReq = true; 
    portEXIT_CRITICAL(&gOledMux);
  }
}

static void oledRenderBase() {
  if (!gOledOK) return;
  bool staUp = (WiFi.status() == WL_CONNECTED);
  IPAddress ip = staUp ? WiFi.localIP() : WiFi.softAPIP();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  display.print("WebCan "); display.println(staUp ? "(STA)" : "(AP)");
  display.println(staUp ? gStaCfg.ssid : gApCfg.ssid);
  display.println(ip);
  display.println("---------------------");
  display.print("CAN: "); display.print(currentKbps); display.println(" kbps");
  display.print("State: "); display.println(canReady ? "ACTIVE" : "CLOSED");

  // Note: Transient msg drawn in Tick for simplicity in this merging strategy
  display.display();
}

static void oledTick() {
  if (!gOledOK) return;
  
  static bool asleep = false;
  static uint32_t lastAct = 0;
  static char currentMsg[22] = {0};
  static uint32_t msgExpire = 0;

  bool wake = gOledWakeReq; gOledWakeReq = false;
  bool redraw = gOledRedrawReq; gOledRedrawReq = false;
  
  if (gOledMsgReq) {
    portENTER_CRITICAL(&gOledMux);
    strcpy(currentMsg, gOledMsgBuf);
    gOledMsgReq = false;
    portEXIT_CRITICAL(&gOledMux);
    msgExpire = millis() + 1500;
    wake = true; redraw = true;
  }

  if (wake) {
    if (asleep) { display.ssd1306_command(SSD1306_DISPLAYON); asleep = false; }
    lastAct = millis();
  }

  if (!asleep && (millis() - lastAct > 30000)) {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    asleep = true;
  }

  if (msgExpire && millis() > msgExpire) {
    currentMsg[0] = 0; msgExpire = 0; redraw = true;
  }

  if (redraw && !asleep) {
    oledRenderBase();
    if (currentMsg[0]) {
       display.fillRect(0, 45, 128, 19, SSD1306_WHITE);
       display.setTextColor(SSD1306_BLACK);
       display.setCursor(2, 50);
       display.print(currentMsg);
       display.display();
    }
  }
}

static void oledShowIP(const char* msg) {
  gOledRedrawReq = true;
  if(msg) oledBump(msg);
}

// ================== CAN LOGIC ==================
static bool reconfigure(uint16_t kbps) {
  ACAN2515Settings settings(QUARTZ_FREQUENCY, (unsigned)(kbps * 1000U));
  settings.mRequestedMode = ackMode ? ACAN2515Settings::NormalMode : ACAN2515Settings::ListenOnlyMode;
  settings.mTripleSampling = (kbps <= 125);
  settings.mReceiveBufferSize = 128; 
  
  const uint16_t ec = can.begin(settings, []{ can.isr(); });
  if (ec != 0) {
    canReady = false;
    char err[32]; snprintf(err,32,"Err 0x%X", ec);
    oledShowIP(err);
    return false;
  }
  canReady = true;
  currentKbps = kbps;
  oledShowIP("CAN Config OK");
  return true;
}

// ================== SLCAN HANDLER ==================
static void slcanHandle(const char *cmd) {
    size_t len = strlen(cmd);
    if (len == 0) { slcanERR(); return; }

    switch (cmd[0]) {
    case 'S': case 's': {
        if (len < 2) { slcanERR(); return; }
        uint16_t k = 500;
        switch(cmd[1]) {
            case '0': k=10; break; case '1': k=20; break; case '2': k=50; break;
            case '3': k=100; break; case '4': k=125; break; case '5': k=250; break;
            case '6': k=500; break; case '7': k=800; break; case '8': k=1000; break;
            default: slcanERR(); return;
        }
        currentKbps = k; slcanOK(); break;
    }
    case 'O': ackMode = true;  if(reconfigure(currentKbps)) slcanOK(); else slcanERR(); break;
    case 'L': ackMode = false; if(reconfigure(currentKbps)) slcanOK(); else slcanERR(); break;
    case 'C': canReady = false; can.end(); mcp2515HardReset(); oledShowIP("SLCAN Closed"); slcanOK(); break;
    case 't':
    case 'T': {
        if (!canReady) { slcanERR(); return; }
        bool ext = (cmd[0] == 'T');
        int idLen = ext ? 8 : 3;
        if ((int)len < (1 + idLen + 1)) { slcanERR(); return; } 

        bool ok = true;
        uint32_t id = 0;
        for(int i=0; i<idLen; i++) {
            int8_t n = hexNibble(cmd[1+i]); 
            if(n<0) ok=false; 
            id = (id<<4)|n; 
        }
        uint8_t dlc = cmd[1+idLen] - '0';
        if (dlc > 8 || !ok) { slcanERR(); return; }

        FrameLite f; f.id = id; f.len = dlc; f.flags = (ext?1:0); 
        int dataStart = 1 + idLen + 1;
        for(int i=0; i<dlc; i++) f.data[i] = hex2(cmd + dataStart + (i*2), ok);
        if(!ok) { slcanERR(); return; }

        if(xQueueSend(qTx, &f, 0) == pdTRUE) slcanOK(); else slcanERR();
        break;
    }
    case 'V': case 'v': Serial.print("V1015\r"); break;
    default: slcanERR(); break;
    }
}

// ================== TASKS ==================
static void slcanDump(const FrameLite &f) {
  char buf[64]; int idx = 0;
  bool ext = fl_ext(f);
  bool rtr = fl_rtr(f);

  buf[idx++] = ext ? (rtr ? 'R' : 'T') : (rtr ? 'r' : 't');
  if (ext) {
    for(int i=7; i>=0; i--) buf[idx++] = hexDigit((f.id >> (i*4)) & 0xF);
  } else {
    buf[idx++] = hexDigit((f.id >> 8) & 0xF);
    buf[idx++] = hexDigit((f.id >> 4) & 0xF);
    buf[idx++] = hexDigit((f.id) & 0xF);
  }
  buf[idx++] = (char)('0' + f.len);
  if (!rtr) {
      for(int i=0; i<f.len; i++) {
          buf[idx++] = hexDigit(f.data[i] >> 4);
          buf[idx++] = hexDigit(f.data[i] & 0xF);
      }
  }
  buf[idx++] = '\r';

  // SAFETY CHECK: If Serial is slow, don't block. Drop frame if buffer full.
  if (Serial.availableForWrite() >= idx) {
    Serial.write((uint8_t*)buf, idx);
  }
}

static void canTask(void* pv) {
  for(;;) {
    // 1. Process TX 
    FrameLite txf;
    while(xQueueReceive(qTx, &txf, 0) == pdTRUE) {
      CANMessage m; 
      m.id = txf.id; m.ext = fl_ext(txf); m.rtr = fl_rtr(txf); m.len = txf.len;
      memcpy(m.data, txf.data, txf.len);
      for(int i=0; i<3; i++) { if(can.tryToSend(m)) break; vTaskDelay(1); }
    }
    // 2. Process RX
    if (canReady) {
       CANMessage rx;
       int count = 0;
       while (can.available() && count < 64) {
         can.receive(rx);
         FrameLite f; f.id = rx.id; f.len = rx.len; 
         f.flags = (rx.ext?1:0) | (rx.rtr?2:0);
         memcpy(f.data, rx.data, rx.len);

         // To Web
         if (xQueueSend(qRx, &f, 0) != pdTRUE) {
            FrameLite d; xQueueReceive(qRx, &d, 0); xQueueSend(qRx, &f, 0);
         }
         // To Serial
         slcanDump(f);
         count++;
       }
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

static void wsFlushIfNeeded(WebSocketsServer &w, bool force = false) {
  uint32_t now = millis();
  if (force || gWsLen > (WS_BATCH_BYTES - 100) || (now - gWsLastFlush > WS_FLUSH_EVERY_MS)) {
    if (gWsLen > 0) {
       if(w.connectedClients() > 0) w.broadcastTXT((uint8_t*)gWsBuf, gWsLen);
       gWsLen = 0;
    }
    gWsLastFlush = now;
  }
}

static void netTask(void* pv) {
  for(;;) {
    http.handleClient();
    ws.loop();

    FrameLite f;
    while(xQueueReceive(qRx, &f, 0) == pdTRUE) {
      char line[128];
      char idStr[10];
      if(fl_ext(f)) snprintf(idStr, 10, "%08lX", f.id); else snprintf(idStr, 10, "%03lX", f.id);
      int n = snprintf(line, sizeof(line), "[ID:0x%s %s %s DLC:%u Data:", 
          idStr, fl_ext(f)?"EXT":"STD", fl_rtr(f)?"RTR":"DAT", f.len);
      for(int i=0; i<f.len; i++) n += snprintf(line+n, sizeof(line)-n, " %02X", f.data[i]);
      if(n < sizeof(line)-2) { line[n++]=']'; line[n++]='\n'; }

      if (gWsLen + n < WS_BATCH_BYTES) { memcpy(gWsBuf + gWsLen, line, n); gWsLen += n; } 
      else { wsFlushIfNeeded(ws, true); memcpy(gWsBuf, line, n); gWsLen = n; }
    }
    wsFlushIfNeeded(ws);
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

// ================== SETUP & LOOP ==================
void setup() {
  Serial.begin(SERIAL_BAUD_RATE); // 115200 for compatibility
  oledInit();

  loadApConfig();
  loadStaConfig();

  if(gStaCfg.enabled && strlen(gStaCfg.ssid)) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(gStaCfg.ssid, gStaCfg.pass);
    uint32_t t = millis();
    while(WiFi.status() != WL_CONNECTED && millis()-t < 10000) delay(100);
  }
  if(WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(gApCfg.ssid, gApCfg.pass, 6, false, 4);
    oledBump("AP Mode");
  } else {
    oledBump("WiFi OK");
  }
  
  ws.begin(); 
  ws.onEvent([](uint8_t n, WStype_t t, uint8_t *p, size_t l){
    if(t == WStype_CONNECTED) { oledBump("Web Client"); }
  });

  http.on("/", [](){ http.send_P(200, "text/html", INDEX_HTML); });
  
  http.on("/api/can/send", HTTP_POST, [](){
     if(!canReady) { http.send(400, "application/json", "{\"ok\":0}"); return; }
     String idS = http.arg("id");
     if(idS.startsWith("0x")) idS = idS.substring(2);
     uint32_t id = strtoul(idS.c_str(), NULL, 16);
     uint8_t dlc = http.arg("dlc").toInt();
     String d = http.arg("data"); d.replace(" ", "");
     FrameLite f; f.id = id; f.len = dlc; 
     f.flags = (http.arg("ext")=="1"?1:0) | (http.arg("rtr")=="1"?2:0);
     for(int i=0; i<dlc; i++) {
        char b[3] = { d[i*2], d[i*2+1], 0 };
        f.data[i] = strtoul(b, NULL, 16);
     }
     if(xQueueSend(qTx, &f, 0)) http.send(200, "application/json", "{\"ok\":1}");
     else http.send(503, "application/json", "{\"ok\":0}");
  });
  
  http.on("/api/apcfg", HTTP_POST, [](){ saveApConfig(http.arg("ssid"), http.arg("pass")); http.send(200, "application/json", "{\"ok\":1}"); });
  http.on("/api/stacfg", HTTP_POST, [](){ saveStaConfig(http.arg("ssid"), http.arg("pass"), http.arg("enabled")=="1"); http.send(200, "application/json", "{\"ok\":1}"); });
  http.on("/api/reset", HTTP_POST, [](){ http.send(200, "application/json", "{\"ok\":1}"); delay(100); ESP.restart(); });
  http.on("/api/can/open", HTTP_POST, [](){
     ackMode = (http.arg("mode")!="listen");
     if(reconfigure(http.arg("kbps").toInt())) http.send(200, "application/json", "{\"ok\":1}");
     else http.send(500, "application/json", "{\"ok\":0}");
  });
  http.on("/api/can/close", HTTP_POST, [](){ canReady=false; http.send(200, "application/json", "{\"ok\":1}"); });

  http.begin();

  SPI.begin(MCP2515_SCK, MCP2515_MISO, MCP2515_MOSI);
  SPI.setFrequency(8000000);
  pinMode(MCP2515_INT, INPUT_PULLUP);
  
  reconfigure(currentKbps); 

  qRx = xQueueCreate(128, sizeof(FrameLite));
  qTx = xQueueCreate(64, sizeof(FrameLite));
  eg = xEventGroupCreate();
  
  xTaskCreatePinnedToCore(canTask, "CAN", 4096, NULL, 10, &hCanTask, 1);
  xTaskCreatePinnedToCore(netTask, "NET", 6144, NULL, 5, &hNetTask, 0);
}

static char slBuf[64]; static int slIdx = 0;
void loop() {
  oledTick();
  while(Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') {
      if(slIdx > 0) { slBuf[slIdx] = 0; slcanHandle(slBuf); slIdx = 0; }
    } else if (slIdx < 63) slBuf[slIdx++] = c;
  }
  vTaskDelay(1);
}