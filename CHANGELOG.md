# Changelog

Notable changes to the `idryer-link` firmware — the link module between iDryerControllerV2 and the portal. Russian version: [CHANGELOG.ru.md](CHANGELOG.ru.md).

## Types of changes

- **Added** — for new features.
- **Changed** — for changes in existing functionality.
- **Deprecated** — for soon-to-be removed features.
- **Removed** — for now removed features.
- **Fixed** — for any bug fixes.
- **Security** — in case of vulnerabilities.

## [3.0.0] — 2026-09-23

### Added

- **Firmware updates over the air.** Both microcontrollers are updated: the link itself and iDryerControllerV2, whose firmware travels through the link. After the reboot the link checks that the pair of versions is compatible and only then commits the update. If it does not match, it rolls back.
- **Portal binding without a PIN.** The device receives its key on connection and is addressed in the cloud by its own identifier.
- **Unbinding from the portal.** A portal command wipes the key and the device returns to the pairing state.
- **Heater temperature in telemetry.**
- **Flash layout without a filesystem.** Each firmware partition is now 1984 KB instead of 1280 KB: the firmware never used a filesystem, yet the space was reserved for one.

### Changed

- **The local network keeps control even when the cloud is off.** The "ignore external commands" setting now blocks the portal only. The app on the same network keeps controlling the dryer.
- **An unavailable sensor reports an empty value** instead of a zero. No more false zeroes in the portal.
- The firmware moved to the shared `idryer-core` library: portal link, binding and protocol are the same across the ecosystem.
- **Better link stability on ESP32-C3 Super Mini.** Wi-Fi transmit power is limited on these boards.
- **Bambu is not compiled in.** The dryer does not talk to a printer through this module; the image is 14 KB smaller.

### Fixed

- **Builds were published under the wrong version.** The major came from the fallback value in the firmware instead of the controller's menu, so version 1.x.y went to the flasher while the device reported 3.x.y. The portal treated such a build as a downgrade and offered no update.

## [2.x]

A series of trial builds. The direction was closed, the work carried over into 3.0.0.
