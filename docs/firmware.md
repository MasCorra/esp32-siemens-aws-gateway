# Gateway firmware

`firmware/PLC_AWS_Gateway` acquires five measurements, monitors the PLC heartbeat and publishes data to AWS IoT Core. It uses the advanced thesis layout, with a PLC DB of at least 312 bytes. The earlier 26-byte layout is limited to historical sketches.

The PLC DB number, date encoding, date format and time zone must match the target PLC. Startup stops before any connection if these settings are incomplete.

## Data layout and payload

The data layout follows figures 7 and 10 of the thesis report, *Cloud Processing e Monitoring*:

| Value | DB offset / source |
|---|---|
| PLC date text | Starts at byte 2; encoding must be configured |
| `Spool_Fbk` | Big-endian REAL at 256 |
| `Spool_CMD` | Big-endian REAL at 260 |
| `Axe_Position_Fbk` | Big-endian REAL at 264 |
| `Axe_Position_Reference` | Big-endian REAL at 268 |
| `Hyd_Pressure` | Big-endian REAL at 272 |
| `CounterPLC` | Signed INT at 308, not published |
| `Msg_Counter` | Signed INT at 310, used for local heartbeat monitoring |

The read requires **at least 312 bytes**. Verify the DB number, full block size and physical units in the PLC project. The preset IP address, rack and slot come from the test bench and must match the target device.

Settimino's `S7.StringAt(Target, 2)` returns a pointer to byte 2 without decoding a STRING header. Configuration therefore distinguishes an S7 STRING with its header at bytes 0/1 from NUL-terminated text starting at byte 2.

Messages are published to `esp32/counter` with these eight fields. Example with **dummy values**:

```json
{
  "EspID": "EXAMPLE-DEVICE",
  "TimeStamp": 1704067200,
  "Spool_Fbk": 1.25,
  "Spool_CMD": -2.5,
  "Axe_Position_Fbk": 3,
  "Axe_Position_Reference": 4,
  "Hyd_Pressure": 5,
  "PLC_Status": true
}
```

`TimeStamp` is Unix time in **seconds**, derived from the PLC date. NTP is used only for TLS certificate validation. `EspID` uses the local `GATEWAY_DEVICE_ID` override or a stable 12-digit hexadecimal identifier from the ESP32 eFuse MAC. Associate this identifier with the relevant dashboards.

## Required settings

Set these macros in `GatewayConfig.h` after checking the PLC project:

| Setting | Default | Accepted values |
|---|---|---|
| `GATEWAY_PLC_DB_NUMBER` | `0`, unset | Actual DB number; zero prevents startup |
| `GATEWAY_PLC_DATE_ENCODING` | `0`, unset | `1`: S7 header at 0/1; `2`: NUL-terminated text from 2 |
| `GATEWAY_PLC_DATE_FORMAT` | `0`, unset | `1`: `YYYY-MM-DD HH:MM:SS` or a `T` separator; `2`: `DD/MM/YYYY HH:MM:SS` |
| `GATEWAY_PLC_UTC_OFFSET_MINUTES` | `32767`, unknown | UTC offset from -840 to +840 minutes |

ISO dates may include `Z` or a `+HH:MM` / `-HH:MM` suffix; an explicit zone takes precedence over the configured offset. The parser supports 1970–2099 and rejects impossible dates, fractional seconds, unsupported formats and zone-free dates without a known offset. A fixed offset must account for the recorded date and daylight-saving rules; UTC or an explicit zone avoids ambiguity.

Validation-build overrides are dummy settings for compilation, not deployment values.

## Acquisition and publishing

- All five REAL values and the date are validated before a sample is accepted. Failed reads and non-finite values do not become new measurements.
- Measurement changes are checked independently of the heartbeat. The first valid sample and changes to measurements or `PLC_Status` can trigger publication; date or counter changes alone do not.
- The heartbeat counter is observed on every valid read. Stagnation, communication failures and recovery update the local state. `PLC_Status` is not a general plant diagnosis or a safety signal.
- Sampling defaults to **250 ms** and heartbeat timeout to **12 seconds**, both configurable. The reference PLC heartbeat interval is 5 seconds; sampling is independent of that interval.
- A state-only notification may retain the last valid sample's values and timestamp. No measurement is published before the first valid sample.
- The published baseline advances only after local `publish` success. Reconnection restores subscriptions and sends the current state. `mqtt.loop()` is serviced after synchronous PLC calls.
- JSON size and MQTT buffer limits are checked.

MQTT is **QoS 0**: local success does not confirm persistence in IoT Core or Timestream. Only the latest state is retained while offline; there is no persistent queue or guarantee that every intermediate change reaches the cloud.

Network and Settimino calls are synchronous within library timeouts. The PLC receive timeout alone does not bound TCP connection time. A 312-byte read may span multiple PDUs and is not an atomic snapshot of a changing DB. Timestamp/measurement consistency must be checked in the PLC program.

## Optional relays

Relays are disabled by default. When enabled, they use GPIO 15, 2, 4 and 22, topics `esp32/relay1`–`esp32/relay4`, and active-low logic. Initialization registers each pin as input, preloads HIGH, then enables output. Boot transients and external circuitry require physical testing.

Commands must contain exactly one literal `status` field equal to `0`, `1`, `"0"` or `"1"`. JSON whitespace is allowed; payloads are limited to 128 bytes and topics must match exactly. Extra or duplicate fields, nesting, booleans, floats, escaped keys and trailing data are rejected. This restricted grammar avoids allocation in the MQTT callback.

## Software validation

The C++17 native suite uses ArduinoJson 7.0.4 with `-Wall -Wextra -Wpedantic -Werror`. It contains **25 tests and 761 assertions** covering the PLC layout, dates and offsets, heartbeat, publication state, JSON fields, buffers, commands and GPIO initialization. Regression cases include a stalled heartbeat after a full `millis()` wrap and relative paths in the PowerShell runner.

Windows and Linux CI run the native suite and two builds for ESP32 Dev Module (`esp32:esp32:esp32`), core 3.0.7 and Arduino CLI 1.6.0. See the [build guide](build.md) and [CI results](https://github.com/MasCorra/esp32-siemens-aws-gateway/actions).

Settimino produces five warnings per build: strict aliasing, arithmetic on `void*` and an unused variable. Its source remains unchanged. Binary size may vary slightly across hosts and does not measure runtime heap use or device performance.

PLC, relay and AWS commissioning, as well as device latency, throughput and power measurements, remain outstanding.
