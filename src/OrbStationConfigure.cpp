/**
 * Orb Reset and Formatting Station
 * 
 * I2C pins:
 * SDA: A4 (Pin 27)
 * SCL: A5 (Pin 28)
 * 
 * Button Pins:
 * S1: D8     // Next trait button
 * S2: D9     // Previous trait button  
 * S3: D10    // Reset orb button
 * S4: D11    // Format orb button
 */

#include "OrbStation.h"
#include "ButtonDisplay.cpp"

class OrbStationConfigure : public OrbStation {
private:
    ButtonDisplay display;
    TraitId selectedTrait;

    void updateDisplay() {
        display.clearDisplay();
        display.setCursor(0,0);
        
        if (isOrbConnected) {
            display.println("Orb Connected");
            display.print("Current Trait: ");
            display.println(getTraitName());
            display.println("Total Energy: ");
            display.print(getTotalEnergy());
            display.println();
        } else {
            display.println("No Orb Connected");
            display.println("Selected Trait:");
            display.println(TRAIT_NAMES[selectedTrait]);
            display.println("\nControls:");
            display.println("BTN1/2: Cycle trait");
            display.println("BTN3: Reset orb");
            display.println("BTN4: Format orb");
        }
        
        display.updateDisplay();
    }

public:
    OrbStationConfigure() : OrbStation(StationId::CONFIGURE) {
        selectedTrait = TraitId::RUMINATION;
    }

    void begin() {
        OrbStation::begin();
        display.begin();
        updateDisplay();
    }

    void loop() override {
        OrbStation::loop();

        // Handle button inputs
        if (display.isButton1Pressed()) {
            int trait = static_cast<int>(selectedTrait) + 1;
            if (trait >= NUM_TRAITS) trait = 0;
            selectedTrait = static_cast<TraitId>(trait);
            Serial.print(F("Next trait: "));
            Serial.println(TRAIT_NAMES[selectedTrait]);
            delay(200); // Simple debounce
            updateDisplay();
        }
        
        if (display.isButton2Pressed()) {
            int trait = static_cast<int>(selectedTrait) - 1;
            if (trait < 0) trait = NUM_TRAITS - 1;
            selectedTrait = static_cast<TraitId>(trait);
            Serial.print(F("Previous trait: "));
            Serial.println(TRAIT_NAMES[selectedTrait]);
            delay(200); // Simple debounce
            updateDisplay();
        }

        if (display.isButton3Pressed() && isOrbConnected) {
            Serial.println(F("Reset orb"));
            resetOrb();
            delay(200);
            updateDisplay();
        }

        if (display.isButton4Pressed()) {
            Serial.println(F("Format orb"));
            formatNFC(selectedTrait);
            delay(200);
            updateDisplay();
        }
    }

protected:
    void onOrbConnected() override {
        updateDisplay();
    }

    void onOrbDisconnected() override {
        updateDisplay();
    }

    void onError(const char* errorMessage) override {
        display.showError(errorMessage);
        delay(2000);
        updateDisplay();
    }

    void onUnformattedNFC() override {
        display.clearDisplay();
        display.setCursor(0,0);
        display.println("Unformatted NFC");
        display.println("Press BTN4 to");
        display.println("format with");
        display.println(TRAIT_NAMES[static_cast<int>(selectedTrait)]);
        display.updateDisplay();
    }
};
