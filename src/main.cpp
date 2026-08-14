// =========================================================
// WebCan Terminal (Web-only)
// ESP32 + MCP2515 (ACAN2515) + Secure MQTT (MQTTS)
// =========================================================

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ACAN2515.h>
#include "ledstatus.h"
#include <FS.h>
#include <SPIFFS.h>
#include <string.h>
#include "time.h"
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <ESPmDNS.h>
#include "webInterface.h" // Provides INDEX_HTML (PROGMEM)
#include <Adafruit_MCP23X17.h>

Adafruit_MCP23X17 mcp;

#define IMMO_A 1
#define IMMO_B 0

bool termResistorEnabled = false; // Tracks the current state

// ================== Global Objects ==================
WiFiClientSecure secureClient;
PubSubClient mqtt(secureClient);
WebServer http(80);
WebSocketsServer ws(81); // ws://<ip>:81/
LedStatus led;

const char *root_ca = nullptr;

// ================== ESP32 <-> MCP2515 Pins ==================
static const byte MCP2515_SCK = 14;
static const byte MCP2515_MOSI = 13;
static const byte MCP2515_MISO = 12;

// CAN 1
static const byte MCP2515_1_CS = 26;
static const byte MCP2515_1_INT = 25;

// CAN 2
static const byte MCP2515_2_CS = 33;
static const byte MCP2515_2_INT = 27;

ACAN2515 can1(MCP2515_1_CS, SPI, MCP2515_1_INT);
ACAN2515 can2(MCP2515_2_CS, SPI, MCP2515_2_INT);
static const uint32_t QUARTZ_FREQUENCY = 16UL * 1000UL * 1000UL; // 16 MHz

// ================== RTOS Structures & Queues ==================
struct FrameLite
{
  uint32_t id;
  uint8_t len;
  uint8_t flags;
  uint8_t bus;
  uint8_t data[8];
};

inline bool fl_ext(const FrameLite &f) { return (f.flags & 0x01) != 0; }
inline bool fl_rtr(const FrameLite &f) { return (f.flags & 0x02) != 0; }

static QueueHandle_t qRx = nullptr;     // CAN -> Net
static QueueHandle_t qTx = nullptr;     // Net -> CAN
static QueueHandle_t qMqttTx = nullptr; // CAN -> MQTT

static EventGroupHandle_t eg = nullptr;
static const EventBits_t BIT_WS_HAS_CLIENT = (1 << 0);

static uint32_t blockedIds[64];
static uint8_t blockedIdCount = 0;

// Protects blockedIds/blockedIdCount (read in canTask, rewritten in HTTP handler)
static portMUX_TYPE gBlockedMux = portMUX_INITIALIZER_UNLOCKED;

// Serializes CAN controller access between canTask and reconfigure()
static SemaphoreHandle_t canMutex = nullptr;

inline bool isBlocked(uint32_t id)
{
  bool blocked = false;
  portENTER_CRITICAL(&gBlockedMux);
  for (uint8_t i = 0; i < blockedIdCount; i++)
  {
    if (blockedIds[i] == id)
    {
      blocked = true;
      break;
    }
  }
  portEXIT_CRITICAL(&gBlockedMux);
  return blocked;
}
// --------------------------------------------

// ================== State & Configuration ==================
enum SystemStatus
{
  STATUS_IDLE,
  STATUS_CONNECTING,
  STATUS_CONNECTED,
  STATUS_ERROR
};

// 'volatile' ensures cross-core writes are visible and not cached in registers.
// Controller-level operations are additionally serialized with canMutex.
static volatile bool canReady = false;
static volatile bool bridgeMode = true;
static volatile bool ackMode = true;
static uint16_t currentKbps = 500;

// Cached MQTT state (mirrors gMqttCfg.enabled / mqtt.connected() for cross-task reads)
static volatile bool mqttEnabledFlag = false;
static volatile bool mqttConnectedFlag = false;

// Dynamic Manipulation State
static volatile bool manipEnabled = false;
static volatile uint32_t manipTargetId = 0;        // The specific CAN ID to target
static volatile bool manipRequireTargetId = false; // False = apply to all IDs
static volatile bool manipZeroMode = false;        // True = Zero payload, False = Single byte
static volatile uint8_t manipFilterByte = 0x21;    // The byte to look for at index 0
static volatile uint8_t manipByteIndex = 4;        // The byte position to change (0 to 7)
static volatile uint8_t manipNewValue = 0x00;      // The new hex value to inject

// ================== Config Structs ==================
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

struct MqttConfig
{
  char server[65];
  uint16_t port;
  char user[33];
  char pass[65];
  char subTopic[65];
  char pubTopic[65];
  bool enabled;
};
static MqttConfig gMqttCfg;
static const char *MQTT_CFG_PATH = "/mqttcfg.txt";

const char *AP_SSID = "WebCan";
const char *AP_PASSWORD = "12345678";
static const uint8_t AP_CHANNEL = 6;
static const bool AP_HIDDEN = false;
static const uint8_t AP_MAX_CONN = 4;

// ================== Forward Declarations ==================
static void canTask(void *pv);
static void netTask(void *pv);
static void mqttTask(void *pv);
static void startRtosPipelines();
static void mqttCallback(char *topic, byte *payload, unsigned int length);
static bool reconfigure(uint16_t kbps);

