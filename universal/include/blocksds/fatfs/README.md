These headers describe the FatFS ABI used by BlocksDS 1.24.0. The implementation
is linked from the SDK's libnds; no second filesystem library is built.

- `ff.h`: [wf-fatfs, 2b5c26e](https://github.com/blocksds/wf-fatfs/blob/2b5c26e2a1eb4cdbbcb57070d49a6ebf7b60c7f9/source/ff.h)
- `ffconf.h`: [libnds, 7fd8ccb](https://github.com/blocksds/libnds/blob/7fd8ccbe781ed48a06fe063a2dcbb69035fa97d3/source/arm9/libc/fatfs/ffconf.h)

The adapter in `universal/sdmmc/arm9/source/blocksds_storage.c` isolates this
dependency. It preserves custom SD/NAND drivers, remounts after detecting a
flashcard, and reads directory attributes without repeated path lookups.
Recheck the ABI and update both headers when upgrading BlocksDS.
