# Hardware Overview

The PCB, schematic, and production files are published on OSHWLab:

- [PY32 T12 controller project](https://oshwlab.com/dhruvgoel3829/project_wyxcfekm)

This repository does not contain the PCB fabrication data. Use the OSHWLab project for the schematic, layout, BOM, and Gerbers.

## Blocks

| Block | Part / description |
|---|---|
| MCU | Puya PY32F002Ax5, 20 KB flash, 3 KB RAM |
| Supply | 24 V DC input, 3 A or higher recommended |
| Logic rail | 5 V regulator (78L05 class) |
| Heater switch | CDD58N06 N-channel MOSFET, high-side/24 V switching |
| Gate drive | MMBT2222A NPN stage with gate pull-up and Zener clamp |
| Tip sensor | T12 thermocouple, amplified by LM358 |
| Display | SSD1306 128x64 OLED, I2C |
| User input | Rotary encoder with integrated push button |
| Tip detect | Reed switch |
| Programming | 3.3 V/5 V SWD header |

## Pinout

| MCU pin | Direction | Function | Firmware definition |
|---|---|---|---|
| PA0 | analog | VIN divider | `adc_read_vin_raw()` |
| PA1 | AF13 | Heater PWM, TIM1 CH4 | `heater_init()` |
| PA2 | open-drain | OLED SDA | `firmware/src/oled.c` |
| PA3 | open-drain | OLED SCL | `firmware/src/oled.c` |
| PA4 | analog | T12 thermocouple amplifier output | `adc_read_tip_raw()` |
| PA5 | input, pull-up | Reed switch | `REED_PIN` |
| PA6 | input, pull-up | Encoder DT | `ENC_DT_PIN` |
| PA7 | input, pull-up | Encoder CLK | `ENC_CLK_PIN` |
| PB0 | input, pull-up | Encoder push button | `BTN_PIN` |
| PB1 | output | Buzzer | `BUZZER_PIN` |
| PA13 | SWD | SWDIO | debug probe |
| PA14 | SWD | SWCLK | debug probe |

The firmware uses a **software I2C** implementation on PA2/PA3 so the OLED does not depend on a hardware I2C peripheral.

## Heater drive

The N-channel MOSFET is used as a high-side switch. The gate is pulled up toward the 24 V rail, and the MMBT2222A pulls the gate low from the MCU pin. This means the MCU pin has **inverted polarity**:

- MCU output low → NPN on → MOSFET gate low → heater **off**
- MCU output high / PWM → NPN off → gate pulled up → heater **on**

The firmware uses:

```c
#define HEATER_ACTIVE_LOW 1
```

If your board uses a different gate-drive polarity, change this definition and verify the heater is off at reset and while flashing.

## Tip sensor

The T12 tip contains a thermocouple in series with the heater. The LM358 amplifies the small thermocouple voltage, and the result is fed to **PA4/ADC_IN4**.

The firmware measures the tip with the heater switched off:

1. Set heater power to zero.
2. Wait 950 µs for the tip line to settle.
3. Read and average eight 12-bit ADC samples.
4. Restore the requested heater power.

If the heater is on during the measurement, or the amplifier output does not reach PA4, the reading will be wrong or stuck at zero.

## Power and grounding

- Keep the 24 V heater current path away from the thermocouple amplifier input.
- Use a star ground or a carefully placed ground pour; heater return current must not flow through the sensor ground.
- Keep the OLED/encoder wiring away from the heater switching node.
- Add adequate decoupling at the MCU and the LM358.
- The SWD header should be referenced to the MCU logic ground.

## Safety

- The tip reaches soldering temperature in seconds. Use a proper stand.
- Do not leave the iron unattended.
- Verify that a reset, floating MCU pin, or unplugged probe leaves the heater **off**.
- Use an isolated 24 V supply of adequate current rating.
- Never work on the board while the 24 V supply is connected.

## First-power-up checks

Before connecting a T12 handle:

1. Inspect the board for solder bridges and reversed polarized parts.
2. Power the board from a current-limited 24 V supply.
3. Measure the 5 V (or logic) rail.
4. Verify the MCU is running and the SWD probe can identify the target.
5. Check the heater pin with the firmware held in reset: the heater must be off.
6. Connect a T12 handle and confirm the tip sensor voltage changes when the tip is warmed.
