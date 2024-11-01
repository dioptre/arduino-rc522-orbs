#include <Arduino.h>
#include "OrbStation.h"

class OrbStationExample : public OrbStation {
public:
    OrbStationExample(StationId id) : OrbStation(id) {}

protected:
    void onOrbConnected() override {
        Serial.print("Orb connected with trait: ");
        Serial.println(getTraitName());
        
        // Example of accessing station data
        Serial.print("Current station energy: ");
        // Shorthand for orbInfo.stations[stationId].energy
        Serial.println(getCurrentStationInfo().energy);
        
        // Example of modifying station data
        setVisited(true);
        addEnergy(10);

        // Show total energy
        Serial.print("Total energy: ");
        Serial.println(getTotalEnergy());
        
        // Change LED pattern when orb connects
        // setLEDPattern(LED_PATTERN_ORB_CONNECTED);
    }
    
    void onOrbDisconnected() override {
        Serial.println("Orb disconnected");
        //setLEDPattern(LED_PATTERN_NO_ORB);
    }

    void onError(const char* errorMessage) override {
        Serial.print("Error: ");
        Serial.println(errorMessage);
    }

    void onUnformattedNFC() override {
        Serial.println("Unformatted NFC card detected");
        // Example: Format card with RUMINATION trait
        // TraitId trait = TraitId::RUMINATE;
        // formatNFC(trait);
    }
};
