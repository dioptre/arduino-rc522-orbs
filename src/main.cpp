#include <Arduino.h>
#include "OrbStationExample.cpp"

OrbStationExample orbStation(StationId::RETOXIFY);

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    orbStation.begin();
}

void loop() {
    orbStation.loop();
}
