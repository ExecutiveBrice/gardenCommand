#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// ===== Wi-Fi =====
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ===== Configuration =====
struct ValveConfig {
  const char* id;
  uint8_t pin;
  bool activeHigh; // true = HIGH opens valve, false = LOW opens valve
};

ValveConfig valves[] = {
  {"zone1", 18, true},
  {"zone2", 19, true},
  {"zone3", 21, true},
  {"zone4", 22, true}
};

const size_t VALVE_COUNT = sizeof(valves) / sizeof(valves[0]);

WebServer server(80);

// ===== Helpers =====
void setValveState(const ValveConfig& valve, bool open) {
  const uint8_t level = (open == valve.activeHigh) ? HIGH : LOW;
  digitalWrite(valve.pin, level);
}

bool getValveState(const ValveConfig& valve) {
  const uint8_t level = digitalRead(valve.pin);
  return valve.activeHigh ? (level == HIGH) : (level == LOW);
}

const ValveConfig* findValveById(const String& valveId) {
  for (size_t i = 0; i < VALVE_COUNT; i++) {
    if (valveId.equalsIgnoreCase(valves[i].id)) {
      return &valves[i];
    }
  }
  return nullptr;
}

void sendJson(int statusCode, JsonDocument& doc) {
  String body;
  serializeJson(doc, body);
  server.send(statusCode, "application/json", body);
}

void sendError(int statusCode, const char* message) {
  JsonDocument doc;
  doc["ok"] = false;
  doc["error"] = message;
  sendJson(statusCode, doc);
}

void sendValve(const ValveConfig& valve) {
  JsonDocument doc;
  doc["ok"] = true;
  JsonObject data = doc["valve"].to<JsonObject>();
  data["id"] = valve.id;
  data["pin"] = valve.pin;
  data["open"] = getValveState(valve);
  sendJson(200, doc);
}

void handleHealth() {
  JsonDocument doc;
  doc["ok"] = true;
  doc["service"] = "esp32-valves";
  doc["ip"] = WiFi.localIP().toString();
  sendJson(200, doc);
}

void handleListValves() {
  JsonDocument doc;
  doc["ok"] = true;
  JsonArray arr = doc["valves"].to<JsonArray>();

  for (size_t i = 0; i < VALVE_COUNT; i++) {
    JsonObject v = arr.add<JsonObject>();
    v["id"] = valves[i].id;
    v["pin"] = valves[i].pin;
    v["open"] = getValveState(valves[i]);
  }

  sendJson(200, doc);
}

void handleGetValve() {
  const String id = server.pathArg(0);
  const ValveConfig* valve = findValveById(id);

  if (!valve) {
    sendError(404, "Valve not found");
    return;
  }

  sendValve(*valve);
}

void handleSetValve(bool open) {
  const String id = server.pathArg(0);
  const ValveConfig* valve = findValveById(id);

  if (!valve) {
    sendError(404, "Valve not found");
    return;
  }

  setValveState(*valve, open);
  sendValve(*valve);
}

void handleOpenValve() {
  handleSetValve(true);
}

void handleCloseValve() {
  handleSetValve(false);
}

void handleNotFound() {
  sendError(404, "Endpoint not found");
}

void setup() {
  Serial.begin(115200);

  for (size_t i = 0; i < VALVE_COUNT; i++) {
    pinMode(valves[i].pin, OUTPUT);
    setValveState(valves[i], false); // keep everything closed at boot
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.println();
  Serial.print("Connected. IP: ");
  Serial.println(WiFi.localIP());

  server.on("/health", HTTP_GET, handleHealth);
  server.on("/valves", HTTP_GET, handleListValves);
  server.on(UriBraces("/valves/{}"), HTTP_GET, handleGetValve);
  server.on(UriBraces("/valves/{}/open"), HTTP_POST, handleOpenValve);
  server.on(UriBraces("/valves/{}/close"), HTTP_POST, handleCloseValve);

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("REST API ready");
}

void loop() {
  server.handleClient();
}
