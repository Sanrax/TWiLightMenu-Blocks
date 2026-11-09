# Bootstrap handoff control

Build explicitly with `make -C tests/bootstrap-handoff` on `diagnostics` using
BlocksDS 1.24.0. The makefile builds the current shared menu loader first; it
does not use a prebuilt blob or modify production build routines.

Copy `bootstrap_handoff.nds` to the console SD as `BOOT_HANDOFF.NDS` and launch
it through Unlaunch in DSi mode. It uses the SDK's default ARM7, native SD
mount, and the production `runNdsFile`/VRAM loader to start
`sd:/_nds/nds-bootstrap-release.nds` when A is pressed. It uses the existing
bootstrap INI, so first select the failing game in TWiLight. It neither writes
that INI nor loads menu assets, custom NAND discovery, music or settings.
Bootstrap itself retains its normal save/cache/log writes.

The probe omits installation of TWiLight's exit bootstub, so it tests launch
only, not Quit Game. If bootstrap returns a loader error it stays on the
console screen. It deliberately makes no attempt to recover or rewrite caches.

An optional comparison is the same official bootstrap selected directly in
Unlaunch. For this comparison, check which binary supplies NitroFS: 2.17.0
falls back to `/_nds/nds-bootstrap-nightly.nds` when its launch argument lacks
a recognized `sd:/` or `fat:/` prefix. A different nightly binary makes the
comparison inconclusive. If direct launch boots but the probe fails,
investigate the SDK/loader handoff. If both
boot but the full menu fails, investigate menu state, bootstub installation and
configuration writes. Preserve the corresponding `NDSBTSRP.LOG` for each run.
Neither an emulator stall nor success after a debugger intervention establishes
a hardware pass.
