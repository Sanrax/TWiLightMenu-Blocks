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

`make -C booter_fc diagnostic` builds an isolated hybrid entry point at
`booter_fc/booter_fc-diagnostic.nds`. Copy it beside the working entry as
`BOOT_TEST.NDS` and launch it through Pico-Launcher to investigate the DSi
flashcard boot failure. It displays startup stages and, after a filesystem
mount succeeds, writes `_nds/TWiLightMenu/boot-diagnostic.log` on the
flashcard (or on internal SD when no flashcard mount succeeds). Each run
replaces this diagnostic log. The normal build contains no stage logging;
diagnostic objects use a separate directory.
The `Power/navigation r4` diagnostic records the IPC mailbox, ARM7 SD access,
card presence and initialization result, followed by sector-read results,
continuation counts and the last controller/NDMA snapshot. Read traces stay
in RAM until the filesystem mount returns: writing a log from inside a disk
callback would reenter the shared FatFS sector cache. An unacknowledged IPC
request stops disk logging and leaves the diagnostic on screen for a restart.

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

The user confirmed that the r3 ALT and diagnostic entries boot and function
on DSpico through Pico-Launcher on a DSi LL with console SD inserted. Their
new log records successful SD initialization, sector reads, filesystem mount
and staged menu handoff. The remaining reported power-button failure was
reproduced in melonDS using a fixture that enables software power IRQs:
the old callback exits ARM7 `main()` into the SDK return-to-loader path and
produces the same red horizontal lines. Power requests now remain in ARM7's
main loop and invoke the existing firmware reboot routine. Under the same
fixture, the emulator records `ARM7 ReturnToDSiMenu` and `BPTWL: soft-reset`.
The callback only queues the request; the reboot and I2C status transactions
are protected against interruption by another I2C operation. Local I2C code
is retained for DS-mode callers that cannot execute SDK TWL-only functions.
The user has now confirmed that pressing the power button successfully
reboots from ALT on DSpico in DSi mode. Boot entries remain separate while
their header, startup and launcher compatibility is assessed for consolidation.

Folder, page and storage-device navigation in the DSi/3DS selector now fades
only the game/folder icon row while keeping the surrounding interface at
normal brightness. The old icons fade out before their banks are replaced;
the new row fades in after its metadata and title are ready. These paths no
longer display the loading label/progress screen. The animation uses nine
frames (about 150 ms, excluding file I/O) and continues servicing audio.
VBlank keeps servicing background work while the page is prepared. Moving
an app across pages uses the same publication step. Startup also skips the
loading label, spinner and progress bar: the first page and title are
prepared before fading directly into the menu. Launch effects and operation
dialogs retain their existing behavior.

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

The following optimization pass lets the loading-screen fade run in VBlank
while files are read, instead of waiting for the fade before starting that
work. The DSi menu also starts its icon drop immediately and uses the normal
fast fade to reveal the menu. In a melonDS DS/DLDI fixture with 48 homebrew
entries, the measured ARM9 `main()` to first interactive frame interval fell
from 2.03 s to 1.47 s. The drop animation continues after that frame. This
excludes the booter, title program and SDK startup, and is not a hardware
cold-boot measurement.

Browsing checks the custom-icon directory once per page and skips per-file
lookups when it is absent. It reads only the first 512 bytes of each NDS
header, retaining all fields used for browsing, and reuses the resolved
banner for `.argv` icons. The same fixture's first-page metadata phase fell
from about 405 ms to 355–358 ms. PNG custom-icon palettes are transformed
once after construction. Failed ARM9 signature reads now leave zeroes
instead of reusing the previous game's signature.

The next browsing pass initializes the page's inexpensive directory/type
flags, then loads banners only for the current seven-icon draw window
(fewer at either end). Cursor and touch navigation load each newly exposed
entry once, before its title, settings or launch flags are used. Directory
changes, page changes and app reordering invalidate these slot caches;
the carried app retains its separate cache. In-memory box art loads on
selection instead of preloading all 40 files. Reset slots also clear old
classification and animation state, and malformed NDS headers display an
unknown icon rather than a stale banner.

