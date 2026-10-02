#include "display.h"
#include "config.h"
#include "geo.h"
#include "lgfx_config.h"
#include "../assets/plane_icon.h"

namespace {

LGFX gfx;
LGFX_Sprite iconSprite(&gfx);

constexpr uint16_t COLOR_BG = 0x0000;           // nero
constexpr uint16_t COLOR_PLANE = 0xFFFF;        // bianco
constexpr uint16_t COLOR_TEXT = 0xFFFF;
constexpr uint16_t COLOR_DIM = 0x7BEF;          // grigio, per righe secondarie
constexpr uint16_t COLOR_CHECK = 0x07E0;        // verde
constexpr uint16_t COLOR_TRANSPARENT_KEY = 0xF81F;  // magenta: chiave di trasparenza sprite

DisplayMode currentMode = DisplayMode::CASELLE;
bool screenAsleep = false;

void screenWake() {
    if (screenAsleep) {
        gfx.wakeup();
        screenAsleep = false;
    }
}

void screenSleep() {
    if (!screenAsleep) {
        gfx.fillScreen(COLOR_BG);
        gfx.sleep();
        screenAsleep = true;
    }
}

void buildIconSprite() {
    iconSprite.setColorDepth(16);
    iconSprite.createSprite(PLANE_ICON_SIZE, PLANE_ICON_SIZE);
    iconSprite.fillSprite(COLOR_TRANSPARENT_KEY);

    const int bytesPerRow = PLANE_ICON_SIZE / 8;
    for (int y = 0; y < PLANE_ICON_SIZE; y++) {
        for (int x = 0; x < PLANE_ICON_SIZE; x++) {
            uint8_t byte = PLANE_ICON_BITS[y * bytesPerRow + (x / 8)];
            bool set = byte & (1 << (7 - (x % 8)));
            if (set) {
                iconSprite.drawPixel(x, y, COLOR_PLANE);
            }
        }
    }
}

String fmtOrDash(const String& s) {
    return s.isEmpty() ? String("--") : s;
}

void drawCaselleView(const CaselleView& v) {
    if (v.state == CaselleState::IDLE) {
        screenSleep();
        return;
    }
    screenWake();
    gfx.fillScreen(COLOR_BG);

    bool showCheck = (v.state == CaselleState::LANDED) &&
                      (millis() - v.landedAnimationStartMs >= LANDED_ANIMATION_MS / 2);

    if (showCheck) {
        // Spunta verde: due tratti spessi al posto dell'icona aereo.
        gfx.drawWideLine(78, 122, 105, 150, 10, COLOR_CHECK);
        gfx.drawWideLine(105, 150, 165, 90, 10, COLOR_CHECK);
    } else {
        iconSprite.pushSprite(120 - PLANE_ICON_SIZE / 2, 42, COLOR_TRANSPARENT_KEY);
    }

    gfx.setTextDatum(lgfx::textdatum_t::middle_center);

    gfx.setTextColor(COLOR_TEXT, COLOR_BG);
    gfx.setTextSize(2);
    gfx.drawString(fmtOrDash(v.current.route.originIata), 120, 165);

    gfx.setTextColor(COLOR_DIM, COLOR_BG);
    gfx.setTextSize(1);
    gfx.drawString(fmtOrDash(v.current.route.aircraftModel), 120, 195);
}

void drawNearestView(const NearestView& v) {
    if (!v.current.valid) {
        screenSleep();
        return;
    }
    screenWake();
    gfx.fillScreen(COLOR_BG);

    GeoPoint home{ HOME_LAT, HOME_LON };
    GeoPoint est = v.current.estimatedPosition(millis());
    double bearing = geoBearingDegrees(home, est);
    double screenAngle = normalizeAngle360(bearing - DEVICE_FACING_DEGREES);
    double distanceKm = geoDistanceMeters(home, est) / 1000.0;

    iconSprite.pushRotateZoom(120, 90, screenAngle, 1.0f, 1.0f, COLOR_TRANSPARENT_KEY);

    char line1[24];
    snprintf(line1, sizeof(line1), "%s -> %s",
             fmtOrDash(v.current.route.originIata).c_str(),
             fmtOrDash(v.current.route.destIata).c_str());

    char line3[24];
    if (isnan(v.current.baroAltitudeM)) {
        snprintf(line3, sizeof(line3), "%.0f km", distanceKm);
    } else {
        snprintf(line3, sizeof(line3), "%.0fm - %.0f km", v.current.baroAltitudeM, distanceKm);
    }

    gfx.setTextDatum(lgfx::textdatum_t::middle_center);

    gfx.setTextColor(COLOR_TEXT, COLOR_BG);
    gfx.setTextSize(2);
    gfx.drawString(line1, 120, 155);

    gfx.setTextColor(COLOR_DIM, COLOR_BG);
    gfx.setTextSize(1);
    gfx.drawString(fmtOrDash(v.current.route.aircraftModel), 120, 180);
    gfx.drawString(line3, 120, 200);
}

}  // namespace

void displayInit() {
    gfx.init();
    gfx.setRotation(0);
    gfx.fillScreen(COLOR_BG);
    buildIconSprite();
}

void displayToggleMode() {
    currentMode = (currentMode == DisplayMode::CASELLE) ? DisplayMode::NEAREST : DisplayMode::CASELLE;
}

DisplayMode displayGetMode() {
    return currentMode;
}

void displayRender(const FlightSelectors& selectors) {
    if (currentMode == DisplayMode::CASELLE) {
        drawCaselleView(selectors.caselle());
    } else {
        drawNearestView(selectors.nearest());
    }
}
