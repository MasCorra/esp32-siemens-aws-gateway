# Authors, provenance and licenses

## Project code

**Project author: Corrado Maschio.** The [MIT license](LICENSE) covers the main firmware in `firmware/PLC_AWS_Gateway`, tests, scripts, CI configuration and original project documentation.

Historical sources in `archive/sketches` and third-party components retain their existing rights and license terms. The project MIT license does not replace those terms. Historical comments and diagnostic messages are translated into English; original attribution and legal notices remain intact.

## Settimino

PLC communication uses **Settimino 2.0.0 by Davide Nardella**, also listed as Dave Nardella in library metadata. Source headers carry copyright 2013, 2019 and the **LGPL-3.0-or-later** license.

- [Official project](https://sourceforge.net/projects/settimino/)
- [Davide Nardella's profile](https://sourceforge.net/u/davenardella/profile/)
- [Official licensing page](https://settimino.sourceforge.net/licensing.html)
- [Version 2.0.0 distribution](https://sourceforge.net/projects/settimino/files/2.0.0/)

`third_party/Settimino` preserves the five files from the project's local `ESP32_WIFI` library configuration. Source, comments, copyright and platform selection are unchanged. Provenance and SHA-256 hashes are recorded in [third_party/settimino-source.json](third_party/settimino-source.json). Complete GPLv3 and LGPLv3 texts accompany the source.

The historical `ESP32-PLC` and `ESP32_PLC-security` demos derive from **ReadDemo by Davide Nardella**, created on 12 December 2016 and modified on 10 March 2019 according to its original header. These contributions retain the applicable Settimino terms.

## Other dependencies

Build dependencies are downloaded into the isolated environment. Their original notices and license files remain applicable.

| Component | Version | Attribution and license |
|---|---|---|
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | 7.0.4 | Benoit Blanchon, copyright 2014–2024; MIT |
| [PubSubClient](https://github.com/knolleary/pubsubclient) | Index 2.8.0, metadata 2.8 | Nicholas / Nick O'Leary, copyright 2008–2020; MIT |
| [Ethernet](https://github.com/arduino-libraries/Ethernet/tree/2.0.2) | 2.0.2 | Arduino and contributors, including Paul Stoffregen, Bjoern Hartmann, MCQN Ltd. and Cristian Maglie; per-file MIT, Apache-2.0, GPL-2.0 or LGPL-2.1, and public-domain notices; see AUTHORS and file headers |
| [Arduino core ESP32](https://github.com/espressif/arduino-esp32/tree/3.0.7) | 3.0.7 | Espressif, Arduino and contributors; Arduino/WiFi/TLS headers include LGPL-2.1-or-later; core and SDK components also carry their own licenses |
| [Arduino CLI](https://github.com/arduino/arduino-cli) | Pinned in the build lock | External Arduino tool; retain the notices shipped in its distribution |

Historical sketches also reference [Blynk](https://github.com/blynkkk/blynk-library) 1.3.2 (Volodymyr Shymanskyy, MIT) and [ESP32Ping](https://github.com/marian-craciunescu/ESP32Ping) 1.6 (Daniele Colanardi and Marian Craciunescu in metadata, LGPL-2.1-or-later with additional permissive notices in `ping.cpp`). They are not dependencies of the main gateway build.

This inventory covers the repository and selected build dependencies. A distributed ESP32 binary may contain additional components whose licenses must also be considered.