// =========================================================
//                   TIME & CERTIFICATE
// =========================================================
bool syncTime()
{
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  unsigned long t0 = millis();
  while (now < 8 * 3600 * 2)
  {
    if (millis() - t0 > 10000UL)
    {
      Serial.println(" Time sync FAILED (timeout)!");
      return false;
    }
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println(" Time synced!");
  return true;
}

const char *loadCertificateBuffer(const char *path)
{
  Serial.printf("\n[Cert] Attempting to load from SPIFFS: %s\n", path);

  if (!SPIFFS.exists(path))
  {
    Serial.println("[Cert] ERROR: File does not exist!");
    return nullptr;
  }

  File file = SPIFFS.open(path, "r");
  if (!file)
  {
    Serial.println("[Cert] ERROR: Failed to open file for reading!");
    return nullptr;
  }

  size_t fileSize = file.size();
  Serial.printf("[Cert] File opened successfully. Size: %zu bytes\n", fileSize);

  if (fileSize == 0)
  {
    Serial.println("[Cert] ERROR: File is completely empty (0 bytes)!");
    file.close();
    return nullptr;
  }

  char *buf = new char[fileSize + 1];
  if (!buf)
  {
    Serial.println("[Cert] ERROR: RAM Memory allocation failed!");
    file.close();
    return nullptr;
  }

  file.readBytes(buf, fileSize);
  buf[fileSize] = '\0';
  file.close();

  Serial.println("[Cert] SUCCESS! Certificate loaded into RAM.");
  Serial.println("=== CERTIFICATE PREVIEW ===");
  String certStr = String(buf);
  if (certStr.length() > 120)
  {
    Serial.println(certStr.substring(0, 60));
    Serial.println("... [snip] ...");
    Serial.println(certStr.substring(certStr.length() - 60));
  }
  else
  {
    Serial.println(certStr);
  }
  Serial.println("===========================\n");

  return buf;
}

void setupSecureMQTT()
{
  Serial.println("[MQTTS] Initializing Secure MQTT...");
  if (!syncTime())
  {
    Serial.println("[MQTTS] WARNING: NTP sync failed - TLS handshake will likely fail.");
  }

  // Prevent memory leak if re-initialized
  if (root_ca != nullptr)
  {
    delete[] (char *)root_ca;
    root_ca = nullptr;
  }

  root_ca = loadCertificateBuffer("/isrgrootx1.pem");

  if (root_ca)
  {
    secureClient.setCACert(root_ca);
    Serial.println("[MQTTS] CA Certificate applied to secure client.");
  }
  else
  {
    Serial.println("[MQTTS] FATAL: Failed to load CA! Handshake will fail (State -2).");
  }

  mqtt.setServer(gMqttCfg.server, gMqttCfg.port);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(2048);

  Serial.printf("[MQTTS] Configured for Broker: %s:%d\n", gMqttCfg.server, gMqttCfg.port);
}

// =========================================================
//                   CONFIGURATION (SPIFFS)
// =========================================================
static void setDefaultMqttConfig()
{
  gMqttCfg.server[0] = 0;
  gMqttCfg.port = 1883;
  gMqttCfg.user[0] = 0;
  gMqttCfg.pass[0] = 0;
  strncpy(gMqttCfg.subTopic, "webcan/tx", sizeof(gMqttCfg.subTopic));
  strncpy(gMqttCfg.pubTopic, "webcan/rx", sizeof(gMqttCfg.pubTopic));
  gMqttCfg.enabled = false;
  mqttEnabledFlag = false;
}

static bool loadMqttConfig()
{
  if (!SPIFFS.begin(true))
    return false;
  if (!SPIFFS.exists(MQTT_CFG_PATH))
  {
    setDefaultMqttConfig();
    return true;
  }
  File f = SPIFFS.open(MQTT_CFG_PATH, "r");
  if (!f)
  {
    setDefaultMqttConfig();
    return false;
  }

  String server = f.readStringUntil('\n');
  server.trim();
  String portStr = f.readStringUntil('\n');
  portStr.trim();
  String user = f.readStringUntil('\n');
  user.trim();
  String pass = f.readStringUntil('\n');
  pass.trim();
  String sub = f.readStringUntil('\n');
  sub.trim();
  String pub = f.readStringUntil('\n');
  pub.trim();
  String en = f.readStringUntil('\n');
  en.trim();
  f.close();

  server.toCharArray(gMqttCfg.server, sizeof(gMqttCfg.server));
  gMqttCfg.port = portStr.toInt() > 0 ? portStr.toInt() : 1883;
  user.toCharArray(gMqttCfg.user, sizeof(gMqttCfg.user));
  pass.toCharArray(gMqttCfg.pass, sizeof(gMqttCfg.pass));

  if (sub.length() > 0)
    sub.toCharArray(gMqttCfg.subTopic, sizeof(gMqttCfg.subTopic));
  else
    strncpy(gMqttCfg.subTopic, "webcan/tx", sizeof(gMqttCfg.subTopic));

  if (pub.length() > 0)
    pub.toCharArray(gMqttCfg.pubTopic, sizeof(gMqttCfg.pubTopic));
  else
    strncpy(gMqttCfg.pubTopic, "webcan/rx", sizeof(gMqttCfg.pubTopic));

  gMqttCfg.enabled = (en == "1");
  mqttEnabledFlag = gMqttCfg.enabled;
  return true;
}

static bool saveMqttConfig(const String &server, uint16_t port, const String &user, const String &pass, const String &sub, const String &pub, bool enabled)
{
  if (!SPIFFS.begin(true))
    return false;
  File f = SPIFFS.open(MQTT_CFG_PATH, "w");
  if (!f)
    return false;

  f.println(server);
  f.println(port);
  f.println(user);
  f.println(pass);
  f.println(sub);
  f.println(pub);
  f.println(enabled ? "1" : "0");
  f.close();

  server.toCharArray(gMqttCfg.server, sizeof(gMqttCfg.server));
  gMqttCfg.port = port;
  user.toCharArray(gMqttCfg.user, sizeof(gMqttCfg.user));
  pass.toCharArray(gMqttCfg.pass, sizeof(gMqttCfg.pass));
  sub.toCharArray(gMqttCfg.subTopic, sizeof(gMqttCfg.subTopic));
  pub.toCharArray(gMqttCfg.pubTopic, sizeof(gMqttCfg.pubTopic));
  mqttEnabledFlag = enabled;
  gMqttCfg.enabled = enabled;
  return true;
}

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

  String ssid = f.readStringUntil('\n');
  ssid.trim();
  String pass = f.readStringUntil('\n');
  pass.trim();
  f.close();

  if (ssid.isEmpty())
    ssid = AP_SSID;
  if (pass.isEmpty())
    pass = AP_PASSWORD;

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

  String ssid = f.readStringUntil('\n');
  ssid.trim();
  String pass = f.readStringUntil('\n');
  pass.trim();
  String en = f.readStringUntil('\n');
  en.trim();
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

// =========================================================
//                   WIFI & NETWORK
// =========================================================
static void startWiFi()
{
  loadApConfig();
  loadStaConfig();

  if (gStaCfg.enabled && strlen(gStaCfg.ssid) > 0)
  {
    Serial.printf("[WiFi] Attempting to connect to STA: %s\n", gStaCfg.ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(gStaCfg.ssid, gStaCfg.pass);

    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 8000)
    {
      delay(200);
    }

    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.print("[WiFi] Connected! STA IP Address: ");
      Serial.println(WiFi.localIP());

      // --- START mDNS HERE ---
      if (MDNS.begin("webcan"))
      {
        Serial.println("[mDNS] Responder started. You can now access it at http://webcan.local");
      }
      // -----------------------

      return;
    }
    Serial.println("[WiFi] STA Connection failed. Falling back to AP mode.");
  }

  // Fallback or intentionally booting as AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(gApCfg.ssid, gApCfg.pass, AP_CHANNEL, AP_HIDDEN, AP_MAX_CONN);
  Serial.print("[WiFi] Access Point started. AP IP Address: ");
  Serial.println(WiFi.softAPIP());
}
void onWsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
  switch (type)
  {
  case WStype_CONNECTED:
    ws.sendTXT(num, "WebCAN ready (web-only).");
    if (eg)
      xEventGroupSetBits(eg, BIT_WS_HAS_CLIENT);
    break;
  case WStype_DISCONNECTED:
    if (ws.connectedClients() == 0 && eg)
    {
      xEventGroupClearBits(eg, BIT_WS_HAS_CLIENT);
    }
    break;
  case WStype_TEXT:
    break;
  default:
    break;
  }
}

void toggleTerminationResistor(bool enable) {
  if (enable) {
    mcp.digitalWrite(IMMO_A, HIGH);
    mcp.digitalWrite(IMMO_B, LOW);
  } else {
    mcp.digitalWrite(IMMO_A, LOW);
    mcp.digitalWrite(IMMO_B, HIGH);
  }
  
  // 40ms pulse to latch the relay
  // Note: Using delay() is fine here if called via an HTTP handler task, 
  // otherwise use vTaskDelay(pdMS_TO_TICKS(40)) if strict FreeRTOS is required.
  delay(40); 
  
  // Unpower coils
  mcp.digitalWrite(IMMO_A, LOW);
  mcp.digitalWrite(IMMO_B, LOW);
  
  termResistorEnabled = enable;
}

// =========================================================
//                   HTTP SERVER SETUP
// =========================================================
static void setupHttp()
{

  // API: Toggle Termination Resistors
  http.on("/api/can/term", HTTP_POST, []() {
    if (http.hasArg("enable")) {
      bool enable = (http.arg("enable") == "1");
      toggleTerminationResistor(enable);
      http.send(200, "application/json", "{\"ok\":1}");
    } else {
      http.send(400, "application/json", "{\"ok\":0}");
    }
  });
  
  // API: Block IDs in Bridge
  http.on("/api/can/block", HTTP_POST, []()
          {
    if (http.hasArg("ids")) {
      String idList = http.arg("ids");
      portENTER_CRITICAL(&gBlockedMux);
      blockedIdCount = 0;
      int start = 0;
      while (start < idList.length() && blockedIdCount < 64) {
        int comma = idList.indexOf(',', start);
        if (comma == -1) comma = idList.length();
        String idStr = idList.substring(start, comma);
        idStr.trim();
        if (idStr.length() > 0) {
          blockedIds[blockedIdCount++] = strtoul(idStr.c_str(), NULL, 16);
        }
        start = comma + 1;
      }
      portEXIT_CRITICAL(&gBlockedMux);
      http.send(200, "application/json", "{\"ok\":1}");
    } else {
      http.send(400, "application/json", "{\"ok\":0,\"err\":\"missing_ids\"}");
    } });

  // API: Get MQTT Config
  http.on("/api/mqttcfg", HTTP_GET, []()
          {
    char buf[512];
    snprintf(buf, sizeof(buf), 
             "{\"server\":\"%s\",\"port\":%u,\"user\":\"%s\",\"pass\":\"%s\",\"subTopic\":\"%s\",\"pubTopic\":\"%s\",\"enabled\":%s}",
             gMqttCfg.server, gMqttCfg.port, gMqttCfg.user, gMqttCfg.pass, 
             gMqttCfg.subTopic, gMqttCfg.pubTopic, gMqttCfg.enabled ? "true" : "false");
    http.send(200, "application/json", buf); });

  // API: Post MQTT Config
  http.on("/api/mqttcfg", HTTP_POST, []()
          {
    if (!http.hasArg("enabled") || !http.hasArg("server") || !http.hasArg("port")) { 
      http.send(400, "application/json", "{\"ok\":0,\"err\":\"missing_params\"}"); 
      return; 
    }
    
    String server = http.arg("server"); server.trim();
    uint16_t port = http.arg("port").toInt();
    String user = http.hasArg("user") ? http.arg("user") : ""; user.trim();
    String pass = http.hasArg("pass") ? http.arg("pass") : ""; pass.trim();
    String subTopic = http.hasArg("subTopic") ? http.arg("subTopic") : ""; subTopic.trim();
    String pubTopic = http.hasArg("pubTopic") ? http.arg("pubTopic") : ""; pubTopic.trim(); 
    bool enabled = http.arg("enabled") == "1";

    if (server.length() > 64) { 
      http.send(400, "application/json", "{\"ok\":0,\"err\":\"server_len\"}"); 
      return; 
    }

    if (saveMqttConfig(server, port, user, pass, subTopic, pubTopic, enabled)) {
      http.send(200, "application/json", "{\"ok\":1}");
    } else {
      http.send(500, "application/json", "{\"ok\":0,\"err\":\"save_failed\"}");
    } });

  // API: Manipulate Data
  http.on("/api/can/manip", HTTP_POST, []()
          {
    if (http.hasArg("enable")) manipEnabled = (http.arg("enable") == "1");
    if (http.hasArg("filter")) manipFilterByte = (uint8_t)strtol(http.arg("filter").c_str(), NULL, 16);
    if (http.hasArg("index"))  manipByteIndex = (uint8_t)constrain(http.arg("index").toInt(), 0, 7);
    if (http.hasArg("val"))    manipNewValue = (uint8_t)strtol(http.arg("val").c_str(), NULL, 16);
    if (http.hasArg("zero"))   manipZeroMode = (http.arg("zero") == "1");
    
    if (http.hasArg("id")) {
      String idStr = http.arg("id");
      if (idStr.length() > 0) {
        manipTargetId = strtoul(idStr.c_str(), NULL, 16);
        manipRequireTargetId = true;
      } else {
        manipRequireTargetId = false; // Empty string means apply globally
      }
    }
    http.send(200, "application/json", "{\"ok\":1}"); });

  // API: Bridge Mode
  http.on("/api/can/bridge", HTTP_POST, []()
          {
    if (http.hasArg("enable")) {
      bridgeMode = (http.arg("enable") == "1");
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"ok\":1,\"bridge\":%s}", bridgeMode ? "true" : "false");
    http.send(200, "application/json", buf); });

  // Serve Main UI
  http.on("/", HTTP_GET, []()
          { http.send_P(200, "text/html", INDEX_HTML); });

  // Local Health Ping
  http.on("/api/ping", HTTP_GET, []()
          { http.send(200, "application/json", "{\"ok\":1}"); });

  // API: Reboot
  http.on("/api/reset", HTTP_POST, []()
          {
    http.send(200, "application/json", "{\"ok\":1,\"msg\":\"Rebooting...\"}");
    delay(300);
    ESP.restart(); });

  // API: AP Config Get/Post
  http.on("/api/apcfg", HTTP_GET, []()
          {
    char buf[200];
    snprintf(buf, sizeof(buf), "{\"ssid\":\"%s\",\"pass\":\"%s\"}", gApCfg.ssid, gApCfg.pass);
    http.send(200, "application/json", buf); });

  http.on("/api/apcfg", HTTP_POST, []()
          {
    if (!http.hasArg("ssid") || !http.hasArg("pass")) { 
      http.send(400, "application/json", "{\"ok\":0,\"err\":\"missing_params\"}"); return; 
    }
    String ssid=http.arg("ssid"); ssid.trim();
    String pass=http.arg("pass"); pass.trim();
    
    if (ssid.length()<1 || ssid.length()>32) { http.send(400,"application/json","{\"ok\":0,\"err\":\"ssid_len\"}"); return; }
    if (pass.length()<8 || pass.length()>64) { http.send(400,"application/json","{\"ok\":0,\"err\":\"pass_len\"}"); return; }
    
    if (saveApConfig(ssid, pass)) http.send(200, "application/json", "{\"ok\":1}");
    else http.send(500, "application/json", "{\"ok\":0,\"err\":\"save_failed\"}"); });

  // API: STA Config Get/Post
  http.on("/api/stacfg", HTTP_GET, []()
          {
    char buf[256];
    snprintf(buf, sizeof(buf), "{\"ssid\":\"%s\",\"pass\":\"%s\",\"enabled\":%s}",
             gStaCfg.ssid, gStaCfg.pass, gStaCfg.enabled ? "true" : "false");
    http.send(200, "application/json", buf); });

  http.on("/api/stacfg", HTTP_POST, []()
          {
    if (!http.hasArg("enabled") || !http.hasArg("ssid") || !http.hasArg("pass")) { 
      http.send(400, "application/json", "{\"ok\":0,\"err\":\"missing_params\"}"); return; 
    }
    String ssid=http.arg("ssid"); ssid.trim();
    String pass=http.arg("pass"); pass.trim();
    bool enabled = http.arg("enabled") == "1";
    
    if (ssid.length()>32) { http.send(400, "application/json", "{\"ok\":0,\"err\":\"ssid_len\"}"); return; }
    if (!ssid.isEmpty() && (pass.length()>64)) { http.send(400, "application/json", "{\"ok\":0,\"err\":\"pass_len\"}"); return; }
    
    if (saveStaConfig(ssid, pass, enabled)) http.send(200, "application/json", "{\"ok\":1}");
    else http.send(500, "application/json", "{\"ok\":0,\"err\":\"save_failed\"}"); });

  // API: Open CAN
  http.on("/api/can/open", HTTP_POST, []()
          {
    if (!http.hasArg("kbps") || !http.hasArg("mode")) { 
      http.send(400, "application/json", "{\"ok\":0,\"err\":\"missing_params\"}"); return; 
    }
    uint16_t kbps = (uint16_t) http.arg("kbps").toInt();
    String mode = http.arg("mode"); mode.toLowerCase();
    
    if (!(kbps==10||kbps==20||kbps==50||kbps==100||kbps==125||kbps==250||kbps==500||kbps==800||kbps==1000)) {
      http.send(400, "application/json", "{\"ok\":0,\"err\":\"bad_kbps\"}"); return;
    }
    
    ackMode = (mode=="normal");
    if (!reconfigure(kbps)) { 
      http.send(500, "application/json", "{\"ok\":0,\"err\":\"acan_init\"}"); return; 
    }
    http.send(200, "application/json", "{\"ok\":1}"); });

  // API: Close CAN
  http.on("/api/can/close", HTTP_POST, []()
          {
    canReady = false; 
    http.send(200, "application/json", "{\"ok\":1}"); });

  // API: Send CAN Frame
  http.on("/api/can/send", HTTP_POST, []()
          {
    if (!canReady) { http.send(400, "application/json", "{\"ok\":0,\"err\":\"not_ready\"}"); return; }
    if (!http.hasArg("id") || !http.hasArg("dlc")) { http.send(400, "application/json", "{\"ok\":0,\"err\":\"missing_params\"}"); return; }

    String idS = http.arg("id"); idS.trim();
    bool ext = http.hasArg("ext") ? (http.arg("ext")=="1") : (idS.length()>3);
    bool rtr = http.hasArg("rtr") ? (http.arg("rtr")=="1") : false;
    uint8_t dlc = (uint8_t) constrain(http.arg("dlc").toInt(), 0, 8);

    if (idS.startsWith("0x")||idS.startsWith("0X")) idS.remove(0,2);
    if (idS.length() < 1 || idS.length() > 8) { http.send(400, "application/json", "{\"ok\":0,\"err\":\"bad_id_len\"}"); return; }

    uint32_t id = 0;
    for (int i=0; i<idS.length(); ++i) {
      char c=idS[i]; uint8_t v;
      if      (c>='0'&&c<='9') v=c-'0';
      else if (c>='a'&&c<='f') v=10+(c-'a');
      else if (c>='A'&&c<='F') v=10+(c-'A');
      else { http.send(400, "application/json", "{\"ok\":0,\"err\":\"bad_id_char\"}"); return; }
      id = (id<<4)|v;
    }

    uint8_t bytes[8] = {0};
    if (!rtr && dlc>0 && http.hasArg("data")) {
      String data = http.arg("data");
      data.toUpperCase(); data.replace(",", " "); data.replace("0X", ""); data.trim();
      if (data.length()) {
        String flat = data; flat.replace(" ", "");
        if (flat.length() % 2) flat = "0" + flat; 
        uint8_t bi=0;
        auto hx = [](char c)->int{
          if(c>='0'&&c<='9')return c-'0';
          if(c>='A'&&c<='F')return 10+(c-'A');
          return -1;
        };
        for (int i=0; i<flat.length() && bi<dlc; i+=2){
          int v1=hx(flat[i]), v2=hx(flat[i+1]);
          if (v1<0||v2<0) { http.send(400, "application/json", "{\"ok\":0,\"err\":\"bad_data\"}"); return; }
          bytes[bi++] = (uint8_t)((v1<<4)|v2);
        }
        if (bi < dlc) dlc = bi;
      }
    }

FrameLite f{};
    f.id = id;
    f.len = dlc;
    f.flags = (ext ? 1 : 0) | (rtr ? 2 : 0);
    f.bus = 0; 
    memcpy(f.data, bytes, dlc);

    if (xQueueSend(qTx, &f, 0) == pdTRUE) {
      http.send(200, "application/json", "{\"ok\":1}");
    } else {
      http.send(503, "application/json", "{\"ok\":0,\"err\":\"tx_queue_full\"}"); 
    } });

  http.begin();
}

