#include "opensky.h"
#include "config.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

bool openSkyFetchStates(std::vector<AircraftState>& out) {
    out.clear();

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

    int httpCode = http.GET();
    if (httpCode != 200) {
        Serial.printf("[opensky] HTTP %d\n", httpCode);
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
