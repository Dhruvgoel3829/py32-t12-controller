# py32-t12-controller

Low-cost PY32F002A-based PID controller for **T12 soldering iron tips**, with an SSD1306 OLED and 24 V operation.

[Photo of the assembled board]

[![License](https://img.shields.io/badge/license-[LICENSE]-blue)](LICENSE)

## Overview

A compact, open-source digital soldering station controller built around the **Puya PY32F002A** (Cortex-M0+). It reads the T12 thermocouple through an LM358 amplifier, runs a PID loop, and switches the heater with a CDD58N06 MOSFET driven by an MMBT2222A NPN stage. Everything runs from a single 24 V supply.

- Project page: [OSHWLab link](https://oshwlab.com/dhruvgoel3829/project_wyxcfekm)
- Predecessor: [Solder_Station](https://github.com/Dhruvgoel3829/Solder_Station) (Arduino Nano version)

## Features

- PID temperature control, 10 Hz loop
- Temperature range: [200–450 °C]
- SSD1306 OLED showing set temperature, actual temperature, and heater power
- Rotary encoder for setting temperature and navigating the menu
- Sleep / standby with auto-shutoff
- Tip-change and no-tip detection
- Calibration offset stored in flash
- Hand-solderable, with cheap and easy-to-source parts

## Hardware

| Block | Part / Description |
|---|---|
| MCU | Puya PY32F002A, running at 5 V |
| Input power | 24 V DC, 3 A or higher recommended |
| 5 V regulator | 78L05 (use SOT-89 or SO-8 for better heat dissipation) |
| Heater switch | CDD58N06 N-channel MOSFET |
| MOSFET driver | MMBT2222A NPN |
| Thermocouple amplifier | LM358 |
| Display | SSD1306 OLED, I²C |
| Programming | SWD header |

### Heater drive

The MOSFET gate is pulled up from the 24 V rail for full enhancement. The MMBT2222A pulls the gate low from the 5 V MCU logic, and an 18 V Zener between gate and source clamps Vgs.

The gate drive is **inverted**: the MCU turns the BJT on to switch the heater **off**. PWM polarity in the firmware accounts for this.

> **Safety note:** if the MCU pin is floating or in reset, the BJT is off and the heater can turn on. [Describe how your board handles this, e.g. a base pull-up to 5 V so the heater defaults to off during reset.]

## Firmware

- Language / framework: [C / PY32 HAL / PlatformIO / Keil]
- PID loop at 10 Hz (100 ms). The heater is switched off for about 10 ms at the end of each cycle, and the thermocouple is sampled after it settles, so switching noise doesn't corrupt the reading. Samples are averaged to reduce noise.
- Maximum heater duty is about 90%.
- Anti-windup clamps the integral term.

### Build and flash

```bash
# 
```

Flash over SWD using [ST-Link / J-Link / CMSIS-DAP].

## Getting started

1. Order the PCB and parts from the BOM (`/hardware/bom`)
2. Solder the components
3. Flash the firmware over SWD
4. Connect a 24 V supply and a T12 handle
5. Calibrate against a reference thermometer, then start soldering

## Repository layout

```
/hardware    schematic, PCB, BOM, gerbers
/firmware    source code
/docs        images and notes
```

[Edit to match your repo]

## Safety

- Use a properly rated, isolated 24 V DC supply.
- The T12 tip heats up in seconds. Always use a stand and never leave the iron unattended.
- Calibrate temperature readings before relying on them.
- You build and use this at your own risk.

## License

[MIT], see [LICENSE](LICENSE).

## Author

Designed by **Dhruv**.