// =========================================================
//                   ACAN2515 INITIALIZATION
// =========================================================
static bool reconfigure(uint16_t kbps)
{
  if (canMutex)
    xSemaphoreTake(canMutex, portMAX_DELAY);

  ACAN2515Settings settings(QUARTZ_FREQUENCY, (unsigned)(kbps * 1000U));
  settings.mRequestedMode = ackMode ? ACAN2515Settings::NormalMode : ACAN2515Settings::ListenOnlyMode;
  settings.mTripleSampling = (kbps <= 125);
  settings.mReceiveBufferSize = 128;

  const uint16_t ec1 = can1.begin(settings, []
                                  { can1.isr(); });
  const uint16_t ec2 = can2.begin(settings, []
                                  { can2.isr(); });

  bool ok = true;
  if (ec1 != 0 || ec2 != 0)
  {
    char msg[64];
    snprintf(msg, sizeof(msg), "ACAN config error (1: 0x%X, 2: 0x%X)", ec1, ec2);
    ws.broadcastTXT(msg);
    canReady = false;
    ok = false;
  }
  else
  {
    canReady = true;
    currentKbps = kbps;
    char okMsg[64];
    snprintf(okMsg, sizeof(okMsg), "Dual ACAN2515 initialized @ %u kbps (%s)", kbps, ackMode ? "NORMAL" : "LISTEN");
    ws.broadcastTXT(okMsg);
  }

  if (canMutex)
    xSemaphoreGive(canMutex);
  return ok;
}

