// ============================
// WebCan Terminal (Web-only)
// ESP32 + MCP2515 (ACAN2515)
// ============================

// Put Arduino first so stdint types (uint8_t/uint32_t) are available everywhere
#include <Arduino.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ACAN2515.h>
#include "ledstatus.h"
#include <FS.h>
#include <SPIFFS.h>
#include <string.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "webInterface.h" // Provides INDEX_HTML (PROGMEM)

// ---------- RTOS bridge types & forwards ----------
struct FrameLite {
  uint32_t id;
  uint8_t  len;    // 0..8
  uint8_t  flags;  // bit0=ext, bit1=rtr
  uint8_t  data[8];
};
inline bool fl_ext(const FrameLite& f){ return (f.flags & 0x01) != 0; }
inline bool fl_rtr(const FrameLite& f){ return (f.flags & 0x02) != 0; }

// Forward declarations so they exist before startRtosPipelines()
static void canTask(void* pv);
static void netTask(void* pv);
static void startRtosPipelines();

// Queues: adjust sizes to your bus rate & RAM
static QueueHandle_t qRx = nullptr;   // FrameLite from CAN -> Net
static QueueHandle_t qTx = nullptr;   // FrameLite from Net -> CAN

// Event group: bit 0 = at least one WS client
static EventGroupHandle_t eg = nullptr;
static const EventBits_t BIT_WS_HAS_CLIENT = (1<<0);

// Task handles (optional)
static TaskHandle_t hCanTask = nullptr;
static TaskHandle_t hNetTask = nullptr;

// ================== Status LED ==================
LedStatus led;

// ================== OLED pins/addr ==================
#ifndef OLED_SDA
  #define OLED_SDA 21
#endif
#ifndef OLED_SCL
  #define OLED_SCL 22
#endif
#ifndef OLED_ADDR
  #define OLED_ADDR 0x3C
#endif
#define OLED_RESET -1
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);
static bool gOledOK = false;

// ================== WS batching (legacy helpers; netTask has its own too) ==================
#define WS_BATCH_BYTES 8192
#define WS_FLUSH_EVERY_MS 5

static char gWsBuf[WS_BATCH_BYTES];
static size_t gWsLen = 0;
static uint32_t gWsLastFlush = 0;

static inline void wsFlushIfNeeded(WebSocketsServer &w, bool force = false)
{
  uint32_t now = millis();
  if (force || gWsLen > (WS_BATCH_BYTES - 96) || (now - gWsLastFlush) >= WS_FLUSH_EVERY_MS)
  {
    if (gWsLen)
    {
      if (w.connectedClients() > 0)
      {
        w.broadcastTXT((const uint8_t *)gWsBuf, gWsLen);
      }
      gWsLen = 0; // drop buffer either way to avoid backpressure
    }
    gWsLastFlush = now;
  }
}

static uint32_t gWsKeepAliveAt = 0;
static const uint32_t WS_KEEPALIVE_MS = 5000; // 5s

// ================== Forward decls ==================
struct CANMessage; // from ACAN2515.h
static bool reconfigure(uint16_t kbps);
static void oledShowIP(const char *extraLine = nullptr);

// ================== CAN state ==================
static bool canReady = false;
static bool ackMode = true;        // true=NORMAL (ACK), false=ListenOnly
static bool printFrames = true;    // push frames to WS
static uint16_t currentKbps = 500; // active bitrate

// ================== Wi-Fi configs in flash ==================
struct ApConfig
{
  char ssid[33];
  char pass[65];
};
static ApConfig gApCfg;
static const char *AP_CFG_PATH = "/apcfg.txt";

struct StaConfig
{
  char ssid[33];
  char pass[65];
  bool enabled;
};
static StaConfig gStaCfg;
static const char *STA_CFG_PATH = "/stacfg.txt";

// ================== Wi-Fi (AP fallback defaults) ==================
const char *AP_SSID = "WebCan";
const char *AP_PASSWORD = "12345678";
static const uint8_t AP_CHANNEL = 6;
static const bool AP_HIDDEN = false;
static const uint8_t AP_MAX_CONN = 4;

// ================== Web ====================
WebServer http(80);
WebSocketsServer ws(81); // ws://<ip>:81/

