#pragma once

#include <cmath>

struct GeoPoint {
    double lat;
    double lon;
};

inline double geoDeg2Rad(double d) { return d * M_PI / 180.0; }
inline double geoRad2Deg(double r) { return r * 180.0 / M_PI; }

// Distanza in metri tra due punti (formula di Haversine).
inline double geoDistanceMeters(const GeoPoint& a, const GeoPoint& b) {
    constexpr double R = 6371000.0;
    double dLat = geoDeg2Rad(b.lat - a.lat);
    double dLon = geoDeg2Rad(b.lon - a.lon);
    double la1 = geoDeg2Rad(a.lat);
    double la2 = geoDeg2Rad(b.lat);
    double h = sin(dLat / 2) * sin(dLat / 2) +
               cos(la1) * cos(la2) * sin(dLon / 2) * sin(dLon / 2);
    return 2.0 * R * atan2(sqrt(h), sqrt(1.0 - h));
}

// Rotta iniziale in gradi (0-360, 0 = nord) da a verso b.
inline double geoBearingDegrees(const GeoPoint& a, const GeoPoint& b) {
    double la1 = geoDeg2Rad(a.lat);
    double la2 = geoDeg2Rad(b.lat);
    double dLon = geoDeg2Rad(b.lon - a.lon);
    double y = sin(dLon) * cos(la2);
    double x = cos(la1) * sin(la2) - sin(la1) * cos(la2) * cos(dLon);
    double brng = geoRad2Deg(atan2(y, x));
    return fmod(brng + 360.0, 360.0);
}

// Estrapola la posizione dopo dtSeconds, dati velocità (m/s) e rotta (gradi).
// Approssimazione piana: valida per le brevi distanze/tempi di questo progetto
// (pochi km, pochi secondi tra un poll OpenSky e l'altro).
inline GeoPoint geoExtrapolate(const GeoPoint& from, double speedMs, double trackDeg, double dtSeconds) {
    double distM = speedMs * dtSeconds;
    double trackRad = geoDeg2Rad(trackDeg);
    double dNorthM = distM * cos(trackRad);
    double dEastM = distM * sin(trackRad);
    double dLat = dNorthM / 111320.0;
    double dLon = dEastM / (111320.0 * cos(geoDeg2Rad(from.lat)));
    return GeoPoint{ from.lat + dLat, from.lon + dLon };
}

inline double normalizeAngle360(double deg) {
    double d = fmod(deg, 360.0);
    return d < 0 ? d + 360.0 : d;
}