// =========================================================
//                   FREERTOS TASKS
// =========================================================
static void ledStatusTask(void *pv)
{
  const int LED_PIN = 5;
  pinMode(LED_PIN, OUTPUT);

  for (;;)
  {
    int pulses = 1;
    int ms = 50;
    int gap = 2000;

    if (!canReady)
    {
      pulses = 5;
      ms = 50;
      gap = 200;
    }
    else if (WiFi.status() != WL_CONNECTED)
    {
      pulses = 1;
      ms = 1000;
      gap = 1000;
    }
    else if (!mqttConnectedFlag && mqttEnabledFlag)
    {
      pulses = 2;
      ms = 150;
      gap = 800;
    }
    else
    {
      pulses = 1;
      ms = 50;
      gap = 2500;
    }

    for (int i = 0; i < pulses; i++)
    {
      digitalWrite(LED_PIN, HIGH);
      vTaskDelay(pdMS_TO_TICKS(ms));
      digitalWrite(LED_PIN, LOW);
      vTaskDelay(pdMS_TO_TICKS(ms));
    }
    vTaskDelay(pdMS_TO_TICKS(gap));
  }
}

static void canTask(void *)
{
  const TickType_t rxPollDelay = pdMS_TO_TICKS(1);
  const uint16_t maxDrain = 256;

  for (;;)
  {
    if (canMutex)
      xSemaphoreTake(canMutex, portMAX_DELAY);

    // 1) Drain TX queue
    FrameLite txf;
    while (xQueueReceive(qTx, &txf, 0) == pdTRUE)
    {
      CANMessage tx;
      tx.id = txf.id;
      tx.ext = fl_ext(txf);
      tx.rtr = fl_rtr(txf);
      tx.len = txf.len;
      memcpy(tx.data, txf.data, txf.len);

      for (int i = 0; i < 3; i++)
      {
        if (can1.tryToSend(tx))
          break;
        vTaskDelay(pdMS_TO_TICKS(1));
      }
    }

    uint16_t drained = 0;
    CANMessage rx;

    // 2) Drain CAN1 RX and Bridge to CAN2
    while (canReady && can1.available() && drained < maxDrain)
    {
      can1.receive(rx);

      if (manipEnabled && rx.len > 0)
      {
        bool idMatch = !manipRequireTargetId || (rx.id == manipTargetId);

        // Target the specific ID (if set) and keep the B[0] identifier check
        if (idMatch && rx.data[0] == manipFilterByte)
        {

          if (manipZeroMode)
          {
            // Keep B[0] untouched, wipe everything from index 1 to the end of DLC
            if (rx.len > 1)
            {
              memset(&rx.data[1], 0, rx.len - 1);
            }
          }
          else
          {
            // Standard single-byte overwrite
            if (rx.len > manipByteIndex)
            {
              rx.data[manipByteIndex] = manipNewValue;
            }
          }
        }
      }

      if (bridgeMode && ackMode && !isBlocked(rx.id))
      {
        can2.tryToSend(rx);
      }

      FrameLite f{};
      f.id = rx.id;
      f.len = rx.len;
      f.flags = (rx.ext ? 1 : 0) | (rx.rtr ? 2 : 0);
      f.bus = 1;
      memcpy(f.data, rx.data, rx.len);

      if (xQueueSend(qRx, &f, 0) != pdTRUE)
      {
        FrameLite dummy;
        xQueueReceive(qRx, &dummy, 0);
        xQueueSend(qRx, &f, 0);
      }

      if (mqttEnabledFlag)
      {
        if (xQueueSend(qMqttTx, &f, 0) == pdTRUE)
        {
          static int debugCount1 = 0;
          if (debugCount1++ % 100 == 0)
            Serial.println("[CAN1 -> MQTT Queue] OK");
        }
        else
        {
          FrameLite dummy;
          xQueueReceive(qMqttTx, &dummy, 0);
          xQueueSend(qMqttTx, &f, 0);
        }
      }
      drained++;
    }

    // 3) Drain CAN2 RX and Bridge to CAN1
    while (canReady && can2.available() && drained < maxDrain)
    {
      can2.receive(rx);

      if (manipEnabled && rx.len > 0)
      {
        bool idMatch = !manipRequireTargetId || (rx.id == manipTargetId);

        // Target the specific ID (if set) and keep the B[0] identifier check
        if (idMatch && rx.data[0] == manipFilterByte)
        {

          if (manipZeroMode)
          {
            // Keep B[0] untouched, wipe everything from index 1 to the end of DLC
            if (rx.len > 1)
            {
              memset(&rx.data[1], 0, rx.len - 1);
            }
          }
          else
          {
            // Standard single-byte overwrite
            if (rx.len > manipByteIndex)
            {
              rx.data[manipByteIndex] = manipNewValue;
            }
          }
        }
      }

      if (bridgeMode && ackMode && !isBlocked(rx.id))
      {
        can1.tryToSend(rx);
      }

      FrameLite f{};
      f.id = rx.id;
      f.len = rx.len;
      f.flags = (rx.ext ? 1 : 0) | (rx.rtr ? 2 : 0);
      f.bus = 2;
      memcpy(f.data, rx.data, rx.len);

      if (xQueueSend(qRx, &f, 0) != pdTRUE)
      {
        FrameLite dummy;
        xQueueReceive(qRx, &dummy, 0);
        xQueueSend(qRx, &f, 0);
      }

      if (mqttEnabledFlag)
      {
        if (xQueueSend(qMqttTx, &f, 0) == pdTRUE)
        {
          static int debugCount2 = 0;
          if (debugCount2++ % 100 == 0)
            Serial.println("[CAN2 -> MQTT Queue] OK");
        }
        else
        {
          FrameLite dummy;
          xQueueReceive(qMqttTx, &dummy, 0);
          xQueueSend(qMqttTx, &f, 0);
        }
      }
      drained++;
    }

    if (canMutex)
      xSemaphoreGive(canMutex);

    if (drained == 0)
      vTaskDelay(rxPollDelay);
    else
      taskYIELD();
  }
}

