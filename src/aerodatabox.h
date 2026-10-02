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
// httpCode (opzionale): codice HTTP ricevuto, 0 se la richiesta non è partita/andata a buon fine.
bool aeroDataBoxFetchRoute(const String& callsign, RouteInfo& out, int* httpCode = nullptr);