In the same 48-entry DS/DLDI fixture, the initial metadata phase fell from
about 358 ms to 88 ms, and the ARM9-main-to-first-interactive-frame interval
fell from 1.47 s to 1.24 s. Normal DS scrolling and DSi-mode 3DS-theme tests
cover unique per-entry titles/colors, an offscreen PNG custom icon, missing
argv targets, malformed headers, page round trips, app reordering, animated
icons, per-game settings and a successful argv handoff. Adjacent red/blue
box-art fixtures verify that the DSi in-memory cache follows selection.
These remain emulator results, not hardware cold-boot measurements.

Bundled translated "No photo" PNGs now also have a build-generated indexed
RGBA representation. The menu reads its palette/indices directly and computes
dithered and color-filtered colors per palette entry, while user photos keep
the PNG path. Missing, malformed or unsupported indexed data falls back to PNG.
The generated assets retain all 34 translations, alpha blending, odd-sized
photo centering, both deband frames and color LUTs; host tests compare every
output pixel with the original renderer. Their temporary pixel buffer is
about 32 KiB instead of the PNG renderer's 127 KiB RGBA buffer. The indexed
assets add about 1.1 MiB to NitroFS and remain generated/ignored build outputs.
A PNG with over 256 colors keeps the ordinary PNG path.

The same indexed RGBA format now covers all 84 bundled theme PNGs. Their
cache payload totals about 42 KiB. Small UI images convert colors once per
palette entry, retaining the PNG texture loader's opacity, magenta key and
color-LUT behavior. Only bundled NitroFS PNGs use this cache; custom skin
PNGs retain their existing loader, and GRF files keep their priority. Missing,
malformed or unsupported caches fall back to PNG. Pixel comparisons cover
all 84 images with and without a LUT, plus alpha thresholds, truncated data,
unavailable-cache markers and custom PNGs beside conflicting indexed files.
In a paired DS/DLDI fixture, the UI texture phase falls from about 67 to
29 ms and battery textures from 40 to 25 ms. Sprite uploads remain about
3 ms. These are emulator phase measurements, not hardware cold-boot times.

DS Classic now uses indexed RGBA for 13 ordinary bundled backgrounds,
calendar/clock images and status icons. Flat screen art uses bounded
run-length encoding when smaller than raw indices, reducing the additional
image data from 138,551 to 27,768 bytes. RGB555 conversion runs once per
palette color. Custom FAT PNGs retain PNG decoding, while the favorite-color
top bar and calendar marker keep their appended palette variants. Missing,
unsupported or corrupt caches fall back to PNG. Comparisons cover every
converted pixel and alpha value with and without a color LUT, both indexed
formats, malformed run lengths/indices, missing files, dimension limits and
custom PNGs beside conflicting caches. Fast Classic fades advance at the
DSi menu's existing rate; slow launch-effect timing is retained.

In one paired DS/DLDI direct-entry fixture, Classic ARM9 `main()` through
its first interactive frame falls from 1,132,792 to 564,432 microseconds.
Image changes alone reduce that interval to 815,166 microseconds; the faster
fade reduces its final wait from about 510 to 259 ms. These figures exclude
the preceding program handoff and are not hardware cold-boot measurements.

GRF files are now read sequentially through the existing stream, using
cached file-size metadata instead of seeking to the end and back. Malformed
or truncated GRFs fall back to the next supported skin format. GPU source
buffers include zeroed padding for the power-of-two dimensions uploaded by
gl2d, while logical image sizes remain unchanged. Short palettes are padded
to at least 16 entries for VRAM uploads, independently of the palette length
used to read favorite-color files. Tests compare all 150 bundled GRFs and
exercise malformed assets, upload padding and custom three-color palettes.
Custom folder/dialog GRFs also render in flashcard emulation; the DSi and
3DS interfaces and per-game dialog render from internal SD. These changes
do not have a new hardware startup-time measurement.

