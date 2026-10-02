#include <Arduino.h>
#include <WiFi.h>
#include <vector>

#include "config.h"
#include "display.h"
#include "flight_selectors.h"
#include "opensky.h"
#include "secrets.h"

namespace {

FlightSelectors selectors;

unsigned long lastPollMs = 0;
unsigned long lastTickMs = 0;
unsigned long lastWifiAttemptMs = 0;
bool didInitialPoll = false;

int lastButtonReading = HIGH;
int stableButtonState = HIGH;
unsigned long lastButtonChangeMs = 0;

void connectWifi() {
    if (WiFi.status() == WL_CONNECTED) return;
    if (millis() - lastWifiAttemptMs < WIFI_RETRY_INTERVAL_MS) return;
    lastWifiAttemptMs = millis();

    Serial.println("[wifi] connessione...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void pollFlights() {
    std::vector<AircraftState> states;
    if (openSkyFetchStates(states)) {
        Serial.printf("[opensky] %u velivoli nel raggio\n", (unsigned)states.size());
        selectors.onPoll(states);
    } else {
        Serial.println("[opensky] poll fallito, riprovo al prossimo giro");
    }
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
            displayRender(selectors);
        }
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    pinMode(PIN_BUTTON, INPUT_PULLUP);

    displayInit();
    connectWifi();
}

void loop() {
    connectWifi();
    handleButton();

    bool wifiUp = (WiFi.status() == WL_CONNECTED);

    if (wifiUp && (!didInitialPoll || millis() - lastPollMs >= OPENSKY_POLL_INTERVAL_MS)) {
        lastPollMs = millis();
        didInitialPoll = true;
        pollFlights();
    }

    if (millis() - lastTickMs >= RENDER_TICK_INTERVAL_MS) {
        lastTickMs = millis();
        selectors.onTick();
        displayRender(selectors);
    }
}
