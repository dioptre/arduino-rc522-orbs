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

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "OrbStation.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define BTN_NEXT 8     // Next trait button
#define BTN_PREV 9     // Previous trait button  
#define BTN_RESET 10   // Reset orb button
#define BTN_FORMAT 11  // Format orb button

class OrbStationConfigure : public OrbStation {
private:
    Adafruit_SSD1306 display;
    TraitId selectedTrait;
    bool buttonsInitialized;

    void initButtons() {
        if (!buttonsInitialized) {
            pinMode(BTN_NEXT, INPUT_PULLUP);
            pinMode(BTN_PREV, INPUT_PULLUP);
            pinMode(BTN_RESET, INPUT_PULLUP);
            pinMode(BTN_FORMAT, INPUT_PULLUP);
            buttonsInitialized = true;
        }
    }

    void initDisplay() {
        Wire.begin();
        display = Adafruit_SSD1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
        if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
            Serial.println(F("SSD1306 allocation failed"));
            return;
        }
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.display();
    }

    void updateDisplay() {
        display.clearDisplay();
        display.setCursor(0,0);
        
        if (isOrbConnected) {
            display.println(F("Orb Connected"));
            display.print(F("Current Trait: "));
            display.println(getTraitName());
            display.println(F("Total Energy: "));
            display.println(getTotalEnergy());
        } else {
            display.println(F("No Orb Connected"));
            display.println(F("Selected Trait:"));
            display.println(TRAIT_NAMES[static_cast<int>(selectedTrait)]);
            display.println(F("\nControls:"));
            display.println(F("BTN1/2: Cycle trait"));
            display.println(F("BTN3: Reset orb"));
            display.println(F("BTN4: Format orb"));
        }
        
        display.display();
    }
public:
    OrbStationConfigure() : OrbStation(StationId::CONFIGURE) {
        selectedTrait = TraitId::RUMINATION;
        buttonsInitialized = false;
        initDisplay();
    }

    void begin() {
        OrbStation::begin();
        initButtons();
        updateDisplay();
    }

    void loop() override {
        OrbStation::loop();

        // Handle button inputs
        if (!digitalRead(BTN_NEXT)) {
            int trait = static_cast<int>(selectedTrait) + 1;
            if (trait >= NUM_TRAITS) trait = 0;
            selectedTrait = static_cast<TraitId>(trait);
            Serial.print(F("Next trait: "));
            Serial.println(TRAIT_NAMES[static_cast<int>(selectedTrait)]);
            delay(200); // Simple debounce
            updateDisplay();
        }
        
        if (!digitalRead(BTN_PREV)) {
            int trait = static_cast<int>(selectedTrait) - 1;
            if (trait < 0) trait = NUM_TRAITS - 1;
            selectedTrait = static_cast<TraitId>(trait);
            Serial.print(F("Previous trait: "));
            Serial.println(TRAIT_NAMES[static_cast<int>(selectedTrait)]);
            delay(200); // Simple debounce
            updateDisplay();
        }

        if (!digitalRead(BTN_RESET) && isOrbConnected) {
            Serial.println(F("Reset orb"));
            resetOrb();
            delay(200);
            updateDisplay();
        }

        if (!digitalRead(BTN_FORMAT)) {
            Serial.println(F("Format orb"));
            formatNFC(selectedTrait);
            delay(200);
            updateDisplay();
        }
    }

protected:
    void onOrbConnected() override {
        updateDisplay();
        printNFCStorage();
    }

    void onOrbDisconnected() override {
        updateDisplay();
    }

    void onError(const char* errorMessage) override {
        display.clearDisplay();
        display.setCursor(0,0);
        display.println(F("Error:"));
        display.println(errorMessage);
        display.display();
        delay(2000);
        updateDisplay();
    }

    void onUnformattedNFC() override {
        display.clearDisplay();
        display.setCursor(0,0);
        display.println(F("Unformatted NFC"));
        display.println(F("Press BTN4 to"));
        display.println(F("format with"));
        display.println(TRAIT_NAMES[static_cast<int>(selectedTrait)]);
        display.display();
    }
};