// ================== ESP32 <-> MCP2515 pins ==================
static const byte MCP2515_SCK = 14;
static const byte MCP2515_MOSI = 13;
static const byte MCP2515_MISO = 12;
static const byte MCP2515_CS = 26;
static const byte MCP2515_INT = 25;

// ================== ACAN2515 ==================
ACAN2515 can(MCP2515_CS, SPI, MCP2515_INT);
static const uint32_t QUARTZ_FREQUENCY = 8UL * 1000UL * 1000UL; // 16 MHz

// ===== OLED transient overlay (auto-clears) =====
static char     gOledTransient[22] = {0}; // up to ~21 chars
static uint32_t gOledTransientUntil = 0;  // millis() deadline

static void oledRenderBase(); // forward

// ===== OLED idle sleep (10s) =====
static bool     gOledAsleep = false;
static uint32_t gLastActivity = 0;
static const uint32_t OLED_IDLE_MS = 10000; // 10 seconds

// Put these under your existing OLED helpers:
static void oledSleep(bool on) {
  if (!gOledOK) return;
  if (on) {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    gOledAsleep = true;
  } else {
    display.ssd1306_command(SSD1306_DISPLAYON);
    gOledAsleep = false;
    oledRenderBase(); // redraw base immediately on wake
  }
}

static void oledFlash(const char* msg, uint32_t ms = 1000) {
  if (!gOledOK || !msg || !msg[0]) return;
  strncpy(gOledTransient, msg, sizeof(gOledTransient) - 1);
  gOledTransient[sizeof(gOledTransient) - 1] = 0;
  gOledTransientUntil = millis() + ms;
  oledRenderBase(); // draw base + overlay immediately
}

// Bump activity and optionally flash a short overlay.
// If the OLED is asleep, wake it first.
static inline void oledBump(const char* transient = nullptr, uint32_t flashMs = 800) {
  gLastActivity = millis();
  if (gOledAsleep) {
    oledSleep(false); // wake + redraw base
  }
  if (transient && transient[0]) {
    oledFlash(transient, flashMs); // uses your existing transient system
  }
}


static void oledTick() {
  if (!gOledOK) return;
  const uint32_t now = millis();

  // Put display to sleep after inactivity
  if (!gOledAsleep && (now - gLastActivity) >= OLED_IDLE_MS) {
    oledSleep(true); // off
    return;          // stop drawing while asleep
  }

  // Handle transient overlay expiry
  if (gOledTransientUntil && (int32_t)(now - gOledTransientUntil) >= 0) {
    gOledTransient[0] = 0;
    gOledTransientUntil = 0;
    if (!gOledAsleep) {
      oledRenderBase(); // only redraw when awake
    }
  }
}

