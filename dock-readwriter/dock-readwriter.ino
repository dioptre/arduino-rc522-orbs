/* Orb Dock Reader
 * 
 *   RFID-RC522 (SPI connexion)
 *   
 *   CARD RC522      Arduino (UNO)
 *     SDA  -----------  10 (Configurable, see SS_PIN constant)
 *     SCK  -----------  13
 *     MOSI -----------  11
 *     MISO -----------  12
 *     IRQ  -----------  
 *     GND  -----------  GND
 *     RST  -----------  9 (onfigurable, see RST_PIN constant)
 *     3.3V ----------- 3.3V
 *     

Can store information for up to 16 stations.
For each station: Visited yes/no, and Energy 0-255

Stations:
0 - Control console - CONSOLE
1 - Thought Distiller - DISTILLER
2 - Casino - CASINO
3 - Forest - FOREST
4 - Alchemization Station - ALCHEMY
5 - Pipes - PIPES
6 - ?

TODO:
- Add a way to set the orb's trait
- Add a way to set the orb's stations
- Add a way to set the orb's energy
- Add a way to set the orb's visited status
- Change writes to only write the specific page required, to minimize risk of corrupting other data
- Change to using one page per station, to minimize risk of corrupting other data. We'll have space for 10 stations total.
- Allow bytes 3 and 4 of the station data to be used for whatever purposes the attached microcontroller wants.
*/


#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_NeoPixel.h>


// Pin constants
#define SS_PIN          9                   // SPI pin - Change for nano vs mega etc
#define RST_PIN         8                   // Reset pin - Change for nano vs mega etc
#define LED_BUILTIN     13                  // Arduino LED pin
#define LED_SHIELD      11                  // Protoshield LED pin
#define BUTTON          2                   // Protoshield button pin 

// Button constants
#define PRESSED LOW
#define RELEASED HIGH

// NeoPixel LED ring
#define NEOPIXEL_PIN    6                   // NeoPixel pin
#define NEOPIXEL_COUNT  8                   // Number of NeoPixels
// NeoPixel strip object
Adafruit_NeoPixel strip(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
// Tracking patterns
unsigned long pixelPrevious = 0;            // Previous Pixel Millis
unsigned long patternPrevious = 0;          // Previous Pattern Millis
int           patternCurrent = 0;           // Current Pattern Number
int           patternInterval = 5000;       // Pattern Interval (ms)
bool          patternComplete = false;
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
  LED_PATTERN_ORB_CONNECTED,
  LED_PATTERN_ORB_READING,
  LED_PATTERN_ORB_WRITING,
  LED_PATTERN_ORB_ERROR,
};
// Current LED pattern
LEDPattern currentLEDPattern = LED_PATTERN_NO_ORB;

// Status constants
#define STATUS_SUCCEEDED 1
#define STATUS_FAILED 2
#define STATUS_FALSE 3
#define STATUS_TRUE 4

// Stations
#define NUM_STATIONS 16
#define NUM_STATIONS_PRINT 6 // Temporary
struct Station {
  bool visited;
  byte energy;
};
Station stations[NUM_STATIONS];
int totalStationEnergy = 0;
const char* stationIDs[NUM_STATIONS] = {
  "CONSOLE", "DISTILLER", "CASINO", "FOREST", "ALCHEMY", "PIPES",
  "TBD 7", "TBD 8", "TBD 9", "TBD 10", "TBD 11", "TBD 12", "TBD 13", "TBD 14", "TBD 15", "TBD 16"
};

// Orb Traits
#define NUM_TRAITS 6
#define TRAIT_NONE 0
const char* traits[] = {
  "NONE", "RUMI", "SELF", "SHAM", "HOPE", "DISC"
};
const char* traitNames[] = {
  "None", "Rumination", "Self Doubt", "Shame Spiral", "Hopelessness", "Discontentment"
};
const uint32_t traitColors[] = {
  strip.Color(139, 0, 0),      // Dark red for NONE
  strip.Color(255, 128, 0),    // Orange for RUMI (Rumination)
  strip.Color(255, 220, 0),    // Warmer Yellow for SELF (Self Doubt)
  strip.Color(0, 255, 0),      // Green for SHAM (Shame Spiral)
  strip.Color(0, 0, 255),      // Blue for HOPE (Hopelessness)
  strip.Color(200, 0, 180)     // Deeper Magenta for DISC (Discontentment)
};
char trait[5] = "NONE";

