# ESP32 - Contrôle d'électrovannes via API REST

Ce dépôt contient un exemple Arduino/ESP32 pour piloter des électrovannes via des sorties GPIO et des endpoints REST.

## Fonctionnalités

- Gestion de plusieurs électrovannes (`zone1`, `zone2`, ...).
- Contrôle **ouverture / fermeture** par endpoint REST.
- Endpoint de santé (`/health`).
- Retour JSON simple.

## Fichier principal

- `esp32_electrovannes_rest.ino`

## Dépendances Arduino

Installe ces bibliothèques dans l'IDE Arduino (ou PlatformIO):

- `WiFi` (incluse avec ESP32)
- `WebServer` (incluse avec ESP32)
- `ArduinoJson`

## Configuration

Dans `esp32_electrovannes_rest.ino`:

1. Remplace les identifiants Wi-Fi:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
```

2. Adapte la table des électrovannes:

```cpp
ValveConfig valves[] = {
  {"zone1", 18, true},
  {"zone2", 19, true},
  {"zone3", 21, true},
  {"zone4", 22, true}
};
```

- `id`: identifiant utilisé dans l'URL.
- `pin`: pin GPIO.
- `activeHigh`:
  - `true` => `HIGH` ouvre la vanne.
  - `false` => `LOW` ouvre la vanne (utile avec certains relais inversés).

## Endpoints REST

Supposons que l'ESP32 ait l'IP `192.168.1.50`.

- `GET /health`
- `GET /valves`
- `GET /valves/{id}`
- `POST /valves/{id}/open`
- `POST /valves/{id}/close`

### Exemples `curl`

```bash
curl http://192.168.1.50/health
curl http://192.168.1.50/valves
curl http://192.168.1.50/valves/zone1
curl -X POST http://192.168.1.50/valves/zone1/open
curl -X POST http://192.168.1.50/valves/zone1/close
```

## Notes matériel importantes

- **Ne branche pas une électrovanne directement** sur un GPIO de l'ESP32.
- Utilise un module relais ou un MOSFET + alimentation adaptée à la vanne.
- Relie correctement les masses (GND commun) selon ton montage.
- Prévois une protection contre les retours inductifs (diode de roue libre si besoin).

## Sécurité (recommandé)

Ce code d'exemple n'ajoute pas d'authentification. Pour une installation réelle:

- Isole le réseau (VLAN / Wi-Fi dédié IoT).
- Ajoute une authentification (token/API key).
- Ou place un reverse proxy avec TLS + auth devant l'ESP32.
