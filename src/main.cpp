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
unsigned long lastFastFrameMs = 0;
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
        int retryAfterSec = 0;
        {
            NetLockGuard guard;
            ok = openSkyFetchStates(states, &retryAfterSec);
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

        // Limite crediti esaurito (429): inutile insistere ogni 22 s, aspettiamo
        // quanto indicato da OpenSky (max 6 ore per non sforare i tick FreeRTOS).
        unsigned long waitMs = OPENSKY_POLL_INTERVAL_MS;
        if (retryAfterSec > 0) {
            waitMs = (unsigned long)retryAfterSec * 1000UL + 5000UL;
            if (waitMs > 6UL * 3600UL * 1000UL) waitMs = 6UL * 3600UL * 1000UL;
            Serial.printf("[opensky] limite crediti esaurito: riprovo tra %lu min\n", waitMs / 60000UL);
        }
        vTaskDelay(pdMS_TO_TICKS(waitMs));
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

    WiFi.onEvent([](arduino_event_id_t event, arduino_event_info_t info) {
        if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
            // 201 = rete non trovata (o solo 5GHz), 202/15/204 = password o handshake, 205 = connessione fallita
            Serial.printf("[wifi] non connesso, motivo=%d\n", (int)info.wifi_sta_disconnected.reason);
        } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
            Serial.printf("[wifi] connesso, IP %s\n", WiFi.localIP().toString().c_str());
        }
    });

    displayInit();
    connectWifi();

    xTaskCreatePinnedToCore(pollTask, "opensky_poll", 16384, nullptr, 1, nullptr, 0);
}

void loop() {
    connectWifi();
    handleButton();
    applyPendingPoll();

    bool tickFired = false;
    if (millis() - lastTickMs >= RENDER_TICK_INTERVAL_MS) {
        lastTickMs = millis();
        selectors.onTick();
        tickFired = true;
    }

    if (displayWantsFastFrames(selectors)) {
        if (millis() - lastFastFrameMs >= FAST_FRAME_INTERVAL_MS) {
            lastFastFrameMs = millis();
            displayRender(selectors);
        }
    } else if (tickFired) {
        displayRender(selectors);
    }
}
