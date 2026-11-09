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
