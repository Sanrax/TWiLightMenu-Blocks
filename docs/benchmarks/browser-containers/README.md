# Directory and container audit

The checks suggested alongside the Battery Timer hardware report found two
independent browser errors. Neither is confirmed as the icon corruption cause.

| Check | Finding |
| --- | --- |
| Directory/FILE implementation casts | Already removed by the BlocksDS port. All three browsers use `twlmReadDirectory()` and typed FatFS `FILINFO` attributes. Legacy devkitPro NitroFS is excluded by `#ifndef __BLOCKSDS__`. No additional production change. |
| Writes into an empty image vector, as in [pkmn-chest PR 99](https://github.com/Universal-Team/pkmn-chest/pull/99) | The relevant decoded PNG, texture and indexed-image buffers are populated/sized before indexed writes. Static banner and pending icon pixels use fixed arrays. No matching error found in those paths; this is a scoped audit, not proof of all container use. |
| Cached-directory placeholders | `getDirectoryContents()` cleared the vector but retained `dirContentBlankFilled` and `lockOutDirContentBlankFilling`. Loading page 2 of one `dirInfo.twlm.ini` directory then page 0 of another attempted `erase(begin())` on an empty vector. Reset both flags with `pageLoaded`. |
| Custom sort comparator | A new folder, a saved game at position 0 and a saved folder at position 1 formed a cycle: A < B, B < C, C < A. This violates `std::sort`'s strict weak ordering requirement in all three browsers. Group new folders, saved positions and remaining files consistently, preserving saved entries' relative order. |

`sort-before.log` and `cache-before.log` are failures from the actual pre-fix
functions on diagnostics commit `3fbf66acb` (master `c62effc4a` has the same
algorithms). The cache failure uses valid contiguous lists; it does not require
a malformed INI. Master fix `aa33e263a` is merged into diagnostics as
`fa3c4ff61`; `after.log` records its corrected production functions.

Run manually from the diagnostics branch, optionally pointing at master:

```sh
python3 tests/browser_containers.py --source /path/to/checkout
/opt/wonderful/toolchain/gcc-arm-none-eabi/bin/arm-none-eabi-gcc \
  -std=gnu11 -D__NDS__ -DARM9 -D__BLOCKSDS__ \
  -I/opt/blocksds/core/libs/libnds/include -Iuniversal/include \
  -fsyntax-only tests/browser_directory_abi.c
```

The host script extracts production functions verbatim, mocking SDK/filesystem
calls. Checked libstdc++ iterators, AddressSanitizer and UndefinedBehaviorSanitizer
cover 660 cached-directory rebuilds, comparator ordering and 100 shuffled
1,024-entry lists per browser, and 1,000 typed-directory reopen cycles. The
adapter cases cover attributes, types, copied filenames, 255-byte names and I/O
errors. LeakSanitizer is disabled because it cannot inspect threads under the
ptrace sandbox. The separate ARM compile checks real SDK filename buffer sizes;
host tests mock FatFS types because the allocator ABI is ARM32-specific.

BlocksDS 1.24.0 builds and distribution staging passed for
`romsel_dsimenutheme` (DSi/3DS/Saturn/HBL), `romsel_aktheme` and `romsel_r4theme`.
No new emulator or hardware test was performed for this audit. The r25 hardware
icon diagnostic remains useful; these changes do not replace that investigation.
