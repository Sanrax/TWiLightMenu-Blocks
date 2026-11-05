# BlocksDS port and startup work

## Build

Use BlocksDS **1.24.0**, Wonderful's ARM toolchain, GNU Make, Python 3,
and a host C/C++ compiler. Initialize the repository's submodules before
packaging. `BLOCKSDS` defaults to `/opt/blocksds/core` and
`WONDERFUL_TOOLCHAIN` to `/opt/wonderful`. The Dockerfile and workflows use
`skylyrac/blocksds:slim-v1.24.0`.

```sh
make clean
make -j6 all
make test
make check-build
make package
```

Outputs retain the existing layout under `7zfile/`. Manual page content is
downloaded separately, as in the existing release workflow. The host R4 menu
encoder is built from the vendored, licensed source under `tools/r4denc/`;
devkitPro binaries are no longer required to produce the autoboot packages.
Archived subprojects under `archived/` are outside this build.

For hardware-test archives, generate the CIA from the rebuilt `booter.nds`
using `booter/make_cia`, as in the release workflow. Extract the official
nds-bootstrap release (the tested version is 2.17.0), then run:

```sh
python3 tools/package_hardware.py --bootstrap path/to/nds-bootstrap --cia booter/booter.cia
```

This writes separate DSi, 3DS and flashcard archives under
`build/hardware-test/dist/`. They include the rebuilt menu components and
image viewer, unmodified bootstrap binaries, installation notes, build
provenance and SHA-256 manifests. The 3DS archive includes the rebuilt CIA.
These are overlays for an existing installation; emulator add-ons remain
separate. User settings, ROMs, saves, NAND and BIOS images are not inputs to
this packaging tool.

## Compatibility boundaries

All live components and themes remain available. DSi and 3DS themes share
`dsimenu.srldr`; their textures are selected by the existing theme branch.
Wood and R4 use separate binaries. Keeping them in the package does not add
their initialization work to the selected menu.

`universal/blocksds.mk` supplies the common SDK tools and rules. Custom ARM7
cores, memory layouts, bootloaders, SD/NAND IPC, and pico-loader handoff are
retained. Source files named `*.arm.c` must compile in ARM state: several
routines are copied to fixed addresses and entered directly by ARM7.
`make check-build` checks this in the linked images, alongside NDS headers and
DLDI allocation sizes. It also checks that the title extends BlocksDS's
uncached MPU region 5 without replacing executable RAM in region 6. The
existing 32 KiB DLDI space is preserved in all
applications, including the Slot-1 launcher and flashcard autoboot variants.

BlocksDS's FatFS replaces libfat. The storage adapter owns its FAT mounts,
preserves the existing custom drivers, allows late flashcard mounts and
remounts, and prevents NitroFS initialization from replacing them. Its small
interface to internal FatFS symbols is isolated in
`universal/sdmmc/arm9/source/blocksds_storage.c`. The corresponding headers
are pinned to the exact SDK ABI under `universal/include/blocksds/fatfs/`.
Compilation deliberately rejects a different SDK version until that ABI
has been reviewed.

The menu uses native NitroFS. A separate read-only `boot:` device reads
nds-bootstrap's embedded templates and loaders without replacing the menu's
NitroFS mount. FAT alias paths use the SDK's short-name API rather than
private libfat structures. Loader selection, custom emulator configuration,
argument ordering, compressed ROM handling, bootstrap RAM disks, and pico
launch paths remain in place.

The port also fixes an absolute-path bug in `.argv` launching (for example,
`fat:/probe.nds` previously became `fat:/fat:/probe.nds`), supplies a valid
fallback `argv[0]` in the common loader, and removes compiler-proven null
file closes encountered in the launch and Settings paths. Ordinary cold
boot now handles a missing or truncated optional soft-reset state file
without closing a null stream.

## Startup changes

The November 4 hardware-feedback revision also fixes these port regressions:

* The DSi menu queues its scroll-stop sound for the main thread. The debugger
  reproduced a deadlock in `mmEffectEx` called by VBlank: Maxmod was waiting
  for a FIFO reply while running inside an interrupt. This left the title
  panel unfinished and stopped navigation. After the fix, the title panel,
  clock and repeated scrolling work in DSi emulation.
* Icon banks keep their eight-frame texture and palette allocations. VBlank
  uploads only pixel/palette data, avoiding heap operations in the renderer.
  Initial unknown icons are read at their actual one-frame size.
* Splash OAM palettes are limited to 16 colors, preventing 256-color Grit
  output from overwriting neighboring palette banks. The DS Classic cursor
  palette is padded to 16 colors; the SDK's short byte-copy path cannot
  upload its previous three-color palette to VRAM.
