#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------


TWLM_ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../../../..)
include $(TWLM_ROOT)/universal/blocksds.mk

# define SFX_SELECT	0

NAMES			:= select.wav

TARGETS			:= sfx.bin

 
.PHONY: clean all 

all: $(TARGETS)

sfx.bin : $(NAMES)
	@mmutil $^ -d -v -osfx.bin

	
clean:
	@echo Clean ...
