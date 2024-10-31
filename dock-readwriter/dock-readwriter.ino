/* 
ORB DOCK - ORB NFC READER, NEOPIXEL RING CONTROL AND COMMUNICATION WITH EXTERNAL MICROCONTROLLERS VIA USB

For orb and dock details, see https://docs.google.com/document/d/15TdBDqpzjQM84aWcbPIud8OpBZvtFHylhdnx2yqqLoc

TROUBLESHOOTING:
IMPORTANT - remember to set the switches on the PN532 to the "SPI" position - left/1 down, right/2 up
IMPORTANT - remember to set Arduino IDE's Serial Monitor baud rate to 115200

PIN CONNECTIONS 

PN532 RFID READER: 
  - ARDUINO NANO/UNO:
    - 5V -> 5V
    - GND -> GND
    - SCK  -> Digital 2
    - MISO -> Digital 3
    - MOSI -> Digital 4
    - SS   -> Digital 5
  
NEOPIXEL RING:
 - Data out -> Digital 6

STATIONS
Can store information for up to 16 stations. (NOTE: May change this to 10 so we only need 1 page per station)
For each station: Visited yes/no, and Energy 0-255
  0 - Control console - CONSOLE
  1 - Thought Distiller - DISTILLER
  2 - Casino - CASINO
  3 - Forest - FOREST
  4 - Alchemization Station - ALCHEMY
  5 - Pipes - PIPES
  6 - Init/reset station

TODO:
- Add a way to set the orb's trait
- Add a way to set the orb's stations
- Add a way to set the orb's energy
- Add a way to set the orb's visited status
- Change writes to only write the specific page required, to minimize risk of corrupting other data
- Change to using one page per station, to minimize risk of corrupting other data. We'll have space for 10 stations total.
- Allow bytes 3 and 4 of the station data to be used for whatever purposes the attached microcontroller wants.
*/


#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
#include <Adafruit_NeoPixel.h>


