// SPDX-License-Identifier: GPL-2.0-or-later
#include "common/storage7.h"
#include <string.h>

void twlmReadStorageCid(int device, u32 *cid) {
    if (!twlmSdAccessible()) {
        memset(cid, 0, 16);
        return;
    }
    const u32 result = SDMMC_init(device);
    if ((result != SDMMC_ERR_NONE && result != SDMMC_ERR_INITIALIZED)
        || SDMMC_getCidRaw(device, cid) != SDMMC_ERR_NONE) {
        memset(cid, 0, 16);
    }
}
