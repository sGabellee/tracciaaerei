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
bool openSkyFetchStates(std::vector<AircraftState>& out);
