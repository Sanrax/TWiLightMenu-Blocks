# Slot-1 ARM7 initialization context

Run explicitly on `diagnostics`; this test is not part of builds or CI:

```sh
make -C tests/slot1-init-context -j2 all
make -C tests/slot1-init-context -j2 APP=settings all
```

Each ROM links its component's real ARM7 main, card initializer, encryption,
libc and SDK FIFO implementation. Only cartridge I/O is replaced: a valid
synthetic DSPI header and chip ID `C00000C2` exercise the normal-chip KEY1/KEY2
sequence, including real `rand()` calls and timer delays. A wrapped SDK crash
handler relays its message without formatting in the interrupt.

Before the fix, both cores report `Using TLS from IRQ` before the first
transfer, with IRQ nesting 1. After the fix, each of eight requests must return
zero and complete 43 transfers with IRQ nesting zero at entry and the first
and last transfers. `VARIANT` only names outputs when preserving before/after
builds; it does not change the implementation being tested.

Run with built-in emulator BIOS and no SD images. The test does not mount a
filesystem or emulate DSpico's protocol, and cannot establish physical-card
compatibility. Hardware must independently verify cold boot and Settings exit.
