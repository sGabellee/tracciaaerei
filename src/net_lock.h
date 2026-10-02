#pragma once

// Un solo client HTTPS alla volta: OpenSky, AeroDataBox e il download delle
// tile della mappa girano su task diversi e due sessioni TLS contemporanee
// non entrerebbero nella RAM interna dell'ESP32-S3.
void netLockInit();
void netLock();
void netUnlock();

struct NetLockGuard {
    NetLockGuard() { netLock(); }
    ~NetLockGuard() { netUnlock(); }
    NetLockGuard(const NetLockGuard&) = delete;
    NetLockGuard& operator=(const NetLockGuard&) = delete;
};
