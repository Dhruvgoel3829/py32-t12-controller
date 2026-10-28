# Tools

Support files for flashing, debugging, and inspecting the firmware.

| Path | Purpose |
|---|---|
| `pyocd.yaml` | PyOCD pack configuration |
| `Puya.PY32F0xx_DFP.1.1.7.pack` | Puya device support pack used by PyOCD |
| `SVD/py32f002axx.svd` | peripheral register descriptions for debuggers |
| `jlink/jlink-command` | J-Link Commander script for `make flash FLASH_PROGRM=jlink` |
| `jlink/jlink-script` | J-Link reset/connection script |
| `jlink/JLinkDevices/` | J-Link device definitions and flash algorithms |

## PyOCD

`tools/pyocd.yaml` adds the bundled Puya pack:

```yaml
pack:
  - tools/Puya.PY32F0xx_DFP.1.1.7.pack
```

Use it with the Makefile:

```bash
make flash
```

or directly:

```bash
pyocd list
pyocd erase -t py32f002ax5 --chip --config tools/pyocd.yaml
pyocd load Build/t12-station.elf -t py32f002ax5 --config tools/pyocd.yaml
```

## J-Link

Install the device files once:

```bash
mkdir -p ~/.config/SEGGER
cp -r tools/jlink/JLinkDevices ~/.config/SEGGER/
```

Then:

```bash
make flash FLASH_PROGRM=jlink
```

The command script loads `Build/t12-station.hex`. If you change `PROJECT` in the Makefile, update `tools/jlink/jlink-command` to match the new hex filename.

## SVD

Point your debugger at `tools/SVD/py32f002axx.svd` to get peripheral names and register fields in the debug view. A sample Cortex-Debug configuration is in [../docs/BUILD.md](../docs/BUILD.md).
