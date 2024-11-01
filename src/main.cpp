#include <Arduino.h>
#include "OrbDockConfigurizer.cpp"

OrbDockConfigurizer orbDock{};

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    orbDock.begin();
}

void loop() {
    orbDock.loop();
}
