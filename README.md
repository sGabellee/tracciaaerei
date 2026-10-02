# tracciaaerei

Firmware ESP32-S3 per un piccolo display che mostra i voli in arrivo all'aeroporto
di Torino-Caselle visibili da casa, o in alternativa il velivolo più vicino in un
raggio di 40km — vedi il piano completo in `plans/` (o nella cronologia della chat)
per il contesto delle scelte fatte.

## Hardware

- ESP32-S3 "Super Mini" (o simile)
- Display rotondo GC9A01 240x240, SPI (7 pin: RST, CS, DC, SDA, SCL, GND, VCC)
- Un pulsante (cambia vista Caselle / Più vicino)
- PCB custom 40x60mm

### Collegamenti

| Segnale display | Pin ESP32-S3 |
|---|---|
| RST               | GPIO13 |
| CS                | GPIO12 |
| DC                | GPIO11 |
| SDA (MOSI)        | GPIO10 |
| SCL (clock)       | GPIO9  |
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
   - `secrets.h` è escluso da git (vedi `.gitignore`): non finisce mai su GitHub.
3. `pio run` per compilare, `pio run -t upload` per flashare, `pio device monitor`
   per il log seriale di debug.

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
  verso cui punta il "sopra" dello schermo una volta montato — di default 270
  (ovest), regolalo tu.
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
