// SPDX-License-Identifier: GPL-2.0-or-later
// Diagnostic-only startup watchdog. It never writes a trace to the SD card.
#ifdef TWLM_STARTUP_TRACE
#include <nds.h>
#include <nds/fifomessages.h>
#include "common/startupTrace.h"
#include "common/slot1Trace.h"

static VoidFn applicationVblank;
static const char *volatile phase = "before main";
static const char *volatile operation;
static volatile u32 sector, sectors;
static volatile unsigned frames;
static volatile bool active;
#ifdef TWLM_SLOT1_TRACE
static volatile bool slot1Active, slot1Arm9;
static volatile u32 arm7Phase, arm7Extra, arm7Romctrl, arm7Timer, slot1Error;
static volatile u32 arm7Command, arm7CommandExtra;

static const char *arm7PhaseName(u32 value) {
    switch (value) {
        case S1_HEADER: return "read header / reset";
        case S1_CHIP_ID: return "read chip ID";
        case S1_KEY1: return "activate KEY1";
        case S1_KEY2_COMMAND: return "activate KEY2";
        case S1_KEY2_SEED: return "apply KEY2 seed";
        case S1_SECURE_ID: return "secure chip ID";
        case S1_SECURE_AREA: return "secure area read";
        case S1_DATA_MODE: return "enter data mode";
        case S1_COMPLETE: return "initialization complete";
        case S1_DELAY: return "wait for TIMER0 = FFFF";
        case S1_DELAY_DONE: return "timer delay complete";
        default: return "no progress received";
    }
}

static void slot1Arm7Report(int bytes, void *unused) {
    (void)unused;
    u32 packet[4];
    if (bytes != sizeof(packet)) {
        fifoGetDatamsg(FIFO_USER_06, 0, NULL);
        return;
    }
    fifoGetDatamsg(FIFO_USER_06, sizeof(packet), (u8*)packet);
    arm7Phase = packet[0]; arm7Extra = packet[1];
    if (arm7Phase != S1_DELAY && arm7Phase != S1_DELAY_DONE) {
        arm7Command = arm7Phase; arm7CommandExtra = arm7Extra;
    }
    arm7Romctrl = packet[2]; arm7Timer = packet[3];
    frames = 0;
}

void slot1TraceBegin(void) {
    arm7Phase = arm7Extra = arm7Romctrl = arm7Timer = arm7Command = arm7CommandExtra = 0;
    scanKeys();
    slot1Arm9 = (keysHeld() & KEY_X) != 0;
    slot1Error = 0;
    slot1Active = true;
    fifoSetDatamsgHandler(FIFO_USER_06, slot1Arm7Report, NULL);
    startupTraceBegin();
}
void slot1TraceEnd(void) { slot1Active = false; startupTraceEnd(); }
bool slot1TraceForceArm9(void) { return slot1Arm9; }
void slot1TraceFailure(const char *failedPhase, u32 code) {
    startupTraceMark(failedPhase);
    slot1Error = code;
    frames = 60 * 20;
    for (;;) swiWaitForVBlank();
}
#endif


static void stopped(void) {
    // Terminal failure reporting only. Trace builds reserve a 4 KiB IRQ stack
    // for the console; normal VBlank bookkeeping performs no I/O/allocation.
    REG_IME = 0;
    REG_IE = 0;
    consoleDemoInit();
    REG_MASTER_BRIGHT = 0;
    REG_MASTER_BRIGHT_SUB = 0;
    // Direct console drawing avoids libc's thread-local stdio state in IRQs.
    consolePrintString("TWiLight diagnostic trace r21\n\nStopped during:\n");
    consolePrintString(phase);
    consolePrintString("\n\n");
    if (operation) {
        consolePrintString(operation);
        consolePrintString("\nsector ");
        consolePrintUnsigned(sector, 10);
        consolePrintString(", count ");
        consolePrintUnsigned(sectors, 10);
        consolePrintString("\n\n");
    }
#ifdef TWLM_SLOT1_TRACE
    if (slot1Active) {
        if (slot1Error) { consolePrintString("Error "); consolePrintUnsigned(slot1Error, 16); consolePrintString("\n"); }
        consolePrintString("ARM7: "); consolePrintString(arm7PhaseName(arm7Phase));
        if (arm7Phase == S1_DELAY || arm7Phase == S1_DELAY_DONE) {
            consolePrintString("\nduring "); consolePrintString(arm7PhaseName(arm7Command));
            consolePrintString(" "); consolePrintUnsigned(arm7CommandExtra, 16);
        }
        consolePrintString("\nextra "); consolePrintUnsigned(arm7Extra, 16);
        consolePrintString(" / ROMCTRL "); consolePrintUnsigned(arm7Romctrl, 16);
        consolePrintString("\nTIMER0 ctl/data "); consolePrintUnsigned(arm7Timer, 16);
        consolePrintString(slot1Arm9 ? "\nDriver: ARM9 override\n\n" : "\nDriver: SDK automatic\n\n");
        consolePrintString("SCFG_MC "); consolePrintUnsigned(REG_SCFG_MC, 16);
        consolePrintString(" / EXMEM "); consolePrintUnsigned(REG_EXMEMCNT, 16);
        consolePrintString("\nARM9 ROMCTRL "); consolePrintUnsigned(REG_ROMCTRL, 16);
        consolePrintString("\n\n");
    }
#endif
    consolePrintString("No SD log was written.\nReport these lines.\nThen power off.");
    for (;;) __asm__ volatile ("nop");
}

static void traceVblank(void) {
    if (active && ++frames >= (operation ? 60 * 10 : 60 * 20)) stopped();
    if (applicationVblank) applicationVblank();
}

void __real_irqSet(u32 mask, VoidFn handler);
void __wrap_irqSet(u32 mask, VoidFn handler) {
    if (mask & IRQ_VBLANK) {
        applicationVblank = handler;
        __real_irqSet(IRQ_VBLANK, traceVblank);
        mask &= ~IRQ_VBLANK;
    }
    if (mask) __real_irqSet(mask, handler);
}

void startupTraceMark(const char *next) {
    int saved = enterCriticalSection();
    phase = next;
    frames = 0;
    leaveCriticalSection(saved);
}

void startupTraceBegin(void) {
    startupTraceMark("browser entry / ARM7 hello");
    active = true;
    __real_irqSet(IRQ_VBLANK, traceVblank);
    irqEnable(IRQ_VBLANK);
}

void startupTraceEnd(void) { active = false; }

static void beginIo(const char *next, sec_t start, sec_t count) {
    int saved = enterCriticalSection();
    operation = next;
    sector = start;
    sectors = count;
    frames = 0;
    leaveCriticalSection(saved);
}

static bool endIo(bool result) {
    int saved = enterCriticalSection();
    operation = NULL;
    frames = 0;
    leaveCriticalSection(saved);
    return result;
}

// Observe the FIFO boundary: references inside the SDK's own SD object do
// not pass through --wrap=sdmmc_ReadSectors (its DISC_INTERFACE is local).
bool __real_fifoSendDatamsg(u32, u32, u8*);
bool __wrap_fifoSendDatamsg(u32 channel, u32 bytes, u8 *data) {
    if (channel == FIFO_STORAGE && bytes >= sizeof(FifoMessage)) {
        const FifoMessage *msg = (const FifoMessage*)data;
        const char *name = NULL;
        switch (msg->type) {
            case SDMMC_SD_READ_SECTORS: name = "native SD read"; break;
            case SDMMC_SD_WRITE_SECTORS: name = "native SD write"; break;
            case SDMMC_NAND_READ_SECTORS: name = "native NAND read"; break;
            case DLDI_STARTUP: name = "DLDI startup"; break;
            case DLDI_READ_SECTORS: name = "DLDI read"; break;
            case DLDI_WRITE_SECTORS: name = "DLDI write"; break;
            default: break;
        }
        if (name) beginIo(name, msg->type == DLDI_STARTUP ? 0 : msg->sdParams.startsector,
                              msg->type == DLDI_STARTUP ? 0 : msg->sdParams.numsectors);
    }
    bool result = __real_fifoSendDatamsg(channel, bytes, data);
    if (channel == FIFO_STORAGE && !result) endIo(false);
    return result;
}

bool __real_fifoSendValue32(u32, u32);
bool __wrap_fifoSendValue32(u32 channel, u32 value) {
    if (channel == FIFO_STORAGE)
        beginIo(value == DLDI_IS_INSERTED ? "DLDI is inserted" : "native storage command", value, 0);
    bool result = __real_fifoSendValue32(channel, value);
    if (channel == FIFO_STORAGE && !result) endIo(false);
    return result;
}

u32 __real_fifoGetValue32(u32);
u32 __wrap_fifoGetValue32(u32 channel) {
    u32 result = __real_fifoGetValue32(channel);
    if (channel == FIFO_STORAGE) endIo(true);
    return result;
}
#endif
