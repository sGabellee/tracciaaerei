#pragma once

// Matematica pura della vista mappa (nessuna dipendenza da Arduino/LovyanGFX),
// così si può verificare anche fuori dall'ESP32.
//
// Convenzioni:
//  - coordinate "mercator unit": x,y in [0,1) sull'intero mappamondo OSM
//    (x cresce verso est, y cresce verso sud, come le tile).
//  - un pixel alla zoom Z vale 1/(256*2^Z) di mercator unit.
//  - rotationDeg = rotazione ORARIA della mappa sullo schermo (0 = nord in alto).

#include <cmath>
#include <cstdint>

struct MapCamera {
    double mx = 0.5;
    double my = 0.5;
    double zoom = 10.0;       // zoom continuo (le tile hanno zoom intero)
    double rotationDeg = 0.0;
};

inline double mapMercX(double lonDeg) {
    return (lonDeg + 180.0) / 360.0;
}

inline double mapMercY(double latDeg) {
    double s = sin(latDeg * M_PI / 180.0);
    return 0.5 - log((1.0 + s) / (1.0 - s)) / (4.0 * M_PI);
}

// Zoom (continuo) a cui due punti distanti distMercUnits stanno entro
// fitDiameterPx pixel di schermo, limitato a [zoomMin, zoomMax].
inline double mapFitZoom(double distMercUnits, double fitDiameterPx, double zoomMin, double zoomMax) {
    if (distMercUnits < 1e-9) return zoomMax;
    double z = log2(fitDiameterPx / (distMercUnits * 256.0));
    if (z < zoomMin) return zoomMin;
    if (z > zoomMax) return zoomMax;
    return z;
}

// Livello di zoom intero delle tile da usare per uno zoom continuo: il più
// vicino, così la scala di disegno resta tra ~0.71 e ~1.41.
inline int mapTileZoom(double zoom, int minZ, int maxZ) {
    int z = (int)floor(zoom + 0.5);
    if (z < minZ) z = minZ;
    if (z > maxZ) z = maxZ;
    return z;
}

struct MapTileRef {
    int z;
    int x;
    int y;
};

// Tile (a zoom tileZ) che intersecano il cerchio visibile, ordinate dalla più
// vicina al centro. Restituisce quante ne ha scritte (max maxOut).
inline int mapVisibleTiles(const MapCamera& cam, int tileZ, int viewRadiusPx, MapTileRef* out, int maxOut) {
    const double worldPx = 256.0 * (double)(1 << tileZ);
    const double scale = pow(2.0, cam.zoom - tileZ);
    const double cwx = cam.mx * worldPx;
    const double cwy = cam.my * worldPx;
    const double r = viewRadiusPx / scale;  // raggio visibile in pixel-tile

    int tx0 = (int)floor((cwx - r) / 256.0);
    int tx1 = (int)floor((cwx + r) / 256.0);
    int ty0 = (int)floor((cwy - r) / 256.0);
    int ty1 = (int)floor((cwy + r) / 256.0);
    const int n = 1 << tileZ;

    int count = 0;
    double dist[16];
    for (int ty = ty0; ty <= ty1; ty++) {
        if (ty < 0 || ty >= n) continue;
        for (int tx = tx0; tx <= tx1; tx++) {
            int wtx = ((tx % n) + n) % n;
            double rx0 = tx * 256.0, ry0 = ty * 256.0;
            double nx = cwx < rx0 ? rx0 : (cwx > rx0 + 256.0 ? rx0 + 256.0 : cwx);
            double ny = cwy < ry0 ? ry0 : (cwy > ry0 + 256.0 ? ry0 + 256.0 : cwy);
            double d2 = (nx - cwx) * (nx - cwx) + (ny - cwy) * (ny - cwy);
            if (d2 > r * r) continue;
            if (count >= maxOut || count >= 16) continue;
            double cdx = rx0 + 128.0 - cwx, cdy = ry0 + 128.0 - cwy;
            double d = cdx * cdx + cdy * cdy;
            int pos = count++;
            while (pos > 0 && dist[pos - 1] > d) {
                dist[pos] = dist[pos - 1];
                out[pos] = out[pos - 1];
                pos--;
            }
            dist[pos] = d;
            out[pos] = MapTileRef{ tileZ, wtx, ty };
        }
    }
    return count;
}

// Restituisce il puntatore ai pixel RGB565 (256x256, formato grezzo identico
// a quello del frame buffer) della tile richiesta, oppure nullptr se non è
// (ancora) disponibile.
using MapTileGetter = const uint16_t* (*)(void* ctx, int z, int x, int y);