// ================== OLED ==================
static void oledInit()
{
  Wire.begin(OLED_SDA, OLED_SCL);
  gOledOK = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (!gOledOK)
    return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("ESP32 CAN Terminal");
  display.println("OLED ready");
  display.display();
}
// Draw the persistent “base” status (no sticky messages)
static void oledRenderBase() {
  if (!gOledOK) return;
  bool staUp = (WiFi.status() == WL_CONNECTED);
  IPAddress ip = staUp ? WiFi.localIP() : WiFi.softAPIP();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  display.println("WebCan Interface");
  display.print ("Mode: "); display.println(staUp ? "STA" : "AP");
  display.print ("SSID: "); display.println(staUp ? gStaCfg.ssid : gApCfg.ssid);
  display.print ("IP:   "); display.println(ip.toString());
  display.print ("CAN:  ");
  display.print (currentKbps); display.print(" kbps ");
  display.println(ackMode ? "NORM" : "LISTEN");

  // If a transient message is active, draw a small overlay box at the bottom
  if (gOledTransient[0]) {
    const int x = 0, y = 48, w = 128, h = 16;
    display.fillRect(x, y, w, h, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(2, y + 4);
    display.print(gOledTransient);
    display.setTextColor(SSD1306_WHITE);
  }

  display.display();
}

// Backward-compatible wrapper: still accepts extraLine, but shows it as a
// 1-second transient overlay instead of keeping it permanently.
static void oledShowIP(const char *extraLine) {
  if (!gOledOK) return;
  oledRenderBase();                    // render base
  if (extraLine && extraLine[0]) {     // flash overlay briefly
    oledFlash(extraLine, 1000);
  }
}

// ================== AP/STA cfg helpers ==================
static void setDefaultApConfig()
{
  strncpy(gApCfg.ssid, AP_SSID, sizeof(gApCfg.ssid));
  gApCfg.ssid[32] = 0;
  strncpy(gApCfg.pass, AP_PASSWORD, sizeof(gApCfg.pass));
  gApCfg.pass[64] = 0;
}
static bool loadApConfig()
{
  if (!SPIFFS.begin(true))
    return false;
  if (!SPIFFS.exists(AP_CFG_PATH))
  {
    setDefaultApConfig();
    return true;
  }
  File f = SPIFFS.open(AP_CFG_PATH, "r");
  if (!f)
  {
    setDefaultApConfig();
    return false;
  }
  String ssid = f.readStringUntil('\n'); ssid.trim();
  String pass = f.readStringUntil('\n'); pass.trim();
  f.close();
  if (ssid.isEmpty()) ssid = AP_SSID;
  if (pass.isEmpty()) pass = AP_PASSWORD;
  ssid.toCharArray(gApCfg.ssid, sizeof(gApCfg.ssid));
  pass.toCharArray(gApCfg.pass, sizeof(gApCfg.pass));
  return true;
}
static bool saveApConfig(const String &ssid, const String &pass)
{
  if (!SPIFFS.begin(true))
    return false;
  File f = SPIFFS.open(AP_CFG_PATH, "w");
  if (!f)
    return false;
  f.println(ssid);
  f.println(pass);
  f.close();
  ssid.toCharArray(gApCfg.ssid, sizeof(gApCfg.ssid));
  pass.toCharArray(gApCfg.pass, sizeof(gApCfg.pass));
  return true;
}
static void setDefaultStaConfig()
{
  gStaCfg.ssid[0] = 0;
  gStaCfg.pass[0] = 0;
  gStaCfg.enabled = true;
}
static bool loadStaConfig()
{
  if (!SPIFFS.begin(true))
    return false;
  if (!SPIFFS.exists(STA_CFG_PATH))
  {
    setDefaultStaConfig();
    return true;
  }
  File f = SPIFFS.open(STA_CFG_PATH, "r");
  if (!f)
  {
    setDefaultStaConfig();
    return false;
  }
  String ssid = f.readStringUntil('\n'); ssid.trim();
  String pass = f.readStringUntil('\n'); pass.trim();
  String en = f.readStringUntil('\n');   en.trim();
  f.close();
  ssid.toCharArray(gStaCfg.ssid, sizeof(gStaCfg.ssid));
  pass.toCharArray(gStaCfg.pass, sizeof(gStaCfg.pass));
  gStaCfg.enabled = (en == "1");
  return true;
}
static bool saveStaConfig(const String &ssid, const String &pass, bool enabled)
{
  if (!SPIFFS.begin(true))
    return false;
  File f = SPIFFS.open(STA_CFG_PATH, "w");
  if (!f)
    return false;
  f.println(ssid);
  f.println(pass);
  f.println(enabled ? "1" : "0");
  f.close();
  ssid.toCharArray(gStaCfg.ssid, sizeof(gStaCfg.ssid));
  pass.toCharArray(gStaCfg.pass, sizeof(gStaCfg.pass));
  gStaCfg.enabled = enabled;
  return true;
}

// ================== PRETTY WS frame line (UI-compatible) ==================
static inline void appendPrettyFrame(const CANMessage &f)
{
  char line[160];
  char idbuf[10];

  if (f.ext)
    snprintf(idbuf, sizeof(idbuf), "%08lX", f.id);
  else
    snprintf(idbuf, sizeof(idbuf), "%03lX", f.id);

  int n = snprintf(
      line, sizeof(line),
      "[ID:0x%s %s %s DLC:%u Data:",
      idbuf,
      f.ext ? "EXT" : "STD",
      f.rtr ? "RTR" : "DAT",
      (unsigned)f.len);

  for (uint8_t i = 0; i < f.len && i < 8; i++)
  {
    if (n + 3 >= (int)sizeof(line))
      break;
    n += snprintf(line + n, sizeof(line) - n, " %02X", f.data[i]);
  }
  if (n + 2 < (int)sizeof(line))
  {
    line[n++] = ']';
    line[n++] = '\n';
  }

  if (gWsLen + (size_t)n > WS_BATCH_BYTES)
    wsFlushIfNeeded(ws, true);
  if ((size_t)n <= (WS_BATCH_BYTES - gWsLen))
  {
    memcpy(gWsBuf + gWsLen, line, (size_t)n);
    gWsLen += (size_t)n;
  }
  else
  {
    ws.broadcastTXT((const uint8_t *)line, (size_t)n);
  }
}

// ================== Reconfigure (ACAN2515) ==================
static bool reconfigure(uint16_t kbps)
{
  ACAN2515Settings settings(QUARTZ_FREQUENCY, (unsigned)(kbps * 1000U));
  settings.mRequestedMode = ackMode ? ACAN2515Settings::NormalMode
                                    : ACAN2515Settings::ListenOnlyMode;
  settings.mTripleSampling = (kbps <= 125);
  settings.mReceiveBufferSize = 128;

  const uint16_t ec = can.begin(settings, []{ can.isr(); });
  if (ec != 0)
  {
    char msg[64];
    snprintf(msg, sizeof(msg), "ACAN2515 config error 0x%X", ec);
    ws.broadcastTXT(msg);
    oledShowIP(msg);
    canReady = false;
    return false;
  }
  canReady = true;
  currentKbps = kbps;
  char ok[64];
  snprintf(ok, sizeof(ok), "ACAN2515 initialized @ %u kbps (%s)",
           kbps, ackMode ? "NORMAL" : "LISTEN");
  ws.broadcastTXT(ok);

oledBump("CAN updated", 1000); // flash + wake + reset idle timer

  oledShowIP("CAN updated");
  return true;
}

// ================== WebSocket events ==================
void onWsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
    case WStype_CONNECTED:
    {
      ws.sendTXT(num, "WebCAN ready (web-only).");
      if (eg) xEventGroupSetBits(eg, BIT_WS_HAS_CLIENT);
      wsFlushIfNeeded(ws, true);
      oledBump("WS connected", 800);
      break;
    }
    case WStype_DISCONNECTED:
    {
      if (ws.connectedClients() == 0 && eg) {
        xEventGroupClearBits(eg, BIT_WS_HAS_CLIENT);
      }
      oledBump("WS closed", 800);
      break;
    }
    case WStype_TEXT:
      // web-only: ignore text/SLCAN; UI uses HTTP
      break;
    default:
      break;
  }
}

