# Operation

## Main screen

The main screen shows:

- `SET:` target temperature
- large actual tip temperature
- state text: `ERROR`, `OFF`, `SLEEP`, `BOOST`, `WORKY`, `HEAT`, or `HOLD`
- tip name (default `BC1.5`)
- input voltage (`VIN`)

## Controls

| Action | Result |
|---|---|
| Rotate encoder | Change set temperature |
| Click encoder | Toggle boost |
| Hold encoder for 500 ms | Open the setup menu |
| Rotate in a menu | Move selection |
| Click in a menu | Select / confirm |
| Move encoder or trigger reed switch | Wake from sleep/off, register activity |

The long-press timer opens the menu **while the button is still held**. Releasing after the menu opens does not also toggle boost.

## Setup menu

```text
SETUP MENU
  TEMPERATURE
  TIMERS
  CONTROL
  BUZZER
  SCREEN FLIP
  EC REVERSE
  CALIBRATE
  INFORMATION
  ADC MONITOR
  SAVE & EXIT
```

### TEMPERATURE

| Item | Default | Range |
|---|---|---|
| DEFAULT TEMP | 320 °C | 150–400 °C |
| SLEEP TEMP | 150 °C | 20–200 °C |
| BOOST TEMP | 50 °C | 10–100 °C above set point |

`BOOST TEMP` is an offset, not an absolute temperature. The boost target is clamped to `TEMP_CUTOFF` (450 °C).

### TIMERS

| Item | Default | Range |
|---|---|---|
| SLEEP TIMER | 5 min | 0–30 min (0 disables) |
| OFF TIMER | 15 min | 0–60 min (0 disables) |
| BOOST TIMER | 40 s | 0–180 s (0 disables) |

Activity resets the sleep/off timers.

### CONTROL

- `DIRECT` — simple on/off control. This is the default.
- `PID` — 10 Hz PID control with anti-windup.

PID uses fixed-point gains in `firmware/src/main.c`. The effective gains are approximately:

| Condition | Kp | Ki | Kd |
|---|---|---|---|
| Error < 30 °C | 11 | 0.3 | 50 |
| Error ≥ 30 °C | 11 | 0.5 | 10 |

PID is disabled by default after a fresh flash or full chip erase.

### BUZZER

Enable or disable the buzzer.

### SCREEN FLIP

Rotate the OLED output 180°.

### EC REVERSE

Reverse the encoder direction if your encoder turns the wrong way.

### CALIBRATE

Three-point calibration using a reference thermometer:

1. Select `CALIBRATE`. The heater is enabled.
2. The screen shows `STEP 1/3`, the target (`SET`), current firmware reading (`ACT`), and the value you are entering (`MEAS`).
3. Let the tip stabilise. Rotate the encoder until `MEAS` matches the reference thermometer.
4. Click to store that point.
5. Repeat for steps 2/3 and 3/3.
6. The values are accepted only if they increase by at least 10 °C between points. On success the screen shows `CALIBRATION SAVED`.

The default calibration points are `216`, `308`, and `390`.

### INFORMATION

Shows firmware version, internal chip temperature, input voltage, and ADC reference voltage.

### ADC MONITOR

Shows raw 12-bit ADC values with the heater forced off:

```text
ADC RAW  HEATER OFF
PA4 TIP       <raw>
PA0 VIN       <raw>
VREF          <raw>
TEMP SENS     <raw>
PRESS TO EXIT
```

Use this screen to verify that the ADC is converting and to check the tip-sensor input. On a working board, `VREF` and `TEMP SENS` are nonzero. If those are valid but `PA4 TIP` is zero, the problem is on the PA4/op-amp/tip-sensor path.

### SAVE & EXIT

Writes changed settings to the reserved flash page and returns to the main screen. Settings are also saved when the menu exits if any item was changed.

## Sleep, off, and boost

- Sleep lowers the target to `SLEEP TEMP`.
- Off turns the heater off.
- Boost raises the target by `BOOST TEMP` for the boost duration.
- Encoder movement or a reed-switch change wakes the station.

## Fault handling

- A reading above 500 °C is treated as a missing/invalid tip (`ERROR`).
- The heater is forced off when the tip is missing, the station is off, the reading is invalid, or the tip is above the 450 °C cutoff.

## Settings storage

Settings are stored in the last 128-byte flash page at `0x08004F80`. The linker script reserves this page. A full chip erase (including `make flash`) erases the page and restores defaults.

## Default settings

| Setting | Default |
|---|---|
| Default temperature | 320 °C |
| Sleep temperature | 150 °C |
| Boost offset | 50 °C |
| Sleep timer | 5 min |
| Off timer | 15 min |
| Boost time | 40 s |
| Control | DIRECT |
| Buzzer | enabled |
| Screen flip | off |
| Encoder reverse | off |
| Calibration | 216 / 308 / 390 |
| Tip name | BC1.5 |
