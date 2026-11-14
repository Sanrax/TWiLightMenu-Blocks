# r24 icon uploads

Production base: `1f345f725`; BlocksDS 1.24.0; melonDS 1.1 DS/DLDI, built-in
BIOS, disposable 32 MiB image, default skins, box art/music disabled. The
fixture combines static icons, synthetic eight-frame banners and bundled
GBARunner2. Patched synthetic animations select a different frame/palette
every tick. No commercial ROM, BIOS or NAND is included here.

- `halted-fast-palette-before.json`: pre-fix snapshot, explicit GDB halt.
  Visible animated slot 18 selected palette 0, while its bank still held other
  colors and its loaded flag was -1. Slot 16 is clipped; its mismatch is not
  evidence of a displayed error. IRQ nesting was not recorded in this older
  capture. Pixel bytes in all loaded banks matched their staged source data.
- `fixed-*.json`: patched captures after startup/scrolling. Loaded icon bytes
  match; visible animated palettes match their selected frames. Later records
  also verify palette VRAM against its CPU cache. Each patched capture records
  IRQ nesting zero. Saturn's unselected neighbors are deliberately excluded.
- `copy-timing.json`: debugger breakpoints at `glLoadIcon` entry and its return
  in the patched 3DS UI; 256-row/4 KiB upload, line 192 to 195 (three scanlines,
  rounded to scanline boundaries). This is an emulator margin check, not a
  physical timing guarantee or a startup speedup measurement.
- PNGs show the patched default UIs; generated striped icons are intentional.

GBATEK documents rendering starting at line 214, **during** VBlank. The old
guard allowed texture remaps through line 250. Production now starts full
uploads only on lines 192–200 and small palette updates through 208, with
remaining work deferred. Startup/page publication waits for queued banks
while servicing audio. Frame selection precedes palette synchronization.

The host regression compiles the production functions with hardware shims.
It checks late entry, copying ownership, multi-frame queue preservation,
duplicate suppression, stale palette flags, off-window aliases, moving icons
and Saturn/dialog selection. It does not emulate physical storage latency.
The exact intermittent hardware tile corruption was not reproduced locally;
the r24 hardware overlay is a candidate for confirming the timing correction.
