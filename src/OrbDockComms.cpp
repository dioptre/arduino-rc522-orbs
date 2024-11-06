#include "OrbDockComms.h"

// Constructor
OrbDockComms::OrbDockComms(uint8_t rxPin, uint8_t txPin)
    : OrbDock(StationId::GENERIC), // Initialize with a default StationId if needed
      commsSerial(rxPin, txPin) // Initialize SoftwareSerial with RX and TX pins
{
}

// Initialize communication
void OrbDockComms::begin() {
    OrbDock::begin();
    commsSerial.begin(9600); // Initialize SoftwareSerial at 9600 baud
    Serial.println(F("OrbDockComms initialized for serial communication."));
}

// Main loop with communication handling
void OrbDockComms::loop() {
    OrbDock::loop();

    // Handle incoming messages if needed
    while (commsSerial.available()) {
        String received = commsSerial.readStringUntil('\n');
        Serial.print(F("Received via Comms Serial: "));
        Serial.println(received);
        // Handle received messages if implementing commands
    }
}

// Event: Orb Connected
void OrbDockComms::onOrbConnected() {
    OrbDock::onOrbConnected();
    sendMessage("ORB_INSERTED");
    sendMessage("ENERGY_LEVEL:" + String(orbInfo.energy));
}

// Event: Orb Disconnected
void OrbDockComms::onOrbDisconnected() {
    OrbDock::onOrbDisconnected();
    sendMessage("ORB_REMOVED");
}

// Event: Energy Level Changed
void OrbDockComms::onEnergyLevelChanged(byte newEnergy) {
    sendMessage("ENERGY_LEVEL:" + String(newEnergy));
}

// Event: Error Occurred
void OrbDockComms::onError(const char* errorMessage) {
    OrbDock::onError(errorMessage);
    sendMessage("ERROR:" + String(errorMessage));
}

// Event: Unformatted NFC Detected
void OrbDockComms::onUnformattedNFC() {
    OrbDock::onUnformattedNFC();
    sendMessage("UNFORMATTED_NFC");
}

// Helper method to send messages over SoftwareSerial
void OrbDockComms::sendMessage(const String& message) {
    commsSerial.println(message);
    Serial.print(F("Sent via Comms Serial: "));
    Serial.println(message);
}
