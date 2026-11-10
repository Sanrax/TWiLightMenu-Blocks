#include "../../../universal/source/common/startupTrace.c"
#ifdef SLOT1_TRACE_IPC_SELFTEST
static void discardStatus(u32 value, void *context) { (void)value; (void)context; }
#endif
int main(void) {
    slot1TraceBegin();
    startupTraceMark("Slot-1: ARM7 card init");
#ifdef SLOT1_TRACE_IPC_SELFTEST
    fifoSetValue32Handler(FIFO_USER_03, discardStatus, NULL);
    sysSetCardOwner(BUS_OWNER_ARM7);
    fifoSendValue32(FIFO_USER_02, 0x54494E49);
    fifoWaitValue32(FIFO_USER_02);
    const u32 result = fifoGetValue32(FIFO_USER_02);
    if (!arm7Phase) slot1TraceFailure("IPC self-test: no ARM7 trace", 1);
    slot1Error = result;
    startupTraceMark("IPC self-test: ARM7 replied");
#else
    arm7Command = S1_KEY2_COMMAND;
    arm7Phase = S1_DELAY; arm7Extra = 0xD7E; arm7Romctrl = 0xA0186000; arm7Timer = 0x0082FF00;
#endif
    frames = 60 * 20 - 2;
    for (;;) swiWaitForVBlank();
}
