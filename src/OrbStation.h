#ifndef ORB_STATION_H
#define ORB_STATION_H

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <Adafruit_NeoPixel.h>

// PN532 pin definitions
#define PN532_SCK  (2)
#define PN532_MISO (3)
#define PN532_MOSI (4)
#define PN532_SS   (5)
#define NEOPIXEL_PIN (6)

// Status constants
#define STATUS_FAILED    0
#define STATUS_SUCCEEDED 1
#define STATUS_FALSE     2
#define STATUS_TRUE      3

// Communication constants
#define MAX_RETRIES      4
#define RETRY_DELAY      50
#define NFC_TIMEOUT      1000
#define DELAY_AFTER_CARD_PRESENT 300
#define NFC_CHECK_INTERVAL 500

// NFC constants
#define PAGE_OFFSET 4
#define ORBS_PAGE (PAGE_OFFSET + 0)
#define TRAIT_PAGE (PAGE_OFFSET + 1) 
#define STATIONS_PAGE_OFFSET (PAGE_OFFSET + 2)
#define ORBS_HEADER "ORBS"

// LED constants
#define NEOPIXEL_COUNT  24

// Station constants
#define NUM_STATIONS 14
#define NUM_TRAITS 6

enum TraitId {
    NONE, RUMINATION, SELF_DOUBT, SHAME, HOPELESSNESS, DISCONTENT
};

const char* const TRAIT_NAMES[] = {
    "NONE",
    "RUMINATION",
    "SELF_DOUBT",
    "SHAME",
    "HOPELESSNESS",
    "DISCONTENT"
};

const uint32_t TRAIT_COLORS[] = {
    0xFFFFFF,  // NONE - White
    0xFF0000,  // RUMI - Red
    0x00FF00,  // SELF - Green
    0x0000FF,  // SHAM - Blue
    0xFFFF00,  // HOPE - Yellow
    0xFF00FF   // DISC - Purple
};

enum StationId {
    CONFIGURE, CONSOLE, DISTILLER, CASINO, FOREST,
    ALCHEMY, PIPES, CHECKER, SLERP, RETOXIFY,
    GENERATOR, STRING, CHILL, HUNT
};

const char* const STATION_NAMES[] = {
    "CONFIGURE", "CONSOLE", "DISTILLER", "CASINO", "FOREST",
    "ALCHEMY", "PIPES", "CHECKER", "SLERP", "RETOXIFY",
    "GENERATOR", "STRING", "CHILL", "HUNT"
};

// Station struct
struct Station {
    bool visited;
    byte energy;
    byte custom1;
    byte custom2;
};

enum LEDPatternId {
    LED_PATTERN_NO_ORB,
    LED_PATTERN_ORB_CONNECTED
};

struct LEDPatternConfig {
    int id;
    uint8_t brightness;
    uint16_t interval;
    float brightnessInterval;
};

const LEDPatternConfig LED_PATTERNS[] = {
    {
        .id = LED_PATTERN_NO_ORB,
        .brightness = 50,
        .interval = 15,
        .brightnessInterval = 0.1f
    },
    {
        .id = LED_PATTERN_ORB_CONNECTED,
        .brightness = 100,
        .interval = 100,
        .brightnessInterval = 0.1f
    }
};

// Additional helper structs/enums
struct OrbInfo {
    TraitId trait;
    Station stations[NUM_STATIONS];
};

class OrbStation {
public:
    OrbStation(StationId id);
    virtual ~OrbStation();
    
    void begin();
    virtual void loop();

protected:
    // State variables
    StationId stationId;
    OrbInfo orbInfo;
    bool isOrbConnected;
    
    // Timing variables
    unsigned long currentMillis;

    // Virtual methods for child classes to implement
    virtual void onOrbConnected() = 0;
    virtual void onOrbDisconnected() = 0;
    virtual void onError(const char* errorMessage) = 0;
    virtual void onUnformattedNFC() = 0;

    // Helper methods that child classes can use
    Station getCurrentStationInfo();
    // Returns the total energy of the orb across all stations
    byte getTotalEnergy();
    // Returns the trait name
    const char* getTraitName();
    // Resets the station information, but keeps the trait
    int resetOrb();
    // Resets the orb with a new trait
    int formatNFC(TraitId newTrait);
    // Writes the trait to the orb
    int setTrait(TraitId newTrait);
    // Adds energy to the current station
    int addEnergy(byte amount);
    // Removes energy from the current station
    int removeEnergy(byte amount);
    // Sets the energy of the current station
    int setEnergy(byte amount);
    // Sets the visited status of the current station
    int setVisited(bool visited);
    // Sets the custom1 value of the current station
    int setCustom1(byte value);
    // Sets the custom2 value of the current station
    int setCustom2(byte value);
    // Sets the LED pattern
    void setLEDPattern(LEDPatternId patternId);
    // Reads and prints the entire NFC storage
    void printNFCStorage();

private:
    // NFC helper methods
    int writeStation(int stationID);
    int writeStations();
    int writePage(int page, uint8_t* data);
    int readPage(int page);
    int readOrbInfo();
    int writeOrbInfo();
    void reInitializeStations();
    bool isNFCPresent();
    bool isNFCActive();
    int isOrb();
    void printOrbInfo();

    // LED pattern methods
    void runLEDPatterns();
    void led_rainbow();
    void led_trait_chase();
    uint32_t dimColor(uint32_t color, uint8_t intensity);
    float lerp(float start, float end, float t);

    // Additional helper methods
    void handleError(const char* message);
    
    // Hardware objects
    Adafruit_NeoPixel strip;
    Adafruit_PN532 nfc;
    
    // LED variables
    LEDPatternConfig ledPatternConfig;
    
    // NFC
    bool isNFCConnected;
    byte page_buffer[4];
};

#endif