* NitroFS initialization restores the card as the default filesystem.
  Settings additionally uses explicit device paths for bootstrap versions.
* Settings reads the initial backlight state from ARM7's PMIC, instead of
  ARM9's unrelated 3D-engine power bits. It restores the screens after lid
  reopening; the emulated lid cycle and subsequent input pass.

Browsing now reads each banner's actual version size: 2,112 bytes for an
ordinary DS banner instead of 9,152. Chinese/Korean titles, DSi animations,
NDZ banners and banner-save overrides are retained. Icon refreshes no longer
insert one full frame of waiting per icon. DS Classic transitions use a
16-frame audio fade and wait for the actual visual fade instead of a fixed
60-frame pause. The title skips its 25-frame fade wait when already faded
out, including splash-disabled cold boots. These are removed waits and I/O
counts, not measured hardware speedup claims.

* DSi/3DS and R4 startup no longer call `statvfs` on both devices just to
  determine cluster size. The warning reads the mounted filesystem's
  cluster size directly, avoiding a scan of the allocation table.
* Wood free-space queries run only when its free-space panel is visible,
  for the displayed drive. Cached values are invalidated after deletion.
* All three file browsers get FAT attributes during directory enumeration.
  They no longer inspect private libfat directory layouts or repeat a path
  lookup for every entry.
* Cursor movement, page changes and metadata loading take directory lists
  by reference instead of copying every filename and nested directory list.
* Photo rendering traverses rows directly instead of doing a software divide
  for every pixel. Dither order, transparency, LUTs and both debanding frames
  match the original renderer across 64 comparison cases. Invalid photos
  fall back to the default image.
* PNG textures are decoded through the already-open stream. BMP loading uses
  correctly sized fields, sequential rows, proper padding and orientation,
  and handles omitted image sizes. Invalid PNG/BMP overrides fall back safely;
  compressed GRF buffers round up for odd byte lengths.
* Shared filename checks use bounded `string_view` comparisons. Literal
  extension lists do not allocate vectors, and configured lists are passed
  by reference.
* INI files are read in 4 KiB chunks. A compact index stores line numbers
  sorted by key, replacing repeated full-file scans and line copies for
  every setting. Value updates retain the index, and insertions adjust its
  offsets. The parser retains duplicate-key precedence, section/key case,
  history delimiters and GBARunner's compact LF output. Read/write failures
  are reported, and missing empty defaults can be saved.

These remove identifiable I/O and allocation work. There is no claimed
percentage speedup or hardware boot-time measurement yet. SD speed, FAT
size, directory contents, custom skins, and splash settings all affect the
observed boot time. Measure cold boots on the same card and settings when
comparing against the original version.

## Measuring menu startup

`make profile-startup` builds a separate
`romsel_dsimenutheme/romsel_dsimenutheme-profile.nds`. It uses a separate
ARM9 object directory, so normal builds keep profiling disabled. Use this
binary as `_nds/TWiLightMenu/dsimenu.srldr` for either the DSi or 3DS theme.
After its first interactive frame it writes
`_nds/TWiLightMenu/startup-profile.csv`. Restore the normal SRL after measuring.

The CSV records phase and cumulative microseconds from the menu's ARM9
`main()`. It excludes the preceding booter, title program and SDK startup.
It includes the selected theme's usual fades and audio. Timestamps are
collected in RAM using ARM9 timers 0/1; CSV writes happen after capture,
and normal builds contain none of the profiling code. Disable ordinary
`LOGGING` when comparing load times. Compare the same card, directory,
theme, photos, music, fonts and settings.

In melonDS 1.1 DS/DLDI emulation, the default DSi photo phase fell from
about 199 ms to 149 ms, with all 32,448 photo pixels unchanged. The tested
menu interval fell from about 1.70 s to 1.62 s after the photo and PNG
changes. With the same directory, the 3DS theme's interval fell from about
1.27 s to 1.16 s, and folder navigation followed by `.argv` launching still
passes. These are emulator measurements of that interval, not end-to-end
or hardware boot-time claims. Native BlocksDS already initializes a NitroFS
cluster lookup cache; adding another did not improve these profiles and
has been omitted.

## Validation

`make test` runs host regression checks for the bootstrap filesystem,
malformed filesystem bounds, independent file handles, read-only behavior,
extension views, launch plans, argument ordering, explicit devices,
compressed/uncompressed SNES RAM disks, banner encoding, and R4/Ace3DS+
encoder compatibility vectors. INI checks cover duplicate keys/sections,
empty values, history, reloads, long lines, UTF-8 BOMs, GBARunner formatting,
and failed writes. Texture tests check BMP/PNG pixels and fallbacks, plus
all 150 built-in GRF assets. `make check-build` requires a completed SDK
build and checks all 13 applications, five flashcard variants, and seven
ARM9 loader images.

