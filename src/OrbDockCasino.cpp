/**
 * Orb Casino Dock
 * 
 * I2C pins:
 * SDA: A4 (Pin 27)
 * SCL: A5 (Pin 28)
 * 
 * Button Pins:
 * S1: D8     // Add 1 energy
 * S2: D9     // Add 5 energy  
 * S3: D10    // Remove 1 energy
 * S4: D11    // Remove 5 energy
 */

#include "OrbDock.h"
#include "ButtonDisplay.h"

class OrbDockCasino : public OrbDock {
private:
    const uint8_t* font = u8g_font_fub49n;
    ButtonDisplay display{font};

    void updateDisplay() {
        display.clearDisplay();
        
        if (isOrbConnected) {
            char energyStr[8];
            itoa(getEnergy(), energyStr, 10);
            display.println(energyStr);
        } else {
            display.println("::");
        }
        
        display.updateDisplay();
    }

public:
    OrbDockCasino() : OrbDock(StationId::CASINO) {
    }

    void begin() {
        OrbDock::begin();
        display.begin();
        updateDisplay();
    }

    void loop() override {
        OrbDock::loop();

        if (!isOrbConnected) return;

        // Handle button inputs
        if (display.isButton1Pressed()) {
            addEnergy(1);
            Serial.println(F("Added 1 energy"));
            //delay(100); // Simple debounce
            updateDisplay();
        }
        
        if (display.isButton2Pressed()) {
            addEnergy(5);
            Serial.println(F("Added 5 energy"));
            //delay(100); // Simple debounce
            updateDisplay();
        }

        if (display.isButton3Pressed()) {
            Station station = getCurrentStationInfo();
            if (station.energy > 5) {
                removeEnergy(5);
                Serial.println(F("Removed 5 energy"));
            }
            else {
                removeEnergy(station.energy);
                Serial.println(F("Removed all energy"));
            }
            //delay(100);
            updateDisplay();
        }

        if (display.isButton4Pressed()) {
            Station station = getCurrentStationInfo();
            if (station.energy >= 1) {
                removeEnergy(1);
                Serial.println(F("Removed 1 energy"));
            }
            //delay(100);
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
        display.showError(":::::");
        delay(2000);
        updateDisplay();
    }
};
