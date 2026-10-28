# Credits and Upstream Work

This project is a port, not a from-scratch design. The control behavior, menu structure, and much of the application logic come from the original open-source project below.

## ATmega-Soldering-Station

- Author: Stefan Wagner (wagiminator)
- Project: <https://github.com/wagiminator/ATmega-Soldering-Station>
- License: Creative Commons Attribution-ShareAlike 3.0 Unported (CC BY-SA 3.0)
- License text: <https://creativecommons.org/licenses/by-sa/3.0/>

The T12 control logic, temperature model, menu concept, and operating behavior in this repository are derived from that project.

## PY32 build scaffolding

- Project: <https://github.com/IOsetting/py32f0-template>
- The Makefile/rules layout and the original vendor-tree selection were based on this template.

## Puya PY32 SDK and CMSIS

The files under `firmware/vendor/` come from the Puya PY32F0xx SDK and Arm CMSIS:

- Puya PY32F0xx LL drivers and device headers — BSD-3-Clause
- Arm CMSIS Core headers — Apache-2.0
- GCC startup file and system source — Puya SDK, BSD-3-Clause

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the detailed list.

## Tooling

- GNU Arm Embedded Toolchain — GPLv3 with runtime exception
- PyOCD — Apache-2.0
- SEGGER J-Link — proprietary SEGGER software, used only as an optional flashing tool
- Puya device pack — Puya license, redistributed for convenience in `tools/`

## This repository

Original port work, hardware adaptation, documentation, and the cleaned-up build tree are by Dhruv Goel.

The repository-level [LICENSE](LICENSE) is MIT for the original work added here. **The upstream-derived firmware portions remain subject to CC BY-SA 3.0.** If you redistribute a firmware image or a modified source tree that includes the derived control logic, keep the upstream attribution and comply with CC BY-SA 3.0 for those portions.