// NFC/RFID Reader
MFRC522 rfid(SS_PIN, RST_PIN);              // Create MFRC522 instance
MFRC522::StatusCode status;                 // Variable to get card status
#define maxRetries      10                  // Number of retries for failed read/write operations
#define retryDelay      200                 // Delay between retries in milliseconds
#define DELAY_AFTER_CARD_PRESENT 1000       // Delay after card is presented

// NFC/RFID Communication
// Ultralight memory is 16 pages. 4 bytes per page.  
// Pages 0 to 4 are reserved for special functions.
// Page 6 contains the word "ORBS" so we can verify the NFC has been configured as an orb.
// Page 7 contains the orb's trait.
// Pages 8 to 16 contain the station information.
byte buffer[4 * (2 + NUM_STATIONS / 2)];      // Data transfer buffer
#define BUFFER_SIZE sizeof(buffer)            // Size of the buffer
#define NUM_PAGES (BUFFER_SIZE / 4)           // Number of pages to use
#define PAGE_OFFSET (0x10 - NUM_PAGES)        // Page offset for read/write start
const char header[] = "ORBS";                 // Header to check if the NFC has been initialized as an orb
#define STATION_PAGE_OFFSET (PAGE_OFFSET + 2)

// Button state
int buttonState = 0;


/********************** SETUP *****************************/

void setup() {

   // Initialize serial communications and wait for serial port to open (for Arduinos based on ATMEGA32U4)
  Serial.begin(9600);
  while (!Serial) {}

  // Initialize LEDs and button
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  pinMode(LED_SHIELD, OUTPUT);
  digitalWrite(LED_SHIELD, LOW);
  pinMode(BUTTON, INPUT_PULLUP);

  // Initialize NeoPixel LED ring
  strip.begin();                        // Initialize NeoPixel strip object
  strip.show();                         // Turn OFF all pixels ASAP
  strip.setBrightness(pixelBrightness); // Set BRIGHTNESS to about 1/5 

  // Initialize NFC reader
  SPI.begin();                          // Init SPI bus
  rfid.PCD_Init();                      // Init MFRC522 card  
  delay(4);
  Serial.print("MFRC522 software version = ");
  Serial.println(rfid.PCD_ReadRegister(rfid.VersionReg), HEX);
  rfid.PCD_DumpVersionToSerial();       // Show details of PCD - MFRC522 Card Reader details
  Serial.println("PUT YOUR ORBS IN MEEEEE");
}


/********************** LOOP *****************************/

void loop() {

  // Run NeoPixel LED ring patterns
  runLEDPatterns();

  // Wait for a new card and select the card
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial())
    return;

  // Turn on status LED
  digitalWrite(LED_SHIELD, HIGH);

  // Wait for NFC to get closer
  // delay(DELAY_AFTER_CARD_PRESENT);

  // If the NFC has not been configured as an orb, do so
  int checkORBSStatus = checkORBS();
  if (checkORBSStatus == STATUS_FAILED) {
    printDebugInfo();
    return;
  }
  if (checkORBSStatus == STATUS_FALSE) {
    int resetOrbStatus = resetOrb();
    if (resetOrbStatus == STATUS_FAILED) {
      printDebugInfo();
      return;
    }
  }
  
  // Read the station's information
  int readStationsStatus = readStations();
  if (readStationsStatus == STATUS_FAILED) {
    printDebugInfo();
    return;
  }

  // Set the LED pattern to show the Orb's trait
  currentLEDPattern = LED_PATTERN_ORB_CONNECTED;

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

  while (isNFCActive()) {
    // Run NeoPixel LED ring patterns
    runLEDPatterns();

    if (digitalRead(BUTTON) == PRESSED) {
      // Update stations
      int updateStationsStatus = updateStations();
      if (updateStationsStatus == STATUS_FAILED) {
        printDebugInfo();
        break;  // Exit the loop if update fails
      }
      readStations();

      // Debounce button
      while (digitalRead(BUTTON) == PRESSED) {
        delay(50);
      }
    }
    
    // Small delay to prevent excessive CPU usage
    // TODO: Change this to non-blocking
    //delay(250);
  }

  // Reset various variables so they don't polluate other orbs
  Serial.println("Orb disconnected, ending session...");
  // TODO: Send a signal to the attached Raspberry Pi that orb has disconnected
  currentLEDPattern = LED_PATTERN_NO_ORB;
  memset(buffer, 0, sizeof(buffer));
  initializeStations();
  totalStationEnergy = 0;

  // turn the LED off
  digitalWrite(LED_SHIELD, LOW);

  // Halt PICC
  rfid.PICC_HaltA();
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

  Serial.println("Writing stations to orb...");

  // Write header to the first page and trait to the second page
  memcpy(buffer, header, 4);
  memcpy(buffer + 4, trait, 4);

  // Start writing station data from the 5th byte (index 4)
  for (int i = 0; i < NUM_STATIONS; i++) {
    buffer[8 + i*2] = stations[i].visited ? 1 : 0;
    buffer[8 + i*2 + 1] = stations[i].energy;
  }

  int writeDataStatus = writeData();
  if (writeDataStatus == STATUS_FAILED) {
      Serial.println("Failed to write stations to orb");
      return STATUS_FAILED;
  }
  return STATUS_SUCCEEDED;
}


