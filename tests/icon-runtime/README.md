# Homebrew icon regression (r24)

Build the normal master browser, title, settings and flashcard booter. Then:

```sh
python3 tests/icon_uploads.py --source /path/to/production-checkout
python3 tests/icon-runtime/fixture.py --source /path/to/production-checkout --theme 0
```

The generator uses `mformat`/`mcopy` from PATH (or the existing ignored local
mtools installation). It replaces only the disposable
`build/research/icon-r24/dldi.img` and generated staging files. The image is
32 MiB, contains 18 synthetic static/animated banners and the already bundled
GBARunner2, and needs no private ROM, BIOS or NAND. Synthetic titles are for
browsing only; do not launch them. Animated frames change palette every tick.

Configure an isolated melonDS 1.1 DS instance with built-in BIOS, this image
as DLDI storage, and its ARM9 GDB server on localhost:4333. Launch the normal
`booter_fc/booter_fc.nds`. Browse using DS controls. Never send desktop quit
shortcuts; terminate only a verified test-owned process. Do not alter the
user's existing emulator configuration or storage images.

At a settled selection, inspect the running browser:

```sh
python3 tests/icon-runtime/gdb_readback.py --source /path/to/production-checkout \
  --output /tmp/icons.json
```

The inspector derives addresses from the matching ARM9 ELF. It explicitly
halts ARM9 before reading, temporarily maps/restores texture and palette VRAM,
then detaches. Check IRQ nesting is zero; retry a capture stopped inside an
IRQ. Loaded banks must have zero pixel differences, palettes must match their
VRAM copies, and visible animated slots must match their selected palettes.
Unloaded banks contain the unknown icon and are intentionally excluded.
Fully clipped neighboring entries are excluded from palette assertions.

Stop the owned emulator before regenerating the image. Repeat with `--theme 1`,
`4` and `5` for 3DS, Saturn and HBL; add `--saturn` to inspection because Saturn
displays only the selected title. Test startup, scrolling, folder/page changes
and move mode. Hardware retesting remains necessary for the intermittent
glitched-tile report and native console SD timing.

Retained r24 captures/readbacks are in `docs/benchmarks/r24/`. Delete the
generated image and staging tree after testing to keep disk use bounded.