The same fixture's photo phase fell from roughly 166–182 ms to 82 ms, and
the first interactive frame reached about 1.14 s. Normal page loading now
continues its existing fade instead of fading back to white before revealing
the ready menu. Together these changes bring that interval to about 0.92 s,
compared with the preceding browsing update's 1.24 s. DS-mode folder entry and
page changes pass; DSi-mode testing covers a corrupt user photo falling back
to the default with debanding, touchscreen scrollbar navigation to an
initially unloaded entry, and subsequent lid reopening and navigation. A valid
180×120 user PNG with partial transparency also renders centered in DSi mode.
These figures still exclude the preceding booter, title and SDK startup.

Built-in sound banks load their launch sound and startup jingle on first
use. Navigation and lid sounds remain preloaded; custom banks retain the
memory interface to support skin paths longer than Maxmod's file loader
allows. The default bank initially needs 183,424 sample bytes instead of
the entire 435,500-byte bank, saving about 246 KiB before those deferred
effects are used. The timing probes show that sound initialization's earlier
roughly 265 ms phase included about 200 ms spent waiting for the loading
screen fade; the file-based bank alone did not shorten startup in this test.

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

Seven programs now share a linear translation-escape decoder instead of
rebuilding a string's prefix and suffix for every newline or button icon.
The decoder preserves UTF-8, upper/lowercase button escapes and direction
marks, and safely handles an empty input. Compatibility checks compare all
198 bundled translation INIs and all seven programs' default strings with
the original decoder, alongside generated malformed and mixed UTF-8 inputs.
In a paired DS/DLDI direct-entry fixture, the menu's language setup phase
fell from about 63 to 54 ms. This is that phase only; the direct-entry test
builds a cold font cache and is not comparable to the preceding full-chain
0.92 s menu interval or to a hardware cold boot.

The DSi/3DS browser, Settings, DS Classic, title, manual and image viewer
now prepare their bundled translations during the build. The shared
`universal/languagepacks.mk` compiles each program's `language.inl` defaults
and translation INIs into ordered `LNG1` files. Translators continue editing
the same INIs; changing an INI, the defaults or converter rebuilds the pack.
The runtime reads one packed allocation, validates the schema, offsets and
data hash, then assigns the strings. It shares the INI fallback across these
programs, preserving missing/blank defaults, duplicate-key precedence,
button/direction escapes and UTF-8. The title retains its explicit language
override and live language selection. The small 3DS splash has no bundled
translation INIs and keeps its original loader and blank-string behavior.

Host checks compare 23,093 strings across all 198 bundled INIs and additional
fixtures, and verify that invalid packs leave the outputs untouched before
falling back. Missing, truncated, old-schema, damaged and out-of-bounds packs
are exercised; ASan/UBSan checks also pass. The largest bundled pack is
30,057 bytes, including its header/table, and its temporary allocation is
released after language setup. The INIs remain in NitroFS for fallback.

In paired DS/DLDI direct-entry tests, English language setup falls from
about 54 to 8 ms in the browser and from 98 to 12 ms in Settings. Japanese
falls from 57 to 10 ms and from 103 to 14 ms respectively; Arabic falls
from 58 to 10 ms and from 108 to 16 ms. All seven tested menu/Settings
bottom-screen comparisons are pixel-identical. Missing/old packs also reach
the menu through the INI fallback on the console build. First-run language
selection, live preview, saving and menu handoff, and the reset prompt's
saved-language override pass in the emulator. These are emulator phase
measurements, not hardware startup claims. Ordered tables also reduce the
linked ARM9 sizes by about 30 KB for the browser and 23 KB for Settings;
the NitroFS images grow because the original INIs are retained.

The DSi/3DS browser also caches formatted clock/date text for each RTC
second. It shares one calendar conversion between the date and time, and
returns references to the cached strings. Colon blinking still updates on
the existing VBlank schedule. Changed formats, 12/24-hour selection, RTC
jumps and resuming after sleep refresh the text; formats containing seconds
remain supported. Empty or overlong formats produce bounded empty text.
Host checks cover midnight, leap day, backwards time changes, DST and format
changes; ASan/UBSan checks pass. Console captures cover blinking, midnight
and subsequent lid recovery in DSi UI, DSi-mode 3DS UI and macro mode.
Clock-row copies also skip their scanline wait while master brightness
fully hides the destination, retaining the wait for visible updates.

