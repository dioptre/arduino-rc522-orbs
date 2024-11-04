#ifndef ORBDOCKCOMMS_H
#define ORBDOCKCOMMS_H

#include "OrbDock.h"
#include <SoftwareSerial.h>

class OrbDockComms : public OrbDock {
public:
    OrbDockComms(uint8_t rxPin, uint8_t txPin);
    void begin() override;
    void loop() override;

protected:
    void onOrbConnected() override;
    void onOrbDisconnected() override;
    void onEnergyLevelChanged(uint16_t newEnergy) override;
    void onError(const char* errorMessage) override;
    void onUnformattedNFC() override;

private:
    SoftwareSerial commsSerial;
    void sendMessage(const String& message);
};

#endif // ORBDOCKCOMMS_H