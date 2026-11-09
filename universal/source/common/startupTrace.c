// SPDX-License-Identifier: GPL-2.0-or-later
// Diagnostic-only startup watchdog. It never writes a trace to the SD card.
#ifdef TWLM_STARTUP_TRACE
#include <nds.h>
#include <nds/fifomessages.h>
#include "common/startupTrace.h"

static VoidFn applicationVblank;
static const char *volatile phase = "before main";
static const char *volatile operation;
static volatile u32 sector, sectors;
static volatile unsigned frames;
static volatile bool active;

static void stopped(void) {
    // Terminal failure reporting only. Trace builds reserve a 4 KiB IRQ stack
    // for the console; normal VBlank bookkeeping performs no I/O/allocation.
    REG_IME = 0;
    REG_IE = 0;
    consoleDemoInit();
    REG_MASTER_BRIGHT = 0;
    REG_MASTER_BRIGHT_SUB = 0;
    // Direct console drawing avoids libc's thread-local stdio state in IRQs.
    consolePrintString("TWiLight r15 startup trace\n\nStopped during:\n");
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
            default: break;
        }
        if (name) beginIo(name, msg->sdParams.startsector, msg->sdParams.numsectors);
    }
    bool result = __real_fifoSendDatamsg(channel, bytes, data);
    if (channel == FIFO_STORAGE && !result) endIo(false);
    return result;
}

bool __real_fifoSendValue32(u32, u32);
bool __wrap_fifoSendValue32(u32 channel, u32 value) {
    if (channel == FIFO_STORAGE) beginIo("native storage command", value, 0);
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
