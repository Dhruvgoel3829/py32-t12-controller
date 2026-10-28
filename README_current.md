# PY32 T12 Soldering Controller

Low-cost, open-source **T12 soldering station** firmware for the **Puya PY32F002A** (Cortex-M0+), with an SSD1306 OLED, rotary encoder, and 24 V heater control.

This repository contains the complete firmware, a minimal PY32 LL/CMSIS vendor tree, build/flash tooling, and replication notes. It is a port of the original [ATmega-Soldering-Station](https://github.com/wagiminator/ATmega-Soldering-Station) firmware to the PY32F002A.

- Hardware design files (schematic, PCB, BOM, Gerbers): [OSHWLab project](https://oshwlab.com/dhruvgoel3829/project_wyxcfekm)
- Firmware repository: [github.com/Dhruvgoel3829/py32-t12-controller](https://github.com/Dhruvgoel3829/py32-t12-controller)
- Predecessor: [Solder_Station](https://github.com/Dhruvgoel3829/Solder_Station) (Arduino Nano version)
- License: [MIT](LICENSE) for this repository; see [CREDITS.md](CREDITS.md) for upstream notices

## Features

- Direct (bang-bang) or PID heater control, selectable from the menu
- 10 Hz PID loop with anti-windup
- 12-bit tip ADC with 8-sample averaging
- Heater is switched off for 950 µs before each tip measurement
- SSD1306 128x64 OLED over software I2C
- 1 ms encoder polling with button debounce
- Boost mode, sleep timer, and automatic off timer
- Tip-present / no-tip detection
- Three-point temperature calibration stored in the last flash page
- Screen flip, encoder reverse, and buzzer options
- Diagnostic ADC raw-value screen

## Hardware

The board is published on OSHWLab: <https://oshwlab.com/dhruvgoel3829/project_wyxcfekm>

| Block | Part / description |
|---|---|
| MCU | Puya PY32F002Ax5, 20 KB flash, 3 KB RAM |
| Input power | 24 V DC, 3 A or higher recommended |
| 5 V regulator | 78L05 (use a package with adequate thermal performance) |
| Heater switch | CDD58N06 N-channel MOSFET |
| MOSFET driver | MMBT2222A NPN stage |
| Thermocouple amplifier | LM358 |
| Display | SSD1306 OLED, 128x64, I2C |
| User input | Rotary encoder with push button |
| Tip detection | Reed switch input |
| Programming | SWD header |

The heater gate drive is inverted: the MCU turns the NPN stage on to switch the heater **off**. The firmware accounts for this with `HEATER_ACTIVE_LOW 1` in `firmware/src/main.c`.

See [hardware/README.md](hardware/README.md) for the full pinout, wiring notes, and safety information.

## Pinout

| Pin | Function |
|---|---|
| PA0 | VIN divider, ADC channel 0 |
| PA1 | Heater PWM, TIM1 CH4 |
| PA2 | OLED SDA |
| PA3 | OLED SCL |
| PA4 | T12 thermocouple input, ADC channel 4 |
| PA5 | Reed switch input, pull-up |
| PA6 | Encoder DT |
| PA7 | Encoder CLK |
| PB0 | Encoder push button |
| PB1 | Buzzer |
| PA13 / PA14 | SWDIO / SWCLK |

## Repository layout

```text
.
├── firmware/
│   ├── src/                 # application source
│   ├── ld/                  # linker script
│   └── vendor/              # minimal PY32 LL/CMSIS vendor tree
├── tools/                   # PyOCD/J-Link flashing and SVD files
├── hardware/                # hardware overview and pinout
├── docs/                    # build, operation, replication, troubleshooting
├── Makefile
├── rules.mk
├── LICENSE
└── README.md
```

## Quick start

### 1. Install the toolchain and PyOCD

```bash
# Debian/Ubuntu example
sudo apt install make git python3-pip
# Install the Arm GNU toolchain for arm-none-eabi
# https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads

pip install pyocd
```

The Makefile expects `arm-none-eabi-gcc` in `/usr/bin`. Override it if needed:

```bash
make ARM_TOOLCHAIN=/opt/gcc-arm/bin
```

### 2. Build

```bash
make
```

Build artifacts are written to `Build/`:

- `Build/t12-station.elf`
- `Build/t12-station.hex`
- `Build/t12-station.bin`

### 3. Flash

With an ST-Link or CMSIS-DAP probe:

```bash
make flash
```

With J-Link:

```bash
make flash FLASH_PROGRM=jlink
```

See [docs/BUILD.md](docs/BUILD.md) for probe setup, debugging, and J-Link device-file installation.

## Operation

- Rotate the encoder to change the set temperature.
- Click the encoder to toggle boost.
- **Hold the encoder for 500 ms** to open the setup menu.
- In the menu, rotate to move and click to select.
- `SAVE & EXIT` writes changed settings to flash.

Full menu and calibration instructions are in [docs/OPERATION.md](docs/OPERATION.md).

## Replication

If you want to build your own unit, start with [docs/REPLICATE.md](docs/REPLICATE.md). It walks through the board, assembly checks, flashing, first power-up, calibration, and a test checklist.

## Known hardware checks

The ADC diagnostic screen (`SETUP MENU → ADC MONITOR`) shows four raw values with the heater forced off:

- `PA4 TIP`
- `PA0 VIN`
- `VREF`
- `TEMP SENS`

If `VREF` and `TEMP SENS` are valid but `PA4 TIP` stays at zero, the ADC and channel switching are working; check the tip-sensor/op-amp path and continuity to PA4. The firmware can only measure the voltage that reaches that pin.

See [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) for this and other common issues.

## Safety

- Use a properly rated, isolated 24 V DC supply.
- The T12 tip heats in seconds. Always use a stand and never leave the iron unattended.
- Verify the heater is off during reset, flashing, and fault conditions.
- Calibrate the temperature reading before relying on it.
- You build and use this at your own risk.

## Credits

The control logic is derived from Stefan Wagner's [ATmega-Soldering-Station](https://github.com/wagiminator/ATmega-Soldering-Station), licensed CC BY-SA 3.0. The PY32 build scaffolding was originally based on [IOsetting/py32f0-template](https://github.com/IOsetting/py32f0-template). See [CREDITS.md](CREDITS.md) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
