#pragma once

#include "flight_selectors.h"

enum class DisplayMode { CASELLE, NEAREST };

void displayInit();
void displayToggleMode();
DisplayMode displayGetMode();

// Da chiamare periodicamente dal loop principale (ogni ~1-2s va benissimo).
void displayRender(const FlightSelectors& selectors);