A DS/DLDI ARM9 benchmark of 1,000 paired clock/date updates takes 181,807 µs
with the previous code and 6,492 µs with the cache. This excludes drawing and
filesystem I/O. Directory scanning calls the same background operations for
every entry. With 500 NDS files in a matching emulator fixture, the measured
directory phase falls from 185,460 to 99,313 µs in the DSi UI and from
185,517 to 99,244 µs in the 3DS UI. Their full menu initialization intervals
fall from 917,045 to 817,317 µs and from 832,960 to 750,832 µs respectively.
The smaller DSi fixture's directory phase falls from 17,843 to 8,768 µs.
These are emulator measurements; initialization excludes the preceding
bootloader/title handoff and does not predict hardware cold-boot timings.

All eight font implementations now share their NFTR character-map reader
and bounded glyph lookup. The reader batches explicit and indexed mappings
with one 512-byte scratch buffer, retaining all three map formats and the
existing glyph cache. The initial reader change reduced the two large
default fonts to 266 map `fread` calls each, from roughly 36,200. All 19 bundled fonts produce the same maps
as the original reader. Checks also cover missing characters, unmapped
entries, buffer boundaries, linked blocks, truncated reads and invalid map
indices; a missing glyph no longer reads one element beyond the map.

In the same DS/DLDI direct-entry fixture as the translation comparison,
cold font initialization falls from about 651 to 112 ms, and menu ARM9
`main()` to the first interactive frame falls from 1.416 to 0.882 s. The
fixture uses the full external default fonts. These paired figures exclude
the booter, title and SDK startup and remain emulator measurements.

Font/startup r8 repacks only the bundled Default fonts' character maps,
using the existing direct, indexed and explicit NFTR encodings. Each font
now uses 29 map blocks and 165 map reads, saving 19,536 bytes per file.
Glyph bitmaps, spacing, font settings and every mapping retain their
original identities. `tools/compact_nftr.py` performs this conversion;
custom fonts continue through the normal NFTR reader. In the DS/DLDI
fixture, this reduces cold font setup from 113 to about 95 ms in the
browser, and from 105 to about 87 ms in Settings.

DSi and debug-RAM font caches now populate glyphs in 512-byte file pages
as displayed text needs them, rather than eagerly reading over 2 MB.
The reserved capacity stays the same, plus 572 bytes of headers/validity
bits for the two Default fonts. Populated glyphs and validity survive menu
handoffs; a distinct cache tag prevents confusion with the old full cache.
Old fully populated cache entries remain readable. All six common-cache
consumers use the shared pager, and the smaller DS tile cache is retained.
Font buffers track ownership separately, and the image viewer now uses
the large-font cache tag for its large font. Font selection in Settings
continues to invalidate the common cache.

With the compact fonts in both cases, DSi/DLDI direct-entry measurements
show the glyph pager reducing browser font setup from 569,200 to 78,154
microseconds, and its main-to-ready interval from 1,042,167 to 557,210
microseconds. Settings font setup falls from 561,731 to 69,447 microseconds,
and initialization from 640,109 to 150,305 microseconds. These are matching
English emulator fixtures, excluding the preceding title/loader/SDK work;
they are not physical-console or full cold-boot measurements. Page boundary,
partial-read/retry, reset/reuse and full glyph identity tests cover all 19
fonts, including glyphs crossing sectors. Unread or failed pages are never
rendered. Page misses run on the main thread, outside graphics interrupts.
English, Japanese and Arabic browser/Settings bottom-screen captures are
pixel-identical with eager versus paged glyphs. Warm DSi common-cache
handoffs initialize Settings in about 88 ms and the returning DSi browser
in about 495 ms; the Arabic 3DS-theme case measures 98 and 444 ms. These
phases exclude the loader handoff. Custom-font DS Classic, Settings lid
reopening, return and subsequent navigation have also been exercised in
DS and DSi modes.

