# Industrial IoT Gateway: ESP32, Siemens PLC and AWS

Master's thesis project for monitoring industrial automotive processes. An ESP32 reads a Siemens S7-1500 PLC and sends measurements to AWS over MQTT with TLS. TIA Portal and PLCSIM support the test bench; Amazon Timestream and Grafana provide the storage and visualization layer.

**Author:** Corrado Maschio. **Main firmware license:** [MIT](LICENSE). Siemens S7 communication uses **Settimino by Davide Nardella**, licensed under LGPL-3.0-or-later. See [third-party notices](THIRD_PARTY_NOTICES.md).

```mermaid
flowchart LR
    T[TIA Portal] --> P[PLC / PLCSIM]
    P -->|S7 over TCP/IP · Settimino| E[ESP32]
    E -->|JSON · MQTT over TLS| A[AWS IoT Core]
    A --> D[Amazon Timestream]
    D --> G[Grafana]
```

The main sketch, `PLC_AWS_Gateway`, reads a PLC data block of at least **312 bytes**, acquires five measurements and publishes the PLC timestamp and heartbeat status. Earlier sketches are kept in `archive/sketches`.

**Before running:** configure the PLC DB number, date encoding, date format and time zone. These settings have no operational defaults; incomplete configuration prevents startup.

The [Windows and Linux CI pipeline](https://github.com/MasCorra/esp32-siemens-aws-gateway/actions) runs **25 tests with 761 assertions** and two ESP32 builds on each system. Hardware and cloud integration still require commissioning. Settimino produces five known warnings per build; see [firmware validation](docs/firmware.md#software-validation).

## Getting started

- [Main sketch](firmware/PLC_AWS_Gateway/PLC_AWS_Gateway.ino) and [firmware behavior and settings](docs/firmware.md).
- [Configuration](docs/configuration.md): board, PLC settings and local credentials.
- [Reproducible build](docs/build.md): isolated setup and CI on Windows and Linux.
- [Architecture](docs/architecture.md): acquisition, protocols and cloud services.
- [Historical versions](docs/historical-versions.md): previous sketches and known limitations.
- [Development workflow](docs/development.md): cloning, checking changes and publishing.

## Repository layout

```text
firmware/PLC_AWS_Gateway/   main firmware, PLC DB >= 312 bytes
archive/sketches/          seven historical sketches
third_party/Settimino/     preserved ESP32-configured library
scripts/                  setup, repository checks, tests and builds
tests/                    native logic and repository checks
docs/                     configuration, architecture and validation
.github/workflows/ci.yml   Windows and Linux CI
dependencies.lock.json    versions, download URLs and checksums
```

Each sketch directory contains a matching `.ino` file. Historical filenames, including `Funzionante`, identify earlier versions; they do not indicate current compatibility. Historical sketches are excluded from CI builds.

## Build and test

Requirements: PowerShell 7, Git and a `g++` compiler supporting C++17 on Windows x64 or Linux x64. From the repository root:

```powershell
./scripts/check-repository.ps1
./tests/repository_checks.ps1
./scripts/prepare-environment.ps1 -NativeOnly
./scripts/test-native.ps1
```

To build the ESP32 firmware:

```powershell
./scripts/prepare-environment.ps1
./scripts/build-firmware.ps1
./scripts/build-firmware.ps1 -ValidateConfiguration
```

The last command uses dummy PLC settings for compilation only. Build scripts use an isolated sketch with placeholder credentials and never upload firmware. See the [build guide](docs/build.md) for details.

For deployment, copy `secrets.h.example` to `secrets.h` beside the main sketch, add local credentials and complete the PLC settings. Credentials and build artifacts are excluded from Git.

## Limitations

MQTT uses QoS 0 and retains only the latest state while offline. Delivery of every intermediate change, real-time timing and an atomic snapshot of the entire PLC DB are not guaranteed. The four optional relays are disabled by default and require electrical testing before use. S7 requests in the firmware only read PLC data.

## License

MIT covers the main firmware, tests, scripts and project documentation. Historical and third-party contributions retain their own terms. Settimino is included with its original source and license notices. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
