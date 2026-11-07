#pragma once
#include <nds/ndstypes.h>
int enterCriticalSection(void);
void leaveCriticalSection(int old);
#define IRQ_SDMMC (1u << 0)
#define IRQ_SDIO (1u << 1)
#define IRQ_I2C (1u << 6)
#define IRQ_IPC_SYNC (1u << 16)
void irqDisableAUX(u32 mask);
void irqSet(u32 mask, void (*handler)(void));
void irqEnable(u32 mask);
