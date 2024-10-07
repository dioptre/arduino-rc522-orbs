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

// Pin constants
#define SS_PIN          9                   // SPI pin - Change for nano vs mega etc
#define RST_PIN         8                   // Reset pin - Change for nano vs mega etc
#define LED_BUILTIN     13                  // Arduino LED pin
#define LED_SHIELD      11                  // Protoshield LED pin
#define BUTTON          2                   // Protoshield button pin 

// RFID constants
MFRC522 rfid(SS_PIN, RST_PIN);              // Create MFRC522 instance
MFRC522::StatusCode status;                 // Variable to get card status
#define maxRetries      10                  // Number of retries for failed read/write operations
#define retryDelay      200                 // Delay between retries in milliseconds
#define DELAY_AFTER_CARD_PRESENT 1000       // Delay after card is presented

// Status constants
#define STATUS_SUCCEEDED 1
#define STATUS_FAILED 2
#define STATUS_FALSE 3
#define STATUS_TRUE 4

// Button constants
#define PRESSED LOW
#define RELEASED HIGH

// Our station information
#define NUM_STATIONS 16
#define NUM_STATIONS_PRINT 6 // Temporary
struct Station {
  bool visited;
  byte energy;
};
Station stations[NUM_STATIONS];
int totalStationEnergy = 0;

// Array of station IDs
const char* stationIDs[NUM_STATIONS] = {
  "CONSOLE", "DISTILLER", "CASINO", "FOREST", "ALCHEMY", "PIPES",
  "TBD 7", "TBD 8", "TBD 9", "TBD 10", "TBD 11", "TBD 12", "TBD 13", "TBD 14", "TBD 15", "TBD 16"
};

// Array of traits
const char* traits[] = {
  "NONE", "LAME", "DUMB", "COOL", "SICK", "FISH", "NERD"
};
char trait[5] = "NONE";

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

// Variable for reading the pushbutton status
int buttonState = 0;  // variable for reading the pushbutton status


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

  // Initialize NFC reader
  SPI.begin();                        // Init SPI bus
  rfid.PCD_Init();                    // Init MFRC522 card  
  delay(4);
  Serial.print("MFRC522 software version = ");
  Serial.println(rfid.PCD_ReadRegister(rfid.VersionReg), HEX);
  rfid.PCD_DumpVersionToSerial();     // Show details of PCD - MFRC522 Card Reader details
  Serial.println("PUT YOUR ORBS IN MEEEEE");
}


/********************** LOOP *****************************/

void loop() {

  // Wait for a new card and select the card
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial())
    return;

  digitalWrite(LED_SHIELD, HIGH);  // turn the LED on

  // Wait for NFC to get closer
  delay(DELAY_AFTER_CARD_PRESENT);

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
    if (digitalRead(BUTTON) == PRESSED) {
      // Briefly flash LED
      flashLED();

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
    delay(250);
  }

  // Reset various variables so they don't polluate other orbs
  Serial.println("Orb disconnected, ending session...");
  // TODO: Send a signal to the attached Raspberry Pi that orb has disconnected
  memset(buffer, 0, sizeof(buffer));
  initializeStations();
  totalStationEnergy = 0;

  // turn the LED off
  digitalWrite(LED_SHIELD, LOW);

  // Halt PICC
  rfid.PICC_HaltA();
}


/********************** FUNCTIONS *****************************/

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


// Dump a byte array as hex values to Serial.
void printHex(byte *buffer, byte bufferSize) {

  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}


// Dump debug info about the card; PICC_HaltA() is automatically called
void printDebugInfo() {

  rfid.PICC_DumpToSerial(&(rfid.uid));
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  Serial.print(F("RFID Tag UID:"));
  printHex(rfid.uid.uidByte, rfid.uid.size);
  Serial.println("");
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
