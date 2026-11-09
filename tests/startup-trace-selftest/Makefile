BLOCKSDS ?= /opt/blocksds/core
NAME := watchdog_check
GAME_TITLE := Startup watchdog check
DEFINES := -DTWLM_STARTUP_TRACE
INCLUDEDIRS := ../../universal/include
LDFLAGS := -Wl,--defsym=__irq_stack_size=4096 -Wl,--wrap=irqSet -Wl,--wrap=fifoSendDatamsg -Wl,--wrap=fifoSendValue32 -Wl,--wrap=fifoGetValue32
include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile
