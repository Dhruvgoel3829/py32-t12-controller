# Replication Guide

This is the practical path from a bare board to a working T12 station.

## 1. Get the hardware

The PCB, schematic, BOM, and Gerbers are on OSHWLab:

- [PY32 T12 controller project](https://oshwlab.com/dhruvgoel3829/project_wyxcfekm)

The main blocks are listed in [hardware/README.md](../hardware/README.md). The board uses:

- Puya PY32F002Ax5
- 24 V input with a 5 V logic regulator
- CDD58N06 N-channel heater MOSFET with an MMBT2222A gate driver
- LM358 thermocouple amplifier
- SSD1306 I2C OLED
- Rotary encoder and reed switch

## 2. Assemble and inspect

1. Solder the MCU, regulator, MOSFET, gate-drive parts, op-amp, passives, connectors, OLED, and encoder.
2. Inspect for solder bridges, especially around the 24 V and heater pins.
3. Check the MOSFET gate pull-up and Zener orientation.
4. Check the LM358 orientation and the thermocouple input path.
5. Verify the OLED and encoder pin order before powering.

## 3. First power-up without a tip

Use a current-limited 24 V supply.

1. Set the current limit low (for example 100 mA).
2. Power the board.
3. Measure the logic rail.
4. Confirm the OLED backlight/initial display and that the MCU is running.
5. Confirm the heater output is off while the MCU is in reset.
6. Check the SWD probe can identify the PY32F002Ax5.

Do not connect a T12 handle until the heater is verified to be off at reset and while flashing.

## 4. Build and flash

```bash
make clean
make
make flash
```

Build and probe details are in [BUILD.md](BUILD.md). `make flash` performs a full chip erase, so settings and calibration are reset.

## 5. Connect the T12 handle

1. Power off the 24 V supply.
2. Connect the T12 handle.
3. Power on with the current limit still low.
4. Confirm the station boots and the tip is detected.
5. Check the ADC monitor (`SETUP MENU → ADC MONITOR`):
   - `PA4 TIP` should change when the tip warms.
   - `VREF` and `TEMP SENS` should be nonzero.
   - `PA0 VIN` should reflect the input divider.
6. If `PA4 TIP` stays zero, stop and check the sensor/amplifier path before trusting the heater control.

## 6. Calibrate

Use a reference thermometer or a calibrated tip meter.

1. Enter `SETUP MENU → CALIBRATE`.
2. Let each of the three points stabilise.
3. Rotate the encoder until `MEAS` matches the reference.
4. Click to store each point.
5. Confirm `CALIBRATION SAVED`.
6. Return to the main screen and verify the displayed temperature is sensible.

## 7. Functional test checklist

- [ ] OLED shows the main screen without garbage
- [ ] Encoder changes the set temperature smoothly
- [ ] Encoder click toggles boost
- [ ] 500 ms hold opens the setup menu
- [ ] Menu selection does not jump when the encoder is untouched
- [ ] Buzzer beeps if enabled
- [ ] Reed switch wakes/registers activity
- [ ] Heater reaches the set temperature and holds it
- [ ] Tip reading rises when the tip is hot
- [ ] Sleep timer enters `SLEEP`
- [ ] Off timer enters `OFF`
- [ ] Boost timer returns to the normal set point
- [ ] `SAVE & EXIT` preserves settings after a power cycle
- [ ] Heater is off at reset and while flashing

## 8. Enclosure and strain relief

- Provide strain relief for the T12 cable and the 24 V input.
- Keep the OLED and encoder away from the heater switching node.
- Do not route the thermocouple signal parallel to the heater wires.
- Leave clearance around the 5 V regulator and the MOSFET.

## 9. Report your build

If you publish a build, include:

- board revision
- logic-rail voltage
- MOSFET type and gate-drive polarity
- tip model
- calibration values
- firmware commit hash

That makes it easier for someone else to reproduce your results.
