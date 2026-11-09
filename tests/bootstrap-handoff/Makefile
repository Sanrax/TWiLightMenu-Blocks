.PHONY: all clean

# Prepare assets before the SDK makefile enumerates BINDIRS. Keep this probe
# opt-in: production builds and CI must not depend on it.
all:
	$(MAKE) -C ../../universal/bootloader_menu
	mkdir -p data
	cp ../../universal/bootloader_menu/load.bin data/load.bin
	$(MAKE) -f sdk.mk

clean:
	$(MAKE) -f sdk.mk clean
	rm -rf data universal
