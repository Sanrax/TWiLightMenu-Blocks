# Diagnostics branch

This branch carries instrumentation as an overlay on production `master`.
Release fixes and optimizations remain on both branches. Timing markers,
boot/SD traces, emulator debug output, measurement records and the homebrew
launch probe are kept here; production error handling and regression tests
remain on `master`.

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
ARM9 keeps SD-read traces in RAM until disk callbacks finish; ARM7 provides
controller/DMA snapshots. An unacknowledged IPC request stops disk logging.
The normal timeout and transfer protections are also present on `master`.

`tests/nds-argv/probe.nds` records launch arguments and optional filesystem
checks in `argv-result.txt`. Press B to exercise normal homebrew exit back to
the menu. It is separate from nds-bootstrap's retail-game quit path.

Restore production binaries after testing. To maintain this branch, merge
new production commits from `master`, retain the instrumentation overlay,
and keep new timing/trace hooks out of `master`.
