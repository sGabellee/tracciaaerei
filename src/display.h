#pragma once

#include "flight_selectors.h"

enum class DisplayMode { CASELLE, NEAREST, MAP };

void displayInit();
void displayToggleMode();
DisplayMode displayGetMode();

// Da chiamare periodicamente dal loop principale: ogni ~2s per le viste
// Caselle/Più vicino, ogni FAST_FRAME_INTERVAL_MS quando displayWantsFastFrames().
void displayRender(const FlightSelectors& selectors);

// true quando la vista attuale è animata (vista Mappa, animazione di
// atterraggio) e va ridisegnata a ~25 fps invece che ogni 2 s.
bool displayWantsFastFrames(const FlightSelectors& selectors);
