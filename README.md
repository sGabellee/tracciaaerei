# tracciaaerei

Firmware ESP32-S3 per un piccolo display che mostra i voli in arrivo all'aeroporto
di Torino-Caselle visibili da casa, o in alternativa il velivolo più vicino in un
raggio di 40km — vedi il piano completo in `plans/` (o nella cronologia della chat)
per il contesto delle scelte fatte.

## Hardware

- ESP32-S3 "Super Mini" (o simile)
- Display rotondo GC9A01 240x240, SPI (7 pin: RST, CS, DC, SDA, SCL, GND, VCC)
- Un pulsante (cicla le viste: Caselle → Più vicino → Mappa)
- PCB custom 40x60mm

### Collegamenti

| Segnale display | Pin ESP32-S3 |
|---|---|
| RST               | GPIO13 |
| CS                | GPIO12 |
| DC                | GPIO11 |
| SDA (MOSI)        | GPIO2  |
| SCL (clock)       | GPIO3  |
| GND               | GND    |
| VCC               | 3V3    |


Bottone: tra GPIO1 e GND (pull-up interno, nessuna resistenza esterna necessaria
con il firmware così com'è — se il tuo PCB ha già una resistenza di pull-down
con logica invertita, aggiorna `handleButton()` in `src/main.cpp`).

Il modulo display non ha un pin di backlight separato: lo "spegnimento" schermo
usa il comando sleep del controller GC9A01 (funzione `screenSleep()` in
`src/display.cpp`), non un taglio di alimentazione.

## Setup software

1. Installa [PlatformIO](https://platformio.org/) (estensione VS Code, oppure CLI).
2. Copia `src/secrets.h.example` in `src/secrets.h` e compila:
   - `WIFI_SSID` / `WIFI_PASSWORD`: la tua rete di casa.
   - `AERODATABOX_API_KEY`: da RapidAPI → AeroDataBox → tab "Endpoints" di un
     endpoint qualsiasi → campo `x-rapidapi-key`.
   - (consigliato) `OPENSKY_CLIENT_ID` / `OPENSKY_CLIENT_SECRET`: senza account
     OpenSky concede solo 400 crediti/giorno (HTTP 429 dopo ~2 ore a un poll
     ogni 22 s); con un account gratuito sono 4000. Registrati su
     opensky-network.org, nella pagina Account crea un "API client" e aggiungi
     in `secrets.h`: `#define OPENSKY_CLIENT_ID "..."` e
     `#define OPENSKY_CLIENT_SECRET "..."`.
   - `secrets.h` è escluso da git (vedi `.gitignore`): non finisce mai su GitHub.
3. `pio run` per compilare, `pio run -t upload` per flashare, `pio device monitor`
   per il log seriale di debug.

## Vista Mappa (terza modalità del bottone)

Tutto il cerchio dello schermo è una mappa OpenStreetMap: pallino rosso = casa,
aeroplanino giallo = velivolo più vicino (stessi dati della vista "Più vicino").
Lo zoom inquadra sia casa che aereo (più è vicino, più si zooma) e ogni cambio
di velivolo è animato come spostamento+zoom della camera sulla mappa.

- **Nessuna chiave API**: le tile di `tile.openstreetmap.org` sono pubbliche.
  La [policy](https://operations.osmfoundation.org/policies/tiles/) chiede però
  uno User-Agent che identifichi l'app (`MAP_TILE_USER_AGENT` in `src/config.h`:
  mettici il tuo nome/contatto), niente download massivo, e l'attribuzione
  "© OpenStreetMap contributors" (qui: in questo README). Per uso personale
  con una manciata di tile in cache va bene.
- Se un giorno servisse un provider con chiave (MapTiler, Stadia, CARTO...):
  cambia solo `MAP_TILE_URL_FMT` in `src/config.h` (%d = z, x, y; la chiave
  si mette nell'URL).
- **Serve la PSRAM** (cache di 10 tile da 128 KB): `platformio.ini` la abilita.
  Senza PSRAM la vista Mappa si disattiva da sola e il bottone alterna solo le
  due viste originali.
- Le tile si scaricano in un task a parte; quelle mancanti si vedono prima in
  versione sfocata (livello di zoom inferiore) e poi si nitidiscono.
- La mappa ha sempre il nord in alto (non usa `DEVICE_FACING_DEGREES`); l'aeroplanino
  giallo ruota secondo la rotta reale dell'aereo.

## Rigenerare l'icona

Se sostituisci `plane.png`, rigenera `assets/plane_icon.h`:

```bash
python tools/convert_icon.py
```

Richiede Pillow (`pip install Pillow`).

## Cosa affinare dopo i primi test

- `CASELLE_TRIGGER_LAT/LON` in `src/config.h`: punto di partenza plausibile a
  sud di Torino, pensato per ~4-5 minuti di preavviso. Osserva i primi
  atterraggi reali (log seriale) e aggiusta se il timing non torna.
- `DEVICE_FACING_DEGREES` in `src/config.h`: direzione reale (gradi, 0=nord)
  verso cui punta il "sopra" dello schermo una volta montato — ora 40
  (nord-est), regolalo tu.
- `RUNWAY_HEADING_TOLERANCE_DEG` e `CASELLE_MAX_ALTITUDE_M`: se la vista
  Caselle si attiva troppo spesso (overflight non diretti lì) o troppo poco,
  stringi/allarga questi filtri.
- Colori/layout in `src/display.cpp`: scritti "alla cieca" senza schermo fisico
  davanti, quasi certamente da aggiustare a occhio una volta accesi.

## Punti verificati / ancora da verificare visivamente

- **Flash 4MB, non 8MB**: confermato su hardware reale (la board generica
  `esp32-s3-devkitc-1` assume 8MB di default, causava un crash di avvio).
  Già corretto in `platformio.ini` con `board_upload.flash_size = 4MB` +
  `board_build.partitions = default.csv`. Se cambi board_config, ricontrolla.
- `gfx.sleep()` / `gfx.wakeup()`, `iconSprite.pushRotateZoom(...)` (freccia),
  `gfx.drawWideLine(...)` (spunta verde): compilano senza errori, ma non
  ancora confermati visivamente sullo schermo reale — verifica quando testi
  le due viste.
- `cfg.invert = true` in `src/lgfx_config.h`: se i colori sono invertiti
  all'accensione (nero/bianco scambiati), prova a metterlo `false`.
- Dopo aver cambiato `board_upload.flash_size` o altri parametri di board,
  se la build dà errori tipo "no such file or directory" su file che prima
  compilavano, è la cache `.pio/build` rimasta incoerente — cancellala
  (`rm -rf .pio/build` o `pio run -t clean`) e ricompila da zero.
