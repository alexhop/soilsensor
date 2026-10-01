#ifndef NETWORK_H
#define NETWORK_H

#include "sensors.h"

// Connect to WiFi with retry logic. Returns true on success.
bool networkConnect();

// Build JSON batch payload and POST to the server.
// Only includes readings for sensors where available == true.
// Returns true if server responded with 2xx.
bool networkPost(const SensorData& data);

// POST a heartbeat to /api/network/heartbeat with device info.
// Always called each cycle, even with zero sensors.
// Returns true if server responded with 2xx.
bool networkHeartbeat(const SensorData& data, int sensorsOnline);

// Clean WiFi disconnect and radio off (for minimal deep sleep current).
void networkDisconnect();

#endif