static void netTask(void *)
{
  static char wsBuf[1536];
  size_t wsLen = 0;
  uint32_t lastFlush = millis();

  auto flush = [&]()
  {
    if (wsLen == 0)
      return;
    EventBits_t b = xEventGroupGetBits(eg);
    if (b & BIT_WS_HAS_CLIENT)
    {
      ws.broadcastTXT((const uint8_t *)wsBuf, wsLen);
    }
    wsLen = 0;
    lastFlush = millis();
  };

  auto appendPretty = [&](const FrameLite &f)
  {
    char line[160];
    char idbuf[10];
    if (fl_ext(f))
      snprintf(idbuf, sizeof(idbuf), "%08lX", f.id);
    else
      snprintf(idbuf, sizeof(idbuf), "%03lX", f.id);

    int n = snprintf(line, sizeof(line), "[CAN%u] [ID:0x%s %s %s DLC:%u Data:",
                     f.bus, idbuf, fl_ext(f) ? "EXT" : "STD", fl_rtr(f) ? "RTR" : "DAT", f.len);
    for (uint8_t i = 0; i < f.len && i < 8; i++)
    {
      n += snprintf(line + n, sizeof(line) - n, " %02X", f.data[i]);
    }
    if (n + 2 < (int)sizeof(line))
    {
      line[n++] = ']';
      line[n++] = '\n';
    }
    if (wsLen + (size_t)n > sizeof(wsBuf))
      flush();
    if ((size_t)n <= sizeof(wsBuf) - wsLen)
    {
      memcpy(wsBuf + wsLen, line, n);
      wsLen += (size_t)n;
    }
    else
    {
      flush();
      ws.broadcastTXT((const uint8_t *)line, (size_t)n);
    }
  };

  const TickType_t tick5 = pdMS_TO_TICKS(5);
  for (;;)
  {
    http.handleClient();
    ws.loop();

    static uint32_t lastKA = 0;
    uint32_t now = millis();
    if (now - lastKA >= 5000)
    {
      static const char ka[] = "{\"type\":\"ka\"}";
      if (ws.connectedClients() > 0)
        ws.broadcastTXT(ka, sizeof(ka) - 1);
      lastKA = now;
    }

    static uint32_t lastStatusUpdate = 0;
    if (now - lastStatusUpdate >= 2000)
    {
      char statusBuf[64];
      snprintf(statusBuf, sizeof(statusBuf), "{\"type\":\"status\",\"mqtt\":%d}", mqttConnectedFlag ? 1 : 0);
      if (ws.connectedClients() > 0)
      {
        ws.broadcastTXT(statusBuf);
      }
      lastStatusUpdate = now;
    }

    uint32_t tStart = millis();
    FrameLite f;
    while ((millis() - tStart) < 5)
    {
      if (xQueueReceive(qRx, &f, 0) != pdTRUE)
        break;
      appendPretty(f);
    }

    if (now - lastFlush >= 8)
      flush();
    vTaskDelay(tick5);
  }
}

