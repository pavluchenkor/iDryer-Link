#include "credential_store.h"

bool CredentialStore::begin() {
  return true;  // NVS всегда доступна
}

bool CredentialStore::load(DeviceIdentity &identity) {
  if (!prefs_.begin(kNamespace, true)) {  // read-only
    return false;
  }

  identity.token = prefs_.getString("token", "");
  identity.deviceId = prefs_.getString("deviceId", "");
  identity.serialNumber = prefs_.getString("serial", "");

  prefs_.end();

  return identity.token.length() > 0;
}

bool CredentialStore::save(const DeviceIdentity &identity) {
  if (!prefs_.begin(kNamespace, false)) {  // read-write
    return false;
  }

  prefs_.putString("token", identity.token);
  prefs_.putString("deviceId", identity.deviceId);
  prefs_.putString("serial", identity.serialNumber);

  prefs_.end();
  return true;
}

void CredentialStore::clear() {
  if (!prefs_.begin(kNamespace, false)) {
    return;
  }
  prefs_.clear();
  prefs_.end();
}

