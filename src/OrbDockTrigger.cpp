#include <Arduino.h>
#include "OrbDock.h"

class OrbDockTrigger : public OrbDock {
private:
    uint8_t _triggerPin;
    unsigned long _triggerStartTime;

public:
    OrbDockTrigger(uint8_t triggerPin) 
        : OrbDock(StationId::PIPES),
        _triggerPin(triggerPin),
        _triggerStartTime(0)
    {
    }

    void begin() override {
        OrbDock::begin();
        pinMode(_triggerPin, OUTPUT);
        digitalWrite(_triggerPin, LOW);
    }

    void loop() override {
        OrbDock::loop();
        
        // Check if trigger should turn off after 20 seconds
        if (_triggerStartTime > 0 && millis() - _triggerStartTime >= 20000) {
            digitalWrite(_triggerPin, LOW);
            _triggerStartTime = 0;
        }
    }

    void onOrbConnected() override {
        Serial.println("BALLS CONNECT OK");
        digitalWrite(_triggerPin, HIGH);
        _triggerStartTime = millis();
    }

    void onOrbDisconnected() override {
        digitalWrite(_triggerPin, LOW);
        _triggerStartTime = 0;
    }

    void onError(const char* errorMessage) override {
        // Do nothing
    }

    void onUnformattedNFC() override {
        // Do nothing
    }
};
