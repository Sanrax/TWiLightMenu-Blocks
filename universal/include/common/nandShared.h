// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef TWLM_NAND_SHARED_H
#define TWLM_NAND_SHARED_H

#include <nds.h>

// Only the console key/CID exchange uses this mailbox. Sector transfers use
// the native SDK FIFO service. ARM9 must never cache ARM7's replies.
static inline bool twlmNandRequest(u32 request, u32 buffer) {
    vu32 *const command = (vu32*)0x0CFFFD0C;
    if (*command != 0) return false;
    *(vu32*)0x0CFFFD08 = buffer;
    *command = request;
    // A missing optional NAND response must not prevent the browser opening.
    for (unsigned polls = 4096; *command == request && polls; --polls)
        swiDelay(4096);
    __asm__ volatile ("" ::: "memory");
    return *command == 0;
}

static inline bool twlmNandReadCid(u32 cid[4]) {
    if (!twlmNandRequest(0x454D4D43, 0)) return false; // eMMC CID request
    const vu32 *const reply = (const vu32*)0x0CFFD7BC;
    u32 present = 0;
    for (unsigned i = 0; i < 4; ++i) {
        cid[i] = reply[i];
        present |= cid[i];
    }
    return present != 0;
}

#endif
