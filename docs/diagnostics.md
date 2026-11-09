# Diagnostics branch

This branch carries instrumentation as an overlay on production `master`.
Release fixes and optimizations remain on both branches. Agent regression tests,
linked-image checks, timing markers, boot/SD traces, emulator debug output,
measurement records and the homebrew launch probe live here. `master` contains
no test suite or instrumentation; production error handling remains on both.

## Build

Use BlocksDS 1.24.0 and the same dependencies as the [port build](blocksds-port.md).

```sh
git switch diagnostics
make -j4 all
make -C booter_fc diagnostic
make -C title profile-startup
make -C quickmenu profile-startup
make -C settings profile-startup
make -C romsel_dsimenutheme profile-startup
make -C tests/nds-argv
```

## Manual agent checks

Run these explicitly on this branch when checking changes:

```sh
make test
make check-build
```

`make test` runs host checks for I2C/power, FAT loaders, launch
arguments, INIs/translations, fonts, textures/photos, music queues and encoding.
It requires a host C/C++ compiler and Python, without the SDK or console assets.
`make check-build` requires `make all` outputs and checks 18 NDS headers, DLDI
allocations, palettes, MPU setup, IRQ sound dispatch and ARM-state routines in
seven ARM9 loaders. Builds, packaging and GitHub Actions on either branch do
not invoke these agent checks.

## Startup/exit measurements

Install the chosen instrumented binary in a separate test installation:

| Build output | Install under `_nds/TWiLightMenu/` | CSV written there |
| --- | --- | --- |
| `title/title-profile.nds` | `main.srldr` | `title-startup-profile.csv` |
| `quickmenu/mainmenu-profile.nds` | `mainmenu.srldr` | `classic-startup-profile.csv` |
| `settings/settings-profile.nds` | `settings.srldr` | `settings-startup-profile.csv`, `settings-exit-profile.csv` |
| `romsel_dsimenutheme/romsel_dsimenutheme-profile.nds` | `dsimenu.srldr` | `startup-profile.csv` |

The shared collector in `universal/source/common/startupprofile.cpp` captures
phase timestamps in RAM and writes CSV only after the interval ends. Normal
builds do not enable this collector. Recorded timings are emulator phases,
not hardware cold-boot measurements; compare matching settings/assets and
keep separate optimization baselines separate. See [retained phase data](benchmarks/startup.json).

## Boot/storage traces and argv probe

Copy `booter_fc/booter_fc-diagnostic.nds` as `BOOT_TEST.NDS`. It displays stages
and writes `_nds/TWiLightMenu/boot-diagnostic.log` after storage mounts.
The legacy IPC Sync driver and its register snapshots were removed when
production switched to native BlocksDS storage. Do not restore that driver
merely to collect logs; inspect the SDK FIFO/TMIO path with a debugger instead.

`tests/nds-argv/probe.nds` records launch arguments and optional filesystem
checks in `argv-result.txt`. Press B to exercise normal homebrew exit back to
the menu. It is separate from nds-bootstrap's retail-game quit path.

Restore production binaries after testing. To maintain this branch, merge
new production commits from `master`, retain the instrumentation overlay,
and keep new timing/trace hooks out of `master`.

## Native SD control (r13)

`make -C tests/native-sd-control` builds a small SD write/readback control with
BlocksDS 1.24.0's unmodified default ARM7. It writes only
`sd:/native-sd-probe.bin`; use a disposable SD image. In the local melonDS 1.1
DSi setup it mounts successfully but stalls at `Mounted; writing...`, waiting
for native CMD24 completion, as does the production menu. This is a controlled
emulator observation, not a hardware result. Never bundle BIOS, NAND or ROMs.

`tests/inisave.cpp` injects a short write and a failed publish rename into the
actual INI implementation, checking original preservation, rollback and backup
recovery. `tests/check_build.py` checks linked native storage ownership and the
copied white-screen handoff routines. Set `TWLM_BUILD_ROOT` to a separately built
production checkout to audit its outputs without rebuilding instrumentation.

## Browser white-screen trace (r14)

`make -C romsel_dsimenutheme trace-startup` produces
`romsel_dsimenutheme-trace.nds`. Install it as
`_nds/TWiLightMenu/dsimenu.srldr` over an r14 test installation.
After 10 seconds in a pending SDK sector transfer, or 20 seconds without
a startup milestone, a terminal screen shows the phase and pending I/O.
It draws directly through the console API without libc stdio/TLS in the IRQ,
and writes no SD log. Normal builds do not enable it. Keep the production
`dsimenu.srldr` to restore after collecting the result. The watchdog needs
ARM9 VBlank interrupts to keep running; an earlier loader or IRQ stall may
still remain white.

