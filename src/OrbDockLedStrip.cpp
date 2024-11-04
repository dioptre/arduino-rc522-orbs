/**
 * OrbDockLedStrip implementation that controls an LED strip based on orb state
 * and displays different patterns when orbs are connected/disconnected
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
#include <FastLED.h>

#define NUM_LEDS 16
#define LED_STRIP_PIN 6

class OrbDockLedStrip : public OrbDock {
private:
    CRGB leds[NUM_LEDS];
    uint16_t hue = 0;

public:
    OrbDockLedStrip() : OrbDock(StationId::GENERIC) {
        FastLED.addLeds<WS2812B, LED_STRIP_PIN, GRB>(leds, NUM_LEDS);
        FastLED.setBrightness(50);
        FastLED.show();
    }

protected:
    void onOrbConnected() override {
        
        // Set all LEDs to the trait color
        const char* traitName = getTraitName();
        CRGB color;
        
        // Match trait to color
        if (strcmp(traitName, "RUMINATE") == 0) {
            color = CRGB::Orange;
        } else if (strcmp(traitName, "SHAME") == 0) {
            color = CRGB::Yellow;
        } else if (strcmp(traitName, "DOUBT") == 0) {
            color = CRGB::Green;
        } else if (strcmp(traitName, "DISCONTENT") == 0) {
            color = CRGB::Pink;
        } else if (strcmp(traitName, "HOPELESS") == 0) {
            color = CRGB::Blue;
        } else {
            color = CRGB::Red;
        }

        fill_solid(leds, NUM_LEDS, color);
        FastLED.show();
    }

    void onOrbDisconnected() override {
        Serial.println(F("Orb disconnected"));
        
        // Return to rainbow pattern
        for(int i = 0; i < NUM_LEDS; i++) {
            leds[i] = CRGB::Black;
        }
        FastLED.show();
    }

    void onError(const char* errorMessage) override {
        Serial.print(F("Error: "));
        Serial.println(errorMessage);
        
        // // Flash red on error
        // for(int i = 0; i < 3; i++) {
        //     fill_solid(leds, NUM_LEDS, CRGB::Red);
        //     FastLED.show();
        //     delay(200);
        //     fill_solid(leds, NUM_LEDS, CRGB::Black);
        //     FastLED.show();
        //     delay(200);
        // }
    }

    void onUnformattedNFC() override {
        Serial.println(F("Unformatted NFC detected"));
        
        // // Pulse white for unformatted NFC
        // for(int brightness = 0; brightness < 255; brightness++) {
        //     fill_solid(leds, NUM_LEDS, CRGB::White);
        //     FastLED.setBrightness(brightness);
        //     FastLED.show();
        //     delay(5);
        // }
        // for(int brightness = 255; brightness >= 0; brightness--) {
        //     fill_solid(leds, NUM_LEDS, CRGB::White);
        //     FastLED.setBrightness(brightness);
        //     FastLED.show();
        //     delay(5);
        // }
        // FastLED.setBrightness(50);
    }
};
