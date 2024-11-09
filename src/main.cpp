#include <Arduino.h>
#include "OrbDockConfigurizer.cpp"
#include "OrbDockBasic.cpp"
#include "OrbDockCasino.cpp"
#include "OrbDockLedStrip.cpp"
#include "OrbDockComms.h"
#include "OrbDockTrigger.cpp"

//OrbDockBasic orbDock{};
//OrbDockConfigurizer orbDock{};
//OrbDockCasino orbDock{};
//OrbDockLedStrip orbDock{};
//OrbDockComms orbDock(10, 11, 12);
OrbDockTrigger orbDock(12, 11, 10);

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    orbDock.begin();
}

void loop() {
    orbDock.loop();
}
