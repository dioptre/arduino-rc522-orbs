#include "OrbStation.h"


// Constructor
OrbStation::OrbStation(StationId id) :
    strip(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800),
    nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS) {
    // Initialize member variables
    stationId = id;
    isNFCConnected = false;
    isOrbConnected = false;
    setLEDPattern(LED_PATTERN_NO_ORB);
    ledInterval = 0;
    currentMillis = 0;
}

// Destructor
OrbStation::~OrbStation() {
    // Cleanup code if needed
}

void OrbStation::begin() {
    // Initialize NeoPixel strip
    strip.begin();
    strip.setBrightness(0);
    strip.show();

    // Initialize NFC
    nfc.begin();
    uint32_t versiondata = nfc.getFirmwareVersion();
    if (!versiondata) {
        Serial.println("Didn't find PN53x board");
        // Flash red LED to indicate error
        while (1) {
            strip.setPixelColor(0, 255, 0, 0); // Red
            strip.show();
            delay(1000);
            strip.setPixelColor(0, 0, 0, 0); // Off
            strip.show(); 
            delay(1000);
        }
    }
    nfc.SAMConfig();
}

void OrbStation::loop() {
    currentMillis = millis();

    // Run LED patterns
    runLEDPatterns();

    // Check for NFC / Orb presence periodically
    static unsigned long lastNFCCheckTime = 0;
    if (currentMillis - lastNFCCheckTime < NFC_CHECK_INTERVAL) {
        return;
    }
    lastNFCCheckTime = currentMillis;

    // While orb is connected, check if it's still connected
    if (isNFCConnected && isOrbConnected) {
        if (!isNFCActive()) {
            setLEDPattern(LED_PATTERN_NO_ORB);
            isOrbConnected = false;
            isNFCConnected = false;
            onOrbDisconnected();
        }
        return;
    }

    // Check for NFC presence
    if(isNFCPresent()) {
        isNFCConnected = true;
        // NFC is present! Check if it's an orb
        int orbStatus = isOrb();
        switch (orbStatus) {
            case STATUS_FAILED:
                handleError("Failed to check orb header");
                return;
            case STATUS_FALSE:
                onUnformattedNFC();
                break;
            case STATUS_TRUE:
                setLEDPattern(LED_PATTERN_ORB_CONNECTED);
                readOrbInfo();
                onOrbConnected();
                break;
        }
    }
}

bool OrbStation::isNFCPresent() {
    uint8_t uid[7];  // Buffer to store the returned UID
    uint8_t uidLength;
    if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 30)) {
        return false;
    }
    if (uidLength != 7) {
        Serial.println(F("Detected non-NTAG203 tag (UUID length != 7 bytes)!"));
        return false;
    }
    Serial.println(F("NFC tag read successfully"));
    return true;
}

// Check if an Orb NFC is connected by reading the orb page
bool OrbStation::isNFCActive() {
  // See if we can read the orb page
  int checkStatus = isOrb();
  if (checkStatus == STATUS_FAILED) {
    return false;
  }
  return true;
}

// Whether the connected NFC is formatted as an orb
int OrbStation::isOrb() {
    Serial.println("Checking for ORBS header...");
    if (readPage(ORBS_PAGE) == STATUS_FAILED) {
        Serial.println(F("Failed to read data from NFC"));
        return STATUS_FAILED;
    }

    if (memcmp(page_buffer, ORBS_HEADER, 4) == 0) {
        Serial.println(F("ORBS header found")); 
        return STATUS_TRUE;
    }

    Serial.println(F("ORBS header not found"));
    return STATUS_FALSE;
}

// Print station information
void OrbStation::printOrbInfo() {
    Serial.println(F("\n*************************************************"));
    Serial.print(F("Trait: "));
    Serial.println(orbInfo.trait);
    Serial.print(F("Total energy: "));
    Serial.println(getTotalEnergy());
    
    for (int i = 0; i < NUM_STATIONS; i++) {
        Serial.print(StationId(i));
        Serial.print(F(": Visited:"));
        Serial.print(orbInfo.stations[i].visited ? "Yes" : "No");
        Serial.print(F(", Energy:"));
        Serial.print(orbInfo.stations[i].energy);
        Serial.print(F(" | "));
    }
    
    Serial.println();
    Serial.println(F("*************************************************"));
    Serial.println();
}

int OrbStation::writeStation(int stationId) {
    // Prepare the page buffer with station data
    page_buffer[0] = orbInfo.stations[stationId].visited ? 1 : 0;
    page_buffer[1] = orbInfo.stations[stationId].energy;
    page_buffer[2] = orbInfo.stations[stationId].custom1;
    page_buffer[3] = orbInfo.stations[stationId].custom2;

    // Write the buffer to the NFC
    int writeDataStatus = writePage(STATIONS_PAGE_OFFSET + stationId, page_buffer);
    if (writeDataStatus == STATUS_FAILED) {
        Serial.println("Failed to write station");
        return STATUS_FAILED;
    }
    return STATUS_SUCCEEDED;
}

int OrbStation::writePage(int page, uint8_t* data) {
    int retryCount = 0;
    while (retryCount < MAX_RETRIES) {
        Serial.print(F("Writing to page "));
        Serial.println(page);
        
        if (nfc.ntag2xx_WritePage(page, data)) {
            Serial.println(F("Write succeeded"));
            return STATUS_SUCCEEDED;
        }
        
        retryCount++;
        if (retryCount < MAX_RETRIES) {
            Serial.println(F("Retrying write"));
            delay(RETRY_DELAY);
            nfc.inListPassiveTarget();
        }
    }

    Serial.println(F("Write failed after retries"));
    return STATUS_FAILED;
}

int OrbStation::readPage(int page) {
    int retryCount = 0;
    while (retryCount < MAX_RETRIES) {
        if (nfc.ntag2xx_ReadPage(page, page_buffer)) {
            return STATUS_SUCCEEDED;
        }
        
        retryCount++;
        if (retryCount < MAX_RETRIES) {
            Serial.println(F("Retrying read"));
            delay(RETRY_DELAY);
            nfc.inListPassiveTarget();
        }
    }

    Serial.println(F("Read failed after retries"));
    return STATUS_FAILED;
}

int OrbStation::setTrait(TraitId newTrait) {
    orbInfo.trait = newTrait;
    uint8_t traitBytes[4] = {static_cast<uint8_t>(newTrait), 0, 0, 0};  // Convert trait to bytes
    memcpy(page_buffer, traitBytes, 4);
    return writePage(TRAIT_PAGE, page_buffer);
}

int OrbStation::setVisited(bool visited) {
    orbInfo.stations[stationId].visited = visited;
    return writeStation(stationId);
}

int OrbStation::setEnergy(byte energy) {
    orbInfo.stations[stationId].energy = energy;
    int result = writeStation(stationId);
    return result;
}

int OrbStation::addEnergy(byte amount) {
    int newEnergy = orbInfo.stations[stationId].energy + amount;
    if (newEnergy > 255) newEnergy = 255;
    
    orbInfo.stations[stationId].energy = newEnergy;
    int result = writeStation(stationId);
    return result;
}

int OrbStation::removeEnergy(byte amount) {
    int newEnergy = orbInfo.stations[stationId].energy - amount;
    if (newEnergy < 0) newEnergy = 0;
    
    orbInfo.stations[stationId].energy = newEnergy;
    int result = writeStation(stationId);
    return result;
}

int OrbStation::setCustom1(byte value) {
    orbInfo.stations[stationId].custom1 = value;
    int result = writeStation(stationId);
    return result;
}

int OrbStation::setCustom2(byte value) {
    orbInfo.stations[stationId].custom2 = value;
    int result = writeStation(stationId);
    return result;
}

byte OrbStation::getTotalEnergy() {
    byte totalEnergy = 0;
    for (int i = 0; i < NUM_STATIONS; i++) {
        totalEnergy += orbInfo.stations[i].energy;
    }
    return totalEnergy;
}

Station OrbStation::getCurrentStationInfo() {
    return orbInfo.stations[stationId];
}

void OrbStation::handleError(const char* message) {
    Serial.println(message);
    onError(message);
}

// Formats the NFC with "ORBS" header, default station information and given trait
int OrbStation::formatNFC(TraitId trait) {
    // Write header
    memcpy(page_buffer, ORBS_HEADER, 4);
    if (writePage(ORBS_PAGE, page_buffer) == STATUS_FAILED) {
        return STATUS_FAILED;
    }
    // Write default stations
    if (resetOrb() == STATUS_FAILED) {
        return STATUS_FAILED;
    }
    // Write trait
    if (setTrait(trait) == STATUS_FAILED) {
        return STATUS_FAILED;
    }
    return STATUS_SUCCEEDED;
}

// Set the orb to default station information - zero energy, not visited
int OrbStation::resetOrb() {
    Serial.println("Initializing orb with default station information...");
    reInitializeStations();
    int status = writeStations();
    if (status == STATUS_FAILED) {
        Serial.println("Failed to reset orb");
        return STATUS_FAILED;
    }
    status = readOrbInfo();
    if (status == STATUS_FAILED) {
        Serial.println("Failed to reset orb");
        return STATUS_FAILED;
    }
    return STATUS_SUCCEEDED;
}

// Initialize stations information to default values
void OrbStation::reInitializeStations() {
    for (int i = 0; i < NUM_STATIONS; i++) {
        orbInfo.stations[i] = {false, 0, 0, 0};
    }
}

// Read station information and trait from orb
int OrbStation::readOrbInfo() {
    Serial.println("Reading trait and station information from orb...");
    
    // Read stations
    for (int i = 0; i < NUM_STATIONS; i++) {
        if (readPage(STATIONS_PAGE_OFFSET + i) == STATUS_FAILED) {
            Serial.println(F("Failed to read station information"));
            return STATUS_FAILED;
        }
        orbInfo.stations[i].visited = page_buffer[0] == 1;
        orbInfo.stations[i].energy = page_buffer[1];
        orbInfo.stations[i].custom1 = page_buffer[2];
        orbInfo.stations[i].custom2 = page_buffer[3];
    }

    // Read trait
    if (readPage(TRAIT_PAGE) == STATUS_FAILED) {
        Serial.println(F("Failed to read trait"));
        return STATUS_FAILED;
    }
    orbInfo.trait = static_cast<TraitId>(page_buffer[0]);

    printOrbInfo();
    return STATUS_SUCCEEDED;
}

