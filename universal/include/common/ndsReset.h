// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef TWLM_NDS_RESET_H
#define TWLM_NDS_RESET_H

#include <nds/arm9/cp15.h>

// Call after disabling interrupts, before changing the RAM mapping or writing
// shared reset addresses. BlocksDS caches 0x02FFFE00; swiSoftReset discards
// dirty cache lines. ARM7 must see every reset address directly in main RAM.
static inline void prepareArm9ForReset(void)
{
	// The SDK routine also uses r8 to hold its return address. Declare that
	// clobber explicitly: unlike an ordinary C call it does not preserve r8.
	__asm__ volatile ("bl CP15_MPUDisable"
		: : : "r0", "r1", "r2", "r3", "r8", "r12", "lr", "cc", "memory");
}

#endif
