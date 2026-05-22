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

const char WEB_UI_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="fr">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Contrôle des électrovannes</title>
  <style>
    :root { color-scheme: light dark; }
    body { font-family: Arial, sans-serif; margin: 1.5rem; }
    h1 { margin-bottom: 0.5rem; }
    .status { margin: 0.25rem 0 1rem; color: #555; }
    .valve { border: 1px solid #bbb; border-radius: 10px; padding: 0.75rem; margin-bottom: 0.75rem; }
    .valve h2 { margin: 0 0 0.5rem; font-size: 1.1rem; }
    .actions { display: flex; gap: 0.5rem; }
    button { padding: 0.5rem 0.75rem; border-radius: 8px; border: 1px solid #888; cursor: pointer; }
    button:disabled { opacity: 0.6; cursor: wait; }
    .open { color: #0a7f2e; font-weight: bold; }
    .closed { color: #a42323; font-weight: bold; }
  </style>
</head>
<body>
  <h1>Interface de pilotage des vannes</h1>
  <p class="status" id="globalStatus">Chargement…</p>
  <button id="closeAllButton" type="button">Fermer toutes les vannes</button>
  <div id="valves"></div>

  <script>
    const globalStatus = document.getElementById('globalStatus');
    const closeAllButton = document.getElementById('closeAllButton');
    const valvesContainer = document.getElementById('valves');

    function setStatus(message) {
      globalStatus.textContent = message;
    }

    function valveStateClass(isOpen) {
      return isOpen ? 'open' : 'closed';
    }

    function valveStateLabel(isOpen) {
      return isOpen ? 'OUVERTE' : 'FERMÉE';
    }

    async function apiRequest(url, options = {}) {
      const response = await fetch(url, options);
      const data = await response.json();
      if (!response.ok || !data.ok) {
        throw new Error(data.error || 'Erreur API');
      }
      return data;
    }

    function renderValve(valve) {
      return `
        <section class="valve">
          <h2>${valve.id}</h2>
          <p>État: <span class="${valveStateClass(valve.open)}">${valveStateLabel(valve.open)}</span></p>
          <div class="actions">
            <button type="button" data-action="open" data-id="${valve.id}">Ouvrir</button>
            <button type="button" data-action="close" data-id="${valve.id}">Fermer</button>
          </div>
        </section>
      `;
    }

    async function loadValves() {
      setStatus('Récupération des états...');
      const data = await apiRequest('/valves');
      valvesContainer.innerHTML = data.valves.map(renderValve).join('');
      setStatus('Prêt');
    }

    async function setValve(id, action) {
      setStatus(`Commande en cours: ${id} -> ${action}`);
      await apiRequest(`/valves/${id}/${action}`, { method: 'POST' });
      await loadValves();
    }

    valvesContainer.addEventListener('click', async (event) => {
      const button = event.target.closest('button[data-action]');
      if (!button) return;
      button.disabled = true;
      try {
        await setValve(button.dataset.id, button.dataset.action);
      } catch (error) {
        setStatus(error.message);
      } finally {
        button.disabled = false;
      }
    });

    closeAllButton.addEventListener('click', async () => {
      closeAllButton.disabled = true;
      try {
        setStatus('Fermeture de toutes les vannes...');
        await apiRequest('/valves/close-all', { method: 'POST' });
        await loadValves();
      } catch (error) {
        setStatus(error.message);
      } finally {
        closeAllButton.disabled = false;
      }
    });

    loadValves().catch((error) => setStatus(error.message));
  </script>
</body>
</html>
)HTML";

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

void handleWebUi() {
  server.send(200, "text/html; charset=utf-8", WEB_UI_HTML);
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
  server.on("/", HTTP_GET, handleWebUi);
  server.on("/valves", HTTP_GET, handleListValves);
  server.on("/valves/close-all", HTTP_POST, handleCloseAllValves);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("REST API ready");
}

void loop() {
  server.handleClient();
}
