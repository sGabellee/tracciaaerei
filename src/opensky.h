#pragma once

#include <Arduino.h>
#include <vector>

struct AircraftState {
    String icao24;
    String callsign;       // già "trim"-ata
    double lat = NAN;
    double lon = NAN;
    double baroAltitudeM = NAN;
    double velocityMs = NAN;
    double trueTrackDeg = NAN;
    bool onGround = false;
};

// Interroga /states/all sul bounding box configurato in config.h.
// Ritorna false solo in caso di errore di rete/HTTP; un cielo vuoto è "true" con out vuoto.
// Se OpenSky risponde 429 (limite crediti giornaliero esaurito) e retryAfterSec
// non è nullptr, ci scrive dopo quanti secondi riprovare (0 = errore diverso).
bool openSkyFetchStates(std::vector<AircraftState>& out, int* retryAfterSec = nullptr);