// Posizione sullo schermo (pixel, origine in alto a sinistra) di un punto
// mercator, data la camera. Il centro dello schermo è (size/2, size/2).
inline void mapProject(const MapCamera& cam, double mx, double my, int size, float& sx, float& sy) {
    const double pxPerUnit = 256.0 * pow(2.0, cam.zoom);
    double dx = (mx - cam.mx) * pxPerUnit;
    double dy = (my - cam.my) * pxPerUnit;
    double th = cam.rotationDeg * M_PI / 180.0;
    double c = cos(th), s = sin(th);
    sx = (float)(size * 0.5 + dx * c - dy * s);
    sy = (float)(size * 0.5 + dx * s + dy * c);
}

// Disegna la mappa nel frame buffer (size x size, RGB565 grezzo) campionando
// pixel per pixel (nearest) solo dentro il cerchio inscritto. Se la tile del
// livello tileZ manca, ripiega sui livelli genitori (più grossolani) già in
// cache, fino a maxFallback livelli sopra. I pixel senza alcuna tile restano
// quelli già presenti nel buffer (il chiamante lo riempie con lo sfondo).
inline void mapRasterize(uint16_t* frame, int size, const MapCamera& cam, int tileZ,
                         MapTileGetter get, void* ctx, int maxFallback) {
    const double worldPx = 256.0 * (double)(1 << tileZ);
    const float scale = (float)pow(2.0, cam.zoom - tileZ);
    const float inv = 1.0f / scale;
    const double th = cam.rotationDeg * M_PI / 180.0;
    const float c = (float)cos(th), s = (float)sin(th);

    // Centro camera in pixel-tile: parte intera (double→int) + frazione (float),
    // per non perdere precisione a zoom alti con la sola aritmetica float.
    const double cwx = cam.mx * worldPx;
    const double cwy = cam.my * worldPx;
    const int ix0 = (int)floor(cwx);
    const int iy0 = (int)floor(cwy);
    const float fx = (float)(cwx - (double)ix0);
    const float fy = (float)(cwy - (double)iy0);
    const int maxPx = (int)worldPx;

    const float half = size * 0.5f;
    const float radius = half;

    // Sorgente risolta per l'ultima tile incontrata (la coerenza spaziale
    // evita di rifare la ricerca ad ogni pixel).
    int lastTx = -1000000, lastTy = -1000000;
    const uint16_t* srcBuf = nullptr;
    int srcShift = 0;

    for (int sy = 0; sy < size; sy++) {
        float dy = (sy + 0.5f) - half;
        float span2 = radius * radius - dy * dy;
        if (span2 <= 0) continue;
        float span = sqrtf(span2);
        int xa = (int)ceilf(half - span - 0.5f);
        int xb = (int)floorf(half + span - 0.5f);
        if (xa < 0) xa = 0;
        if (xb > size - 1) xb = size - 1;

        float dx = (xa + 0.5f) - half;
        // R(-theta) * (dx, dy) / scale, incrementale lungo la riga
        float ux = (dx * c + dy * s) * inv;
        float uy = (-dx * s + dy * c) * inv;
        const float dux = c * inv;
        const float duy = -s * inv;

        uint16_t* row = frame + (size_t)sy * size;
        for (int sx = xa; sx <= xb; sx++, ux += dux, uy += duy) {
            float rx = fx + ux;
            float ry = fy + uy;
            int px = ix0 + (int)floorf(rx);
            int py = iy0 + (int)floorf(ry);
            if (py < 0 || py >= maxPx) continue;
            if (px < 0 || px >= maxPx) {
                px %= maxPx;
                if (px < 0) px += maxPx;
            }

            int tx = px >> 8, ty = py >> 8;
            if (tx != lastTx || ty != lastTy) {
                lastTx = tx;
                lastTy = ty;
                srcBuf = nullptr;
                srcShift = 0;
                int ttx = tx, tty = ty;
                for (int k = 0; k <= maxFallback && tileZ - k >= 0; k++) {
                    const uint16_t* b = get(ctx, tileZ - k, ttx, tty);
                    if (b) {
                        srcBuf = b;
                        srcShift = k;
                        break;
                    }
                    ttx >>= 1;
                    tty >>= 1;
                }
            }
            if (!srcBuf) continue;
            int lx = (int)((px >> srcShift) & 255);
            int ly = (int)((py >> srcShift) & 255);
            row[sx] = srcBuf[ly * 256 + lx];
        }
    }
}
