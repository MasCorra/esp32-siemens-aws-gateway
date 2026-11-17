# Historical versions

The archive contains seven sketches from 2024, six supporting `.cpp` files and six headers: **19 source files** in total. They cover separate PLC and AWS experiments, their integration and an incomplete modular refactoring.

Historical behavior is preserved. Credentials are excluded; the inline SSID and password in `ESP32-PLC` use placeholders. Comments and diagnostic messages are available in English. Historical directory and identifier names are retained for traceability.

## Sketches

| Sketch | Path under `archive/sketches` | Purpose and status |
|---|---|---|
| `PLC_AWS_testing` | `PLC_AWS_testing` | Integrated reference: DB2, 26 bytes, decoded values, JSON and timestamp; MQTT and change-detection issues |
| `ESP32-PLC` | `ESP32-PLC` | DB1 read and serial dump; no AWS integration |
| `ESP32_PLC-security` | `ESP32_PLC-security` | PLC test with external credentials; the name does not indicate encrypted S7 traffic |
| `ESP32-AWS.Funzionante` | `ESP32-AWS.Funzionante` | AWS counter/message and relay test; no PLC read |
| `PLC-AWS.prova` | `PLC-AWS.prova` | Initial PLC/AWS integration; MQTT payload contains a counter rather than PLC measurements |
| `PLC_AWS_testing.rev1` | `PLC_AWS_testing.rev1` | Testing variant with a corrupted include |
| `PLC-AWS_Module.rev0` | `PLC-AWS_Module.rev0` | Incomplete split into WiFi, AWS, PLC, time, data and utility managers |

`PLC_AWS_testing` represents the historical PLC–JSON–MQTT integration. The sketch used in the final thesis demonstration is not documented.

The archive includes source files and credential templates, not installers, disk images, thesis documents or private cloud configuration. [archive/sha256.json](../archive/sha256.json) records the current 25 archived files, including the six credential templates.

## Settimino platform variants

The local Settimino 2.0.0 installations contained the same protocol source but different `Platform.h` selections: `ESP32_WIFI` versus `M5STACK_LAN`. The main build explicitly selects the preserved `ESP32_WIFI` copy in `third_party/Settimino`.

## Known issues

These notes apply to historical sources. The main firmware handles acquisition, publication and commands as described in the [gateway guide](firmware.md).

### Integrated reference

| Location in `PLC_AWS_testing/PLC_AWS_testing.ino` | Issue |
|---|---|
| Main `loop()` | Missing `client.loop()` and `timer.run()`; no complete MQTT reconnection path |
| Change detection | Comparison precedes decoding; component changes are considered only after a machine-state or pressure change |
| Component JSON handling | Local `JsonArray Components` shadows the global array; comparison data is copied from JSON |
| Publication | `client.publish()` result is ignored while the baseline and change flag still advance |
| Failed reads | Buffer extraction can continue after a read error, reusing stale data |
| MQTT callback | Payload length and JSON/status validity are not checked before driving GPIO |
| Time and TLS | Fixed offsets and NTP setup after the first TLS connection require validation |

JSON capacity and MQTT packet size also need checking with the selected library versions.

### Other variants

- `PLC_AWS_testing.rev1` contains a corrupted include and cannot compile as-is.
- The modular main uses `Utility` without its header and calls `markTime/showTime`, while the header declares `MarkTime/ShowTime`. `Utility.cpp` does not explicitly declare its Arduino dependencies.
- `TimeManager.cpp` uses an unqualified `getLocalTime` call that resolves to the method itself rather than `::getLocalTime`.
- `PLCManager` updates static members while the main passes different globals to `DataManager`. The JSON buffer is not serialized before publication, and MQTT servicing is incomplete.

Blocking connection loops and unbounded WiFi/MQTT waits are shared limitations.

## Validation scope

CI verifies archive hashes but builds and tests only `firmware/PLC_AWS_Gateway`. Historical sketches require the issues above to be resolved before operational use. The exact board and dependency versions used in the 2024 builds are not documented.