`make -C tests/startup-trace-selftest` builds a simulated SD-wait control to
check the actual watchdog display independently of storage.
`tests/nand.c` tests the production readers with separate cached/uncached
reply views, missing acknowledgements, bad MBR data and boundary reads.

The r14 hardware result was `sound` without a pending I/O line. This milestone
is after sound initialization, before directory listing. The r15 correction
on `master` restores SD change events instead of reporting the initial card
level as an insertion. `tests/sdstatus.c` checks inserted/absent startup,
fresh module baselines, real removal/reinsertion, latched events and inaccessible
hardware. The r15 trace adds directory/background subphases and an explicit
`SD removal error screen` marker. It can be installed over normal r15, then
restored with the production browser after collecting a result.

## Emulator process ownership

Do not close melonDS by injecting desktop-wide Ctrl+Q or Alt+F4. In the local
Wayland/XWayland setup, Ctrl+Q intended for the emulator coincided with a clean
VS Code shutdown at 19:15:50 on 2026-11-08, without a new crash dump. An X11
focus request is not sufficient protection against directing input elsewhere.
The user subsequently authorized GUI testing, excluding quit keyboard commands.
The local helper now accepts only individual DS gameplay/navigation keys and
rejects modified shortcuts and save-state keys. Cleanup must verify
the recorded PID, executable/arguments and process start time before signalling
the owned emulator. Keep BIOS/NAND copies and SD fixtures isolated and remove
them after the process has exited. The r15 native-SD emulator attempt did not
reach a verified menu and is not counted as a pass.

## Bootstrap handoff control (r16)

The user confirmed that r15 loads the console-SD/Unlaunch menu and survives
Settings changes/return. Mario Kart still white screens; a fresh hardware log
ends at `Header CRC is valid!`. Keep that unresolved separately from the fixed
startup SD-event regression.

`make -C tests/bootstrap-handoff` builds a small default-SDK-ARM7 control using
the production shared NDS loader. See its [instructions](../tests/bootstrap-handoff/README.md).
It starts the existing release bootstrap/INI without menu UI, custom NAND
discovery, settings writes or exit-bootstub installation. Compare it on the
same console/card with the official bootstrap selected directly in Unlaunch.
It is a launch control, not a Quit Game test or a production replacement.

Local melonDS 1.1 observation with a disposable native-SD image: both the
official bootstrap 2.17.0 launched directly and the minimal BlocksDS loader
control stop before header validation in the same ARM7 SD command-start wait.
Status is `0x000000A5`, with CMD_BUSY already clear; the driver's nonblocking
NDMA routine waits to observe CMD_BUSY unless it detects no$gba. This emulator
result does not reproduce the hardware's later stopping point.

An additional debugger-only experiment replaced the conditional at
`0x06020D4E` (`05 D1`) with a Thumb NOP (`C0 46`) in the already loaded
`loadi0` image. That lets the routine check an already completed command on
melonDS. It advanced through header validation, module parameters, decompression
and patching to game entry; the in-game card engine later hit the same kind of
wait. This is not a game-play pass. No bootstrap file or production source was
patched, and the hardware control must use the unchanged official binary.


## Bootstrap phase controls (r17)

Hardware: the user confirmed that r16's minimal control also produces a fresh
log ending at `Header CRC is valid!`, and that `_nds` is wiped for each build.
This reproduces the failure without menu UI, NAND discovery, settings writes
or exit-bootstub installation; an inherited cache is not the explanation.

`tests/bootstrap-handoff/variants.py` makes two explicitly modified diagnostic
copies from the pinned official 2.17.0 binary. It patches only `loadi0` and
refuses other inputs. Original instructions, LZ10 round-trips, frontend/DSi
segments, NDS header CRC and permitted metadata changes are checked. Per-copy
JSON manifests describe the exact changes. This tool is never invoked by
production builds, packaging or CI; release bootstrap binaries stay unchanged.

The rebuilt selector uses X for CPU SD reads during game/header loading and
Y for an unconditional terminal checkpoint after the patch-cache header read.
Y logs one ARM9 state twice as eight hex digits and never boots a game.
See the [probe README](../tests/bootstrap-handoff/README.md) for interpretation.
The checkpoint does not depend on whether the patch cache already exists.

Local CPU-copy test: it advanced past header validation, module parameters,
decompression and game patching without debugger changes, then stalled in a
later NDMA read during additional ROM loading. This validates the selected
CPU path but is not a hardware fix or a gameplay pass. Keep hardware results
separate from the earlier emulator command-start timing issue.

State-copy validation: after bypassing the previously identified emulator
command-start wait in RAM only, ARM7 stops at `0x06008512`, ARM9's state word
is 3, and the SD log gains `00000003`. The state is logged twice so an emulator's
buffered final image write cannot hide its only copy on forced process cleanup.
This validates the checkpoint, not the hardware state or a game launch.
