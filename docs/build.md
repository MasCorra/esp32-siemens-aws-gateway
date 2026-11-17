# Reproducible build on Windows and Linux

The build uses versions and archives pinned in [dependencies.lock.json](../dependencies.lock.json). It needs no AWS account, PLC, connected ESP32 or credentials. Downloads, libraries, the core, caches and output stay in the Git-ignored `.build` directory; global Arduino installations are not changed.

## Requirements

- Windows x64 or Linux x64.
- [PowerShell 7](https://learn.microsoft.com/powershell/scripting/install/installing-powershell), available as `pwsh`.
- Git and a `g++` compiler with C++17 support.
- On Linux, `tar` and `chmod`.
- Internet access for the initial setup.

Run commands from PowerShell in the repository root. On Linux, start `pwsh` first. The native compiler is a system prerequisite; scripts do not install it.

Library archives and the ESP32 toolchain are pinned, but system packages and CI runner images are not fully immutable. The procedure repeats dependency selection and compilation; it does not promise byte-identical binaries across hosts.

## Repository checks and native tests

```powershell
./scripts/check-repository.ps1
./tests/repository_checks.ps1
./scripts/prepare-environment.ps1 -NativeOnly
./scripts/test-native.ps1
```

`-NativeOnly` downloads ArduinoJson 7.0.4, checks its SHA-256 and metadata, and extracts it under `.build/user/libraries/ArduinoJson`. The test wrapper passes this include path explicitly.

To select a compiler:

```powershell
./scripts/test-native.ps1 -Compiler '/absolute/path/to/g++'
```

Repository checks validate required files, archive separation, Git ignore rules, common credential markers, and source hashes for Settimino and the archive. They do not detect every possible secret. Four negative tests use temporary fixtures under `.build` to verify rejection of changed Settimino source, tracked private keys, extra active sketches and missing ignore rules.

## ESP32 builds

```powershell
./scripts/prepare-environment.ps1
./scripts/build-firmware.ps1
./scripts/build-firmware.ps1 -ValidateConfiguration
```

Setup verifies Arduino CLI before running it and installs the ESP32 core under `.build/data`. Library archives are checked against SHA-256; Boards Manager validates core tool archives. Distribution license files are retained.

| Component | Pinned version |
|---|---|
| Arduino CLI | 1.6.0 |
| ESP32 core / build target | 3.0.7 / `esp32:esp32:esp32` |
| ArduinoJson | 7.0.4 |
| PubSubClient | Index 2.8.0, metadata 2.8 |
| Ethernet | 2.0.2 |
| Settimino | 2.0.0, preserved `ESP32_WIFI` configuration |

The build explicitly selects `third_party/Settimino` after checking its five source hashes. The library source is unchanged, and its five compiler warnings remain visible. See [Settimino attribution](../THIRD_PARTY_NOTICES.md#settimino).

Compilation uses an isolated copy of the seven sketch files with `secrets.h.example` only. A local `secrets.h` is excluded. The normal build leaves PLC settings unset; `-ValidateConfiguration` uses dummy DB/date settings to compile the configured path. Neither command uploads firmware or changes source files.

Logs are written to `.build/logs`; build output is under `.build/firmware/<mode>`. Deployment needs the [actual PLC settings](firmware.md#required-settings) and hardware/cloud commissioning.

Cached archives are verified again before use. A hash mismatch stops setup. Investigate a mismatch before removing the affected cached archive and retrying; do not change the lock to accept an unexplained download.

## GitHub Actions

The [workflow](../.github/workflows/ci.yml) runs on `windows-2022` and `ubuntu-24.04` for pushes, pull requests and manual dispatch. It uses the same scripts, a commit-pinned checkout action and `contents: read` permissions. No project credentials, firmware uploads or releases are involved.

Each job runs repository checks, four negative tests, **25 native tests with 761 assertions**, and both ESP32 builds. Historical sketches are checked for integrity but excluded from compilation. [CI results](https://github.com/MasCorra/esp32-siemens-aws-gateway/actions) record the outcome for each commit.

Hardware and cloud testing are separate from these software checks. See [firmware validation](firmware.md#software-validation) for coverage and limitations.
