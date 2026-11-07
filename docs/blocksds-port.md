# BlocksDS port

This fork diverged from upstream at [`e3afe19ca`](https://github.com/DS-Homebrew/TWiLightMenu/commit/e3afe19ca).
Existing upstream fixes through `22979a2b8` are also merged;
`git diff 22979a2b8..master` shows the remaining fork changes.
It retains all live themes and launch methods. Wood and R4 have separate
binaries, so including them does not initialize them when using the DSi/3DS UI.
See [optimizations](optimizations.md) for performance work, its source files and measurements.

## Build this fork

Use **BlocksDS 1.24.0**, Wonderful Toolchain, GNU Make, Python 3 and a host
C/C++ compiler. SDK paths default to `/opt/blocksds/core` and `/opt/wonderful`;
override `BLOCKSDS` and `WONDERFUL_TOOLCHAIN` as needed. This version is pinned
because the storage adapter uses its internal FatFS ABI.

```sh
git clone --recursive https://github.com/Sanrax/TWiLightMenu-Blocks.git
cd TWiLightMenu-Blocks
make -j6 all
make package
```

`make package` stages binaries under `7zfile/`. As upstream does, download
[manual pages](https://github.com/DS-Homebrew/twilight-manual/releases/tag/pages)
into `manual/nitrofiles/pages/` before producing a complete distribution.
The Dockerfile and GitHub build workflows use `skylyrac/blocksds:slim-v1.24.0`.
The original devkitPro instructions in the upstream README are retained for
reference; the commands above apply to this fork.

## Port changes and where to find them

| Area | Change | Source |
| --- | --- | --- |
| Toolchain/build | BlocksDS/Wonderful tools, native library paths, dependency rules, ARM-state rules for copied loader routines, custom memory layouts and 32 KiB DLDI space. All live subprojects build; archived projects are untouched. | [blocksds.mk](../universal/blocksds.mk), ARM7/ARM9 component Makefiles, [ARM9 layout](../universal/arm9/ds_arm9_twlm.mem), Dockerfile and workflows. |
| FAT storage | Mount FatFS through the existing DLDI/SD/NAND drivers; retain late mounts/remounts and prevent native NitroFS from replacing these mounts. Replace private libfat aliases/attributes with SDK APIs. | [blocksds_storage.c](../universal/sdmmc/arm9/source/blocksds_storage.c), [pinned FatFS headers](../universal/include/blocksds/fatfs/), [fat_ext.h](../universal/arm9/include/fat_ext.h). |
| Embedded filesystems | Use native `nitro:` for menu assets and an independent read-only `boot:` for nds-bootstrap templates/loaders, with bounded paths, file tables and independent streams. | [systemdetails.cpp](../universal/source/common/systemdetails.cpp), [bootfs.c](../universal/source/common/bootfs.c), [legacy fallback](../universal/source/common/nitrofs.c). |
| ARM7/SDK interfaces | Adapt input, touch, clock, FIFO, Maxmod, AES/NAND types and GL uploads to native SDK interfaces while retaining custom ARM7 cores and launch engines. Correct OAM palette-bank sizes and pad short GL palettes to avoid missing colors. | Component `arm7/source/main.c`, `arm9/source/nand/`, [ARM7 helpers](../universal/arm7/), [splash palettes](../title/arm9/Makefile), [Classic cursor](../quickmenu/gfx/cursor.grit), [Texture.cpp](../romsel_dsimenutheme/arm9/source/graphics/Texture.cpp). |
| DSi SD/NAND | Enable accessible SD hardware before probing; use DMA word counts, wait for complete transfers, bound failed polls and keep the native IRQ from consuming polling flags. Use an uncached IPC mailbox and clear stale sync before CPU reset. Retain locked-access/iEvolution fallbacks. | [ARM7 SD driver](../universal/sdmmc/arm7/source/my_sdmmc.c), [ARM9 SD IPC](../universal/sdmmc/arm9/source/my_sd.c), [custom FIFO](../universal/sdmmc/arm7/source/my_system.c), reset callers in the shared/component loaders. |
| Power/sleep | Queue power requests for the main loop; reboot through firmware and serialize I2C transactions. Read real backlight state and restore Settings screens on lid reopening. Keep filesystem, formatting and allocation work outside graphics IRQs. | [powerButton.h](../universal/arm7/include/common/powerButton.h), [ARM7 I2C](../universal/arm7/source/my_i2c.c), [Settings ARM7](../settings/arm7/source/main.c), [graphics.cpp](../romsel_dsimenutheme/arm9/source/graphics/graphics.cpp). |
| Launch compatibility | Preserve bootstrap/pico-loader selection, configured emulators, NDZ, RAM disks, argument order and FAT aliases. Fix duplicated device prefixes in absolute `.argv` paths and provide fallback `argv[0]`. Detect flashcard/internal-SD installations explicitly. | [launcherArgs.cpp](../universal/source/common/launcherArgs.cpp), [launch executor](../universal/source/launch/launchExecutor.cpp), [nds_loader_arm9.c](../universal/source/nds_loader/nds_loader_arm9.c), [bootpath.h](../universal/include/common/bootpath.h), [flashcard entry](../booter_fc/arm9/source/main.cpp). |
| SDK file-ID compatibility | BlocksDS 1.24.0 omits device bits from direct NitroFS descriptors; add the NitroFS tag only for that version, with correct `fdopen` failure cleanup. The directory index itself is an optimization. | [nitroFile.cpp](../universal/source/common/nitroFile.cpp). |
| Supporting fixes | Handle absent/truncated soft-reset state, avoid null file closes, restore the card's default drive after NitroFS, use explicit bootstrap-version paths, and extend uncached MPU region 5 without replacing executable region 6. | [title main](../title/arm9/source/main.cpp), [cp15.s](../title/arm9/source/cp15.s), [Settings main](../settings/arm9/source/main.cpp), common launch/filesystem helpers. |

Normal `BOOT.NDS` retains its DS-compatible header; `BOOT_ALT.NDS` retains a
hybrid header and relocating ARM7 startup for DSi-mode flashcards. Runtime
storage detection alone cannot replace those header/startup differences.
Legacy autoboot wrappers retain their card-specific formats and addresses.

## Validation and boundaries

- melonDS 1.1: normal/ALT chains, DSi/3DS/Wood/R4 UIs, Settings/DS Classic
  round trips, lid recovery, argv/configured emulators, internal SD/NAND discovery,
  Unlaunch SD boot and retail bootstrap in DS flashcard mode.
- User hardware checks: DSpico normal/ALT menu operation, nds-bootstrap and
  pico-loader gameplay inputs, and successful ALT power-button reboot in DSi mode.
  Physical 3DS SCFG/widescreen, other flashcards, Slot-1/native GBA and remaining
  SD/NAND timing cases still need broader device coverage.

The DSi-mode flashcard Quit Game black screen is reported upstream in
[nds-bootstrap #1925](https://github.com/DS-Homebrew/nds-bootstrap/issues/1925).
This fork applies no menu-side workaround and does not modify bootstrap.

## Packaging

The release workflow builds the platform/add-on archives and rebuilt CIA when
a GitHub release is created. The upload job explicitly grants release-asset
write permission. Crowdin jobs run on upstream, or on forks that opt in via
the `ENABLE_CROWDIN` repository variable and supply their own secrets.
For a local hardware-test overlay, generate a CIA
from the current `booter.nds` using `booter/make_cia`, extract official bootstrap
(the tested release is **2.17.0**), then run:

```sh
python3 tools/package_hardware.py --bootstrap path/to/nds-bootstrap --cia booter/booter.cia
```

The tool checks CIA contents and archive integrity, bundles exact bootstrap binaries,
and records hashes/source provenance under `build/hardware-test/dist/`.
It takes no user settings, games, saves, NAND or BIOS as inputs. Emulator add-ons
remain separate. Preserve settings and saves when installing an overlay.

Agent regression tests, linked-image checks, boot diagnostics, startup profiling
and homebrew launch probes are maintained on
the [`diagnostics` branch](https://github.com/Sanrax/TWiLightMenu-Blocks/blob/diagnostics/docs/diagnostics.md).
They run manually there; production builds, packaging and GitHub Actions do not
invoke them. Production `master` retains the port fixes without test files,
runtime trace calls, SD snapshots or measurement targets.

Repository-only changes include the licensed vendored R4 encoder,
build/provenance tooling and ignored generated files. The initial fork removed
`AI-GUIDELINES.md`; that changes no runtime feature. The three existing upstream
commits on the destination fork are retained, including homebrew-return and
Mega Drive file-listing corrections. Loader padding after that merge uses
a byte offset within VRAM bank D; the diagnostics branch retains an argv probe
with B/exit support for testing return to the menu.
