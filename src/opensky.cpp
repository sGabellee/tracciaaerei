#include "opensky.h"
#include "config.h"
#include "secrets.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

namespace {

// Autenticazione OAuth2 (client credentials) opzionale: se in secrets.h ci sono
// OPENSKY_CLIENT_ID e OPENSKY_CLIENT_SECRET la quota passa da 400 a 4000
// crediti/giorno. Senza, si resta anonimi.
#if defined(OPENSKY_CLIENT_ID) && defined(OPENSKY_CLIENT_SECRET)
constexpr const char* OPENSKY_TOKEN_URL =
    "https://auth.opensky-network.org/auth/realms/opensky-network/protocol/openid-connect/token";

String g_token;
unsigned long g_tokenExpiresMs = 0;

bool ensureToken() {
    if (!g_token.isEmpty() && (long)(g_tokenExpiresMs - millis()) > 0) return true;
    g_token = "";

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(15000);
    if (!http.begin(client, OPENSKY_TOKEN_URL)) return false;
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String body = String("grant_type=client_credentials&client_id=") + OPENSKY_CLIENT_ID +
                  "&client_secret=" + OPENSKY_CLIENT_SECRET;
    int code = http.POST(body);
    if (code != 200) {
        Serial.printf("[opensky] token HTTP %d (controlla client_id/secret)\n", code);
        http.end();
        return false;
    }
    String payload = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, payload) || doc["access_token"].isNull()) {
        Serial.println("[opensky] risposta token non valida");
        return false;
    }
    g_token = doc["access_token"].as<String>();
    int expiresIn = doc["expires_in"] | 1800;
    if (expiresIn < 180) expiresIn = 180;
    g_tokenExpiresMs = millis() + (unsigned long)(expiresIn - 120) * 1000UL;
    Serial.println("[opensky] token ottenuto (account registrato)");
    return true;
}
#endif

}  // namespace

bool openSkyFetchStates(std::vector<AircraftState>& out, int* retryAfterSec) {
    out.clear();
    if (retryAfterSec) *retryAfterSec = 0;

#if defined(OPENSKY_CLIENT_ID) && defined(OPENSKY_CLIENT_SECRET)
    bool authenticated = ensureToken();
#else
    bool authenticated = false;
#endif

    WiFiClientSecure client;
    // Niente validazione del certificato: pragmatico per un progetto hobbistico
    // che legge solo dati pubblici non sensibili. Da rivedere se in futuro
    // il firmware dovesse mai inviare dati privati.
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(15000);

    char url[220];
    snprintf(url, sizeof(url), "%s?lamin=%.4f&lomin=%.4f&lamax=%.4f&lomax=%.4f",
             OPENSKY_STATES_URL, BBOX_LAMIN, BBOX_LOMIN, BBOX_LAMAX, BBOX_LOMAX);

    if (!http.begin(client, url)) {
        return false;
    }

#if defined(OPENSKY_CLIENT_ID) && defined(OPENSKY_CLIENT_SECRET)
    if (authenticated) http.addHeader("Authorization", String("Bearer ") + g_token);
#endif
    const char* headerKeys[] = { "X-Rate-Limit-Retry-After-Seconds" };
    http.collectHeaders(headerKeys, 1);

    int httpCode = http.GET();
    if (httpCode != 200) {
        Serial.printf("[opensky] HTTP %d%s\n", httpCode, authenticated ? " (account)" : " (anonimo)");
        if (httpCode == 429 && retryAfterSec) {
            int secs = http.header("X-Rate-Limit-Retry-After-Seconds").toInt();
            *retryAfterSec = secs > 0 ? secs : 900;
        }
#if defined(OPENSKY_CLIENT_ID) && defined(OPENSKY_CLIENT_SECRET)
        if (httpCode == 401) g_token = "";  // scaduto: al prossimo giro ne chiede uno nuovo
#endif
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    if (payload.isEmpty()) {
        Serial.println("[opensky] risposta vuota");
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (err) {
        Serial.printf("[opensky] JSON error: %s (primi 120 char: %s)\n",
                       err.c_str(), payload.substring(0, 120).c_str());
        return false;
    }

    JsonArray states = doc["states"].as<JsonArray>();
    if (states.isNull()) {
        return true;  // nessun velivolo nel bounding box in questo momento
    }

    for (JsonArray row : states) {
        if (row.size() < 11) continue;
        if (row[5].isNull() || row[6].isNull()) continue;  // niente posizione, scartiamo

        AircraftState ac;
        ac.icao24 = row[0].as<const char*>();
        const char* cs = row[1].as<const char*>();
        ac.callsign = cs ? String(cs) : String();
        ac.callsign.trim();
        ac.lon = row[5].as<double>();
        ac.lat = row[6].as<double>();
        ac.baroAltitudeM = row[7].isNull() ? NAN : row[7].as<double>();
        ac.onGround = row[8].as<bool>();
        ac.velocityMs = row[9].isNull() ? NAN : row[9].as<double>();
        ac.trueTrackDeg = row[10].isNull() ? NAN : row[10].as<double>();

        if (!ac.onGround) {
            out.push_back(ac);
        }
    }

    return true;
}