static void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  String msg = "";
  for (unsigned int i = 0; i < length; i++)
    msg += (char)payload[i];

  Serial.printf("\n[MQTT RX] Topic: %s\n", topic);
  Serial.printf("[MQTT RX] Raw Payload: %s\n", msg.c_str());

  bool ext = (msg.indexOf("Ext") > 0 || msg.indexOf("EXT") > 0);
  uint32_t id = 0;
  int idIdx = msg.indexOf("ID: ");
  if (idIdx > 0)
  {
    int idEnd = msg.indexOf(" ", idIdx + 4);
    String idStr = msg.substring(idIdx + 4, idEnd);
    id = strtoul(idStr.c_str(), NULL, 16);
  }

  uint8_t dlc = 0;
  int dlcIdx = msg.indexOf("DLC: ");
  if (dlcIdx > 0)
  {
    dlc = msg.substring(dlcIdx + 5, msg.indexOf(" ", dlcIdx + 5)).toInt();
    if (dlc > 8)
      dlc = 8;
  }

  uint8_t data[8] = {0};
  int dataIdx = msg.indexOf("Data: ");
  if (dataIdx > 0)
  {
    int curr = dataIdx + 6;
    for (int i = 0; i < dlc && curr < msg.length(); i++)
    {
      while (curr < msg.length() && msg[curr] == ' ')
        curr++;
      if (curr + 1 < msg.length())
      {
        String byteStr = msg.substring(curr, curr + 2);
        data[i] = (uint8_t)strtoul(byteStr.c_str(), NULL, 16);
        curr += 2;
      }
    }
  }

  FrameLite f{};
  f.id = id;
  f.len = dlc;
  f.flags = (ext ? 1 : 0);
  f.bus = 0;
  memcpy(f.data, data, dlc);

  if (xQueueSend(qTx, &f, 0) == pdTRUE)
  {
    Serial.println("[MQTT] -> Queued for CAN TX");
  }

  if (xQueueSend(qRx, &f, 0) != pdTRUE)
  {
    FrameLite dummy;
    xQueueReceive(qRx, &dummy, 0);
    xQueueSend(qRx, &f, 0);
  }

  Serial.printf("[MQTT PARSED] -> ID: 0x%lX | DLC: %d\n", id, dlc);
}