// ================== Wi-Fi ==================
static void startWiFi()
{
  loadApConfig();
  loadStaConfig();

  if (gStaCfg.enabled && strlen(gStaCfg.ssid) > 0)
  {
    WiFi.mode(WIFI_STA);
    WiFi.begin(gStaCfg.ssid, gStaCfg.pass);
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000)
    {
      delay(200);
    }
    if (WiFi.status() == WL_CONNECTED)
    {
      oledBump("Connected", 1000);
      oledShowIP("Connected");
      return;
    }
  }
  WiFi.mode(WIFI_AP);
  WiFi.softAP(gApCfg.ssid, gApCfg.pass, AP_CHANNEL, AP_HIDDEN, AP_MAX_CONN);
  oledBump("AP active", 1000);
  oledShowIP("AP active");
}

// ================== HTTP (serves PROGMEM INDEX_HTML + JSON API) ==================
static void setupHttp()
{
  http.on("/", HTTP_GET, []()
  {
    http.send_P(200, "text/html", INDEX_HTML);
  });

  // Simple local health probe
  http.on("/api/ping", HTTP_GET, []()
  {
    http.send(200, "application/json", "{\"ok\":1}");
  });

  // Reset device
  http.on("/api/reset", HTTP_POST, []()
  {
    http.send(200, "application/json", "{\"ok\":1,\"msg\":\"Rebooting...\"}");
    delay(300);
    ESP.restart();
  });

  // AP config
  http.on("/api/apcfg", HTTP_GET, []()
  {
    char buf[200];
    snprintf(buf, sizeof(buf), "{\"ssid\":\"%s\",\"pass\":\"%s\"}", gApCfg.ssid, gApCfg.pass);
    http.send(200, "application/json", buf);
  });
  http.on("/api/apcfg", HTTP_POST, []()
  {
    if (!http.hasArg("ssid") || !http.hasArg("pass")) { http.send(400,"application/json","{\"ok\":0,\"err\":\"missing_params\"}"); return; }
    String ssid=http.arg("ssid"); ssid.trim();
    String pass=http.arg("pass"); pass.trim();
    if (ssid.length()<1 || ssid.length()>32) { http.send(400,"application/json","{\"ok\":0,\"err\":\"ssid_len\"}"); return; }
    if (pass.length()<8 || pass.length()>64) { http.send(400,"application/json","{\"ok\":0,\"err\":\"pass_len\"}"); return; }
    if (saveApConfig(ssid, pass)) http.send(200,"application/json","{\"ok\":1}");
    else                          http.send(500,"application/json","{\"ok\":0,\"err\":\"save_failed\"}");
  });

  // STA config
  http.on("/api/stacfg", HTTP_GET, []()
  {
    char buf[256];
    snprintf(buf, sizeof(buf), "{\"ssid\":\"%s\",\"pass\":\"%s\",\"enabled\":%s}",
             gStaCfg.ssid, gStaCfg.pass, gStaCfg.enabled ? "true" : "false");
    http.send(200, "application/json", buf);
  });
  http.on("/api/stacfg", HTTP_POST, []()
  {
    if (!http.hasArg("enabled") || !http.hasArg("ssid") || !http.hasArg("pass")) { http.send(400,"application/json","{\"ok\":0,\"err\":\"missing_params\"}"); return; }
    String ssid=http.arg("ssid"); ssid.trim();
    String pass=http.arg("pass"); pass.trim();
    bool enabled = http.arg("enabled") == "1";
    if (ssid.length()>32) { http.send(400,"application/json","{\"ok\":0,\"err\":\"ssid_len\"}"); return; }
    if (!ssid.isEmpty() && (pass.length()>64)) { http.send(400,"application/json","{\"ok\":0,\"err\":\"pass_len\"}"); return; }
    if (saveStaConfig(ssid, pass, enabled)) http.send(200,"application/json","{\"ok\":1}");
    else                                    http.send(500,"application/json","{\"ok\":0,\"err\":\"save_failed\"}");
  });

  // ===== CAN control API (no SLCAN) =====
  // POST /api/can/open  form: kbps=<10|20|50|100|125|250|500|800|1000>&mode=<normal|listen>
  http.on("/api/can/open", HTTP_POST, []()
  {
    if (!http.hasArg("kbps") || !http.hasArg("mode")) { http.send(400, "application/json", "{\"ok\":0,\"err\":\"missing_params\"}"); return; }
    uint16_t kbps = (uint16_t) http.arg("kbps").toInt();
    String mode = http.arg("mode"); mode.toLowerCase();
    if (!(kbps==10||kbps==20||kbps==50||kbps==100||kbps==125||kbps==250||kbps==500||kbps==800||kbps==1000)) {
      http.send(400, "application/json", "{\"ok\":0,\"err\":\"bad_kbps\"}"); return;
    }
    ackMode = (mode=="normal");
    if (!reconfigure(kbps)) { http.send(500,"application/json","{\"ok\":0,\"err\":\"acan_init\"}"); return; }
    http.send(200, "application/json", "{\"ok\":1}");
  });

  // POST /api/can/close
  http.on("/api/can/close", HTTP_POST, []()
  {
    // If your ACAN version supports can.end(), you can call it here.
    // can.end();
    canReady = false; // soft-close; tasks stop reading
    http.send(200, "application/json", "{\"ok\":1}");
  });

  // POST /api/can/send (form): id,ext(0/1),rtr(0/1),dlc(0..8),data hex concat or space-separated
  http.on("/api/can/send", HTTP_POST, []()
  {
    if (!canReady) { http.send(400,"application/json","{\"ok\":0,\"err\":\"not_ready\"}"); return; }
    if (!http.hasArg("id") || !http.hasArg("dlc")) { http.send(400,"application/json","{\"ok\":0,\"err\":\"missing_params\"}"); return; }

    // --- parse ID (hex) ---
    String idS = http.arg("id"); idS.trim();
    bool ext = http.hasArg("ext") ? (http.arg("ext")=="1") : (idS.length()>3);
    bool rtr = http.hasArg("rtr") ? (http.arg("rtr")=="1") : false;
    uint8_t dlc = (uint8_t) constrain(http.arg("dlc").toInt(), 0, 8);

    if (idS.startsWith("0x")||idS.startsWith("0X")) idS.remove(0,2);
    if (idS.length() < 1 || idS.length() > 8) { http.send(400,"application/json","{\"ok\":0,\"err\":\"bad_id_len\"}"); return; }

    uint32_t id = 0;
    for (int i=0; i<idS.length(); ++i) {
      char c=idS[i]; uint8_t v;
      if      (c>='0'&&c<='9') v=c-'0';
      else if (c>='a'&&c<='f') v=10+(c-'a');
      else if (c>='A'&&c<='F') v=10+(c-'A');
      else { http.send(400,"application/json","{\"ok\":0,\"err\":\"bad_id_char\"}"); return; }
      id = (id<<4)|v;
    }

    // --- DATA: supports continuous hex or spaced/commas ---
    uint8_t bytes[8] = {0};
    if (!rtr && dlc>0 && http.hasArg("data")) {
      String data = http.arg("data");
      data.toUpperCase(); data.replace(",", " "); data.replace("0X", ""); data.trim();
      if (data.length()) {
        String flat = data; flat.replace(" ", "");
        if (flat.length() % 2) flat = "0" + flat; // left-pad odd nibble
        uint8_t bi=0;
        auto hx = [](char c)->int{
          if(c>='0'&&c<='9')return c-'0';
          if(c>='A'&&c<='F')return 10+(c-'A');
          return -1;
        };
        for (int i=0; i<flat.length() && bi<dlc; i+=2){
          int v1=hx(flat[i]), v2=hx(flat[i+1]);
          if (v1<0||v2<0) { http.send(400,"application/json","{\"ok\":0,\"err\":\"bad_data\"}"); return; }
          bytes[bi++] = (uint8_t)((v1<<4)|v2);
        }
        if (bi < dlc) dlc = bi;
      }
    }

    // --- enqueue for CAN task ---
    FrameLite f{};
    f.id = id;
    f.len = dlc;
    f.flags = (ext ? 1 : 0) | (rtr ? 2 : 0);
    memcpy(f.data, bytes, dlc);

    if (xQueueSend(qTx, &f, 0) == pdTRUE) http.send(200,"application/json","{\"ok\":1}");
                                          
    else                                  http.send(503,"application/json","{\"ok\":0,\"err\":\"tx_queue_full\"}");
  });

  http.begin();
}

// ================== Setup RTOS pipelines ==================
static void startRtosPipelines() {
  // depth N means queue can buffer N frames
  qRx = xQueueCreate(512, sizeof(FrameLite));
  qTx = xQueueCreate(128, sizeof(FrameLite));
  eg  = xEventGroupCreate();

  // CAN Task: pin to APP CPU (1) on ESP32
  xTaskCreatePinnedToCore(
    canTask, "canTask", 4096, nullptr, 20, &hCanTask, 1);

  // Net Task: pin to PRO CPU (0)
  xTaskCreatePinnedToCore(
    netTask, "netTask", 6144, nullptr, 18, &hNetTask, 0);
}

// ================== Setup ==================
void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  oledInit();
  delay(50);
  gLastActivity = millis(); // start the idle timer

  led.begin();
  led.on();

  loadApConfig();
  startWiFi();
  setupHttp();

  ws.begin();
  ws.onEvent(onWsEvent);

  // SPI + MCP2515 INT
  SPI.begin(MCP2515_SCK, MCP2515_MISO, MCP2515_MOSI);
  SPI.setFrequency(8000000);
  pinMode(MCP2515_INT, INPUT_PULLUP);

  // Default bitrate (opened immediately)
  ws.broadcastTXT("ESP32 + ACAN2515 Web Terminal (web-only)");
  reconfigure(currentKbps);

  // Start FreeRTOS pipelines
  startRtosPipelines();
}

// ================== Loop ==================
void loop(){
  // Nothing heavy here; tasks do the work
  oledTick();  
  vTaskDelay(1);
}

// ================== CAN Task (Core 1) ==================
static void canTask(void*){
  // Tune these
  const TickType_t rxPollDelay = pdMS_TO_TICKS(1);   // small sleep when idle
  const uint16_t   maxDrain    = 256;                // per cycle

  for(;;){
    // 1) Drain TX queue (non-blocking)
    FrameLite txf;
    while (xQueueReceive(qTx, &txf, 0) == pdTRUE) {
      CANMessage tx;
      tx.id  = txf.id;
      tx.ext = fl_ext(txf);
      tx.rtr = fl_rtr(txf);
      tx.len = txf.len;
      memcpy(tx.data, txf.data, txf.len);

      // try repeatedly for a short time to avoid drops under load
      for (int i=0;i<3;i++){
        if (can.tryToSend(tx)) break;
        vTaskDelay(pdMS_TO_TICKS(1));
      }
    }

    // 2) Drain CAN RX
    uint16_t drained = 0;
    CANMessage rx;
    while (canReady && can.available() && drained < maxDrain) {
      can.receive(rx);
      FrameLite f{};
      f.id = rx.id;
      f.len = rx.len;
      f.flags = (rx.ext?1:0) | (rx.rtr?2:0);
      memcpy(f.data, rx.data, rx.len);

      // if qRx is full, drop oldest to keep UI fresh (optional)
      if (xQueueSend(qRx, &f, 0) != pdTRUE) {
        FrameLite dummy;
        xQueueReceive(qRx, &dummy, 0);      // pop one
        xQueueSend(qRx, &f, 0);             // push current
      }
      drained++;
    }

    // Back off a little to yield CPU/Wi-Fi
    if (drained == 0) vTaskDelay(rxPollDelay);
    else taskYIELD();
  }
}

// ================== Net Task (Core 0) ==================
static void netTask(void*){
  // WS batching config (same idea as before)
  static char  wsBuf[1536];
  size_t       wsLen = 0;
  uint32_t     lastFlush = millis();

  auto flush = [&](){
    if (wsLen == 0) return;
    EventBits_t b = xEventGroupGetBits(eg);
    if (b & BIT_WS_HAS_CLIENT) {
      ws.broadcastTXT((const uint8_t*)wsBuf, wsLen);
    }
    wsLen = 0;
    lastFlush = millis();
  };

  // pretty line builder for FrameLite
  auto appendPretty = [&](const FrameLite& f){
    char line[160];
    char idbuf[10];
    if (fl_ext(f)) snprintf(idbuf, sizeof(idbuf), "%08lX", f.id);
    else           snprintf(idbuf, sizeof(idbuf), "%03lX",  f.id);
    int n = snprintf(line, sizeof(line), "[ID:0x%s %s %s DLC:%u Data:",
        idbuf, fl_ext(f)?"EXT":"STD", fl_rtr(f)?"RTR":"DAT", f.len);
    for (uint8_t i=0;i<f.len && i<8;i++) n += snprintf(line+n, sizeof(line)-n, " %02X", f.data[i]);
    if (n+2 < (int)sizeof(line)) { line[n++]=']'; line[n++]='\n'; }
    if (wsLen + (size_t)n > sizeof(wsBuf)) flush();
    if ((size_t)n <= sizeof(wsBuf) - wsLen) {
      memcpy(wsBuf + wsLen, line, n); wsLen += (size_t)n;
    } else {
      // doesn't fit: flush then direct
      flush(); ws.broadcastTXT((const uint8_t*)line, (size_t)n);
    }
  };

  const TickType_t tick5 = pdMS_TO_TICKS(5);
  for(;;){
    // Service servers
    http.handleClient();
    ws.loop();

    // Keepalive (optional; or use a SW timer)
    static uint32_t lastKA = 0;
    uint32_t now = millis();
    if (now - lastKA >= 5000) {
      static const char ka[] = "{\"type\":\"ka\"}";
      if (ws.connectedClients() > 0) ws.broadcastTXT(ka, sizeof(ka)-1);
      lastKA = now;
    }

    // Drain some frames for ~5ms
    uint32_t tStart = millis();
    FrameLite f;
    while ( (millis() - tStart) < 5 ) {
      if (xQueueReceive(qRx, &f, 0) != pdTRUE) break;
      appendPretty(f);
    }

    // Time-based flush (8–10 ms)
    if (now - lastFlush >= 8) flush();

    vTaskDelay(tick5); // yields, ~5 ms period
  }
}
