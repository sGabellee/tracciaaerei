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

// Atterraggio rilevato sulla posizione STIMATA (estrapolata da ultima posizione,
// velocità e rotta, controllata a ogni tick): scatta quando la distanza dal
// punto aeroporto scende sotto LANDED_TICK_DISTANCE_M. 300 m e non meno perché
// la pista passa a un centinaio di metri dal punto e tra due tick (2 s) l'aereo
// fa ~140 m. Abbassalo se vuoi il check più "a terra", alzalo se non scatta.
static constexpr double LANDED_TICK_DISTANCE_M = 300.0;
// Quota barometrica (MSL, Caselle è a ~300 m) sopra cui un sorvolo NON conta
// come atterraggio.
static constexpr double LANDED_TICK_MAX_ALTITUDE_M = 900.0;
// Quanto a lungo estrapolare la posizione senza nuovi poll (un aereo sparito
// dal radar in finale va seguito a stima fino alla pista).
static constexpr double LANDED_EXTRAPOLATION_MAX_S = 180.0;

// Animazione di atterraggio (solo spunta, nessun testo): l'aeroplanino gira su
// se stesso rimpicciolendosi mentre un anello verde si espande e la spunta si
// disegna. LANDED_ANIMATION_MS = durata dell'animazione; poi la spunta resta
// ferma (con un'onda pulsante) per LANDED_CHECK_HOLD_MS prima di passare al
// prossimo velivolo o spegnere lo schermo.
static constexpr unsigned long LANDED_ANIMATION_MS = 1800;
static constexpr unsigned long LANDED_CHECK_HOLD_MS = 10000;

// Rotta attesa per un atterraggio in pista 36 (arrivo da sud, prua ~nord).
// Tolleranza ampia perché l'ATC vettora gli aerei, non è mai una linea perfetta.
static constexpr double RUNWAY36_HEADING_DEG = 360.0;
static constexpr double RUNWAY_HEADING_TOLERANCE_DEG = 60.0;

// ---------------------------------------------------------------------------
// Freccia direzionale (solo vista "Più vicino"): direzione reale verso cui
// punta il "sopra" dello schermo una volta montato. 40 = Nord ruotato di
// 40° in senso orario (verso Nord-Est). REGOLA TU questo valore (o ruota
// fisicamente il device) finché la freccia non punta correttamente verso aerei
// reali visibili.
// ---------------------------------------------------------------------------
static constexpr double DEVICE_FACING_DEGREES = 40.0;

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
//
// NOTA: i pin 9 e 10 originali si sono bruciati sull'esemplare in uso -
// SDA e SCL sono stati spostati rispettivamente su 2 e 3 (non più in fila
// con RST/CS/DC, serve un filo volante per ciascuno invece del cablaggio
// parallelo descritto sopra).
// ---------------------------------------------------------------------------
static constexpr int PIN_DISPLAY_RST  = 13;
static constexpr int PIN_DISPLAY_CS   = 12;
static constexpr int PIN_DISPLAY_DC   = 11;
static constexpr int PIN_DISPLAY_MOSI = 2;   // va a "SDA" sul modulo display
static constexpr int PIN_DISPLAY_SCLK = 3;

// Bottone: a GND quando premuto, pull-up interno (INPUT_PULLUP). Se il tuo
// PCB usa già una resistenza di pull-down esterna con logica invertita,
// aggiorna la lettura in main.cpp di conseguenza.
static constexpr int PIN_BUTTON = 1;

// ---------------------------------------------------------------------------
// Vista "Mappa" (terza modalità): tutto lo schermo è una mappa OpenStreetMap
// centrata a metà strada tra casa e il velivolo più vicino, con zoom che
// inquadra entrambi, nord sempre in alto; l'aeroplanino punta nella direzione
// di volo reale. Richiede la PSRAM (vedi platformio.ini).
// ---------------------------------------------------------------------------
// Tile raster: nessuna chiave API per tile.openstreetmap.org, ma la policy
// (https://operations.osmfoundation.org/policies/tiles/) impone uno
// User-Agent che identifichi l'app e vieta l'uso intensivo. Se usi un altro
// provider (MapTiler, Stadia, CARTO...) cambia solo questo URL: %d = z, x, y.
static constexpr const char* MAP_TILE_URL_FMT = "https://tile.openstreetmap.org/%d/%d/%d.png";
static constexpr const char* MAP_TILE_USER_AGENT = "tracciaaerei-esp32/1.0 (progetto hobbistico personale)";

static constexpr unsigned long FAST_FRAME_INTERVAL_MS = 40;  // ~25 fps: vista mappa e animazione di atterraggio
static constexpr int MAP_TILE_SLOTS = 10;                   // tile decodificate in PSRAM (128 KB ciascuna)

static constexpr double MAP_ZOOM_MIN = 8.0;    // a ~40 km il fit dà zoom ~9
static constexpr double MAP_ZOOM_MAX = 15.0;
static constexpr double MAP_ZOOM_IDLE = 11.0;  // nessun aereo: mappa attorno a casa
static constexpr double MAP_FIT_DIAMETER_PX = 170.0;  // casa e aereo devono stare in questo diametro

// Costanti di tempo (s) dell'animazione: più alte = movimenti più lenti e morbidi.
static constexpr double MAP_CAMERA_TAU_S = 0.7;
static constexpr double MAP_ZOOM_TAU_S = 0.9;
static constexpr double MAP_ICON_TAU_S = 0.4;

static constexpr int MAP_ICON_PX = 34;  // lato (px) dell'aeroplanino sulla mappa
// L'icona sullo schermo risultava ruotata di 45° in senso orario rispetto alla
// rotta: -45 la riporta in asse (negativo = antiorario). Regolalo se serve.
static constexpr double MAP_ICON_ROTATION_OFFSET_DEG = -45.0;

// ---------------------------------------------------------------------------
// OpenSky (anonimo, nessuna registrazione necessaria a questo ritmo di poll)
// ---------------------------------------------------------------------------
static constexpr const char* OPENSKY_STATES_URL = "https://opensky-network.org/api/states/all";

// ---------------------------------------------------------------------------
// AeroDataBox (RapidAPI) — chiave vera in secrets.h
// ---------------------------------------------------------------------------
static constexpr const char* AERODATABOX_HOST = "aerodatabox.p.rapidapi.com";
