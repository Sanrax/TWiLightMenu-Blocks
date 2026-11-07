#---------------------------------------------------------------------------------
.SUFFIXES:
.DEFAULT_GOAL := all
#---------------------------------------------------------------------------------

ifeq ($(strip $(PROJECT)),)
export PROJECT	:=	$(CURDIR)/../
endif

export TARGET	:=	romsel_dsimenutheme
NITRODATA		:=	nitrofiles

TWLM_ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
include $(TWLM_ROOT)/universal/blocksds.mk

INDEXED_PNG_INPUTS := $(wildcard nitrofiles/languages/*/photo_default.png nitrofiles/themes/*/*/*/*.png)
INDEXED_PNG_OUTPUTS := $(INDEXED_PNG_INPUTS:.png=.pht)
INDEXED_PNG_CONVERTER := ../tools/build/photo-index

$(INDEXED_PNG_CONVERTER): ../tools/photo_index.cpp ../universal/include/common/indexedImage.h ../universal/include/common/lodepng.h ../universal/source/lodepng/lodepng.cpp
	$(MAKE) -C ../tools build/photo-index

%.pht: %.png $(INDEXED_PNG_CONVERTER)
	$(INDEXED_PNG_CONVERTER) $< $@

.PHONY: bootloader bootstub clean makearm7 makearm9

all:	bootloader bootstub $(TARGET).nds

.PHONY: profile-startup
profile-startup: all
	$(MAKE) -C arm9 BUILD=build-profile TARGET=$(TARGET)-profile PROFILE_STARTUP=1
	ndstool -u 00030004 -g SRLA 01 "TWLMENUPP" -c $(TARGET)-profile.nds -7 $(TARGET).arm7.elf -9 arm9/$(TARGET)-profile.elf -d $(NITRODATA) \
	  -b icon.bmp "DSi-based themes;TWiLight Menu++;Rocket Robz"

makearm7:
	$(MAKE) -C arm7
	cp arm7/$(TARGET).elf $(TARGET).arm7.elf

makearm9: bootloader bootstub
	$(MAKE) -C arm9
	cp arm9/$(TARGET).elf $(TARGET).arm9.elf

dist:	all
	@mkdir -p ../7zfile/debug
	@cp $(TARGET).nds ../7zfile/_nds/TWiLightMenu/dsimenu.srldr
	@cp $(TARGET).arm7.elf ../7zfile/debug/$(TARGET).arm7.elf
	@cp $(TARGET).arm9.elf ../7zfile/debug/$(TARGET).arm9.elf

$(TARGET).nds:	makearm7 makearm9 $(INDEXED_PNG_OUTPUTS)
	ndstool	-u 00030004 -g SRLA 01 "TWLMENUPP" -c $(TARGET).nds -7 $(TARGET).arm7.elf -9 $(TARGET).arm9.elf -d $(NITRODATA) \
  -b icon.bmp "DSi-based themes;TWiLight Menu++;Rocket Robz"

clean:
	@echo clean ...
	@rm -fr data
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).nds
	@rm -fr $(TARGET).arm7.elf
	@rm -fr $(TARGET).arm9.elf
	@rm -fr arm9/build-profile arm9/$(TARGET)-profile.elf $(TARGET)-profile.nds
	@rm -f $(INDEXED_PNG_OUTPUTS)
	@$(MAKE) -C $(PROJECT)/universal/bootloader_menu clean
	@$(MAKE) -C $(PROJECT)/universal/bootstub clean
	@$(MAKE) -C arm9 clean
	@$(MAKE) -C arm7 clean

data:
	@mkdir -p data

bootloader: data
	@$(MAKE) -C $(PROJECT)/universal/bootloader_menu LOADBIN=$(CURDIR)/data/load.bin

bootstub: data
	@$(MAKE) -C $(PROJECT)/universal/bootstub BOOTSTUB=$(CURDIR)/data/bootstub.bin
