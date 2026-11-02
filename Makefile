##### T12 Soldering Station #####

PROJECT		?= t12-station
BUILD_DIR	= Build

# This firmware is fixed to one MCU.
MCU_TYPE	= PY32F002Ax5

# Programmer: pyocd or jlink
FLASH_PROGRM	?= pyocd

##### Toolchains #####

ARM_TOOLCHAIN	?= /usr/bin
JLINKEXE	?= /opt/SEGGER/JLink/JLinkExe
PYOCD_EXE	?= pyocd

##### Paths #####

# C source folders
CDIRS		:= firmware/src \
		   firmware/vendor/py32f0xx_ll/Src
# Single C source files
CFILES		:= firmware/vendor/cmsis/Device/PY32F0xx/Source/system_py32f0xx.c

# Assembly source files
AFILES		:= firmware/vendor/cmsis/Device/PY32F0xx/Source/gcc/startup_py32f002a.s

# Include paths
INCLUDES	:= firmware/src \
		   firmware/vendor/cmsis/Core/Include \
		   firmware/vendor/cmsis/Device/PY32F0xx/Include \
		   firmware/vendor/py32f0xx_ll/Inc

##### Device / linker #####

LIB_FLAGS	= PY32F002Ax5 USE_FULL_LL_DRIVER
JLINK_DEVICE	?= PY32F002AX5
PYOCD_DEVICE	?= py32f002ax5
LDSCRIPT	= firmware/ld/py32f002ax5.ld

include ./rules.mk
