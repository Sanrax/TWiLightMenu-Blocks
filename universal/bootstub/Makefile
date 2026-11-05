TWLM_ROOT ?= $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
include $(TWLM_ROOT)/universal/blocksds.mk

TARGET	:=	bootstub
export BOOTSTUB	?= $(TARGET).bin

$(BOOTSTUB): $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

$(TARGET).elf: $(TARGET).s Makefile
	$(CC) -Wl,-Ttext=0 -x assembler-with-cpp -nostartfiles -nostdlib $(TARGET).s -o $@


clean:
	rm -f $(TARGET).elf $(TARGET).bin
