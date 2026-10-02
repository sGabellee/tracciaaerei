#pragma once

// ---------------------------------------------------------------------------
// Posizione di casa (Via Alpignano 18, Torino)
// ---------------------------------------------------------------------------
static constexpr double HOME_LAT = 45.0771142;
static constexpr double HOME_LON = 7.6523159;

// ---------------------------------------------------------------------------
// Bounding box OpenSky: ~40km attorno a casa (copre anche la zona Caselle).
// Un solo poll alimenta sia la vista "Caselle" che "Più vicino".
// ---------------------------------------------------------------------------
static constexpr double BBOX_LAMIN = 44.718;
static constexpr double BBOX_LAMAX = 45.436;
static constexpr double BBOX_LOMIN = 7.144;
static constexpr double BBOX_LOMAX = 8.160;

// ---------------------------------------------------------------------------
// Vista "Più vicino": raggio di ricerca da casa.
// ---------------------------------------------------------------------------
static constexpr double NEAREST_RADIUS_KM = 40.0;

// ---------------------------------------------------------------------------
// Vista "Caselle": punto di riferimento a sud di Torino usato come trigger
// (non è l'aeroporto stesso: è più a sud, per dare qualche minuto di preavviso
// prima dell'atterraggio). Valore di partenza plausibile - AFFINA osservando
// i primi atterraggi reali (regola CASELLE_TRIGGER_LAT/LON finché il timing
// a schermo non corrisponde a quello che vedi/senti davvero).
// ---------------------------------------------------------------------------
static constexpr double CASELLE_TRIGGER_LAT = 44.97;
static constexpr double CASELLE_TRIGGER_LON = 7.65;
static constexpr double CASELLE_TRIGGER_RADIUS_KM = 10.0;

// Coordinate approssimative dell'aeroporto (usate solo per il check di
// prossimità nel rilevamento atterraggio, non serve la soglia pista esatta).
static constexpr double CASELLE_AIRPORT_LAT = 45.1965359;
static constexpr double CASELLE_AIRPORT_LON = 7.647867;

// Rilevamento "atterrato": quota sotto soglia E vicino all'aeroporto.
static constexpr double LANDED_ALTITUDE_M = 150.0;
static constexpr double LANDED_PROXIMITY_KM = 2.0;

// Se il velivolo tracciato sparisce dal poll (perdita ricezione ADS-B a terra,
// tipica in fase di atterraggio) lo consideriamo atterrato solo se l'ultimo
// dato noto era già "in corto finale": soglie più larghe di quelle sopra.
static constexpr double LANDED_FALLBACK_ALTITUDE_M = 600.0;
static constexpr double LANDED_FALLBACK_PROXIMITY_KM = 4.0;

// Quota massima per considerare un velivolo "in avvicinamento" alla vista
// Caselle (filtro di buon senso, oltre a raggio e rotta).
static constexpr double CASELLE_MAX_ALTITUDE_M = 2500.0;

// Durata dell'animazione aereo -> spunta verde.
static constexpr unsigned long LANDED_ANIMATION_MS = 1800;

// Rotta attesa per un atterraggio in pista 36 (arrivo da sud, prua ~nord).
// Tolleranza ampia perché l'ATC vettora gli aerei, non è mai una linea perfetta.
static constexpr double RUNWAY36_HEADING_DEG = 360.0;
static constexpr double RUNWAY_HEADING_TOLERANCE_DEG = 60.0;

// ---------------------------------------------------------------------------
// Freccia direzionale (solo vista "Più vicino"): direzione reale verso cui
// punta il "sopra" dello schermo una volta montato. 270 = Ovest, come
// indicato. REGOLA TU questo valore (o ruota fisicamente il device) finché
// la freccia non punta correttamente verso aerei reali visibili.
// ---------------------------------------------------------------------------
static constexpr double DEVICE_FACING_DEGREES = 270.0;

// ---------------------------------------------------------------------------
// Temporizzazione
// ---------------------------------------------------------------------------
static constexpr unsigned long OPENSKY_POLL_INTERVAL_MS = 22000;   // ~20-25s
static constexpr unsigned long RENDER_TICK_INTERVAL_MS = 2000;     // freccia/estrapolazione
static constexpr unsigned long BUTTON_DEBOUNCE_MS = 200;
static constexpr unsigned long WIFI_RETRY_INTERVAL_MS = 10000;

// Quante callsign teniamo in cache per evitare richieste AeroDataBox ripetute.
static constexpr int ROUTE_CACHE_SIZE = 8;

// ---------------------------------------------------------------------------
// Pin — ESP32-S3 "Super Mini". Letti da foto del modulo, VERIFICA sul tuo
// esemplare prima di saldare il PCB definitivo (40x60mm).
// Display: GC9A01 rotondo 240x240, SPI, 7 pin (RST,CS,DC,SDA,SCL,GND,VCC) —
// NESSUN pin backlight separato: il "sleep" schermo usa il comando sleep
// del controller, non un taglio di alimentazione.
//
// Scelti in ordine 13->9 apposta: sul lato destro della Super Mini i pin
// fisici scendono 13,12,11,10,9 in fila, nello stesso ordine dei segnali
// RST,CS,DC,SDA,SCL sul connettore del display — 5 fili paralleli, nessun
// incrocio. GND/VCC del display vanno invece ai pin GND/3V3 poco più in
// alto sulla stessa fila (vedi README per il dettaglio del cablaggio).
// ---------------------------------------------------------------------------
static constexpr int PIN_DISPLAY_RST  = 13;
static constexpr int PIN_DISPLAY_CS   = 12;
static constexpr int PIN_DISPLAY_DC   = 11;
static constexpr int PIN_DISPLAY_MOSI = 10;   // va a "SDA" sul modulo display
static constexpr int PIN_DISPLAY_SCLK = 9;

// Bottone: a GND quando premuto, pull-up interno (INPUT_PULLUP). Se il tuo
// PCB usa già una resistenza di pull-down esterna con logica invertita,
// aggiorna la lettura in main.cpp di conseguenza.
static constexpr int PIN_BUTTON = 1;

// ---------------------------------------------------------------------------
// OpenSky (anonimo, nessuna registrazione necessaria a questo ritmo di poll)
// ---------------------------------------------------------------------------
static constexpr const char* OPENSKY_STATES_URL = "https://opensky-network.org/api/states/all";

// ---------------------------------------------------------------------------
// AeroDataBox (RapidAPI) — chiave vera in secrets.h
// ---------------------------------------------------------------------------
static constexpr const char* AERODATABOX_HOST = "aerodatabox.p.rapidapi.com";
