#include "display.h"
#include "config.h"
#include "geo.h"
#include "lgfx_config.h"
#include "map_view.h"
#include "../assets/plane_icon.h"

namespace {

LGFX gfx;
LGFX_Sprite iconSprite(&gfx);
LGFX_Sprite iconGreenSprite(&gfx);
LGFX_Sprite landSprite(&gfx);  // frame dell'animazione di atterraggio

constexpr int LAND_SPR = 180;  // lato dello sprite, centrato sullo schermo
constexpr int LAND_C = LAND_SPR / 2;
constexpr int LAND_RING_R = 70;
constexpr int LAND_PLANE_START_DY = -30;  // l'aereo parte dove lo disegna la vista di tracking (y=90)

constexpr uint16_t COLOR_BG = 0x0000;           // nero
constexpr uint16_t COLOR_PLANE = 0xFFFF;        // bianco
constexpr uint16_t COLOR_TEXT = 0xFFFF;
constexpr uint16_t COLOR_DIM = 0x7BEF;          // grigio, per righe secondarie
constexpr uint16_t COLOR_CHECK = 0x07E0;        // verde
constexpr uint16_t COLOR_TRANSPARENT_KEY = 0xF81F;  // magenta: chiave di trasparenza sprite

DisplayMode currentMode = DisplayMode::CASELLE;
bool screenAsleep = false;
bool landSpriteOk = false;
bool landedScreenPrepared = false;

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

void buildIconSprite(LGFX_Sprite& sprite, uint16_t color) {
    sprite.setColorDepth(16);
    sprite.createSprite(PLANE_ICON_SIZE, PLANE_ICON_SIZE);
    sprite.fillSprite(COLOR_TRANSPARENT_KEY);

    const int bytesPerRow = PLANE_ICON_SIZE / 8;
    for (int y = 0; y < PLANE_ICON_SIZE; y++) {
        for (int x = 0; x < PLANE_ICON_SIZE; x++) {
            uint8_t byte = PLANE_ICON_BITS[y * bytesPerRow + (x / 8)];
            bool set = byte & (1 << (7 - (x % 8)));
            if (set) {
                sprite.drawPixel(x, y, color);
            }
        }
    }
}

float clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

float easeInOutCubic(float t) {
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

float easeOutCubic(float t) {
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}

// Punti della spunta, relativi al centro dello sprite.
constexpr float CHECK_P0X = -42.0f, CHECK_P0Y = 2.0f;
constexpr float CHECK_P1X = -15.0f, CHECK_P1Y = 30.0f;
constexpr float CHECK_P2X = 45.0f, CHECK_P2Y = -30.0f;

// Disegna la spunta "a penna": progress 0..1 percorre il primo tratto e poi il secondo.
void drawCheckProgress(LGFX_Sprite& spr, float progress) {
    const float l1 = hypotf(CHECK_P1X - CHECK_P0X, CHECK_P1Y - CHECK_P0Y);
    const float l2 = hypotf(CHECK_P2X - CHECK_P1X, CHECK_P2Y - CHECK_P1Y);
    const float drawn = progress * (l1 + l2);
    const int w = 10;

    float f1 = fminf(1.0f, drawn / l1);
    float x1 = LAND_C + CHECK_P0X + (CHECK_P1X - CHECK_P0X) * f1;
    float y1 = LAND_C + CHECK_P0Y + (CHECK_P1Y - CHECK_P0Y) * f1;
    spr.drawWideLine(LAND_C + CHECK_P0X, LAND_C + CHECK_P0Y, x1, y1, w, COLOR_CHECK);
    spr.fillCircle((int)(LAND_C + CHECK_P0X), (int)(LAND_C + CHECK_P0Y), w / 2, COLOR_CHECK);
    spr.fillCircle((int)x1, (int)y1, w / 2, COLOR_CHECK);

    if (drawn > l1) {
        float f2 = fminf(1.0f, (drawn - l1) / l2);
        float x2 = LAND_C + CHECK_P1X + (CHECK_P2X - CHECK_P1X) * f2;
        float y2 = LAND_C + CHECK_P1Y + (CHECK_P2Y - CHECK_P1Y) * f2;
        spr.drawWideLine(LAND_C + CHECK_P1X, LAND_C + CHECK_P1Y, x2, y2, w, COLOR_CHECK);
        spr.fillCircle((int)x2, (int)y2, w / 2, COLOR_CHECK);
    }
}

// Un frame dell'animazione: l'aeroplanino gira su se stesso e si rimpicciolisce,
// un anello verde si espande, la spunta si disegna, poi un'onda pulsante.
void drawLandedFrame(unsigned long elapsedMs) {
    if (!landSpriteOk) {
        gfx.drawWideLine(78, 122, 105, 150, 10, COLOR_CHECK);
        gfx.drawWideLine(105, 150, 165, 90, 10, COLOR_CHECK);
        return;
    }

    const float t = clamp01((float)elapsedMs / (float)LANDED_ANIMATION_MS);
    landSprite.fillSprite(COLOR_BG);

    if (elapsedMs > LANDED_ANIMATION_MS) {
        float u = fmodf((float)(elapsedMs - LANDED_ANIMATION_MS) / 1500.0f, 1.0f);
        int r = LAND_RING_R + 4 + (int)(u * 14.0f);
        uint16_t c = gfx.color565(0, (uint8_t)(255.0f * (1.0f - u)), 0);
        landSprite.fillCircle(LAND_C, LAND_C, r, c);
        landSprite.fillCircle(LAND_C, LAND_C, r - 2, COLOR_BG);
    }

    float ringP = clamp01((t - 0.12f) / 0.5f);
    if (ringP > 0.0f) {
        int r = (int)(LAND_RING_R * easeOutCubic(ringP));
        if (r > 8) {
            landSprite.fillCircle(LAND_C, LAND_C, r, COLOR_CHECK);
            landSprite.fillCircle(LAND_C, LAND_C, r - 7, COLOR_BG);
        }
    }

    float planeP = clamp01(t / 0.58f);
    if (planeP < 1.0f) {
        float e = easeInOutCubic(planeP);
        float scale = 1.0f - e;
        if (scale > 0.04f) {
            float angle = 720.0f * e;
            float cy = LAND_C + LAND_PLANE_START_DY * (1.0f - easeOutCubic(clamp01(t / 0.3f)));
            LGFX_Sprite& src = (planeP < 0.5f) ? iconSprite : iconGreenSprite;
            src.pushRotateZoom(&landSprite, (float)LAND_C, cy, angle, scale, scale, COLOR_TRANSPARENT_KEY);
        }
    }

    float checkP = clamp01((t - 0.50f) / 0.42f);
    if (checkP > 0.0f) drawCheckProgress(landSprite, easeOutCubic(checkP));

    landSprite.pushSprite(&gfx, (240 - LAND_SPR) / 2, (240 - LAND_SPR) / 2);
}

String fmtOrDash(const String& s) {
    return s.isEmpty() ? String("--") : s;
}

void drawCaselleView(const CaselleView& v) {
    if (v.state == CaselleState::IDLE) {
        landedScreenPrepared = false;
        screenSleep();
        return;
    }
    screenWake();

    if (v.state == CaselleState::LANDED) {
        // Solo animazione + spunta, nessun testo.
        if (!landedScreenPrepared) {
            gfx.fillScreen(COLOR_BG);
            landedScreenPrepared = true;
        }
        drawLandedFrame(millis() - v.landedAnimationStartMs);
        return;
    }
    landedScreenPrepared = false;

    gfx.fillScreen(COLOR_BG);
    iconSprite.pushSprite(120 - PLANE_ICON_SIZE / 2, 42, COLOR_TRANSPARENT_KEY);

    gfx.setTextDatum(lgfx::textdatum_t::middle_center);

    // Se la rotta non è nota (AeroDataBox non risponde / volo sconosciuto) si
    // mostra comunque il callsign, che OpenSky fornisce sempre.
    String title = !v.current.route.originIata.isEmpty() ? v.current.route.originIata
                   : (!v.current.callsign.isEmpty() ? v.current.callsign : String("--"));

    gfx.setTextColor(COLOR_TEXT, COLOR_BG);
    gfx.setTextSize(2);
    gfx.drawString(title, 120, 165);

    gfx.setTextColor(COLOR_DIM, COLOR_BG);
    gfx.setTextSize(1);
    gfx.drawString(v.current.route.aircraftModel, 120, 195);
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
    if (v.current.route.originIata.isEmpty() && v.current.route.destIata.isEmpty() &&
        !v.current.callsign.isEmpty()) {
        snprintf(line1, sizeof(line1), "%s", v.current.callsign.c_str());
    } else {
        snprintf(line1, sizeof(line1), "%s -> %s",
                 fmtOrDash(v.current.route.originIata).c_str(),
                 fmtOrDash(v.current.route.destIata).c_str());
    }

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
    buildIconSprite(iconSprite, COLOR_PLANE);
    buildIconSprite(iconGreenSprite, COLOR_CHECK);
    landSprite.setPsram(psramFound());
    landSprite.setColorDepth(16);
    landSpriteOk = (landSprite.createSprite(LAND_SPR, LAND_SPR) != nullptr);
    if (!landSpriteOk) Serial.println("[display] sprite animazione atterraggio non allocato: spunta statica");
    mapViewInit();
}

void displayToggleMode() {
    switch (currentMode) {
        case DisplayMode::CASELLE:
            currentMode = DisplayMode::NEAREST;
            break;
        case DisplayMode::NEAREST:
            currentMode = mapViewAvailable() ? DisplayMode::MAP : DisplayMode::CASELLE;
            break;
        case DisplayMode::MAP:
            currentMode = DisplayMode::CASELLE;
            break;
    }
    landedScreenPrepared = false;
    mapViewSetActive(currentMode == DisplayMode::MAP);
}

DisplayMode displayGetMode() {
    return currentMode;
}

void displayRender(const FlightSelectors& selectors) {
    switch (currentMode) {
        case DisplayMode::CASELLE:
            drawCaselleView(selectors.caselle());
            break;
        case DisplayMode::NEAREST:
            drawNearestView(selectors.nearest());
            break;
        case DisplayMode::MAP:
            screenWake();
            mapViewRender(gfx, selectors.nearest().current);
            break;
    }
}

bool displayWantsFastFrames(const FlightSelectors& selectors) {
    if (currentMode == DisplayMode::MAP) return true;
    return currentMode == DisplayMode::CASELLE && selectors.caselle().state == CaselleState::LANDED;
}
