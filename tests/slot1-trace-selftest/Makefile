BLOCKSDS ?= /opt/blocksds/core
NAME := slot1_trace_check
GAME_TITLE := Slot1 trace self-test
DEFINES := -DTWLM_STARTUP_TRACE -DTWLM_SLOT1_TRACE
INCLUDEDIRS := ../../universal/include
LDFLAGS := -Wl,--defsym=__irq_stack_size=4096 -Wl,--wrap=irqSet -Wl,--wrap=fifoSendDatamsg -Wl,--wrap=fifoSendValue32 -Wl,--wrap=fifoGetValue32
ifeq ($(IPC_TEST),1)
DEFINES += -DSLOT1_TRACE_IPC_SELFTEST
ARM7ELF := ../../title/arm7/title-slot1-trace.elf
endif
include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile
