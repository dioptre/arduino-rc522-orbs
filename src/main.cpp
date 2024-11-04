#include <Arduino.h>
#include "OrbDockConfigurizer.cpp"
#include "OrbDockBasic.cpp"
#include "OrbDockCasino.cpp"
#include "OrbDockLedStrip.cpp"

OrbDockBasic orbDock{};
//OrbDockConfigurizer orbDock{};
//OrbDockCasino orbDock{};
//OrbDockLedStrip orbDock{};

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    orbDock.begin();
}

void loop() {
    orbDock.loop();
}
