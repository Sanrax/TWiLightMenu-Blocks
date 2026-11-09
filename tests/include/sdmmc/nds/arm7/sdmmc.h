#pragma once
#include <nds/ndstypes.h>
#define SDMMC_DEV_CARD 0
#define SDMMC_STATUS_NOINIT 1u
#define SDMMC_STATUS_NODISK 2u
#define SDMMC_STATUS_PROTECT 4u
u8 SDMMC_getDiskStatus(u8 device);
