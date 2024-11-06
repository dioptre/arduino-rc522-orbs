/**
 * Basic OrbDock implementation that just prints to Serial,
 * and adds 1 energy to the orb for the current station when the orb is connected
 * 
 * Available methods from base class:
 * - onOrbConnected() (override)
 * - onOrbDisconnected() (override)
 * - onError(const char* errorMessage) (override)
 * - onUnformattedNFC() (override)
 * 
 * - addEnergy(uint16_t amount)
 * - setEnergy(uint16_t amount)
 * - getEnergy()
 * - getTotalEnergy()
 * - getTraitName()
 * - getTraitColor()
 * - printOrbInfo()
 */

#include "OrbDock.h"

class OrbDockBasic : public OrbDock {
public:
    OrbDockBasic() : OrbDock(StationId::GENERIC) {
    }

protected:
    void onOrbConnected() override {
        Serial.println(F("Orb connected"));
        if (!getCurrentStationInfo().visited) {
            addEnergy(1);
        }
    }

    void onOrbDisconnected() override {
        Serial.println(F("Orb disconnected")); 
    }

    void onError(const char* errorMessage) override {
        Serial.print(F("Error: "));
        Serial.println(errorMessage);
    }

    void onUnformattedNFC() override {
        Serial.println(F("Unformatted NFC detected"));
    }
};