#include "net_lock.h"

#include <Arduino.h>

namespace {
SemaphoreHandle_t g_netMutex = nullptr;
}

void netLockInit() {
    if (!g_netMutex) g_netMutex = xSemaphoreCreateMutex();
}

void netLock() {
    if (g_netMutex) xSemaphoreTake(g_netMutex, portMAX_DELAY);
}

void netUnlock() {
    if (g_netMutex) xSemaphoreGive(g_netMutex);
}
