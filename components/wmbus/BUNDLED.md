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

- `driver_izar.h`: bounds checks brought in line with wmbusmeters/v5: ignore telegrams shorter than the
  15-byte header (alarm/battery bytes 11..13), refuse payloads that would overrun the 64-byte decode
  buffer, and only publish last-month values when the decoded payload contains them.

- IZAR additions, ported from wmbusmeters (as used by v5) and checked against its six IZAR test telegrams:
  - **Custom key:** `key:` is now used by the IZAR driver itself (Diehl LFSR, not AES). 8- or 16-byte
    keys are accepted; the LFSR seed is the first 4 bytes XOR the next 4, as wmbusmeters does.
  - **Second default key:** with no key, both Diehl default keys are tried (previously only the first),
    so PRIOS meters that use the second one now decode.
  - **New sensors:** `last_month_measure_date` (number, YYMMDD — kept under 16777216 so it survives the 32-bit float sensor state), and for SAP PRIOS frames
    `serial_number` and `manufacture_year`.
  - Drivers can now opt out of the generic AES decrypt (`Driver::decrypts_itself()`), which is what lets
    a key reach the IZAR driver.

No configuration changes: existing `version_3` YAML works as is. Remove `toolchain: platformio` and
`platformio_options: lib_ignore: WiFi` from your config if you added them.
