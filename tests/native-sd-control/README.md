# Native SDK SD control

Build with BlocksDS 1.24.0 using `make -C tests/native-sd-control`.
This uses the SDK default ARM7 and does not link any TWiLight storage code.
Run on a disposable DSi SD image: it overwrites `sd:/native-sd-probe.bin`
with 4 KiB patterns and checks 16 write/read rounds.

Observed in local melonDS 1.1: SD mounting succeeds, then the first write
stalls after `Mounted; writing...`. A debugger finds ARM7 waiting in native
TMIO command 24 (`0x0C18`), as in the native-storage menu. This observation
limits emulator validation and does not establish a hardware failure.
No BIOS, firmware, NAND, SD images or games are included in this test.
