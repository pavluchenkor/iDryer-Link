#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include "device_identity.h"

class CredentialStore {
 public:
  bool begin();
  bool load(DeviceIdentity &identity);
  bool save(const DeviceIdentity &identity);
  void clear();

 private:
  static constexpr const char *kNamespace = "idryer";
  Preferences prefs_;
};
