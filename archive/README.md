# Historical sketches

`sketches/` contains seven historical versions. The maintained firmware is [PLC_AWS_Gateway](../firmware/PLC_AWS_Gateway/PLC_AWS_Gateway.ino).

| Sketch | Purpose |
|---|---|
| `ESP32-PLC` | PLC read and serial dump |
| `ESP32_PLC-security` | PLC test with external credentials |
| `ESP32-AWS.Funzionante` | AWS counter and relay test |
| `PLC-AWS.prova` | Initial PLC/AWS integration |
| `PLC_AWS_testing` | Historical telemetry with a 26-byte DB |
| `PLC_AWS_testing.rev1` | Variant with a damaged include |
| `PLC-AWS_Module.rev0` | Incomplete modular refactoring |

Comments and diagnostic messages are translated into English; historical behavior, identifiers and filenames are preserved. Inline credentials use placeholders. Current file hashes and the hashes preceding translation are recorded in [sha256.json](sha256.json).

Historical sketches are excluded from CI builds and retain the [documented issues](../docs/historical-versions.md#known-issues).

Project author: **Corrado Maschio**. PLC demos also derive from **ReadDemo by Davide Nardella**, author of Settimino. Historical and third-party contributions retain their own license terms; see [third-party notices](../THIRD_PARTY_NOTICES.md).
