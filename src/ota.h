#ifndef OTA_H
#define OTA_H

// Check the server for a firmware update.
// If a newer build is available, downloads the binary, flashes it to the
// inactive OTA slot, and reboots into the new firmware.
// Returns normally (no-op) if already up to date or if the check fails.
// WiFi must already be connected before calling this.
void otaCheck();

#endif
