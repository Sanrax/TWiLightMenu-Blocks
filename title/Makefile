#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(PROJECT)),)
export PROJECT	:=	$(CURDIR)/../
endif

export TARGET	:=	title
NITRODATA		:=	nitrofiles

TWLM_ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
include $(TWLM_ROOT)/universal/blocksds.mk
include $(TWLM_ROOT)/universal/languagepacks.mk

.PHONY: bootloader bootstub clean makearm7 makearm9

all:	bootloader bootstub $(TARGET).nds

.PHONY: profile-startup
profile-startup: all
	$(MAKE) -C arm9 BUILD=build-profile TARGET=title-profile PROFILE_STARTUP=1
	ndstool -u 00030004 -g SRLA 01 "TWLMENUPP" -c title-profile.nds -7 $(TARGET).arm7.elf -9 arm9/title-profile.elf -d $(NITRODATA) \
		-b icon.bmp "Title Splash;TWiLight Menu++;Rocket Robz"

makearm7:
	$(MAKE) -C arm7
	cp arm7/$(TARGET).elf $(TARGET).arm7.elf

makearm9: bootloader bootstub
	$(MAKE) -C arm9
	cp arm9/$(TARGET).elf $(TARGET).arm9.elf

dist:	all
	@mkdir -p ../7zfile/debug
	@cp $(TARGET).nds ../7zfile/_nds/TWiLightMenu/main.srldr
	@cp $(TARGET).arm7.elf ../7zfile/debug/$(TARGET).arm7.elf
	@cp $(TARGET).arm9.elf ../7zfile/debug/$(TARGET).arm9.elf

$(TARGET).nds:	makearm7 makearm9
	ndstool	-u 00030004 -g SRLA 01 "TWLMENUPP" -c $(TARGET).nds -7 $(TARGET).arm7.elf -9 $(TARGET).arm9.elf -d $(NITRODATA) \
			-b icon.bmp "Title Splash;TWiLight Menu++;Rocket Robz"

clean:
	@echo clean ...
	@rm -fr data
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).nds
	@rm -fr $(TARGET).arm7.elf
	@rm -fr $(TARGET).arm9.elf
	@rm -fr arm9/build-profile arm9/title-profile.elf title-profile.nds
	@$(MAKE) -C bootloader clean
	@$(MAKE) -C $(PROJECT)/universal/bootstub clean
	@$(MAKE) -C arm9 clean
	@$(MAKE) -C arm7 clean

data:
	@mkdir -p data

bootloader: data
	@$(MAKE) -C bootloader

bootstub: data
	@$(MAKE) -C $(PROJECT)/universal/bootstub BOOTSTUB=$(CURDIR)/data/bootstub.bin
