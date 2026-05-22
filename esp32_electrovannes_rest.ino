#include <WiFi.h>
#include <WebServer.h>

// ===== Wi-Fi =====
const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ===== Configuration =====
struct ValveConfig {
  const char *id;
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
String jsonEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length() + 4);

  for (size_t i = 0; i < value.length(); i++) {
    char c = value.charAt(i);
    if (c == '"' || c == '\\') {
      escaped += '\\';
    }
    escaped += c;
  }

  return escaped;
}

void sendJson(int statusCode, const String &body) {
  server.send(statusCode, "application/json", body);
}

void sendError(int statusCode, const String &message) {
  sendJson(statusCode, "{\"ok\":false,\"error\":\"" + jsonEscape(message) + "\"}");
}

void setValveState(const ValveConfig &valve, bool open) {
  const uint8_t level = (open == valve.activeHigh) ? HIGH : LOW;
  digitalWrite(valve.pin, level);
}

bool getValveState(const ValveConfig &valve) {
  const uint8_t level = digitalRead(valve.pin);
  return valve.activeHigh ? (level == HIGH) : (level == LOW);
}

const ValveConfig *findValveById(const String &valveId) {
  for (size_t i = 0; i < VALVE_COUNT; i++) {
    if (valveId.equalsIgnoreCase(valves[i].id)) {
      return &valves[i];
    }
  }
  return nullptr;
}

String valveToJson(const ValveConfig &valve) {
  String json = "{";
  json += "\"id\":\"" + jsonEscape(valve.id) + "\",";
  json += "\"pin\":" + String(valve.pin) + ",";
  json += "\"open\":" + String(getValveState(valve) ? "true" : "false");
  json += "}";
  return json;
}

void sendValve(const ValveConfig &valve) {
  sendJson(200, "{\"ok\":true,\"valve\":" + valveToJson(valve) + "}");
}

void closeAllValves() {
  for (size_t i = 0; i < VALVE_COUNT; i++) {
    setValveState(valves[i], false);
  }
}

// ===== Handlers =====
void handleHealth() {
  String body = "{";
  body += "\"ok\":true,";
  body += "\"service\":\"esp32-valves\",";
  body += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
  body += "}";
  sendJson(200, body);
}

void handleListValves() {
  String body = "{\"ok\":true,\"valves\":[";

  for (size_t i = 0; i < VALVE_COUNT; i++) {
    if (i > 0) {
      body += ",";
    }
    body += valveToJson(valves[i]);
  }

  body += "]}";
  sendJson(200, body);
}

void handleCloseAllValves() {
  closeAllValves();
  sendJson(200, "{\"ok\":true,\"message\":\"all valves closed\"}");
}

void handleSetValve(const ValveConfig &valve, bool open) {
  setValveState(valve, open);
  sendValve(valve);
}

bool handleValveRoute(const String &path, HTTPMethod method) {
  const String prefix = "/valves/";
  if (!path.startsWith(prefix)) {
    return false;
  }

  String rest = path.substring(prefix.length());
  int slashIndex = rest.indexOf('/');
  String valveId = slashIndex >= 0 ? rest.substring(0, slashIndex) : rest;
  String action = slashIndex >= 0 ? rest.substring(slashIndex + 1) : "";

  if (valveId.length() == 0) {
    sendError(404, "Valve id is missing");
    return true;
  }

  const ValveConfig *valve = findValveById(valveId);
  if (!valve) {
    sendError(404, "Valve not found");
    return true;
  }

  if (action.length() == 0 && method == HTTP_GET) {
    sendValve(*valve);
    return true;
  }

  if (action == "open" && method == HTTP_POST) {
    handleSetValve(*valve, true);
    return true;
  }

  if (action == "close" && method == HTTP_POST) {
    handleSetValve(*valve, false);
    return true;
  }

  sendError(405, "Method or action not allowed");
  return true;
}

void handleNotFound() {
  if (handleValveRoute(server.uri(), server.method())) {
    return;
  }

  sendError(404, "Endpoint not found");
}

void setup() {
  Serial.begin(115200);

  for (size_t i = 0; i < VALVE_COUNT; i++) {
    pinMode(valves[i].pin, OUTPUT);
  }
  closeAllValves();

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
  server.on("/valves/close-all", HTTP_POST, handleCloseAllValves);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("REST API ready");
}

void loop() {
  server.handleClient();
}
