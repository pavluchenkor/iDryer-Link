# TODO

English is the main version. Russian: [TODO.ru.md](TODO.ru.md).

- **The compatibility version with the controller.**
  The major version of the firmware serves two purposes at once: the compatibility token
  of the pair during the handshake, and the binding to the controller's menu, which is
  compiled into the module. The UART wire version (`uart_protocol_version`) is separate
  from both. It is worth deciding where the menu version should live — in the firmware
  major or in a fingerprint of its own.

- **Printer data on the dryer.**
  Moonraker is compiled in, but the firmware has no handlers for its data: it uses
  neither the chamber target nor the print progress. This will be needed for the Klipper
  macro that starts drying — see the core's TODO.
