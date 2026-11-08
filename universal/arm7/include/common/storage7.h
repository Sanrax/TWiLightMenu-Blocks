// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef TWLM_STORAGE7_H
#define TWLM_STORAGE7_H

#include <nds/system.h>
#include <nds/arm7/sdmmc.h>

// Configure accessible hardware before irqInit() initializes the SDK TMIO driver.
static inline void twlmPrepareStorage(void) {
    if (isDSiMode() && (REG_SCFG_EXT & SCFG_EXT_SCFG_MBK_REG)) {
        REG_SCFG_EXT |= SCFG_EXT_SDMMC | SCFG_EXT_NDMA | SCFG_EXT_INTERRUPT
                      | SCFG_EXT_AES | SCFG_EXT_DMA;
    }
}

static inline bool twlmSdAccessible(void) {
    return isDSiMode() && (REG_SCFG_EXT & SCFG_EXT_SDMMC);
}

static inline bool twlmSdInserted(void) {
    return twlmSdAccessible()
        && !(SDMMC_getDiskStatus(SDMMC_DEV_CARD) & SDMMC_STATUS_NODISK);
}

void twlmReadStorageCid(int device, u32 *cid);

#endif
