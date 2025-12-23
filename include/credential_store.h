#pragma once

#include <Arduino.h>
#include "device_identity.h"

class CredentialStore {
 public:
  bool begin();
  bool load(DeviceIdentity &identity);
  bool save(const DeviceIdentity &identity);
  void clear();

 private:
  static constexpr const char *kPath = "/device_identity.json";
  bool mounted_ = false;
};
