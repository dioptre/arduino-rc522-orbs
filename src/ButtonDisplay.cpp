/**
 * Controller for the button display module 
 * 
 * I2C pins:
 * SDA: A4 (Pin 27)
 * SCL: A5 (Pin 28)
 * 
 * Button Pins:
 * S1: D8
 * S2: D9
 * S3: D10  
 * S4: D11
 */

#include <Wire.h>
#include <U8glib.h>
#include <Arduino.h>

#define SCREEN_WIDTH 16 // Characters per line
#define SCREEN_HEIGHT 8 // Number of lines
#define SCREEN_ADDRESS 0x3C

#define BTN1_PIN 8
#define BTN2_PIN 9
#define BTN3_PIN 10
#define BTN4_PIN 11

class ButtonDisplay {
private:
    U8GLIB_SSD1306_128X64 display;
    char displayBuffer[SCREEN_HEIGHT][SCREEN_WIDTH + 1];  // +1 for null terminator
    bool buttonsInitialized;
    bool displayInitialized;
    uint8_t cursorX;
    uint8_t cursorY;
    const uint8_t charWidth = 8;  // Width of each character in pixels
    const uint8_t charHeight = 8; // Height of each character in pixels

    void initButtons() {
        if (!buttonsInitialized) {
            pinMode(BTN1_PIN, INPUT_PULLUP);
            pinMode(BTN2_PIN, INPUT_PULLUP);
            pinMode(BTN3_PIN, INPUT_PULLUP);
            pinMode(BTN4_PIN, INPUT_PULLUP);
            buttonsInitialized = true;
        }
    }

    void initDisplay() {
        if (!displayInitialized) {
            Serial.println(F("Initializing display"));
            Wire.begin();
            
            Wire.beginTransmission(SCREEN_ADDRESS);
            byte error = Wire.endTransmission();
            if (error != 0) {
                Serial.print(F("I2C device not found at address 0x"));
                Serial.println(SCREEN_ADDRESS, HEX);
                return;
            }

            display.begin();
            display.setFont(u8g_font_6x10);
            display.setFontRefHeightExtendedText();
            display.setDefaultForegroundColor();
            display.setFontPosTop();
            cursorX = 0;
            cursorY = 0;
            displayInitialized = true;
        }
    }

public:
    ButtonDisplay() : display(U8G_I2C_OPT_NONE) {
        buttonsInitialized = false;
        displayInitialized = false;
        cursorX = 0;
        cursorY = 0;
        clearBuffer();
    }

    void begin() {
        initButtons();
        initDisplay();
    }

    // Display functions
    void clearDisplay() {
        clearBuffer();
        cursorX = 0;
        cursorY = 0;
        updateDisplay();
    }

    // Add method to clear buffer
    void clearBuffer() {
        for (int i = 0; i < SCREEN_HEIGHT; i++) {
            memset(displayBuffer[i], 0, SCREEN_WIDTH + 1);
        }
    }

    void updateDisplay() {
        display.firstPage();
        do {
            // Draw all lines from buffer
            for (int i = 0; i < SCREEN_HEIGHT; i++) {
                if (displayBuffer[i][0] != 0) {  // If line is not empty
                    display.drawStr(0, i * charHeight, displayBuffer[i]);
                }
            }
        } while(display.nextPage());
    }

    void setCursor(uint8_t x, uint8_t y) {
        cursorX = x;
        cursorY = y;
    }

    void print(const char* text) {
        // Add text to buffer at current position
        int remaining = SCREEN_WIDTH - cursorX;
        int len = strlen(text);
        if (len > remaining) len = remaining;
        
        if (cursorY < SCREEN_HEIGHT) {
            strncat(displayBuffer[cursorY] + cursorX, text, len);
            cursorX += len;
        }
    }

    void print(int number) {
        char buf[12];
        itoa(number, buf, 10);
        print(buf);
    }

    void print(byte number) {
        print((int)number);
    }

    void println(const char* text) {
        print(text);
        println();
    }

    void println() {
        cursorX = 0;
        cursorY++;
        if (cursorY >= SCREEN_HEIGHT) {
            cursorY = 0;
        }
    }

    void setFont(const uint8_t* font) {
        display.setFont(font);
    }

    // Button functions
    bool isButton1Pressed() {
        return !digitalRead(BTN1_PIN);
    }

    bool isButton2Pressed() {
        return !digitalRead(BTN2_PIN);
    }

    bool isButton3Pressed() {
        return !digitalRead(BTN3_PIN);
    }

    bool isButton4Pressed() {
        return !digitalRead(BTN4_PIN);
    }

    // Utility functions
    void showMessage(const char* message, uint16_t duration = 2000) {
        clearDisplay();
        setCursor(0, 0);
        println(message);
        if (duration > 0) {
            delay(duration);
        }
    }

    void showError(const char* errorMessage) {
        clearDisplay();
        setCursor(0, 0);
        println(errorMessage);
    }

    // Direct display access if needed
    U8GLIB_SSD1306_128X64* getDisplay() {
        return &display;
    }
};
