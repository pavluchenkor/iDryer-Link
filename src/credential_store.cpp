#include "credential_store.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

bool CredentialStore::begin() {
  if (mounted_) {
    return true;
  }
  mounted_ = LittleFS.begin();
  return mounted_;
}

bool CredentialStore::load(DeviceIdentity &identity) {
  if (!begin()) {
    return false;
  }
  if (!LittleFS.exists(kPath)) {
    return false;
  }
  File file = LittleFS.open(kPath, "r");
  if (!file) {
    return false;
  }
  DynamicJsonDocument doc(256);
  auto err = deserializeJson(doc, file);
  file.close();
  if (err) {
    return false;
  }
  identity.token = doc["token"].as<String>();
  identity.deviceId = doc["deviceId"].as<String>();
  identity.serialNumber = doc["serialNumber"].as<String>();
  return identity.deviceId.length() > 0 && identity.token.length() > 0;
}

bool CredentialStore::save(const DeviceIdentity &identity) {
  if (!begin()) {
    return false;
  }
  DynamicJsonDocument doc(256);
  doc["token"] = identity.token;
  doc["deviceId"] = identity.deviceId;
  doc["serialNumber"] = identity.serialNumber;

  File file = LittleFS.open(kPath, "w");
  if (!file) {
    return false;
  }
  const size_t bytes = serializeJson(doc, file);
  file.close();
  return bytes > 0;
}

void CredentialStore::clear() {
  if (!begin()) {
    return;
  }
  if (LittleFS.exists(kPath)) {
    LittleFS.remove(kPath);
  }
}