Audio/startup r9 replaces the browser's two copying PCM buffers with a
circular queue of the same 192,000-frame / 384,000-byte total capacity.
Startup fills at least two seconds (three at the default 16 kHz), then
main-thread background operations populate the remaining capacity in
bounded 4,096-frame reads. The Maxmod interrupt copies published PCM only;
it performs no file I/O and emits silence if data is unavailable. A split
allocation fallback retains the old two 192,000-byte allocation sizes on
fragmented heaps. Frame publication masks interrupts only for the queue
pointer/count update, keeping slow file reads outside the critical section.

One reader handles intro-to-loop transitions, short intros, repeated loop
boundaries and headerless decoded ADPCM caches. This also removes the old
short-intro byte/sample offset error. Stopping for sleep retains queued
audio and playback position; unloading closes both source files and frees
the queue even when playback has already stopped. Existing ARM7 fades and
the startup-jingle delay are retained. The existing 16-bit mono and paired
8-bit stereo PCM layouts are copied without conversion.

Matching DSi/DLDI measurements reduce audio initialization from about 88 to
24 ms and browser initialization from about 557 to 489 ms. The DS/DLDI
fixture measures about 92 to 25 ms for audio and 682 to 614 ms overall.
Music-disabled controls remain unchanged. These exclude the preceding
loader/title/SDK startup and are emulator phase measurements. Host checks
cover exact sample order, both allocation layouts, queue wraps, IRQs before
publication, delays, underruns, a five-second refill pause after warmup and
transient read failures. Direct SDL output verifies built-in music, short
custom mono/stereo intros, raw caches, navigation and music recovery after
lid reopening. PipeWire per-stream recording returned silence for both r8
and r9 controls, so it was not used as playback evidence.

Asset-lookup r10 indexes immutable NitroFS graphics directories once, then
opens matching textures and palettes by the native file ID. Sorted compact
name/ID tables also avoid repeated path traversal for missing extensions or
palette directories. External custom skins retain ordinary stdio and the
same format/override precedence. The cache is bounded to twelve directories,
128 files and 4 KiB of names per directory; oversized or unsupported
directories fall back to normal stdio. Remounting NitroFS clears it.

The pinned BlocksDS 1.24.0 direct-ID API omits descriptor device bits that
its normal open path adds. The helper supplies the NitroFS device tag only
for that SDK version and closes the descriptor if fdopen fails. Without
this correction native stdio dispatches reads to the wrong device. Host
checks cover native IDs, independent streams, case/UTF-8 names, missing
files/directories, bounds, remounts, transient errors and descriptor cleanup;
ASan/UBSan checks pass. Texture access remains on the main thread.

Matching r9/r10 melonDS direct-entry tests reduce DSi-theme graphics setup
from 173 to 69 ms and initialization from 489 to 389 ms. The DSi-mode 3DS
UI measures 147 to 64 ms for graphics and 440 to 356 ms overall. DS-mode
DSi UI measures 183 to 94 ms and 614 to 514 ms respectively; its 3DS UI
initializes in 463 rather than 532 ms. Macro mode and valid external PNG,
GRF and custom-font skins also improve. All seven paired bottom-screen
captures are pixel-identical. These figures
exclude preceding loader/title/SDK startup and are emulator measurements,
not hardware cold-boot timings.

These remove identifiable I/O and allocation work. There is no claimed
percentage speedup or hardware boot-time measurement yet. SD speed, FAT
size, directory contents, custom skins, and splash settings all affect the
observed boot time. Measure cold boots on the same card and settings when
comparing against the original version.

## Measuring menu startup

The small ARM7 loaders now share a dedicated 512-byte FAT-sector cache,
separate from their file/directory buffer. It is invalidated on every mount
and on a failed FAT read. This avoids reading the same FAT sector again
for each cluster link while loading the next menu. All six implementations
are exercised by host tests with FAT12/16/32, contiguous and fragmented
chains, partial sectors, remounts, EOF and injected I/O failures. A contiguous
100 KiB fixture needs one FAT sector read on FAT12/16 and two on FAT32,
compared with 398 on FAT12 and 199 on FAT16/32 before the change. The
reader also normalizes FAT12 end markers and stops on broken chains and
failed data reads.

Those six loaders now also use one shared file reader. It follows actual
FAT links to combine adjacent clusters into transfers of at most 128 sectors
(64 KiB), the largest transfer previously used for one cluster. Fragmented
chains end each run. Drivers that reject a batch are retried one sector at
a time, and unaligned destinations use an aligned sector buffer. Tests cover
512-byte through 64 KiB clusters, mixed contiguous/fragmented runs, transfer
boundaries, unaligned reads, single-sector-only drivers and partial failures.
For the contiguous 100 KiB fixture with 512-byte clusters, file-data commands
fall from 200 to two; FAT metadata counts above are unchanged.

DS Classic can be profiled with `make -C quickmenu profile-startup`. Install
`quickmenu/mainmenu-profile.nds` as `_nds/TWiLightMenu/mainmenu.srldr`.
Its `_nds/TWiLightMenu/classic-startup-profile.csv` covers ARM9 `main()`
through the first interactive frame, with phase markers for graphics,
fonts, language, backgrounds, calendar, clock and fade. It uses timers 0/1;
normal builds contain neither the measurement code nor CSV writes.

The title can be profiled separately with `make -C title profile-startup`.
Install `title/title-profile.nds` as `_nds/TWiLightMenu/main.srldr` in an
existing installation. Its `_nds/TWiLightMenu/title-startup-profile.csv`
covers title ARM9 `main()` through its menu handoff decision/fade, ending
before the loader runs. It uses ARM9 timers 2/3 so a GIF splash can continue
using timer 0. Normal title builds contain no profiling code. With both
splashes disabled, the configured DS/DLDI fixture takes about 121 ms in this
interval. The original title loader's subsequent binary-read phase takes
about 1.15 s in a separate instrumented run. With the dedicated FAT cache,
the same binary-read phase takes about 0.40 s (1,146,944 → 399,183 µs) for
the same target image in DS/DLDI emulation with 512-byte clusters. The shared
batched reader brings this phase to about 393 ms. In DSi internal-SD emulation,
the cached and batched readers take 244 and 214 ms with 512-byte clusters,
and 145 and 143 ms with 32 KiB clusters. Thus batching adds little in the
larger-cluster fixture; these results do not predict the gain on a particular
flashcard. This excludes storage mounting,
memory reset, SDK startup and menu initialization, and is not a hardware
cold-boot measurement. The normal flashcard boot chain, DS Classic round
trip, Settings handoff/return and Settings lid reopening also pass. Internal-SD
tests also reach both retained UIs through the hybrid and normal SD entry
points, change themes through Settings, and complete the DS Classic round
trip in DSi mode. The argv probe still receives all three expected arguments
after the app-loader handoff, with a PNG custom icon and color LUT enabled.
The batched reader also passes the normal flashcard boot and DS Classic
round trip. Its 32 KiB-cluster internal-SD test covers hybrid entry, both
retained UIs, DS Classic, Settings return, lid reopening and subsequent input.

The DSi/3DS menu now skips the background-copy scanline wait while the
destination display is fully hidden by master brightness. Visible updates
still wait for their existing safe scanline. The initial top-screen draw
also reuses the background decoded by `loadBackgrounds()`, before photos
and status graphics modify it. In the same DS/DLDI fixture, these changes
reduce the menu interval from 815,379 to 748,875 µs for the DSi UI and from
748,892 to 682,390 µs for the 3DS UI. A custom top PNG takes 715,634 µs
instead of 764,536 µs. These measurements exclude the preceding handoff.

In macro mode, VBlank background changes now queue status invalidation.
The main loop redraws the profile, clock, battery and volume indicators.
Previously the IRQ called clock formatting and font/file operations directly;
BlocksDS reported `Using TLS from IRQ` and the menu crashed. The baseline
crash and corrected startup, input and lid recovery were reproduced in
the DS/DLDI emulator fixture.

Settings now uses the same fast screen/audio fade as DS Classic when
entered from the DSi/3DS browser, replacing its unconditional 60-frame wait
with 16 frames plus any remaining visual fade. Exiting Settings overlaps
the existing music and screen fades, saves all three settings files, then
hands off immediately. It avoids two unchanged-screen redraws and the
30-frame post-save wait. In the DS/DLDI fixture, Settings' exit code takes
446,887 µs instead of 1,437,509 µs, before its exit callback and loader.
Settings initialization in that r5 fixture was about 339 ms. These are emulator phase
measurements, not hardware or full round-trip timings.

Use `make -C settings profile-startup` to build the isolated
`settings/settings-profile.nds`. Installed as `settings.srldr`, it writes
`settings-startup-profile.csv` after its first prepared frame and
`settings-exit-profile.csv` just before the normal exit callback. Both files
are under `_nds/TWiLightMenu/` on the launch device. The exit profile covers
the screen/music fades and saving, and excludes loading the destination
menu. Normal builds contain no CSV capture or output.

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

Subsequent hardware feedback confirms that the normal flashcard `BOOT.NDS`
works without the previously reported menu regressions. The user also
confirmed that r3 `BOOT_ALT.NDS` and the diagnostic entry boot and function
through Pico-Launcher on DSpico in DSi mode. The user has also confirmed the later r4
power-button correction: ALT now reboots successfully on that hardware. The
two production entry files remain separate pending a compatibility audit
for consolidation.

The supplied DSpico `BOOT_TEST` log from a DSi LL with internal SD inserted
confirms ARM9 entered DSi mode and
mounted the DSpico DLDI filesystem. Its last stage is `Mounting internal SD`,
before `main.srldr` is loaded. This narrows the failure to the SD mount; it
does not identify the individual IPC or controller wait within that mount.
Pico-Loader can disable ARM7 SD access according to the target header. The
legacy driver checked card presence before enabling that access, although
it enabled access later during controller initialization. The driver now
enables accessible SD hardware before probing it, returns unavailable for
locked access, and checks the actual card-presence signal. Controller busy
and synchronous command polls, SD/NAND power-up retries, and ARM9 startup
reply waits are bounded. The iEvolution DLDI fallback is retained. ARM9 now
selects an uncached mailbox from the active RAM mapping and SDK aliases
instead of probing an address by writing to it. This also covers unlocked
DS-mode launchers that enable extended RAM after SDK startup.
The revised diagnostic distinguishes presence and initialization
failures and logs the ARM7 access/status registers. The subsequent r3 hardware run described below confirmed ALT/TEST boot
and operation after the additional transfer and interrupt corrections.

A separate DSi-mode DLDI emulator test with internal SD absent exposed a
reset race after that mount: the SD presence request left IPC sync value 1
asserted. The SDK reset receiver could acknowledge this stale value and
reset ARM7 before ARM9 observed the reply. ARM9 then waited in `resetSync`
while the ARM7 loader waited for ARM9. All five ARM9 reset callers now clear
the application IPC sync value immediately before calling `resetARM7`.
The previously stalled probe now receives `DSi=1`, a mounted flashcard and
`fat:/_nds/TWiLightMenu/main.srldr` as its argument. This verifies the reset
correction in emulation. The subsequent DSpico r3 hardware run confirms
that the complete menu handoff now succeeds.
The rebuilt full title/menu chain also boots in DSi-mode DLDI emulation
with internal SD absent, including scrolling, per-game settings, DS Classic,
Settings return and Settings lid reopening. With both storage devices
present, the r2 log records a successful internal-SD mount, selects the
flashcard menu program, and the menu opens the game folder. Internal-SD-only
testing covers both retained interfaces and the same Settings/DS Classic
round trips. These fixtures use melonDS's DLDI driver and do not emulate
the DSpico cartridge protocol.

