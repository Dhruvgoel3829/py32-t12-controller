# Firmware

Application source for the PY32F002Ax5 T12 controller.

## Application files (`src/`)

| File | Responsibility |
|---|---|
| `main.c` | startup, state machine, heater control, PID, menu, calibration, diagnostics |
| `adc.c/.h` | ADC channel setup, averaging, VIN/VCC/chip-temperature helpers |
| `encoder.c/.h` | 1 ms encoder polling, detent decoding, button debounce |
| `oled.c/.h` | SSD1306 framebuffer, text rendering, software I2C, flush |
| `settings.c/.h` | settings load/save in the reserved flash page |
| `py32f0xx_it.c/.h` | SysTick and interrupt handlers |

## Vendor tree (`vendor/`)

Only the files needed for this MCU and build are kept:

```text
vendor/
├── cmsis/
│   ├── Core/Include/                       # Arm CMSIS Core
│   └── Device/PY32F0xx/
│       ├── Include/                        # py32f0xx.h, py32f002ax5.h, system header
│       └── Source/
│           ├── system_py32f0xx.c
│           └── gcc/startup_py32f002a.s
└── py32f0xx_ll/
    ├── Inc/                                # LL driver headers
    └── Src/                                # LL driver sources
```

The HAL is **not** used. The build defines `USE_FULL_LL_DRIVER` and targets `PY32F002Ax5`.

## Linker script (`ld/py32f002ax5.ld`)

- Flash: `0x08000000`, length `0x4F80` (20 KB minus the last 128-byte settings page)
- RAM: `0x20000000`, length `0x0C00` (3 KB)
- Settings page: `0x08004F80`

Do not increase the flash region without moving the settings page and updating `settings.c`.

## Adding a source file

1. Put application `.c` files in `src/`.
2. Add the file to the wildcard path already compiled by `CDIRS` in the root `Makefile` (no per-file edit needed).
3. Add any new include directory to `INCLUDES` in `Makefile`.

Vendor files should stay under `vendor/` so the upstream license headers remain obvious.

See [../docs/BUILD.md](../docs/BUILD.md).
