# Troubleshooting

## OLED is blank or garbled

The firmware uses software I2C on PA2/PA3 and an SSD1306 controller.

Check:

- OLED `VCC` and `GND`
- SDA on PA2, SCL on PA3
- pull-ups on SDA/SCL (many modules include them)
- OLED address: the firmware probes `0x78` and `0x7A` and prefers `0x78`
- common ground between the MCU and display

The OLED init sequence is sent as one I2C transaction. If the display shows random pixels or a partial image, the issue is usually wiring, pull-ups, power, or bus noise rather than the framebuffer.

The firmware keeps diagnostic counters in RAM:

- `oled_ack_78`
- `oled_ack_7a`
- `oled_ready`
- `oled_nack_count`

A working `0x78` module normally shows `oled_ack_78 = 1`, `oled_ack_7a = 0`, `oled_ready = 1`, and `oled_nack_count = 1` (the intentional probe of `0x7A`).

## Encoder skips or the menu jumps

The encoder is sampled from the 1 ms SysTick:

- 1 ms stable-state filter
- 4 quadrature counts per detent
- 20 ms button debounce

Check:

- encoder `VCC`/`GND`
- common ground
- pull-ups on CLK/DT (the MCU enables internal pull-ups)
- short encoder wires, away from the heater switching node
- `EC REVERSE` if the direction is backwards

A short movement of the encoder may leave a remainder in `encoder_raw`; that is normal until it reaches four counts.

## Probe cannot connect (`Get IDCODE error`)

A successful flash followed by a failed connection usually means the probe/target/power path is intermittent, not that the firmware removed SWD.

Check:

- ST-Link/CMSIS-DAP is detected: `pyocd list`
- target logic rail is present
- SWDIO = PA13, SWCLK = PA14, plus ground
- SWD wires are short and not shared with the heater current path
- try a different USB port/cable, or connect the probe directly
- power-cycle the target and probe
- hold the target in reset while starting the debugger
- disconnect the 24 V heater supply during debugging

The PY32F002A keeps SWD on PA13/PA14. This firmware does not reconfigure those pins.

## ADC monitor shows PA4 TIP = 0

This is the first check to run when the temperature is stuck.

Enter `SETUP MENU → ADC MONITOR`. The screen forces the heater off and shows four raw 12-bit values.

- If `VREF` and `TEMP SENS` are **nonzero**, the ADC and channel switching are working. The problem is the external tip-sensor path or the pin mapping.
- If **all four are zero**, the ADC is not converting or the channels are not being selected.
- If `PA4 TIP` is zero but `PA0 VIN` changes when the input voltage changes, only the PA4 path is dead.

For a zero PA4:

1. Measure the voltage on PA4 with a multimeter while the tip is warm.
2. Check continuity from the LM358 output to PA4.
3. Confirm the LM358 is powered and its input network is intact.
4. Check the T12 handle wiring and thermocouple polarity.
5. Confirm the board actually routes the tip sensor to PA4. If it is on another ADC pin, the firmware must be changed.

A floating PA4 can produce a plausible-looking but meaningless value in the normal temperature loop. The ADC monitor is the better check because it holds the heater off and samples repeatedly.

## Temperature reading is wrong

- Run the three-point calibration.
- Check `VIN` and `VCC` on the information screen.
- Verify the LM358 output range on PA4.
- Check for heater noise on the sensor line.
- Check that the heater is actually off during the 950 µs measurement window.

The temperature conversion assumes:

- raw < 200 → 21 °C to `cal200`
- 200 ≤ raw < 280 → `cal200` to `cal280`
- raw ≥ 280 → `cal280` to `cal360`

## Heater never turns off or never heats

- Verify the MOSFET type and gate-drive polarity.
- The firmware uses `HEATER_ACTIVE_LOW 1` for the NPN + N-channel high-side drive described in the hardware notes.
- If your board uses the opposite polarity, change `HEATER_ACTIVE_LOW` and verify with a current-limited supply.
- Check the MOSFET, NPN, gate pull-up, and Zener.
- Check that the heater pin is inactive while the MCU is held in reset.

Do not debug the heater with a T12 tip connected and a high-current supply until the on/off polarity is confirmed.

## Settings do not survive a power cycle

- Settings are stored at `0x08004F80`.
- The linker script reserves the last 128-byte page, so the application flash region is `0x4F80` bytes.
- `make flash` performs a full chip erase and resets settings.
- If the settings page is erased or the CRC is invalid, defaults are loaded.

## Build warnings about `_close`, `_lseek`, `_read`, `_write`

These are newlib syscall stubs for an embedded target with no filesystem. They are harmless for this firmware and do not indicate a problem.

## Build fails because the toolchain is missing

Install the Arm GNU toolchain, or point the Makefile at it:

```bash
make ARM_TOOLCHAIN=/path/to/toolchain/bin
```

## Flashing works but the station behaves oddly after a flash

A full chip erase resets:

- calibration
- default temperature
- timers
- control mode (DIRECT)
- buzzer/screen/encoder options

Re-enter the menu and configure the station after flashing.
