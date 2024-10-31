#ifndef ORB_STATION_EXAMPLE_H
#define ORB_STATION_EXAMPLE_H

#include "OrbStation.h"

class OrbStationExample : public OrbStation {
public:
    OrbStationExample(StationId id);

protected:
    void onOrbConnected() override;
    void onOrbDisconnected() override;
    void onError(const char* errorMessage) override;
    void onUnformattedNFC() override;
};

#endif