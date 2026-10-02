#pragma once

#include "flight_selectors.h"
#include "lgfx_config.h"

// Alloca i buffer in PSRAM e avvia il task che scarica le tile. Ritorna false
// (e la vista mappa resta disattivata) se la PSRAM non c'è o non basta.
bool mapViewInit();
bool mapViewAvailable();

// Va chiamata quando si entra/esce dalla vista mappa: da attiva scarica le
// tile, da inattiva il task di download sta fermo.
void mapViewSetActive(bool active);

// Disegna un frame sul display. flight.valid == false: mappa attorno a casa.
void mapViewRender(lgfx::LovyanGFX& dst, const TrackedFlight& flight);
