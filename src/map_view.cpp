#include "map_view.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>

#include "config.h"
#include "geo.h"
#include "map_math.h"
#include "net_lock.h"
#include "../assets/plane_icon.h"

namespace {

constexpr int FRAME_SIZE = 240;
constexpr int TILE_PX = 256;
constexpr size_t TILE_BYTES = (size_t)TILE_PX * TILE_PX * 2;
constexpr size_t DOWNLOAD_BUF_BYTES = 96 * 1024;  // una tile PNG è tipicamente 10-40 KB
constexpr int MIN_SLOTS = 6;
constexpr int MAX_WANTED = 9;
constexpr int MAX_FALLBACK_LEVELS = 3;
constexpr unsigned long TILE_RETRY_MS = 15000;
constexpr int FAILURE_SLOTS = 8;

constexpr uint16_t COLOR_MAP_BG = 0x2104;       // grigio scuro finché le tile non arrivano
constexpr uint16_t COLOR_KEY = 0xF81F;          // magenta: trasparenza sprite icona
constexpr uint16_t COLOR_PLANE_YELLOW = 0xFEA0;
constexpr uint16_t COLOR_OUTLINE = 0x0000;
constexpr uint16_t COLOR_HOME_RING = 0xFFFF;
constexpr uint16_t COLOR_HOME_DOT = 0xF800;

constexpr int ICON_PAD = 3;
constexpr int ICON_SPRITE_PX = MAP_ICON_PX + 2 * ICON_PAD;

// ---------------------------------------------------------------------------
// Cache tile (PSRAM)
// ---------------------------------------------------------------------------
enum class SlotState : uint8_t { EMPTY, LOADING, READY };

struct Slot {
    LGFX_Sprite sprite;
    uint16_t* buf = nullptr;
    int z = -1, x = 0, y = 0;
    SlotState state = SlotState::EMPTY;
    uint32_t lastUse = 0;
};

struct Failure {
    int z = -1, x = 0, y = 0;
    unsigned long untilMs = 0;
};

Slot g_slots[MAP_TILE_SLOTS];
int g_slotCount = 0;
Failure g_failures[FAILURE_SLOTS];
int g_failureNext = 0;

SemaphoreHandle_t g_tileMutex = nullptr;  // protegge slot, wanted, failures
MapTileRef g_wanted[MAX_WANTED];
int g_wantedCount = 0;
uint32_t g_frameCounter = 0;

volatile bool g_active = false;
bool g_available = false;

uint8_t* g_downloadBuf = nullptr;

// ---------------------------------------------------------------------------
// Frame buffer + icona + stato camera
// ---------------------------------------------------------------------------
LGFX_Sprite g_frame;
uint16_t* g_frameBuf = nullptr;
LGFX_Sprite g_icon;

MapCamera g_cam;
double g_iconMx = 0.5, g_iconMy = 0.5, g_iconHeading = 0.0;
bool g_iconVisible = false;
bool g_needSnap = true;
unsigned long g_lastFrameMs = 0;

// ---------------------------------------------------------------------------
// Stream che raccoglie il body HTTP (anche chunked) in un buffer fisso.
// ---------------------------------------------------------------------------
class BufStream : public Stream {
public:
    BufStream(uint8_t* buf, size_t cap) : buf_(buf), cap_(cap) {}
    size_t write(uint8_t c) override { return write(&c, 1); }
    size_t write(const uint8_t* data, size_t len) override {
        if (len > cap_ - size_) {
            overflow_ = true;
            return 0;
        }
        memcpy(buf_ + size_, data, len);
        size_ += len;
        return len;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    size_t size() const { return size_; }
    bool overflow() const { return overflow_; }

private:
    uint8_t* buf_;
    size_t cap_;
    size_t size_ = 0;
    bool overflow_ = false;
};

// ---------------------------------------------------------------------------
// Accesso alla cache (da chiamare con g_tileMutex preso)
// ---------------------------------------------------------------------------
Slot* findSlot(int z, int x, int y) {
    for (int i = 0; i < g_slotCount; i++) {
        Slot& s = g_slots[i];
        if (s.state != SlotState::EMPTY && s.z == z && s.x == x && s.y == y) return &s;
    }
    return nullptr;
}

bool isWanted(int z, int x, int y) {
    for (int i = 0; i < g_wantedCount; i++) {
        if (g_wanted[i].z == z && g_wanted[i].x == x && g_wanted[i].y == y) return true;
    }
    return false;
}

bool recentlyFailed(const MapTileRef& t, unsigned long now) {
    for (const Failure& f : g_failures) {
        if (f.z == t.z && f.x == t.x && f.y == t.y && (long)(f.untilMs - now) > 0) return true;
    }
    return false;
}

const uint16_t* tileGetter(void*, int z, int x, int y) {
    for (int i = 0; i < g_slotCount; i++) {
        Slot& s = g_slots[i];
        if (s.state == SlotState::READY && s.z == z && s.x == x && s.y == y) {
            s.lastUse = g_frameCounter;
            return s.buf;
        }
    }
    return nullptr;
}

// Sceglie la prossima tile voluta ancora mancante e le riserva uno slot.
bool claimNextTile(MapTileRef& outTile, Slot*& outSlot) {
    bool claimed = false;
    xSemaphoreTake(g_tileMutex, portMAX_DELAY);
    unsigned long now = millis();
    for (int i = 0; i < g_wantedCount && !claimed; i++) {
        const MapTileRef& t = g_wanted[i];
        if (findSlot(t.z, t.x, t.y)) continue;
        if (recentlyFailed(t, now)) continue;

        Slot* victim = nullptr;
        for (int j = 0; j < g_slotCount; j++) {
            Slot& s = g_slots[j];
            if (s.state == SlotState::EMPTY) { victim = &s; break; }
            if (s.state == SlotState::READY && !isWanted(s.z, s.x, s.y)) {
                if (!victim || s.lastUse < victim->lastUse) victim = &s;
            }
        }
        if (!victim) continue;

        victim->z = t.z;
        victim->x = t.x;
        victim->y = t.y;
        victim->state = SlotState::LOADING;
        outTile = t;
        outSlot = victim;
        claimed = true;
    }
    xSemaphoreGive(g_tileMutex);
    return claimed;
}

void finishTile(Slot* slot, const MapTileRef& tile, bool ok) {
    xSemaphoreTake(g_tileMutex, portMAX_DELAY);
    if (ok) {
        slot->state = SlotState::READY;
        slot->lastUse = g_frameCounter;
    } else {
        slot->state = SlotState::EMPTY;
        slot->z = -1;
        Failure& f = g_failures[g_failureNext];
        g_failureNext = (g_failureNext + 1) % FAILURE_SLOTS;
        f.z = tile.z;
        f.x = tile.x;
        f.y = tile.y;
        f.untilMs = millis() + TILE_RETRY_MS;
    }
    xSemaphoreGive(g_tileMutex);
}

bool fetchTile(WiFiClientSecure& client, HTTPClient& http, const MapTileRef& tile, Slot* slot) {
    char url[112];
    snprintf(url, sizeof(url), MAP_TILE_URL_FMT, tile.z, tile.x, tile.y);

    NetLockGuard guard;
    if (!g_active) return false;

    http.setReuse(true);
    http.setUserAgent(MAP_TILE_USER_AGENT);
    http.setConnectTimeout(6000);
    http.setTimeout(8000);
    if (!http.begin(client, url)) return false;

    int code = http.GET();
    if (code != 200) {
        Serial.printf("[map] tile %d/%d/%d: HTTP %d\n", tile.z, tile.x, tile.y, code);
        http.end();
        return false;
    }

    BufStream body(g_downloadBuf, DOWNLOAD_BUF_BYTES);
    int n = http.writeToStream(&body);
    http.end();
    if (n <= 0 || body.overflow()) {
        Serial.printf("[map] tile %d/%d/%d: download fallito (%d)\n", tile.z, tile.x, tile.y, n);
        return false;
    }

    if (!slot->sprite.drawPng(g_downloadBuf, (uint32_t)body.size(), 0, 0)) {
        Serial.printf("[map] tile %d/%d/%d: decodifica PNG fallita (%u byte)\n",
                      tile.z, tile.x, tile.y, (unsigned)body.size());
        return false;
    }
    return true;
}

void tileTask(void*) {
    WiFiClientSecure client;
    // Come per OpenSky: dati pubblici non sensibili, niente validazione cert.
    client.setInsecure();
    HTTPClient http;
    bool connectionOpen = false;
    unsigned long lastWorkMs = 0;

    for (;;) {
        if (!g_active || WiFi.status() != WL_CONNECTED) {
            if (connectionOpen) {
                client.stop();
                connectionOpen = false;
            }
            vTaskDelay(pdMS_TO_TICKS(250));
            continue;
        }

        MapTileRef tile;
        Slot* slot = nullptr;
        if (!claimNextTile(tile, slot)) {
            // Libera i buffer TLS se la connessione resta ferma a lungo.
            if (connectionOpen && millis() - lastWorkMs > 4000) {
                client.stop();
                connectionOpen = false;
            }
            vTaskDelay(pdMS_TO_TICKS(80));
            continue;
        }

        bool ok = fetchTile(client, http, tile, slot);
        connectionOpen = true;
        lastWorkMs = millis();
        finishTile(slot, tile, ok);
    }
}

// ---------------------------------------------------------------------------
// Icona aeroplano: 1-bit 96x96 -> giallo con bordo nero a MAP_ICON_PX px.
// ---------------------------------------------------------------------------
void buildPlaneIcon() {
    static float cov[ICON_SPRITE_PX][ICON_SPRITE_PX];
    memset(cov, 0, sizeof(cov));

    const int bytesPerRow = PLANE_ICON_SIZE / 8;
    const float step = (float)PLANE_ICON_SIZE / MAP_ICON_PX;
    constexpr int SUB = 4;
    for (int y = 0; y < MAP_ICON_PX; y++) {
        for (int x = 0; x < MAP_ICON_PX; x++) {
            int hits = 0;
            for (int sy = 0; sy < SUB; sy++) {
                for (int sx = 0; sx < SUB; sx++) {
                    int srcX = (int)((x + (sx + 0.5f) / SUB) * step);
                    int srcY = (int)((y + (sy + 0.5f) / SUB) * step);
                    if (srcX >= PLANE_ICON_SIZE || srcY >= PLANE_ICON_SIZE) continue;
                    uint8_t byte = PLANE_ICON_BITS[srcY * bytesPerRow + (srcX / 8)];
                    if (byte & (1 << (7 - (srcX % 8)))) hits++;
                }
            }
            cov[y + ICON_PAD][x + ICON_PAD] = (float)hits / (SUB * SUB);
        }
    }

    g_icon.setColorDepth(16);
    g_icon.createSprite(ICON_SPRITE_PX, ICON_SPRITE_PX);
    g_icon.fillSprite(COLOR_KEY);
    for (int y = 0; y < ICON_SPRITE_PX; y++) {
        for (int x = 0; x < ICON_SPRITE_PX; x++) {
            float maxNear = 0.0f;
            for (int dy = -2; dy <= 2; dy++) {
                for (int dx = -2; dx <= 2; dx++) {
                    int yy = y + dy, xx = x + dx;
                    if (yy < 0 || xx < 0 || yy >= ICON_SPRITE_PX || xx >= ICON_SPRITE_PX) continue;
                    if (cov[yy][xx] > maxNear) maxNear = cov[yy][xx];
                }
            }
            if (cov[y][x] >= 0.5f) {
                g_icon.drawPixel(x, y, COLOR_PLANE_YELLOW);
            } else if (maxNear > 0.2f) {
                g_icon.drawPixel(x, y, COLOR_OUTLINE);
            }
        }
    }
}

double smoothAlpha(double dt, double tau) {
    return 1.0 - exp(-dt / tau);
}

}  // namespace

bool mapViewInit() {
    if (g_available) return true;

    if (!psramFound()) {
        Serial.println("[map] PSRAM non trovata: vista mappa disattivata (vedi platformio.ini)");
        return false;
    }

    constexpr uint32_t caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    g_frameBuf = (uint16_t*)heap_caps_malloc((size_t)FRAME_SIZE * FRAME_SIZE * 2, caps);
    g_downloadBuf = (uint8_t*)heap_caps_malloc(DOWNLOAD_BUF_BYTES, caps);
    if (!g_frameBuf || !g_downloadBuf) {
        Serial.println("[map] PSRAM insufficiente per frame/download buffer");
        return false;
    }

    for (int i = 0; i < MAP_TILE_SLOTS; i++) {
        uint16_t* buf = (uint16_t*)heap_caps_malloc(TILE_BYTES, caps);
        if (!buf) break;
        g_slots[i].buf = buf;
        g_slots[i].sprite.setBuffer(buf, TILE_PX, TILE_PX, 16);
        g_slotCount++;
    }
    if (g_slotCount < MIN_SLOTS) {
        Serial.printf("[map] PSRAM insufficiente per la cache tile (%d slot)\n", g_slotCount);
        return false;
    }

    g_frame.setBuffer(g_frameBuf, FRAME_SIZE, FRAME_SIZE, 16);
    g_tileMutex = xSemaphoreCreateMutex();
    buildPlaneIcon();

    xTaskCreatePinnedToCore(tileTask, "map_tiles", 20480, nullptr, 1, nullptr, 0);

    g_available = true;
    Serial.printf("[map] pronta: %d slot tile, PSRAM libera %u KB\n", g_slotCount,
                  (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
    return true;
}

bool mapViewAvailable() {
    return g_available;
}

void mapViewSetActive(bool active) {
    if (!g_available) return;
    if (active && !g_active) {
        g_needSnap = true;
        xSemaphoreTake(g_tileMutex, portMAX_DELAY);
        for (Failure& f : g_failures) f.z = -1;
        xSemaphoreGive(g_tileMutex);
    }
    g_active = active;
}

void mapViewRender(lgfx::LovyanGFX& dst, const TrackedFlight& flight) {
    if (!g_available) return;

    const unsigned long now = millis();
    double dt = g_lastFrameMs == 0 ? 0.05 : (now - g_lastFrameMs) / 1000.0;
    if (dt < 0.001) dt = 0.001;
    if (dt > 0.25) dt = 0.25;
    g_lastFrameMs = now;
    g_frameCounter++;

    // --- Bersaglio della camera -------------------------------------------
    const double homeMx = mapMercX(HOME_LON), homeMy = mapMercY(HOME_LAT);
    double targetMx = homeMx, targetMy = homeMy, targetZoom = MAP_ZOOM_IDLE;
    double planeMx = 0, planeMy = 0, planeHeading = g_iconHeading;
    const bool hasPlane = flight.valid;

    if (hasPlane) {
        GeoPoint pos = flight.estimatedPosition(now);
        planeMx = mapMercX(pos.lon);
        planeMy = mapMercY(pos.lat);
        if (!isnan(flight.trueTrackDeg)) planeHeading = flight.trueTrackDeg;

        targetMx = (homeMx + planeMx) * 0.5;
        targetMy = (homeMy + planeMy) * 0.5;
        double dist = hypot(planeMx - homeMx, planeMy - homeMy);
        targetZoom = mapFitZoom(dist, MAP_FIT_DIAMETER_PX, MAP_ZOOM_MIN, MAP_ZOOM_MAX);
    }

    const double rotation = 0.0;  // mappa sempre con il nord in alto

    // --- Animazione (filtro esponenziale: segue sia l'aereo che i cambi) ---
    if (g_needSnap) {
        g_cam.mx = targetMx;
        g_cam.my = targetMy;
        g_cam.zoom = targetZoom;
        g_cam.rotationDeg = rotation;
        g_needSnap = false;
    } else {
        double aPos = smoothAlpha(dt, MAP_CAMERA_TAU_S);
        double aZoom = smoothAlpha(dt, MAP_ZOOM_TAU_S);
        g_cam.mx += (targetMx - g_cam.mx) * aPos;
        g_cam.my += (targetMy - g_cam.my) * aPos;
        g_cam.zoom += (targetZoom - g_cam.zoom) * aZoom;
        g_cam.rotationDeg = rotation;
    }

    if (hasPlane) {
        if (!g_iconVisible) {
            g_iconMx = planeMx;
            g_iconMy = planeMy;
            g_iconHeading = planeHeading;
        } else {
            double a = smoothAlpha(dt, MAP_ICON_TAU_S);
            g_iconMx += (planeMx - g_iconMx) * a;
            g_iconMy += (planeMy - g_iconMy) * a;
            double dh = fmod(planeHeading - g_iconHeading + 540.0, 360.0) - 180.0;
            g_iconHeading = normalizeAngle360(g_iconHeading + dh * a);
        }
    }
    g_iconVisible = hasPlane;

    // --- Tile da avere / da disegnare -------------------------------------
    const int tileZ = mapTileZoom(g_cam.zoom, (int)MAP_ZOOM_MIN, 19);
    MapTileRef wanted[MAX_WANTED];
    const int wantedCount = mapVisibleTiles(g_cam, tileZ, FRAME_SIZE / 2, wanted, MAX_WANTED);

    g_frame.fillSprite(COLOR_MAP_BG);

    xSemaphoreTake(g_tileMutex, portMAX_DELAY);
    g_wantedCount = wantedCount;
    for (int i = 0; i < wantedCount; i++) g_wanted[i] = wanted[i];
    mapRasterize(g_frameBuf, FRAME_SIZE, g_cam, tileZ, tileGetter, nullptr, MAX_FALLBACK_LEVELS);
    xSemaphoreGive(g_tileMutex);

    // --- Sovrapposizioni ----------------------------------------------------
    float hx, hy;
    mapProject(g_cam, homeMx, homeMy, FRAME_SIZE, hx, hy);
    g_frame.fillCircle((int)hx, (int)hy, 7, COLOR_HOME_RING);
    g_frame.fillCircle((int)hx, (int)hy, 5, COLOR_HOME_DOT);

    if (g_iconVisible) {
        float ix, iy;
        mapProject(g_cam, g_iconMx, g_iconMy, FRAME_SIZE, ix, iy);
        // Con il nord in alto, la rotta reale (gradi dal nord, in senso orario)
        // è la rotazione oraria dell'icona, corretta dell'offset di disegno.
        float angle = (float)normalizeAngle360(g_iconHeading + MAP_ICON_ROTATION_OFFSET_DEG);
        g_icon.pushRotateZoom(&g_frame, ix, iy, angle, 1.0f, 1.0f, COLOR_KEY);
    }

    dst.startWrite();
    g_frame.pushSprite(&dst, 0, 0);
    dst.endWrite();
}
