#pragma once

#include <Arduino.h>

struct RouteInfo {
    String originIata;
    String destIata;
    String aircraftModel;
    bool valid = false;
};

// Arricchisce una callsign con origine/destinazione/modello via AeroDataBox.
// Da chiamare SOLO per voli nuovi (non ad ogni poll) per restare nel piano gratuito.
bool aeroDataBoxFetchRoute(const String& callsign, RouteInfo& out);