static void mqttTask(void *pv)
{
  for (;;)
  {
    vTaskDelay(pdMS_TO_TICKS(10));

    if (WiFi.status() == WL_CONNECTED && mqttEnabledFlag)
    {
      if (!mqtt.connected())
      {
        mqttConnectedFlag = false;
        Serial.println("[MQTTS] Attempting secure connection...");

        String clientId = "WebCan_" + String(ESP.getEfuseMac(), HEX);
        bool connected = false;

        if (strlen(gMqttCfg.user) > 0)
        {
          connected = mqtt.connect(clientId.c_str(), gMqttCfg.user, gMqttCfg.pass);
        }
        else
        {
          connected = mqtt.connect(clientId.c_str());
        }

        if (connected)
        {
          mqtt.subscribe(gMqttCfg.subTopic);
          mqttConnectedFlag = true;
          Serial.println("[MQTTS] Connected & Encrypted");
        }
        else
        {
          mqttConnectedFlag = false;
          Serial.printf("[MQTTS] Failed, state=%d\n", mqtt.state());
          vTaskDelay(pdMS_TO_TICKS(5000));
        }
      }
      else
      {
        mqttConnectedFlag = true;
        mqtt.loop();

        FrameLite out;
        int processed = 0;

        while (xQueueReceive(qMqttTx, &out, 0) == pdTRUE && processed < 20)
        {
          processed++;
          char idStr[10];
          if (fl_ext(out))
            snprintf(idStr, sizeof(idStr), "%08lX", out.id);
          else
            snprintf(idStr, sizeof(idStr), "%03lX", out.id);

          char dataStr[32] = {0};
          for (int d = 0; d < out.len; d++)
          {
            char hex[4];
            snprintf(hex, sizeof(hex), "%02X%s", out.data[d], (d < out.len - 1) ? " " : "");
            strcat(dataStr, hex);
          }

          char jsonBuf[256];
          snprintf(jsonBuf, sizeof(jsonBuf),
                   "{\"MessageType\":\"internal.debug.can.frame.send.v1\",\"Payload\":{\"canId\":\"%s\",\"canFrame\":\"%s\"}}",
                   idStr, dataStr);

          String pubT = String(gMqttCfg.pubTopic);
          pubT.trim();
          mqtt.publish(pubT.c_str(), jsonBuf);
        }
      }
    }
    else
    {
      mqttConnectedFlag = false;
    }
  }
}

static void startRtosPipelines()
{
  qRx = xQueueCreate(512, sizeof(FrameLite));
  qTx = xQueueCreate(128, sizeof(FrameLite));
  qMqttTx = xQueueCreate(128, sizeof(FrameLite));
  eg = xEventGroupCreate();

  if (!qRx || !qTx || !qMqttTx || !eg || !canMutex)
  {
    Serial.println("[RTOS] FATAL: Failed to allocate queues/mutex. Restarting...");
    delay(500);
    ESP.restart();
  }

  xTaskCreatePinnedToCore(canTask, "can", 4096, NULL, 20, NULL, 1);
  xTaskCreatePinnedToCore(netTask, "net", 6144, NULL, 18, NULL, 0);
  xTaskCreatePinnedToCore(mqttTask, "mqtt", 8192, NULL, 17, NULL, 0);
  xTaskCreatePinnedToCore(ledStatusTask, "led", 2048, NULL, 1, NULL, 1);
}

// =========================================================
//                   MAIN SETUP & LOOP
// =========================================================
void setup()
{
  Serial.begin(115200);
  Serial.println("\n--- WebCan Booting ---");

  pinMode(LED_BUILTIN, OUTPUT);
  led.begin();
  led.on();

  if (loadMqttConfig())
  {
    Serial.printf("Loaded MQTT Config - Server: %s, Port: %d, Enabled: %d\n",
                  gMqttCfg.server, gMqttCfg.port, gMqttCfg.enabled);
  }
  else
  {
    Serial.println("No MQTT config found, using defaults.");
  }

  startWiFi();
  setupHttp();

  ws.begin();
  ws.onEvent(onWsEvent);

  SPI.begin(MCP2515_SCK, MCP2515_MISO, MCP2515_MOSI);
  SPI.setFrequency(8000000);
  pinMode(MCP2515_1_INT, INPUT_PULLUP);
  pinMode(MCP2515_2_INT, INPUT_PULLUP);

  canMutex = xSemaphoreCreateMutex();

  ws.broadcastTXT("ESP32 + ACAN2515 Web Terminal (web-only)");
  reconfigure(currentKbps);

  if (mqttEnabledFlag && strlen(gMqttCfg.server) > 0)
  {
    setupSecureMQTT();
  }

  // Initialize MCP23X17 on I2C (relay control for termination resistor)
  Wire.begin();
  if (!mcp.begin_I2C()) {
    Serial.println("Error initializing MCP23X17.");
  } else {
    mcp.pinMode(IMMO_A, OUTPUT);
    mcp.pinMode(IMMO_B, OUTPUT);
    
    // Ensure relay coils are unpowered on boot
    mcp.digitalWrite(IMMO_A, LOW);
    mcp.digitalWrite(IMMO_B, LOW);
  }

  startRtosPipelines();
}

void loop()
{
  vTaskDelay(pdMS_TO_TICKS(100)); // Yield to tasks
}