The port has been built from clean sources with `make -j6 all`. In melonDS
1.1, using an isolated FAT image and the supplied DS BIOS/firmware, both the
DSi and 3DS **themes** render and navigate. The complete flashcard
`BOOT.NDS` → `main.srldr` → `dsimenu.srldr` chain, including first-run
language and region selection, reaches the 3DS theme. Opening Settings and
returning works. The R4 and Wood interfaces also boot and list files, and
DS Classic returns to the 3DS ROM selector. A homebrew `.argv` launch receives:

```text
argc=3 FAT=1
0: fat:/probe.nds
1: from-argv
2: emulator-launch
```

Build the reusable probe with `make -C tests/nds-argv`, copy `probe.nds` to
the test FAT volume, and create `probe.argv` containing:

```text
fat:/probe.nds from-argv emulator-launch
```

The same probe was launched through a configured emulator entry, receiving
`fat:/probe.nds`, `emulator-check`, and `fat:/emulator-test.emu` in order.
On the DS ABI it also opened `boot:/load.bin` inside the official bootstrap
release while the menu's `nitro:` mount remained accessible, and opened a
long filename through the converted FAT short-name path.

The probe runs these optional storage checks when the bootstrap binary is
present at `fat:/_nds/nds-bootstrap-hb-release.nds`, the menu is available at
`fat:/menu.nds`, and a file beginning with `T` exists at
`fat:/Long Directory/Example file.txt`. It chooses `sd:` instead when an SD
volume is mounted. The extended checks above have passed in DLDI mode.
The rebuilt menu also passes folder entry/return followed by `.argv`
launching with recent-history updates enabled.

For melonDS, enable DLDI and provide the matching menu binary on the FAT
volume so NitroFS can open it. Menu SRLs use a title-style `SRLA` game code;
melonDS may classify them as retail images and omit DLDI patching. A
temporary emulator cartridge copy with game code `####` and a recalculated
header CRC allows the test; keep the production SRL and its FAT copy intact.
The tests above used DS emulation, not DSi or 3DS hardware.

DSi emulation has also been tested with an isolated copy of the supplied
Unlaunch-installed NAND and BIOS files. The full internal-SD `BOOT.NDS` →
`main.srldr` → DSi theme chain works, both with direct cartridge boot and
with Unlaunch 2.0 selecting `sdmc:/BOOT.NDS`. The latter requires an SD image
with an MBR partition table rather than mkfatimg's unpartitioned FAT volume.
NAND mounting and decryption work:
the menu discovers and copies Pictochat and Download Play from NAND and
selects a NAND donor title. Direct `.argv` launching from SD reaches the
BlocksDS probe with `argc=3`, `FAT=1`, `DSi=1`, and the expected `sd:/` path
and arguments.

That testing exposed two port bugs, now fixed: the custom ARM7 SD driver
must enable the SCFG peripherals that BlocksDS now enables in its own
TMIO driver, and a command can complete before polling observes its busy
bit. Polling now accepts the completion flags instead of waiting forever.
The title's MPU helper also had to target BlocksDS's uncached region 5;
overwriting region 6 caused a prefetch abort during DSi cold boot.

The supplied Mario Kart DS image launches through the complete flashcard
boot chain and the **unmodified official nds-bootstrap 2.17.0** retail
binary using melonDS DLDI mode. Its opening demo, menus, profile/save setup,
and a race with input have been exercised. The save file changed after
profile setup.

Internal-SD retail bootstrap reached its own initialization but stalled
before the game. A control boot of the official bootstrap without this
menu also stalled. Its custom SD driver retains the same no$gba-only
completion fallback described above; melonDS 1.1 completes these commands
without exposing the busy state. The external bootstrap binary has not
been patched; hardware-test archives bundle the unmodified release. Its homebrew build uses internal
DSi SD rather than generic DLDI, so DS-mode flashcard emulation cannot
validate that path. These emulator limitations do not establish how it
behaves on hardware.

Physical validation is still required for DSi SD/NAND timing and warm
reboots, 3DS SCFG and widescreen/reboot behavior, actual flashcards and
kernel autoboot, Slot-1, native GBA, and DSpico/pico-loader. No retail game
images, NAND images or BIOS files are included in the tests or repository.

The user has now confirmed retail nds-bootstrap and pico-loader gameplay
input on DSpico hardware. The first hardware archive exposed ALT boot white
screens through Pico-Launcher in DSi mode, DSi folder-scrolling freezes,
missing palette colors and the Settings issues described above. DSi-mode
ALT boot and browsing still need a hardware retest of this revision; an
emulator pass is not a substitute for that retest. The previously reported
emulator menu with a missing title panel was frozen, not a complete pass.
