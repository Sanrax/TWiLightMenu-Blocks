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

## r17 controlled experiments

The user confirmed that r16 also stops at `Header CRC is valid!` on hardware,
with a fresh log and a clean `_nds` installation. This reproduces the failure
without the menu UI, NAND discovery, settings writes or exit bootstub.

Generate **diagnostic copies**, never replacements for the release bootstrap:

```
python3 tests/bootstrap-handoff/variants.py /path/to/official/nds-bootstrap-release.nds output
```

The script accepts only official 2.17.0's pinned SHA-256. It changes `loadi0`
only, checks each original instruction, compresses/round-trips the result and
appends it as a replacement NitroFS entry. The frontend and DSi segments are
byte-identical to upstream. Manifests list every loader change; the original
input and production binaries remain intact. Keep these copies off release CI.

Copy both output NDS files under `sd:/_nds/twlm-diag/`, alongside the existing
release installation, and use the r17 control from commit `ef26f781a`:

- **X / CPU** selects bootstrap's existing CPU SD-read path instead of NDMA
  during game-binary/header loading. One Thumb immediate changes from 0 to 4.
  It can attempt to launch the game; it is not a production workaround.
- **Y / state** keeps normal SD reads. After reading the patch-cache header,
  it logs ARM9's state as eight hex digits (repeated) and stops forever.
  `00000001` means ARM9 started, `00000002` means its memory clear completed,
  and `00000003` means it reached its display/request loop. No new digits means
  this checkpoint was not reached (or its log write did not finish).

Use Mario Kart and preserve a separate fresh `NDSBTSRP.LOG` after each attempt.
The state probe never boots a game, even on working hardware. Its checkpoint
is unconditional, so no cache rename is necessary. No Quit Game changes are
included. Restore normal operation by launching the existing `BOOT.NDS`.

## r18 ARM9 memory snapshots

The hardware X log still ends at header validation. Y reaches its checkpoint
and records state zero twice. This does not prove ARM9 never started: ARM7
has already passed a wait for state >= 1. Check for later clearing, mapping or
loss of the state/code before changing SD routines or bootstrap behavior.

Build with `variants.py --memory INPUT OUTPUT` and rebuild the selector. The
r18 selector uses **Y** for `sd:/_nds/twlm-diag/bootstrap-memory.nds` and **A**
for the unchanged release. Y still stops forever and never starts a game.
Use the existing Mario Kart INI with Logging enabled and collect the full log.
No settings, SD driver, ARM9 instruction or release bootstrap is changed.

The appended ARM7 probe fits between the original loader BSS end and stacks.
It samples state, hardware registers and ARM9 code integrity at three stages:
0 after the START wait, 1 after ARM7's RAM clear, 2 after patch-cache header
reading. Hooks replay overwritten instructions and retain the original SD
paths. Sampling adds timing overhead; a changed outcome is not a release fix.

Each stage prints hexadecimal field IDs and values:

| ID | Meaning |
| --- | --- |
| 00, 0D | ARM9 state before/after the code comparison |
| 01 | ARM7 SCFG_EXT |
| 02, 03 | SCFG_CLK, SCFG_ROM |
| 04 | Mismatched ARM9 code words (normally zero) |
| 05, 06, 07 | First mismatch address, expected word, actual word |
| 08, 09 | Original VRAM code hash, loaded ARM9 code hash |
| 0A, 0B | ARM9 entry word at 023BFB20, read-only sample at 027BFB20 |
| 0C | ARM7 NDMA global control |

The comparison excludes mutable ARM9 BSS. Hashes use 32-bit FNV word steps
over 0xBC8 bytes at source 06000240 and destination 023BF000. The manifest
records the expected hash and payload locations. The entire source ARM9 code
remains byte-identical to official 2.17.0. All logs and patches stay on
`diagnostics`, outside production builds and CI.