// Write station information and trait to orb
int OrbStation::writeOrbInfo() {
    Serial.println("Writing stations to orb...");
    writeStations();
    setTrait(orbInfo.trait);

    return STATUS_SUCCEEDED;
}

// Write station data
int OrbStation::writeStations() {
    for (int i = 0; i < NUM_STATIONS; i++) {
        if (writeStation(i) == STATUS_FAILED) {
            return STATUS_FAILED;
        }
    }
    return STATUS_SUCCEEDED;
}

/********************** LED FUNCTIONS *****************************/

void OrbStation::setLEDPattern(int pattern) {
    ledPattern = pattern;
    const LEDPatternConfig& config = LED_PATTERNS[pattern];
    ledInterval = config.interval;
    ledTargetBrightness = config.brightness;
    ledBrightnessInterval = config.brightnessInterval;
}

void OrbStation::runLEDPatterns() {
    static unsigned long ledPreviousMillis;
    static uint8_t ledBrightness;
    static unsigned long ledBrightnessPreviousMillis;

    if (currentMillis - ledPreviousMillis >= ledInterval) {
        ledPreviousMillis = currentMillis;

        switch (ledPattern) {
            case LED_PATTERN_NO_ORB:
                led_rainbow();
                break;
            case LED_PATTERN_ORB_CONNECTED:
                led_trait_chase();
                break;
            default:
                strip.clear();
                break;
        }

        // Smooth brightness transitions
        if (ledBrightness != ledTargetBrightness && currentMillis - ledBrightnessPreviousMillis >= ledBrightnessInterval) {
            ledBrightnessPreviousMillis = currentMillis;
            ledBrightness = lerp(ledBrightness, ledTargetBrightness, ledBrightnessInterval);
            strip.setBrightness(ledBrightness);
        }

        strip.show();
    }

}

// Rainbow cycle along whole strip. Pass delay time (in ms) between frames.
void OrbStation::led_rainbow() {
    static long firstPixelHue = 0;
    if (firstPixelHue < 5*65536) {
      strip.rainbow(firstPixelHue, 1, 255, 255, true);
      firstPixelHue += 256;
    } else {
      firstPixelHue = 0; // Reset for next cycle
    }
}

// Rotates a weakening dot around the NeoPixel ring using the trait color
void OrbStation::led_trait_chase() {
    static uint16_t currentPixel = 0;
    static uint8_t intensity = 255;
    static uint8_t globalIntensity = 0;
    static int8_t globalDirection = 1;
    
    // Update global intensity
    globalIntensity += globalDirection * 9;  // Adjust 2 to change global fade speed
    if (globalIntensity >= 255 || globalIntensity <= 30) {
        globalDirection *= -1;
        globalIntensity = constrain(globalIntensity, 30, 255);
    }

    // Find the trait color
    uint32_t traitColor = TRAIT_COLORS[static_cast<int>(orbInfo.trait)]; // Default to first trait
    
    // Set the current pixel to trait color with current intensity
    uint8_t adjustedIntensity = (uint16_t)intensity * globalIntensity / 255;
    strip.setPixelColor(currentPixel, dimColor(traitColor, adjustedIntensity));
    
    // Set the next pixels with decreasing intensity
    for (int i = 1; i < NEOPIXEL_COUNT; i++) {
        uint16_t pixel = (currentPixel - i + NEOPIXEL_COUNT) % NEOPIXEL_COUNT;
        float fadeRatio = pow(float(NEOPIXEL_COUNT - i) / NEOPIXEL_COUNT, 2);  // Quadratic fade
        uint8_t fadeIntensity = round(intensity * fadeRatio);
        adjustedIntensity = (uint16_t)fadeIntensity * globalIntensity / 255;
        if (adjustedIntensity > 0) {
        strip.setPixelColor(pixel, dimColor(traitColor, adjustedIntensity));
        } else {
        break;  // Stop if intensity reaches 0
        }
    }
    // Move to next pixel
    currentPixel = (currentPixel + 1) % NEOPIXEL_COUNT;
}

// Helper function to dim a 32-bit color value by a certain intensity (0-255)
uint32_t OrbStation::dimColor(uint32_t color, uint8_t intensity) {
  uint8_t r = (uint8_t)(color >> 16);
  uint8_t g = (uint8_t)(color >> 8);
  uint8_t b = (uint8_t)color;
  
  r = (r * intensity) >> 8;
  g = (g * intensity) >> 8;
  b = (b * intensity) >> 8;
  
  return strip.Color(r, g, b);
}

float OrbStation::lerp(float start, float end, float t) {
    return start + t * (end - start);
}

/********************** MISC FUNCTIONS *****************************/
