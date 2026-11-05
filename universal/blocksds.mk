# Shared BlocksDS tools and rules for the ARM7/ARM9 subprojects.
# Keep the existing source lists and custom loader layouts in each subproject.
.DELETE_ON_ERROR:
export BLOCKSDS ?= /opt/blocksds/core
export BLOCKSDSEXT ?= /opt/blocksds/external
export WONDERFUL_TOOLCHAIN ?= /opt/wonderful

export TWLM_ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
export PREFIX := $(WONDERFUL_TOOLCHAIN)/toolchain/gcc-arm-none-eabi/bin/arm-none-eabi-
export CC := $(PREFIX)gcc
export CXX := $(PREFIX)g++
export AR := $(PREFIX)ar
export OBJCOPY := $(PREFIX)objcopy
export LIBNDS := $(BLOCKSDS)/libs/libnds
export PATH := $(BLOCKSDS)/tools/ndstool:$(BLOCKSDS)/tools/grit:$(BLOCKSDS)/tools/mmutil:$(BLOCKSDS)/tools/dlditool:$(PATH)

CPPFLAGS = -I$(BLOCKSDS)/libs/version/include -D__NDS__ -D__BLOCKSDS__ -I$(BLOCKSDS)/libs/maxmod/include
STORAGE_WRAPS = $(if $(filter blocksds_storage.o,$(OFILES)),$(foreach fn,get_io_dsisd get_io_dsinand sdmmc_GetDiskStatus nand_GetDiskStatus dldiGetInternal fatInit,-Wl$(comma)--wrap=$(fn)))
comma := ,
BLOCKSDS_LIBPATHS = -L$(BLOCKSDS)/libs/maxmod/lib
BLOCKSDS_LDFLAGS = $(if $(filter -lnds9,$(LIBS)),-Wl$(comma)--defsym=__dldi_size=32768)
CPUFLAGS = $(if $(filter -lnds9,$(LIBS)),-mcpu=arm946e-s+nofp,-mcpu=arm7tdmi)
ASSET_OBJECTS ?= $(filter %.bin.o,$(OFILES))
TWLM_BUILD_RULES = $(TWLM_ROOT)/universal/blocksds.mk $(if $(TOPDIR),$(TOPDIR)/Makefile)

.SECONDEXPANSION:

%.arm.o: %.arm.c $$(ASSET_OBJECTS) $$(TWLM_BUILD_RULES)
	@echo CC.ARM $<
	@$(CC) $(CPPFLAGS) $(CFLAGS) -std=gnu11 -marm -mlong-calls -ffunction-sections -fdata-sections -MMD -MP -c $< -o $@

%.arm.o: %.arm.cpp $$(ASSET_OBJECTS) $$(TWLM_BUILD_RULES)
	@echo CXX.ARM $<
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -marm -mlong-calls -ffunction-sections -fdata-sections -MMD -MP -c $< -o $@

%.o: %.c $$(ASSET_OBJECTS) $$(TWLM_BUILD_RULES)
	@echo CC $<
	@$(CC) $(CPPFLAGS) $(CFLAGS) -std=gnu11 -ffunction-sections -fdata-sections -MMD -MP -c $< -o $@

%.o: %.cpp $$(ASSET_OBJECTS) $$(TWLM_BUILD_RULES)
	@echo CXX $<
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -ffunction-sections -fdata-sections -MMD -MP -c $< -o $@

%.o: %.s $$(TWLM_BUILD_RULES)
	@echo AS $<
	@$(CC) $(CPPFLAGS) $(ASFLAGS) -x assembler-with-cpp -MMD -MP -c $< -o $@

%.elf:
	@echo LD $@
	@$(LD) $(CPUFLAGS) $(LDFLAGS) $(BLOCKSDS_LDFLAGS) $(OFILES) $(LIBPATHS) $(BLOCKSDS_LIBPATHS) $(STORAGE_WRAPS) -Wl,--start-group $(if $(findstring g++,$(LD)),-lstdc++) $(LIBS) -lc -Wl,--end-group -o $@

# bin2c keeps the same *_bin.h names used throughout the menu and loaders.
define bin2o
$(BLOCKSDS)/tools/bin2c/bin2c $< .
$(CC) $(CPPFLAGS) $(CFLAGS) -c $(notdir $(subst .,_,$<)).c -o $@
endef
