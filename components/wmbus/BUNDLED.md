# Bundled libraries

This copy of the `wmbus` component (SzczepanLeon/esphome-components, branch `version_3`) bundles the two
PlatformIO libraries it used to fetch at build time, so it builds with ESPHome's native ESP8266 toolchain
(2026.10 and later, after `toolchain: platformio` is removed in 2027.4):

| Files | From | Licence |
|---|---|---|
| `SmartRC_CC1101.*`, `ELECHOUSE_CC1101_SRC_DRV.h` | [LSatan/SmartRC-CC1101-Driver-Lib](https://github.com/LSatan/SmartRC-CC1101-Driver-Lib) v3.0.2 | MIT, see `LICENSE.SmartRC-CC1101` |
| `driver*.h`, `drivers.h` | [SzczepanLeon/wmbus-drivers](https://github.com/SzczepanLeon/wmbus-drivers) 1.3.13 | as upstream |

Changes from upstream `version_3`:
- `__init__.py`: the two `cg.add_library(...)` calls for these libraries are removed.
- `rf_cc1101.h`, `cc1101_rf_settings.h`: `#include <ELECHOUSE_CC1101_SRC_DRV.h>` → `#include "ELECHOUSE_CC1101_SRC_DRV.h"`.

No configuration changes: existing `version_3` YAML works as is. Remove `toolchain: platformio` and
`platformio_options: lib_ignore: WiFi` from your config if you added them.
