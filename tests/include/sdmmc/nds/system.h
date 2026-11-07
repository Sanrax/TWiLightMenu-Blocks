#pragma once
#include <stdbool.h>
#include <nds/ndstypes.h>
typedef volatile u16 vu16;
typedef volatile u32 vu32;
#define BIT(n) (1u << (n))
extern volatile u32 testScfg;
#define REG_SCFG_EXT testScfg
#define SCFG_EXT_SCFG_MBK_REG BIT(31)
#define SCFG_EXT_SDMMC BIT(18)
#define SCFG_EXT_AES BIT(17)
#define SCFG_EXT_INTERRUPT BIT(8)
#define SCFG_EXT_DMA BIT(0)

extern volatile u16 testVcount;
#define REG_VCOUNT testVcount
bool isDSiMode(void);
