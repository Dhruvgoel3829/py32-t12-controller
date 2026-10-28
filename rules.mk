# 'make V=1' will show all compiler calls.
V		?= 0
ifeq ($(V),0)
Q		:= @
NULL	:= 2>/dev/null
endif

PREFIX		?= $(ARM_TOOLCHAIN)/arm-none-eabi-
CC		= $(PREFIX)gcc
XX		= $(PREFIX)g++
AS		= $(PREFIX)as
LD		= $(PREFIX)ld
OBJCOPY		= $(PREFIX)objcopy
OBJDUMP		= $(PREFIX)objdump

TOP		= .
BDIR		= $(TOP)/$(BUILD_DIR)

CSOURCES	:= $(foreach dir, $(CDIRS), $(wildcard $(TOP)/$(dir)/*.c))
CSOURCES	+= $(addprefix $(TOP)/, $(CFILES))
CPPSOURCES	:= $(foreach dir, $(CDIRS), $(wildcard $(TOP)/$(dir)/*.cpp))
CPPSOURCES	+= $(addprefix $(TOP)/, $(CPPFILES))
ASOURCES	:= $(foreach dir, $(ADIRS), $(wildcard $(TOP)/$(dir)/*.s))
ASOURCES	+= $(addprefix $(TOP)/, $(AFILES))

OBJS = $(CSOURCES:$(TOP)/%.c=$(BDIR)/%.o)
OBJS += $(CPPSOURCES:$(TOP)/%.cpp=$(BDIR)/%.o)
OBJS += $(ASOURCES:$(TOP)/%.s=$(BDIR)/%.o)
DEPS = $(CSOURCES:$(TOP)/%.c=$(BDIR)/%.d)

ARCH_FLAGS	:= -mcpu=cortex-m0plus
DEBUG_FLAGS	?= -gdwarf-3
OPT		?= -Os
TGT_CFLAGS	?= $(ARCH_FLAGS) $(DEBUG_FLAGS) $(OPT) -std=c17 $(addprefix -D, $(LIB_FLAGS)) -Wall -ffunction-sections -fdata-sections
TGT_CPPFLAGS	?= $(ARCH_FLAGS) $(DEBUG_FLAGS) $(OPT) -std=c++11 $(addprefix -D, $(LIB_FLAGS)) -Wall -ffunction-sections -fdata-sections
TGT_ASFLAGS	?= $(ARCH_FLAGS) $(DEBUG_FLAGS) $(OPT) -Wa,--warn
TGT_LDFLAGS	?= $(ARCH_FLAGS) -specs=nano.specs -specs=nosys.specs -lc -lm \
				-Wl,-Map=$(BDIR)/$(PROJECT).map \
				-Wl,--gc-sections \
				-Wl,--print-memory-usage

GCC_VERSION := $(shell $(CC) -dumpversion)
IS_GCC_ABOVE_12 := $(shell expr "$(GCC_VERSION)" ">=" "12")
ifeq "$(IS_GCC_ABOVE_12)" "1"
    TGT_LDFLAGS += -Wl,--no-warn-rwx-segments
endif

TGT_INCFLAGS := $(addprefix -I $(TOP)/, $(INCLUDES))

.PHONY: all clean flash echo

all: $(BDIR)/$(PROJECT).elf $(BDIR)/$(PROJECT).bin $(BDIR)/$(PROJECT).hex $(BDIR)/$(PROJECT).lst

echo:
	$(info 1. $(AFILES))
	$(info 2. $(ASOURCES))
	$(info 3. $(CSOURCES))
	$(info 4. $(OBJS))
	$(info 5. $(TGT_INCFLAGS))

-include $(DEPS)

$(BDIR)/%.o: %.c
	@printf "  CC\t$<\n"
	@mkdir -p $(dir $@)
	$(Q)$(CC) $(TGT_CFLAGS) $(TGT_INCFLAGS) -MT $@ -o $@ -c $< -MD -MF $(BDIR)/$*.d -MP

$(BDIR)/%.o: %.cpp
	@printf "  XX\t$<\n"
	@mkdir -p $(dir $@)
	$(Q)$(XX) $(TGT_CPPFLAGS) $(TGT_INCFLAGS) -MT $@ -o $@ -c $< -MD -MF $(BDIR)/$*.d -MP

$(BDIR)/%.o: %.s
	@printf "  AS\t$<\n"
	@mkdir -p $(dir $@)
	$(Q)$(CC) $(TGT_ASFLAGS) -o $@ -c $<

$(BDIR)/$(PROJECT).elf: $(OBJS) $(TOP)/$(LDSCRIPT)
	@printf "  LD\t$(LDSCRIPT) -> $@\n"
	$(Q)$(CC) $(TGT_LDFLAGS) -T$(TOP)/$(LDSCRIPT) $(OBJS) -o $@

%.bin: %.elf
	@printf "  OBJCP BIN\t$@\n"
	$(Q)$(OBJCOPY) -I elf32-littlearm -O binary  $< $@

%.hex: %.elf
	@printf "  OBJCP HEX\t$@\n"
	$(Q)$(OBJCOPY) -I elf32-littlearm -O ihex  $< $@

%.lst: %.elf
	@printf "  OBJDP LST\t$@\n"
	$(Q)$(OBJDUMP) --source $< > $@

clean:
	rm -rf $(BDIR)/*

flash: $(BDIR)/$(PROJECT).elf
ifeq ($(FLASH_PROGRM),jlink)
	$(JLINKEXE) -device $(JLINK_DEVICE) -if swd -speed 4000 -JLinkScriptFile $(TOP)/tools/jlink/jlink-script -CommanderScript $(TOP)/tools/jlink/jlink-command
else ifeq ($(FLASH_PROGRM),pyocd)
	$(PYOCD_EXE) erase -t $(PYOCD_DEVICE) --chip --config $(TOP)/tools/pyocd.yaml
	$(PYOCD_EXE) load $< -t $(PYOCD_DEVICE) --config $(TOP)/tools/pyocd.yaml
else
	@echo "FLASH_PROGRM must be pyocd or jlink"
	@exit 1
endif
