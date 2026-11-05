#include <nds/asminc.h>

    .syntax  unified
    .arch    armv5te
    .cpu     arm946e-s

    .arm

#define CP15_CONFIG_REGION_ENABLE               (1 << 0)

#define CP15_REGION_SIZE_16MB                   (0x17 << 1)
#define CP15_REGION_SIZE_32MB                   (0x18 << 1)

// Extend the uncached main RAM mirror without changing the executable region.

BEGIN_ASM_FUNC CP15_ExtendUncachedRam
    ldr     r0, =(0xC000000 | CP15_CONFIG_REGION_ENABLE | CP15_REGION_SIZE_32MB)
#ifdef __BLOCKSDS__
    // BlocksDS uses region 5 for uncached RAM and region 6 for main RAM.
    mcr     p15, 0, r0, c6, c5, 0
#else
    mcr     p15, 0, r0, c6, c6, 0
#endif
    bx      lr