// Read station information and trait from orb into buffer
int readStations() {

  Serial.println("Reading trait and station information from orb...");
  int readDataStatus = readData();
  if (readDataStatus == STATUS_FAILED) {
    Serial.println(F("Failed to read trait and station information from orb"));
    return STATUS_FAILED;
  }

  // Read the trait from the second page
  memcpy(trait, buffer + 4, 4);

  // Read the station information from the third page onwards
  totalStationEnergy = 0;
  for (int i = 0; i < NUM_STATIONS; i++) {
    stations[i].visited = buffer[STATION_PAGE_OFFSET + i*2] == 1;
    stations[i].energy = buffer[STATION_PAGE_OFFSET + i*2 + 1];
    totalStationEnergy += stations[i].energy;
  }

  printStations();

  return STATUS_SUCCEEDED;
}


// Print station information
void printStations() {

  Serial.println(F("*************************************************"));
  Serial.print(F("Trait: "));
  Serial.println(trait);
  Serial.print(F("Total energy: "));
  Serial.println(totalStationEnergy);
  for (int i = 0; i < NUM_STATIONS_PRINT; i++) {
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
  int readDataStatus = readData();
  if (readDataStatus == STATUS_FAILED) {
    Serial.println(F("Failed to read data from NFC"));
    return STATUS_FAILED;
  }

  if (memcmp(buffer, header, 4) == 0) {
    Serial.println(F("ORBS header found"));
    return STATUS_TRUE;
  }

  Serial.println(F("ORBS header not found"));
  return STATUS_FALSE;
}


// TODO: Take parameters for station and energy to update
int updateStations() {

  // Add one energy to the station with the lowest energy
  int lowestEnergyStation = 0;
  for (int i = 1; i < NUM_STATIONS; i++) {
    if (stations[i].energy < stations[lowestEnergyStation].energy) {
      lowestEnergyStation = i;
    }
  }
  stations[lowestEnergyStation].energy++;
  stations[lowestEnergyStation].energy++;
  stations[lowestEnergyStation].energy++;
  stations[lowestEnergyStation].visited = true;
  int writeStationsStatus = writeStations();
  if (writeStationsStatus == STATUS_FAILED) {
    Serial.println("Failed to update stations");
    return STATUS_FAILED;
  }
  return STATUS_SUCCEEDED;
}

// Writes whatever is in the buffer to the NFC
int writeData() {

  int retryCount = 0;

  while (retryCount < maxRetries) {
    Serial.println(F("Writing data to NFC..."));
    bool writeSuccess = true;

    for (int i = 0; i < NUM_PAGES; i++) {
      // Data is written in blocks of 4 bytes (i.e. one page)
      status = (MFRC522::StatusCode) rfid.MIFARE_Ultralight_Write(PAGE_OFFSET+i, &buffer[i*4], 4);
      if (status != MFRC522::STATUS_OK) {
        Serial.print(F("Write to NFC failed: "));
        Serial.println(rfid.GetStatusCodeName(status));
        writeSuccess = false;
        break;
      }
    }

    if (writeSuccess) {
      Serial.println(F("Write to NFC succeeded"));
      return STATUS_SUCCEEDED;
    }

    retryCount++;
    if (retryCount < maxRetries) {
      Serial.println(F("Retrying write operation."));
      delay(retryDelay);
    }
  }

  Serial.println(F("Write to NFC failed after multiple attempts"));
  return STATUS_FAILED;
}

// Read whatever is in the NFC into buffer
int readData() {

  Serial.println(F("Reading data from NFC..."));
  int retryCount = 0;

  while (retryCount < maxRetries) {
    bool readSuccess = true;

    // We need to read all pages starting from PAGE_OFFSET
    for (int i = 0; i < NUM_PAGES; i++) {
      byte pageBuffer[18];  // Increased buffer size for safety
      byte bufferSize = sizeof(pageBuffer);
      
      status = (MFRC522::StatusCode) rfid.MIFARE_Read(PAGE_OFFSET + i, pageBuffer, &bufferSize);
      if (status != MFRC522::STATUS_OK) {
        Serial.print(F("Read from NFC failed: "));
        Serial.println(rfid.GetStatusCodeName(status));
        Serial.print(F("Page: "));
        Serial.println(PAGE_OFFSET + i);
        readSuccess = false;
        break;
      }
      
      // Copy only the first 4 bytes of the read data to the main buffer
      memcpy(buffer + (i * 4), pageBuffer, 4);
    }

    if (readSuccess) {
      Serial.println(F("Read from NFC succeeded"));
      return STATUS_SUCCEEDED;
    }

    retryCount++;
    if (retryCount < maxRetries) {
      Serial.println(F("Retrying read operation."));
      delay(retryDelay);
    }
  }

  Serial.println(F("Read from NFC failed after multiple attempts"));
  return STATUS_FAILED;
}


// Check if the NFC is connected by reading the first 4 pages
bool isNFCActive() {

  // See if we can read the first 4 pages
  byte tempBuffer[18];
  byte tempBufferSize = sizeof(tempBuffer);
  status = (MFRC522::StatusCode) rfid.MIFARE_Read(PAGE_OFFSET, tempBuffer, &tempBufferSize);
  return status == MFRC522::STATUS_OK;
}


// Dump debug info about the card; PICC_HaltA() is automatically called
void printDebugInfo() {

  rfid.PICC_DumpToSerial(&(rfid.uid));
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  Serial.print(F("RFID Tag UID:"));
  printHex(rfid.uid.uidByte, rfid.uid.size);
  Serial.println("");
}


/********************** LED FUNCTIONS *****************************/


// Run the current NeoPixel LED ring pattern
void runLEDPatterns() {

  unsigned long currentMillis = millis();     // Tracks current time
  if (currentMillis - pixelPrevious >= pixelInterval) {
    pixelPrevious = currentMillis;  // Update the previous time

    switch (currentLEDPattern) {
      case LED_PATTERN_NO_ORB:
        fadeTraitColors(20);  // Adjust the wait time as needed
        break;
      case LED_PATTERN_ORB_CONNECTED:
        rotateWeakeningRedDotWithPulse(100);
        break;
      case LED_PATTERN_ORB_READING:
        theaterChase(strip.Color(0, 255, 0), 100);
        break;
      case LED_PATTERN_ORB_WRITING:
        theaterChase(strip.Color(255, 0, 0), 100);
        break;
      case LED_PATTERN_ORB_ERROR:
        theaterChase(strip.Color(255, 0, 255), 100);
        break;
      case LED_PATTERN_NONE:
      default:
        strip.clear();
        strip.show();
        break;
    }
    
    // Move brightness towards target brightness
    static uint8_t brightnessInterval = 1;
    static uint8_t brightnessPrevious = pixelBrightness;
    if (pixelBrightness != targetBrightness && currentMillis - brightnessPrevious >= brightnessInterval) {
      brightnessPrevious = currentMillis;
      pixelBrightness += (pixelBrightness < targetBrightness) ? 1 : -1;
      strip.setBrightness(pixelBrightness);
    }

    // Update the strip with the new color and brightness
    strip.show();
  }
}


// Fade between all trait colors except NONE
void fadeTraitColors(int wait) {
  // Starting at 1 to skip NONE trait
  static uint8_t colorIndex = 1;
  static uint8_t fadeStep = 0;
  static uint32_t currentColor = traitColors[1];
  static uint32_t nextColor = traitColors[2];;
  
  pixelInterval = wait;  // Update delay time
  targetBrightness = 15;

  uint8_t r = map(fadeStep, 0, 255, (currentColor >> 16) & 0xFF, (nextColor >> 16) & 0xFF);
  uint8_t g = map(fadeStep, 0, 255, (currentColor >> 8) & 0xFF, (nextColor >> 8) & 0xFF);
  uint8_t b = map(fadeStep, 0, 255, currentColor & 0xFF, nextColor & 0xFF);
  uint32_t fadeColor = strip.Color(r, g, b);

  // Set all pixels to the fade color
  for(uint16_t i = 0; i < pixelNumber; i++) {
    strip.setPixelColor(i, fadeColor);
  }

  fadeStep += 1;  // Adjust this value to change fade speed
  if (fadeStep >= 255) {
    fadeStep = 0;
    colorIndex = (colorIndex % (NUM_TRAITS - 1)) + 1;
    currentColor = nextColor;
    nextColor = traitColors[(colorIndex % (NUM_TRAITS - 1)) + 1];
  }
}


// Rotates a weakening red dot around the NeoPixel ring
void rotateWeakeningRedDot(int wait) {
  static uint16_t currentPixel = 0;
  static uint8_t intensity = 255;
  
  pixelInterval = wait;  // Update delay time
  targetBrightness = 200;
  
  // Set the current pixel to red with current intensity
  strip.setPixelColor(currentPixel, strip.Color(intensity, 0, 0));
  
  // Set the next pixels with decreasing intensity
  for (int i = 1; i < NEOPIXEL_COUNT; i++) {
    uint16_t pixel = (currentPixel - i + pixelNumber) % pixelNumber;
    // uint8_t fadeIntensity = intensity * (NEOPIXEL_COUNT - i) / NEOPIXEL_COUNT;
    float fadeRatio = pow(float(NEOPIXEL_COUNT - i) / NEOPIXEL_COUNT, 2);  // Quadratic fade
    uint8_t fadeIntensity = round(intensity * fadeRatio);
    if (fadeIntensity > 0) {
      strip.setPixelColor(pixel, strip.Color(fadeIntensity, 0, 0));
    } else {
      break;  // Stop if intensity reaches 0
    }
  }
  
  // Move to next pixel
  currentPixel = (currentPixel + 1) % pixelNumber;
}


// Rotates a weakening red dot around the NeoPixel ring
void rotateWeakeningRedDotWithPulse(int wait) {
  static uint16_t currentPixel = 0;
  static uint8_t intensity = 255;
  static uint8_t globalIntensity = 0;
  static int8_t globalDirection = 1;
  
  pixelInterval = wait;  // Update delay time
  targetBrightness = 200;
  
  // Update global intensity
  globalIntensity += globalDirection * 5;  // Adjust 2 to change global fade speed
  if (globalIntensity >= 255 || globalIntensity <= 30) {
    globalDirection *= -1;
    globalIntensity = constrain(globalIntensity, 30, 255);
  }
  
  // Set the current pixel to red with current intensity
  uint8_t adjustedIntensity = (uint16_t)intensity * globalIntensity / 255;
  strip.setPixelColor(currentPixel, strip.Color(adjustedIntensity, 0, 0));
  
  // Set the next pixels with decreasing intensity
  for (int i = 1; i < NEOPIXEL_COUNT; i++) {
    uint16_t pixel = (currentPixel - i + pixelNumber) % pixelNumber;
    float fadeRatio = pow(float(NEOPIXEL_COUNT - i) / NEOPIXEL_COUNT, 2);  // Quadratic fade
    uint8_t fadeIntensity = round(intensity * fadeRatio);
    adjustedIntensity = (uint16_t)fadeIntensity * globalIntensity / 255;
    if (adjustedIntensity > 0) {
      strip.setPixelColor(pixel, strip.Color(adjustedIntensity, 0, 0));
    } else {
      break;  // Stop if intensity reaches 0
    }
  }
  
  // Move to next pixel
  currentPixel = (currentPixel + 1) % pixelNumber;
}


// Creates a pulsating effect on each pixel independently
void pulsatingPixels(int wait) {
  static uint8_t pixelIntensities[NEOPIXEL_COUNT];
  static int8_t pixelDirections[NEOPIXEL_COUNT];
  
  pixelInterval = wait;  // Update delay time
  targetBrightness = 200;
  
  // Initialize intensities and directions if not done
  static bool initialized = false;
  if (!initialized) {
    for (int i = 0; i < pixelNumber; i++) {
      pixelIntensities[i] = random(256);  // Start with random intensities
      pixelDirections[i] = random(2) * 2 - 1;  // Either 1 or -1
    }
    initialized = true;
  }
  
  // Update and set the intensity for each pixel
  for (int i = 0; i < pixelNumber; i++) {
    // Update intensity
    pixelIntensities[i] += pixelDirections[i] * 5;  // Adjust 5 to change speed
    
    // Change direction if at max or min
    if (pixelIntensities[i] >= 255 || pixelIntensities[i] <= 0) {
      pixelDirections[i] *= -1;
      pixelIntensities[i] = constrain(pixelIntensities[i], 0, 255);
    }
    
    // Set the pixel color (using red in this example)
    strip.setPixelColor(i, strip.Color(pixelIntensities[i], 0, 0));
  }
  
  strip.show();
}

// Creates a uniform fading effect on all pixels
void uniformFading(uint32_t color, int wait) {
  static uint8_t intensity = 0;
  static int8_t direction = 1;
  
  pixelInterval = wait;  // Update delay time
  targetBrightness = 200;
  
  // Update intensity
  intensity += direction * 3;  // Adjust 5 to change fade speed
  
  // Change direction if at max or min
  if (intensity >= 255 || intensity <= 0) {
    direction *= -1;
    intensity = constrain(intensity, 0, 255);
  }
  
  // Extract RGB components from the color argument
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;
  
  // Set all pixels to the same intensity, using the color argument
  for (int i = 0; i < pixelNumber; i++) {
    uint8_t fadedR = (r * intensity) / 255;
    uint8_t fadedG = (g * intensity) / 255;
    uint8_t fadedB = (b * intensity) / 255;
    strip.setPixelColor(i, strip.Color(fadedR, fadedG, fadedB));
  }
  
  strip.show();
}

// Fill strip pixels one after another with a color. Strip is NOT cleared
// first; anything there will be covered pixel by pixel. Pass in color
// (as a single 'packed' 32-bit value, which you can get by calling
// strip.Color(red, green, blue) as shown in the loop() function above),
// and a delay time (in milliseconds) between pixels.
void colorWipe(uint32_t color, int wait) {
  static uint16_t current_pixel = 0;
  pixelInterval = wait;                        //  Update delay time
  strip.setPixelColor(current_pixel++, color); //  Set pixel's color (in RAM)
  strip.show();                                //  Update strip to match
  if(current_pixel >= pixelNumber) {           //  Loop the pattern from the first LED
    current_pixel = 0;
    patternComplete = true;
  }
}


// Theater-marquee-style chasing lights. Pass in a color (32-bit value,
// a la strip.Color(r,g,b) as mentioned above), and a delay time (in ms)
// between frames.
void theaterChase(uint32_t color, int wait) {

  static uint32_t loop_count = 0;
  static uint16_t current_pixel = 0;

  pixelInterval = wait;                   //  Update delay time

  strip.clear();

  for(int c=current_pixel; c < pixelNumber; c += 3) {
    strip.setPixelColor(c, color);
  }
  strip.show();

  current_pixel++;
  if (current_pixel >= 3) {
    current_pixel = 0;
    loop_count++;
  }

  if (loop_count >= 10) {
    current_pixel = 0;
    loop_count = 0;
    patternComplete = true;
  }
}


// Rainbow cycle along whole strip. Pass delay time (in ms) between frames.
void rainbow(uint8_t wait) {

  if(pixelInterval != wait)
    pixelInterval = wait;                   
  for(uint16_t i=0; i < pixelNumber; i++) {
    strip.setPixelColor(i, Wheel((i + pixelCycle) & 255)); //  Update delay time  
  }
  strip.show();                             //  Update strip to match
  pixelCycle++;                             //  Advance current cycle
  if(pixelCycle >= 256)
    pixelCycle = 0;                         //  Loop the cycle back to the begining
}


//Theatre-style crawling lights with rainbow effect
void theaterChaseRainbow(uint8_t wait) {

  if(pixelInterval != wait)
    pixelInterval = wait;                   //  Update delay time  
  for(int i=0; i < pixelNumber; i+=3) {
    strip.setPixelColor(i + pixelQueue, Wheel((i + pixelCycle) % 255)); //  Update delay time  
  }
  strip.show();
  for(int i=0; i < pixelNumber; i+=3) {
    strip.setPixelColor(i + pixelQueue, strip.Color(0, 0, 0)); //  Update delay time  
  }      
  pixelQueue++;                           //  Advance current queue  
  pixelCycle++;                           //  Advance current cycle
  if(pixelQueue >= 3)
    pixelQueue = 0;                       //  Loop
  if(pixelCycle >= 256)
    pixelCycle = 0;                       //  Loop
}


// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {

  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}


/********************** MISC FUNCTIONS *****************************/


// Dump a byte array as hex values to Serial.
void printHex(byte *buffer, byte bufferSize) {

  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}


// Flash the LED on the shield
void flashLED() {
  digitalWrite(LED_SHIELD, LOW);
  delay(50);
  digitalWrite(LED_SHIELD, HIGH);
  delay(50);
  digitalWrite(LED_SHIELD, LOW);
  delay(50);
  digitalWrite(LED_SHIELD, HIGH);
  delay(50);
}
