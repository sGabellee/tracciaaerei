#include <Arduino.h>
#include <WiFi.h>
#include <vector>

#include "config.h"
#include "display.h"
#include "flight_selectors.h"
#include "net_lock.h"
#include "opensky.h"
#include "secrets.h"

namespace {

FlightSelectors selectors;

unsigned long lastTickMs = 0;
unsigned long lastMapFrameMs = 0;
unsigned long lastWifiAttemptMs = 0;

int lastButtonReading = HIGH;
int stableButtonState = HIGH;
unsigned long lastButtonChangeMs = 0;

// Il poll OpenSky gira su un task a parte: bloccava il loop per qualche
// secondo ad ogni giro, e nella vista mappa l'animazione si sarebbe fermata.
// `selectors` resta usato solo dal loop: il task consegna solo i dati grezzi.
SemaphoreHandle_t pendingMutex = nullptr;
std::vector<AircraftState> pendingStates;
bool hasPending = false;

void connectWifi() {
    if (WiFi.status() == WL_CONNECTED) return;
    if (millis() - lastWifiAttemptMs < WIFI_RETRY_INTERVAL_MS) return;
    lastWifiAttemptMs = millis();

    Serial.println("[wifi] connessione...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void pollTask(void*) {
    for (;;) {
        if (WiFi.status() != WL_CONNECTED) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        std::vector<AircraftState> states;
        bool ok;
        {
            NetLockGuard guard;
            ok = openSkyFetchStates(states);
        }

        if (ok) {
            Serial.printf("[opensky] %u velivoli nel raggio\n", (unsigned)states.size());
            xSemaphoreTake(pendingMutex, portMAX_DELAY);
            pendingStates = std::move(states);
            hasPending = true;
            xSemaphoreGive(pendingMutex);
        } else {
            Serial.println("[opensky] poll fallito, riprovo al prossimo giro");
        }

        vTaskDelay(pdMS_TO_TICKS(OPENSKY_POLL_INTERVAL_MS));
    }
}

void applyPendingPoll() {
    std::vector<AircraftState> states;
    xSemaphoreTake(pendingMutex, portMAX_DELAY);
    bool have = hasPending;
    if (have) {
        states = std::move(pendingStates);
        hasPending = false;
    }
    xSemaphoreGive(pendingMutex);

    if (have) selectors.onPoll(states);
}

void handleButton() {
    int reading = digitalRead(PIN_BUTTON);
    if (reading != lastButtonReading) {
        lastButtonChangeMs = millis();
        lastButtonReading = reading;
    }
    if (millis() - lastButtonChangeMs > BUTTON_DEBOUNCE_MS && reading != stableButtonState) {
        stableButtonState = reading;
        if (stableButtonState == LOW) {  // pull-up interno: premuto = a GND
            displayToggleMode();
            // La vista mappa non mostra la rotta: niente richieste AeroDataBox
            // bloccanti finché ci si resta (al ritorno recupera quelle mancanti).
            selectors.setRouteLookupEnabled(displayGetMode() != DisplayMode::MAP);
            displayRender(selectors);
        }
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    pinMode(PIN_BUTTON, INPUT_PULLUP);

    netLockInit();
    pendingMutex = xSemaphoreCreateMutex();

    displayInit();
    connectWifi();

    xTaskCreatePinnedToCore(pollTask, "opensky_poll", 16384, nullptr, 1, nullptr, 0);
}

void loop() {
    connectWifi();
    handleButton();
    applyPendingPoll();

    const bool mapMode = (displayGetMode() == DisplayMode::MAP);

    if (millis() - lastTickMs >= RENDER_TICK_INTERVAL_MS) {
        lastTickMs = millis();
        selectors.onTick();
        if (!mapMode) displayRender(selectors);
    }

    if (mapMode && millis() - lastMapFrameMs >= MAP_FRAME_INTERVAL_MS) {
        lastMapFrameMs = millis();
        displayRender(selectors);
    }
}
