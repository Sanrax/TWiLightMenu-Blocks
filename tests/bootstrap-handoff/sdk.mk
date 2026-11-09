BLOCKSDS ?= /opt/blocksds/core
NAME := bootstrap_handoff
GAME_TITLE := Bootstrap handoff control
GAME_SUBTITLE := DSi console SD / r18
GAME_AUTHOR := TWiLightMenu-Blocks diagnostics
SOURCEDIRS := source
INCLUDEDIRS := ../../universal/include
BINDIRS := data
include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile
