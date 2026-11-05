#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------


TWLM_ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../../../..)
include $(TWLM_ROOT)/universal/blocksds.mk

# define SFX_WRONG	0
# define SFX_LAUNCH	1
# define SFX_STOP	2
# define SFX_SWITCH	3
# define SFX_STARTUP	4
# define SFX_SELECT	5
# define SFX_BACK	6
# define SFX_LIDCLOSE	7
# define SFX_LIDOPEN	8

NAMES			:= startup.wav wrong.wav launch.wav stop.wav switch.wav select.wav back.wav
NAMES			+= lidclose.wav lidopen.wav

TARGETS			:= sfx.bin bgm.pcm.raw

 
.PHONY: clean all 

all: $(TARGETS)

sfx.bin : $(NAMES)
	@mmutil $^ -d -v -osfx.bin

bgm.pcm.raw   : menu.wav
	sox menu.wav --bits 16 --rate 16000 --encoding signed-integer --endian little bgm.pcm.raw

	
clean:
	@echo Clean ...
	@rm sfx.bin bgm.pcm.raw