// NeoPixel LED ring
#define NEOPIXEL_PIN    6                   // NeoPixel pin
#define NEOPIXEL_COUNT  24                  // Number of NeoPixels
// NeoPixel strip object
Adafruit_NeoPixel strip(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
// Tracking patterns
unsigned long pixelPrevious = 0;            // Previous Pixel Millis
uint8_t       pixelBrightness = 25;         // Max 255
uint8_t       targetBrightness = 25;        // Max 255
// Tracking pixels and pixel timing
int           pixelInterval = 50;           // Pixel Interval (ms)
int           pixelQueue = 0;               // Pattern Pixel Queue
int           pixelCycle = 0;               // Pattern Pixel Cycle
uint16_t      pixelNumber = NEOPIXEL_COUNT; // Total Number of Pixels
// LED patterns
enum LEDPattern {
  LED_PATTERN_NONE,
  LED_PATTERN_NO_ORB,
  LED_PATTERN_ORB_CONNECTED
};
// Current LED pattern
LEDPattern currentLEDPattern = LED_PATTERN_NO_ORB;
#define LED_BRIGHTNESS_NO_ORB 50
#define LED_BRIGHTNESS_ORB_CONNECTED 100;

// Status constants
#define STATUS_FAILED    0
#define STATUS_SUCCEEDED 1
#define STATUS_FALSE     2
#define STATUS_TRUE    3

// Stations
// Station IDs. "CONSOLE" is station 0, "DISTILLER" is station 1, etc.
const char* stationIDs[] = {
  "CONFIGURE", "CONSOLE", "DISTILLER", "CASINO", "FOREST", "ALCHEMY", "PIPES", "CHECKER", "SLERP", 
  "RETOXIFY", "GENERATOR", "STRING", "CHILL", "HUNT"
};
#define NUM_STATIONS (sizeof(stationIDs) / sizeof(stationIDs[0]))
struct Station {
  bool visited;
  byte energy;
  byte custom1;
  byte custom2;
};
Station stations[NUM_STATIONS]; // Array of stations
int totalStationEnergy = 0;     // How much energy the orb has from all stations combined

// Orb Traits
#define NUM_TRAITS 5
const char* traits[] = {
  "RUMI", "SELF", "SHAM", "HOPE", "DISC"
};
const char* traitNames[] = {
  "Rumination", "Self Doubt", "Shame Spiral", "Hopelessness", "Discontentment"
};
const uint32_t traitColors[] = {
  strip.Color(255, 40, 0),      // Orange for RUMI (Rumination)
  strip.Color(255, 0, 210),    // Pink/Magenta for DISC (Discontentment)
  strip.Color(32, 255, 0),      // Green for SHAM (Shame Spiral)
  strip.Color(255, 70, 0),      // Yellow for SELF (Self Doubt)
  strip.Color(20, 0, 255),      // Blue for HOPE (Hopelessness)
};
char trait[5] = "RUMI";

// NFC/RFID Reader - PN532 with software SPI
#define PN532_SCK  (2)
#define PN532_MISO (3)
#define PN532_MOSI (4)
#define PN532_SS   (5)
Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

// Communication constants
#define MAX_RETRIES      4                    // Number of retries for failed read/write operations
#define RETRY_DELAY      1                    // Delay between retries in milliseconds - set low b'cos PN532 lib seems to need us to re-connect the card which itself has some delay
#define DELAY_AFTER_CARD_PRESENT 300          // Delay after card is presented
#define NFC_CHECK_INTERVAL 500                // How often to check for NFC presence

// NFC/RFID Communication
// NTAG213 has pages 4-39 available for user memory (144 bytes, 36 pages) 4 bytes per page.  
// Pages 0 to 3 are reserved for special functions.
// Page 4 contains the word "ORBS" so we can verify the NFC has been configured as an orb.
// Page 5 contains the orb's TRAIT.
// Pages 6 to 39 contain the STATION information.
byte page_buffer[4];                         // Buffer for a single page
//byte full_buffer[4 * (2 + NUM_STATIONS)];     // Data transfer buffer
//#define BUFFER_SIZE sizeof(buffer)            // Size of the buffer
//#define NUM_PAGES (BUFFER_SIZE / 4)           // Number of pages to use
const char header[] = "ORBS";                 // Header to check if the NFC has been initialized as an orb
#define PAGE_OFFSET 4                         // Page offset for read/write start
#define ORBS_PAGE (PAGE_OFFSET + 0)           // Page for the ORBS header 
#define TRAIT_PAGE (PAGE_OFFSET + 1)          // Page for the trait
#define STATIONS_PAGE_OFFSET (PAGE_OFFSET + 2) // Page offset for the station data

// Timing orchestration
unsigned long currentMillis;


/********************** SETUP *****************************/

void setup() {

   // Initialize serial communications and wait for serial port to open (for Arduinos based on ATMEGA32U4)
  Serial.begin(115200);
  while (!Serial) delay(10);

  // Initialize NeoPixel LED ring
  strip.begin();                          // Initialize NeoPixel strip object
  strip.show();                           // Turn OFF all pixels ASAP
  strip.setBrightness(pixelBrightness);   // Set BRIGHTNESS to about 1/5 

  // Initialize PN532 NFC module
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("Didn't find PN532 module, won't start.");
    // Flash the NeoPixel ring red to indicate error
    while (1) {
      strip.fill(strip.Color(255, 0, 0));  // Set all pixels to red
      strip.show();
      delay(500);                          // Wait 500ms
      strip.clear();                       // Turn off all pixels
      strip.show();
      delay(500);                          // Wait 500ms
    }
  }
  nfc.SAMConfig();                        // Configure the PN532 to read RFID tags
  nfc.setPassiveActivationRetries(0x11);  // Set the max number of retry attempts to read from a card

  // Turn off the status LED
  digitalWrite(LED_BUILTIN, LOW);

  // Ready!
  Serial.println("INSERT YOUR ORBS IN MEEEEE");
}


/********************** LOOP *****************************/

void loop() {
  static unsigned long lastNFCCheckTime = 0;
  currentMillis = millis();

  // Run NeoPixel LED ring patterns
  runLEDPatterns();

  // Check for NFC presence periodically
  if (currentMillis - lastNFCCheckTime < NFC_CHECK_INTERVAL) {
    return;
  }
  lastNFCCheckTime = currentMillis;
  if (!isNFCPresent()) {
    Serial.print(".");
    return;
  }

  // We have an NFC card present, turn on the status LED
  digitalWrite(LED_BUILTIN, HIGH);

  // Wait for NFC to get closer just in case
  delay(DELAY_AFTER_CARD_PRESENT);

  // If the NFC has not been configured as an orb, do so
  // TODO: Add a way to set the orb's trait
  int checkORBSStatus = checkORBS();
  if (checkORBSStatus == STATUS_FAILED) {
    return;
  }
  if (checkORBSStatus == STATUS_FALSE) {
    int resetOrbStatus = resetOrb();      
    if (resetOrbStatus == STATUS_FAILED) {
      return;
    }
  }
  
  // Read the station's information
  int readStationsStatus = readStations();
  if (readStationsStatus == STATUS_FAILED) {
    return;
  }

  // Set the LED pattern to show the Orb's trait
  currentLEDPattern = LED_PATTERN_ORB_CONNECTED;
  strip.setBrightness(0);
  targetBrightness = LED_BRIGHTNESS_ORB_CONNECTED;

  // TODO: Send a signal to the attached Raspberry Pi with station information and orb trait

  // TODO: When NFC is removed, send a signal to the attached Raspberry Pi

  // TODO: Wait for any commands from the attached Raspberry Pi
  // ...   These could be:
  // ...   - Set the orb's trait - SET_TRAIT
  // ...   - Update a single station's energy - INCREASE_ENERGY / DECREASE_ENERGY
  // ...   - Update a single station's visited status - SET_VISITED_TRUE / SET_VISITED_FALSE
  // ...   - Reset the orb
  // ...   The format of the command should be:
  // ...   - command
  // ...   - [station_id]
  // ...   - [energy]
  // ...   - [trait]

  // TODO: Can we merge this with the main loop instead of having a separate while loop?
  bool orbActive = true;
  while (orbActive) {
    currentMillis = millis();

    // Check for NFC presence periodically
    if (currentMillis - lastNFCCheckTime > NFC_CHECK_INTERVAL) {
      orbActive = isNFCActive();
      lastNFCCheckTime = currentMillis;
      continue;
    }

    // Run NeoPixel LED ring patterns
    runLEDPatterns();
  }

  // ORB DISCONNECTED
  // Reset various variables so they don't polluate other orbs
  Serial.println("Orb disconnected, ending session...");
  // TODO: Send a signal to the attached Raspberry Pi that orb has disconnected
  // Set current brightness so we don't get a flash
  targetBrightness = LED_BRIGHTNESS_NO_ORB;
  strip.setBrightness(targetBrightness);
  currentLEDPattern = LED_PATTERN_NO_ORB;
  //memset(full_buffer, 0, sizeof(full_buffer));
  memset(page_buffer, 0, sizeof(page_buffer));
  initializeStations();
  totalStationEnergy = 0;

  // turn the LED off
  digitalWrite(LED_BUILTIN, LOW);
}


/********************** ORB FUNCTIONS *****************************/


// Initialize NFC with "ORBS" header and default station information
int resetOrb() {

  Serial.println("Initializing orb with default station information...");
  initializeStations();
  int writeStationsStatus = writeStations();
  if (writeStationsStatus == STATUS_FAILED) {
      Serial.println("Failed to reset orb");
      return STATUS_FAILED;
  }
  return STATUS_SUCCEEDED;
}

// Initialize stations information to default values
void initializeStations() {

  for (int i = 0; i < NUM_STATIONS; i++) {
    stations[i] = {false, 0};
  }
}


// Write station information and trait to orb
int writeStations() {

  int writeDataStatus;

  Serial.println("Writing stations to orb...");

  // Write header to the first page and trait to the second page
  // TODO: Check return status
  memcpy(page_buffer, header, 4);
  writeDataStatus = writePage(ORBS_PAGE, page_buffer);
  if (writeDataStatus == STATUS_FAILED) {
    return STATUS_FAILED;
  }
  memcpy(page_buffer, trait, 4);
  writeDataStatus = writePage(TRAIT_PAGE, page_buffer);
  if (writeDataStatus == STATUS_FAILED) {
    return STATUS_FAILED;
  }

  // Write station data
  for (int i = 0; i < NUM_STATIONS; i++) {
    page_buffer[0] = stations[i].visited ? 1 : 0;
    page_buffer[1] = stations[i].energy;
    page_buffer[2] = stations[i].custom1;
    page_buffer[3] = stations[i].custom2;
    writeDataStatus = writePage(STATIONS_PAGE_OFFSET + i, page_buffer);
    if (writeDataStatus == STATUS_FAILED) {
      return STATUS_FAILED;
    }
  }

  return STATUS_SUCCEEDED;
}


// Read station information and trait from orb into buffer
int readStations() {

  Serial.println("Reading trait and station information from orb...");
  totalStationEnergy = 0;
  for (int i = 0; i < NUM_STATIONS; i++) {
    int readDataStatus = readPage(STATIONS_PAGE_OFFSET + i);
    if (readDataStatus == STATUS_FAILED) {
      Serial.println(F("Failed to read trait and station information from orb"));
      return STATUS_FAILED;
    }
    stations[i].visited = page_buffer[0] == 1;
    stations[i].energy = page_buffer[1];
    stations[i].custom1 = page_buffer[2];
    stations[i].custom2 = page_buffer[3];
    totalStationEnergy += stations[i].energy;
  }

  // Read the trait from the second page
  int readTraitStatus = readPage(TRAIT_PAGE);
  if (readTraitStatus == STATUS_FAILED) {
    Serial.println(F("Failed to read trait from orb"));
    return STATUS_FAILED;
  }
  memcpy(trait, page_buffer, 4);

  printStations();

  return STATUS_SUCCEEDED;
}


