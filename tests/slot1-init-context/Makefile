BLOCKSDS ?= /opt/blocksds/core
.DEFAULT_GOAL := all
APP ?= title
VARIANT ?= current
NAME := slot1_context_$(APP)_$(VARIANT)
GAME_TITLE := Slot1 init context
ARM7ELF := ../../$(APP)/arm7/$(NAME).elf
ARM7_SOURCES := source ../../universal/arm7/source ../../universal/sdmmc/arm7/source ../../tests/slot1-init-context/arm7
ARM7_FLAGS := -specs=$(BLOCKSDS)/sys/crts/ds_arm7.specs -g -mthumb -mcpu=arm7tdmi -mthumb-interwork -Wl,--nmagic -Wl,--wrap=cardReadHeader -Wl,--wrap=cardReadID -Wl,--wrap=cardPolledTransfer -Wl,--wrap=libndsCrash

.PHONY: arm7
arm7:
	$(MAKE) -C ../../$(APP)/arm7 TARGET=$(NAME) BUILD=build-$(NAME) SOURCES='$(ARM7_SOURCES)' LDFLAGS='$(ARM7_FLAGS)'

include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile

$(ROM): arm7
