# Configuration and dependencies

## Board and sketch

Use the [isolated build procedure](build.md) to select the pinned core and libraries. Arduino IDE uses its own installed dependencies and is useful for manual deployment.

Open `firmware/PLC_AWS_Gateway/PLC_AWS_Gateway.ino` and select the actual ESP32 board. CI targets the generic ESP32 Dev Module, `esp32:esp32:esp32`; verify that this matches the bench hardware before uploading.

Arduino requires the main `.ino` name to match its directory. Historical sketch names retain their original dots and hyphens, as allowed by the [Arduino sketch specification](https://docs.arduino.cc/arduino-cli/sketch-specification/).

## Local credentials

1. Copy `secrets.h.example` to `secrets.h` beside the main sketch.
2. Set the WiFi SSID and password.
3. Set the AWS IoT Thing/client name, endpoint, root CA, device certificate and private key, using complete PEM values.

From the repository root:

```powershell
Copy-Item -LiteralPath './firmware/PLC_AWS_Gateway/secrets.h.example' -Destination './firmware/PLC_AWS_Gateway/secrets.h'
```

Keep real values in `secrets.h`, which is ignored by Git. The sketch can compile without that file by falling back to the example; placeholders do not enable connections. Isolated build scripts always use the example and exclude local credentials.

The six historical credential templates follow the same convention; `ESP32_PLC-security` needs WiFi values only. `ESP32-PLC` retains inline placeholders: move credentials into local configuration before using that historical sketch.

## PLC settings

`GatewayConfig.h` contains the PLC address, rack, slot, DB settings, timing and GPIO mapping. The DB number, date encoding, date format and UTC offset must be configured; see [required firmware settings](firmware.md#required-settings).

Check PLC access permissions, WiFi reachability, DB layout and size against the target PLC project. Preset private addresses belong to the original bench and are not universal defaults. The PLC project is not included; an S7-1200 FactoryIO template is not a substitute for the S7-1500 project.

Relays are disabled by default. Verify GPIO boot behavior and external circuitry before enabling them.

## Pinned build dependencies

[dependencies.lock.json](../dependencies.lock.json) pins the main build. Setup places dependencies in `.build` without changing global Arduino libraries.

| Component | Version | Use |
|---|---|---|
| Arduino CLI | 1.6.0 | Build and core management |
| ESP32 core | 3.0.7 | WiFi, TLS, SPI, IP addressing and time |
| Settimino | 2.0.0, `ESP32_WIFI` | PLC reads |
| ArduinoJson | 7.0.4 | JSON serialization with `JsonDocument` |
| PubSubClient | Index 2.8.0, metadata 2.8 | MQTT |
| Ethernet | 2.0.2 | Included by Settimino even in its ESP32 WiFi branch |

Historical sketches also reference Blynk 1.3.2 (`BlynkTimer`, with BlynkNcpDriver 0.6.3 declared by that installation) and ESP32Ping 1.6. They are not dependencies of the main build. Historical JSON code retains `StaticJsonDocument`.

### Settimino

The preserved `Platform.h` selects:

```cpp
#define ESP32_WIFI
// #define M5STACK_LAN
```

The build explicitly uses `third_party/Settimino` and verifies its source hashes, avoiding conflicting global platform selections. The library source and its original notices are unchanged.

Settimino is by **Davide Nardella**, licensed under LGPLv3 or later. Complete GPL/LGPL texts accompany the source. Historical PLC demos also derive from ReadDemo. See [third-party notices](../THIRD_PARTY_NOTICES.md) for credits and license scope.