// Print station information
void printStations() {

  Serial.println("");
  Serial.println(F("*************************************************"));
  Serial.print(F("Trait: "));
  Serial.println(trait);
  Serial.print(F("Total energy: "));
  Serial.println(totalStationEnergy);
  for (int i = 0; i < NUM_STATIONS; i++) {
    Serial.print(stationIDs[i]);
    Serial.print(F(": Visited:"));
    Serial.print(stations[i].visited ? "Yes" : "No");
    Serial.print(F(", Energy:"));
    Serial.print(stations[i].energy);
    Serial.print(F(" | "));
  }
  Serial.println();
  Serial.println(F("*************************************************"));
  Serial.println();
}


// Check if the first page contains "ORBS"
int checkORBS() {

  Serial.println("Checking for ORBS header...");
  int readDataStatus = readPage(ORBS_PAGE);
  if (readDataStatus == STATUS_FAILED) {
    Serial.println(F("Failed to read data from NFC"));
    return STATUS_FAILED;
  }

  if (memcmp(page_buffer, header, 4) == 0) {
    Serial.println(F("ORBS header found"));
    return STATUS_TRUE;
  }

  Serial.println(F("ORBS header not found"));
  return STATUS_FALSE;
}


// Test purposes only -- update the station with the lowest energy
int testUpdateStations() {

  // Add one energy to the station with the lowest energy
  int lowestEnergyStation = 0;
  for (int i = 1; i < NUM_STATIONS; i++) {
    if (stations[i].energy < stations[lowestEnergyStation].energy) {
      lowestEnergyStation = i;
    }
  }
  stations[lowestEnergyStation].energy += 3;
  stations[lowestEnergyStation].visited = true;
  int writeStationStatus = writeStation(lowestEnergyStation);
  if (writeStationStatus == STATUS_FAILED) {
    Serial.println("Failed to update station");
    return STATUS_FAILED;
  }
  return STATUS_SUCCEEDED;
}


int writeStation(int stationID) {

  // Prepare the page buffer with station data
  page_buffer[0] = stations[stationID].visited ? 1 : 0;
  page_buffer[1] = stations[stationID].energy;
  page_buffer[2] = stations[stationID].custom1;
  page_buffer[3] = stations[stationID].custom2;

  // Write the buffer to the NFC
  int writeDataStatus = writePage(STATIONS_PAGE_OFFSET + stationID, page_buffer);
  if (writeDataStatus == STATUS_FAILED) {
    Serial.println("Failed to write station");
    return STATUS_FAILED;
  }
  return STATUS_SUCCEEDED;
}

// Writes whatever is in the page_buffer to the NFC
int writePage(int page, uint8_t* data) {

  int retryCount = 0;

  while (retryCount < MAX_RETRIES) {
    Serial.print(F("Writing data to NFC... page: "));
    Serial.println(page);
    bool writeSuccess = nfc.ntag2xx_WritePage(page, data);
    if (writeSuccess) {
      Serial.println(F("Write to NFC succeeded"));
      return STATUS_SUCCEEDED;
    }
    retryCount++;
    if (retryCount < MAX_RETRIES) {
      Serial.println(F("Retrying write operation."));
      delay(RETRY_DELAY);
      nfc.inListPassiveTarget(); // Seems like we need to do this to recover??
    }
  }

  Serial.println(F("Write to NFC failed after multiple attempts"));
  return STATUS_FAILED;
}

// Read the given page from the NFC into page_buffer, with retries
int readPage(int page) {

  int retryCount = 0;

  while (retryCount < MAX_RETRIES) {
    bool readSuccess = nfc.ntag2xx_ReadPage(page, page_buffer);

    if (readSuccess) {
      Serial.print(F("Read OK. "));
      return STATUS_SUCCEEDED;
    }

    retryCount++;
    if (retryCount < MAX_RETRIES) {
      Serial.println(F("Retrying read operation."));
      delay(RETRY_DELAY);
      nfc.inListPassiveTarget(); // Seems like we need to do this to recover??
    }
  }

  Serial.println(F("Read from NFC failed after multiple attempts"));
  return STATUS_FAILED;
}


// Check if an NFC card is present
bool isNFCPresent() {

  // if (!nfc.inListPassiveTarget()) {
  //   return false;
  // }

  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 30);
  if (!success) {
    return false;
  }

  if (uidLength != 7) {
    Serial.println("Detected non-NTAG203 tag (UUID length != 7 bytes)!");
    return false;
  }

  Serial.println("NFC tag read successfully");
  return true;
}


// Check if an Orb NFC is connected by reading the orb page
bool isNFCActive() {

  // See if we can read the orb page
  int readPageStatus = readPage(ORBS_PAGE);
  if (readPageStatus == STATUS_FAILED) {
    return false;
  }
  return true;
}


/********************** LED FUNCTIONS *****************************/


// Run the current NeoPixel LED ring pattern
void runLEDPatterns() {

  if (currentMillis - pixelPrevious >= pixelInterval) {
    pixelPrevious = currentMillis;  // Update the previous time

    switch (currentLEDPattern) {
      case LED_PATTERN_NO_ORB:
        led_rainbow(15);  // Adjust the wait time as needed
        break;
      case LED_PATTERN_ORB_CONNECTED:
        led_trait_chase(100);
        break;
      case LED_PATTERN_NONE:
      default:
        strip.clear();
        break;
    }
    
    strip.show();
  }

  // Move brightness towards target brightness
  static float brightnessInterval = 0.1;  // Smaller interval for smoother transitions
  static unsigned long brightnessPrevious = currentMillis;
  static float currentBrightness = pixelBrightness;
  if (pixelBrightness != targetBrightness && currentMillis - brightnessPrevious >= 1) {  // Check every millisecond
    brightnessPrevious = currentMillis;
    currentBrightness += (pixelBrightness < targetBrightness) ? brightnessInterval : -brightnessInterval;
    pixelBrightness = (uint8_t)currentBrightness;  // Convert float to uint8_t for actual brightness
    strip.setBrightness(pixelBrightness);
    strip.show();
  }
}


// Rainbow cycle along whole strip. Pass delay time (in ms) between frames.
void led_rainbow(int wait) {
  static unsigned long lastUpdate = 0;
  static long firstPixelHue = 0;

  pixelInterval = wait;
  targetBrightness = LED_BRIGHTNESS_NO_ORB;

  if (currentMillis - lastUpdate >= wait) {
    lastUpdate = currentMillis;
    if (firstPixelHue < 5*65536) {
      strip.rainbow(firstPixelHue, 1, 255, 255, true);
      firstPixelHue += 256;
    } else {
      firstPixelHue = 0; // Reset for next cycle
    }
  }
}


// Rotates a weakening dot around the NeoPixel ring using the trait color
void led_trait_chase(int wait) {
  static uint16_t currentPixel = 0;
  static uint8_t intensity = 255;
  static uint8_t globalIntensity = 0;
  static int8_t globalDirection = 1;
  
  pixelInterval = wait;  // Update delay time
  targetBrightness = LED_BRIGHTNESS_ORB_CONNECTED;
  
  // Update global intensity
  globalIntensity += globalDirection * 9;  // Adjust 2 to change global fade speed
  if (globalIntensity >= 255 || globalIntensity <= 30) {
    globalDirection *= -1;
    globalIntensity = constrain(globalIntensity, 30, 255);
  }

  // Find the trait color
  uint32_t traitColor = traitColors[0]; // Default to first trait
  for(int i = 0; i < NUM_TRAITS; i++) {
    if(strcmp(trait, traits[i]) == 0) {
      traitColor = traitColors[i];
      break;
    }
  }
  
  // Set the current pixel to trait color with current intensity
  uint8_t adjustedIntensity = (uint16_t)intensity * globalIntensity / 255;
  strip.setPixelColor(currentPixel, dimColor(traitColor, adjustedIntensity));
  
  // Set the next pixels with decreasing intensity
  for (int i = 1; i < NEOPIXEL_COUNT; i++) {
    uint16_t pixel = (currentPixel - i + pixelNumber) % pixelNumber;
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
  currentPixel = (currentPixel + 1) % pixelNumber;
}


// Helper function to dim a 32-bit color value by a certain intensity (0-255)
uint32_t dimColor(uint32_t color, uint8_t intensity) {
  uint8_t r = (uint8_t)(color >> 16);
  uint8_t g = (uint8_t)(color >> 8);
  uint8_t b = (uint8_t)color;
  
  r = (r * intensity) >> 8;
  g = (g * intensity) >> 8;
  b = (b * intensity) >> 8;
  
  return strip.Color(r, g, b);
}

/********************** MISC FUNCTIONS *****************************/
