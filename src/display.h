#pragma once

#include "flight_selectors.h"

enum class DisplayMode { CASELLE, NEAREST, MAP };

void displayInit();
void displayToggleMode();
DisplayMode displayGetMode();

// Da chiamare periodicamente dal loop principale: ogni ~2s per le viste
// Caselle/Più vicino, ogni MAP_FRAME_INTERVAL_MS per la vista Mappa.
void displayRender(const FlightSelectors& selectors);
