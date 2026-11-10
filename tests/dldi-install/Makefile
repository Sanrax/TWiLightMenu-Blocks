BLOCKSDS ?= /opt/blocksds/core
TWLM_SOURCE_ROOT ?= $(abspath ../..)
NAME := dldi_install_probe
GAME_TITLE := DLDI install check
GAME_SUBTITLE := TWiLight diagnostics
INCLUDEDIRS := $(TWLM_SOURCE_ROOT)/universal/include $(BLOCKSDS)/libs/version/include
DEFINES := -DTWLM_STORAGE_SOURCE='"$(TWLM_SOURCE_ROOT)/universal/sdmmc/arm9/source/blocksds_storage.c"'
LDFLAGS := -Wl,--defsym=__dldi_size=32768
include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile

source/drivers.h: prepare.py $(wildcard $(TWLM_SOURCE_ROOT)/title/nitrofiles/dldi/*)
	python3 prepare.py $(TWLM_SOURCE_ROOT)

$(OBJS_SOURCES): source/drivers.h
