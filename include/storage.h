#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>

void loadWiFiCredentials();
void saveWiFiCredentials(const String &ssid, const String &pass);
void clearAllConfig();

void loadMQTTConfig();
void saveMQTTConfig();

#endif