The DSpico / DSi LL hardware retest of r2 still white-screened. Its log
confirms that card presence and SD initialization succeed, then stops before
the filesystem mount returns. The r3 revision corrects the subsequent read
path: NDMA's total transfer length counts words, while the old code supplied
bytes, requesting four times the destination size. Completion now requires
both SD status and an inactive DMA channel. A PIO read likewise waits until
all requested bytes have been drained, even if DATAEND arrives earlier.
Read submission returns immediately instead of spinning for a busy flag
inside the ARM7 interrupt handler; continuation polls have a two-second
scanline deadline and an independent polling bound, and failures stop DMA.

BlocksDS `irqInit()` also installs a native TMIO interrupt handler which
clears response/data flags. The custom polling driver now disables that
SDMMC interrupt when it installs its IPC handler, retaining WiFi SDIO and
power-button interrupts. With r3 DMA configuration but before this interrupt
correction, melonDS read the MBR successfully then timed out on partition
sector 2048, with DMA inactive and the completion flags cleared. After the
correction, reads of sectors 0, 2048 and 2049 succeed and both storage devices
mount. The later DSpico hardware log confirms that both reads succeed, the
internal SD mounts, and the menu handoff completes. ALT and TEST boot/function
are now hardware-confirmed, as is the subsequent power-button correction
described above.

Both flashcard entry files contain the same ARM9 program. The normal entry
uses a DS-only 0x200-byte header and an ARM7 image loaded into IWRAM. ALT
uses a hybrid DS/DSi header, a relocating ARM7 startup image in main RAM,
and TWL segments. Pico-Loader selects DS compatibility mode when a target
header does not advertise DSi support. Thus successful normal entry from a
DSi-mode launcher does not prove that the menu retains DSi mode. A single
main entry requires a working hybrid startup as well as runtime storage
selection. Legacy flashcard autoboot wrappers may still need their existing
format and address variants; internal-SD and installed-title entry behavior
also needs to be retained. The entry files have not yet been consolidated.

The shared flashcard booter now accepts an internal-SD-only installation
instead of rejecting it because the flashcard mount failed. It keeps the
SD primary marker, then honors an explicitly supplied SD launch path, and
falls back to an available installation. A DSi handoff now reuses the four
already staged CPU binaries rather than reading the image again after
resetting storage. The same hybrid code passes a DS/DLDI launch probe
(`DSi=0`, `fat:/` argument) and an internal-SD-only DSi probe (`DSi=1`,
`sd:/` argument) in melonDS. The complete hybrid entry → title → DSi menu
chain also boots and scrolls from internal SD. These emulator fixtures do
not emulate the DSpico cartridge protocol. The subsequent user hardware
runs confirmed ALT/TEST boot and the power-button correction; the separate
diagnostic entry remains available for investigating other boot failures.

See the [BlocksDS hybrid-ROM compatibility notes](https://blocksds.skylyrac.net/docs/guides/legacy_support/#23-nds-dsi-hybrid-roms)
and [filesystem/device selection guide](https://blocksds.skylyrac.net/docs/guides/filesystem/).

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
ARM7 storage tests run the actual driver against scripted controller
registers: disabled/locked access, card absence/removal, SDHC and legacy SDSC
initialization, controller stalls, missing replies, failed power-up
commands, bounded SD/NAND retries and the iEvolution fallback. Sanitizer
checks also cover unsigned assembly of high-bit SD response words. Transfer
tests cover exact DMA word counts and destination guards, early DATAEND,
delayed PIO readiness, CRC failures, stalled DMA cancellation, and retaining
unrelated interrupts while taking ownership of SD completion flags.

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
missing palette colors and the Settings issues described above. The user subsequently confirmed
ALT boot and menu operation with r3 and successful power-button reboot
with the r4 correction. New navigation and optimization changes still need
hardware feedback; their emulator checks do not substitute for that testing. The previously reported
emulator menu with a missing title panel was frozen, not a complete pass.
