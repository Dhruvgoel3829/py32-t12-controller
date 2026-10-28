# Building and Flashing

## Requirements

- GNU Arm Embedded Toolchain (`arm-none-eabi-gcc`, `arm-none-eabi-objcopy`)
- GNU Make
- Python 3 and PyOCD, or SEGGER J-Link
- ST-Link, CMSIS-DAP, or J-Link probe
- Target board with a 3.3 V/5 V logic rail and SWD header

The default Makefile uses `/usr/bin/arm-none-eabi-*` and PyOCD.

## Toolchain

Check the compiler is available:

```bash
arm-none-eabi-gcc --version
```

If the compiler is installed elsewhere:

```bash
make ARM_TOOLCHAIN=/path/to/toolchain/bin
```

## Build

From the repository root:

```bash
make clean
make
```

Expected output:

```text
  CC  firmware/src/main.c
  ...
  LD  firmware/ld/py32f002ax5.ld -> Build/t12-station.elf
Memory region         Used Size  Region Size  %age Used
             RAM:        2296 B         3 KB     74.74%
           FLASH:       12492 B      20352 B     61.38%
```

Artifacts:

| File | Purpose |
|---|---|
| `Build/t12-station.elf` | debug/build image |
| `Build/t12-station.hex` | Intel HEX for flashing |
| `Build/t12-station.bin` | raw binary |
| `Build/t12-station.map` | linker map |
| `Build/t12-station.lst` | disassembly/source listing |

The linker reserves the last 128-byte flash page (`0x08004F80`) for saved settings, so the usable application flash region is `0x4F80` bytes.

## Flash with PyOCD / ST-Link

Install PyOCD:

```bash
pip install pyocd
```

The repository includes a PyOCD pack and config:

- `tools/Puya.PY32F0xx_DFP.1.1.7.pack`
- `tools/pyocd.yaml`

Flash:

```bash
make flash
```

This runs a full chip erase before programming. **A full chip erase also erases saved settings**, so calibration and preferences return to defaults.

To check probe detection:

```bash
pyocd list
```

To halt and inspect the target:

```bash
pyocd commander -t py32f002ax5 --config tools/pyocd.yaml -M halt
```

## Flash with J-Link

Install the SEGGER J-Link software, then make the Py32 device files available:

```bash
mkdir -p ~/.config/SEGGER
cp -r tools/jlink/JLinkDevices ~/.config/SEGGER/
```

Flash:

```bash
make flash FLASH_PROGRM=jlink
```

The J-Link command script is `tools/jlink/jlink-command`; it loads `Build/t12-station.hex`.

## Debugging

A minimal PyOCD GDB server:

```bash
pyocd gdbserver -t py32f002ax5 --config tools/pyocd.yaml
```

Then connect with `arm-none-eabi-gdb` or a VSCode Cortex-Debug setup using:

```json
{
  "name": "PY32 T12",
  "type": "cortex-debug",
  "request": "launch",
  "servertype": "pyocd",
  "cwd": "${workspaceFolder}",
  "executable": "${workspaceFolder}/Build/t12-station.elf",
  "device": "py32f002ax5",
  "configFiles": ["${workspaceFolder}/tools/pyocd.yaml"],
  "svdFile": "${workspaceFolder}/tools/SVD/py32f002axx.svd",
  "interface": "swd"
}
```

If the probe cannot identify the target, see [TROUBLESHOOTING.md](TROUBLESHOOTING.md).

## Build options

| Variable | Default | Meaning |
|---|---|---|
| `PROJECT` | `t12-station` | output file base name |
| `BUILD_DIR` | `Build` | build output directory |
| `FLASH_PROGRM` | `pyocd` | `pyocd` or `jlink` |
| `ARM_TOOLCHAIN` | `/usr/bin` | toolchain bin directory |
| `PYOCD_EXE` | `pyocd` | PyOCD executable |
| `JLINKEXE` | `/opt/SEGGER/JLink/JLinkExe` | J-Link executable |
| `OPT` | `-Os` | optimization level |
| `DEBUG_FLAGS` | `-gdwarf-3` | debug information |

Example release build with a different optimization level:

```bash
make OPT=-O2
```

## Clean

```bash
make clean
```
