// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef TWLM_SLOT1_TRACE_H
#define TWLM_SLOT1_TRACE_H
#include <nds.h>

enum Slot1TracePhase {
    S1_HEADER = 1, S1_CHIP_ID, S1_KEY1, S1_KEY2_COMMAND, S1_KEY2_SEED,
    S1_SECURE_ID, S1_SECURE_AREA, S1_DATA_MODE, S1_COMPLETE, S1_DELAY, S1_DELAY_DONE
};

#if defined(TWLM_SLOT1_TRACE) && defined(ARM7)
static inline void slot1TraceArm7(unsigned phase, unsigned extra) {
    const u32 packet[] = {phase, extra, REG_ROMCTRL,
        ((u32)TIMER_CR(0) << 16) | TIMER_DATA(0)};
    fifoSendDatamsg(FIFO_USER_06, sizeof(packet), (u8*)packet);
}
#else
static inline void slot1TraceArm7(unsigned phase, unsigned extra) { (void)phase; (void)extra; }
#endif

#if defined(TWLM_SLOT1_TRACE) && defined(ARM9)
#ifdef __cplusplus
extern "C" {
#endif
void slot1TraceBegin(void);
void slot1TraceEnd(void);
bool slot1TraceForceArm9(void);
void slot1TraceFailure(const char *phase, u32 code);
#ifdef __cplusplus
}
#endif
#endif
#endif
