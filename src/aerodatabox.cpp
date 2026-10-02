#include "aerodatabox.h"
#include "config.h"
#include "secrets.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

bool aeroDataBoxFetchRoute(const String& callsign, RouteInfo& out) {
    out = RouteInfo{};
    if (callsign.isEmpty()) return false;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(15000);

    String url = String("https://") + AERODATABOX_HOST + "/flights/callsign/" + callsign +
                 "?withAircraftImage=false&withLocation=false&withFlightPlan=false";

    if (!http.begin(client, url)) return false;
    http.addHeader("x-rapidapi-host", AERODATABOX_HOST);
    http.addHeader("x-rapidapi-key", AERODATABOX_API_KEY);

    int httpCode = http.GET();
    if (httpCode != 200) {
        Serial.printf("[aerodatabox] HTTP %d per %s\n", httpCode, callsign.c_str());
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    if (payload.isEmpty()) {
        Serial.println("[aerodatabox] risposta vuota");
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (err) {
        Serial.printf("[aerodatabox] JSON error: %s (primi 120 char: %s)\n",
                       err.c_str(), payload.substring(0, 120).c_str());
        return false;
    }

    JsonArray flights = doc.as<JsonArray>();
    if (flights.isNull() || flights.size() == 0) {
        return false;
    }

    // Preferiamo l'entry "EnRoute" (il volo che sta effettivamente volando ora);
    // la ricerca "nearest day" può restituire più voli/date per la stessa callsign.
    JsonObject chosen;
    for (JsonObject f : flights) {
        if (strcmp(f["status"] | "", "EnRoute") == 0) {
            chosen = f;
            break;
        }
    }
    if (chosen.isNull()) {
        chosen = flights[0];
    }

    out.originIata = (const char*)(chosen["departure"]["airport"]["iata"] | "");
    out.destIata = (const char*)(chosen["arrival"]["airport"]["iata"] | "");
    out.aircraftModel = (const char*)(chosen["aircraft"]["model"] | "");
    out.valid = !out.originIata.isEmpty() || !out.destIata.isEmpty() || !out.aircraftModel.isEmpty();

    return out.valid;
}
