// Exercise the actual ARM7 driver with scripted controller registers.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <nds/system.h>
#include <nds/arm9/dldi.h>
#include "../universal/sdmmc/arm7/include/my_sdmmc.h"

volatile u32 testScfg;
volatile u16 testVcount;
static u16 status0, status1, response[8];
static unsigned commands, appAttempts, delays, registerReads, criticalDepth;
static int failedCommand;
static bool present, neverReady, busyForever, noResponse, legacyCard;
static bool dsiMode;
static u32 auxInterrupts, interrupts;
static void (*ipcHandler)(void);
static unsigned fifoHandlers;
static bool pioRead;
static unsigned fifoDelay;

bool isDSiMode(void) { return dsiMode; }
void irqDisableAUX(u32 mask) { assert(dsiMode); auxInterrupts &= ~mask; }
void irqSet(u32 mask, void (*handler)(void)) { assert(mask == (1u << 16)); ipcHandler = handler; }
void irqEnable(u32 mask) { interrupts |= mask; }
void powerValueHandler(u32 value, void *data) { (void)value; (void)data; }
void firmwareMsgHandler(int bytes, void *data) { (void)bytes; (void)data; }
void fifoSetValue32Handler(int channel, void (*handler)(u32, void *), void *data) {
    assert(channel == 0 && handler == powerValueHandler && data == NULL); fifoHandlers++;
}
void fifoSetDatamsgHandler(int channel, void (*handler)(int, void *), void *data) {
    assert(channel == 1 && handler == firmwareMsgHandler && data == NULL); fifoHandlers++;
}

void swiDelay(unsigned cycles) { (void)cycles; delays++; }
int enterCriticalSection(void) { return criticalDepth++; }
void leaveCriticalSection(int old) { assert(criticalDepth == (unsigned)old + 1); criticalDepth--; }
void twlmDldiFixDriverAddresses(DLDI_INTERFACE *io) { (void)io; assert(false); }
static bool dldiStartup(void) { return true; }
static DLDI_INTERFACE dldi = {.ioInterface.startup = dldiStartup};
DLDI_INTERFACE *io_dldi_data = &dldi;

static u16 readRegister(u16 reg) {
    assert(testScfg & SCFG_EXT_SDMMC); // Never touch a disabled controller.
    registerReads++;
    if (reg == REG_SDSTATUS0) return status0 | (present ? TMIO_STAT0_SIGSTATE : 0);
    if (reg == REG_SDSTATUS1) return busyForever ? TMIO_STAT1_CMD_BUSY : status1;
    if (reg == REG_SDDATACTL32 && pioRead) {
        if (fifoDelay) { fifoDelay--; return 0; }
        return 0x100;
    }
    if (reg >= REG_SDRESP0 && reg <= REG_SDRESP7) return response[(reg - REG_SDRESP0) / 2];
    return sdmmc_read16(reg);
}

static void writeRegister(u16 reg, u16 value) {
    assert(testScfg & SCFG_EXT_SDMMC);
    if (reg == REG_SDSTATUS0) status0 = value;
    else if (reg == REG_SDSTATUS1) status1 = value;
    else if (reg == REG_SDCMD) {
        unsigned cmd = value & 63;
        commands++;
        memset(response, 0, sizeof(response));
        status0 = noResponse ? 0 : TMIO_STAT0_CMDRESPEND;
        if (cmd == 18 && pioRead) status0 |= TMIO_STAT0_DATAEND;
        status1 = (int)cmd == failedCommand || (cmd == 8 && legacyCard) ? TMIO_STAT1_CMDTIMEOUT : 0;
        if (cmd == 41 || cmd == 1) {
            appAttempts++;
            response[1] = !neverReady && appAttempts >= 3 ? 0xC000 : 0x4000;
        } else if (cmd == 3) response[1] = 0x1234;
        else if (cmd == 8) response[0] = 0x1AA;
    } else sdmmc_write16(reg, value);
}

// Only the MMIO boundary is replaced; initialization, retry and IRQ code
// below are the same functions compiled into every ARM7 application.
#define sdmmc_read16 readRegister
#define sdmmc_write16 writeRegister
#include "../universal/sdmmc/arm7/source/my_sdmmc.c"
#undef sdmmc_read16
#undef sdmmc_write16
#include "../universal/sdmmc/arm7/source/my_system.c"

static void reset(void) {
    testVcount = 0;
    testScfg = SCFG_EXT_SCFG_MBK_REG;
    status0 = status1 = 0;
    memset(response, 0, sizeof(response));
    memset((void *)0x02FFF000, 0, 0x1000);
    memset((void *)0x04004000, 0, 0x1000);
    commands = appAttempts = delays = registerReads = criticalDepth = 0;
    failedCommand = -1;
    present = true;
    neverReady = busyForever = noResponse = legacyCard = false;
    my_sdmmc_controller_initialised = useDLDI = false;
    memset(&deviceSD, 0, sizeof(deviceSD));
    memset(&deviceNAND, 0, sizeof(deviceNAND));
    dsiMode = false;
    auxInterrupts = interrupts = fifoHandlers = 0;
    ipcHandler = NULL;
    pioRead = false;
    fifoDelay = 0;
}

static u32 request(u32 command) {
    *(vu32 *)0x02FFFA0C = command;
    my_sdmmcHandler();
    assert(criticalDepth == 0);
    return *(vu32 *)0x02FFFA0C;
}

int main(void) {
    assert(mmap((void *)0x02000000, 0x20000, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0) == (void *)0x02000000);
    assert(mmap((void *)0x02FFF000, 0x1000, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0) == (void *)0x02FFF000);
    assert(mmap((void *)0x0303C000, 0x1000, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0) == (void *)0x0303C000);
    assert(mmap((void *)0x04004000, 0x1000, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0) == (void *)0x04004000);

    // The actual FIFO installer must give SD status ownership to polling,
    // while retaining the SDK's SDIO and power-button interrupt handlers.
    for (unsigned mode = 0; mode < 2; ++mode) {
        reset();
        dsiMode = mode;
        auxInterrupts = IRQ_SDMMC | IRQ_SDIO | IRQ_I2C;
        my_installSystemFIFO();
        assert(auxInterrupts == (IRQ_SDIO | IRQ_I2C | (mode ? 0 : IRQ_SDMMC)));
        assert(interrupts == IRQ_IPC_SYNC && ipcHandler == my_sdmmcHandler);
        assert(fifoHandlers == 2);
    }

    reset();
    assert(request(0x56484453) == 1);
    assert(testScfg & SCFG_EXT_SDMMC);
    assert(*(vu32 *)0x02FFFA00 == testScfg);
    assert(request(0x54534453) == 0 && appAttempts == 3);
    assert(deviceSD.isSDHC == 1);
    assert(my_sdmmc_cardinserted());
    present = false;
    assert(!my_sdmmc_cardinserted());

    reset();
    legacyCard = true;
    assert(request(0x54534453) == 0);
    assert(deviceSD.isSDHC == 0);

    reset();
    present = false;
    assert(request(0x56484453) == 0);
    assert(request(0x54534453) != 0 && commands == 0);

    reset();
    testScfg = 0; // Locked SD access, including DS compatibility mode.
    assert(request(0x56484453) == 0);
    assert(request(0x54534453) != 0);
    assert(request(0x5453414E) != 0);
    assert(registerReads == 0);
    *(vu32 *)0x02FFFA04 = *(vu32 *)0x0303C000 = 0x49444C44;
    assert(request(0x54534453) == 0 && useDLDI); // iEvolution fallback retained.

    reset();
    testScfg = SCFG_EXT_SDMMC; // Locked but accessible.
    assert(request(0x56484453) == 1);
    assert(request(0x54534453) == 0);

    const unsigned failedCommands[] = {55, 41};
    for (unsigned i = 0; i < sizeof(failedCommands) / sizeof(failedCommands[0]); ++i) {
        reset();
        failedCommand = failedCommands[i];
        assert(request(0x54534453) != 0);
        assert(commands <= 4);
    }
    reset();
    neverReady = true;
    assert(request(0x54534453) != 0 && appAttempts == 200);
    reset();
    busyForever = true;
    assert(request(0x54534453) != 0 && commands == 0);
    assert(registerReads <= SDMMC_POLL_LIMIT + 10);
    reset();
    enableSdmmc();
    busyForever = true;
    u32 buffer[128];
    assert(my_sdmmc_readsectors(&deviceSD, 0, 1, buffer) != 0);
    assert(*(vu32 *)0x0400411C == 0); // No DMA is armed after a busy timeout.
    reset();
    noResponse = true;
    assert(request(0x54534453) != 0 && commands <= 3);

    // Model DMA using the register-programmed word count, including guards.
    // A byte count here would overwrite three times the requested buffer.
    for (unsigned sectors = 1; sectors <= 128; sectors *= 4) {
        reset();
        enableSdmmc();
        my_sdmmc_controller_init(false);
        u32 *data = (u32 *)0x02000000;
        const unsigned words = sectors * 128;
        for (unsigned i = 0; i < words + 16; ++i) data[i] = 0xDEADBEEF;
        assert(my_sdmmc_readsectors(&deviceSD, 7, sectors, data) == 0x474E4950);
        assert(registerReads < 32); // Posting a command must not spin in the IRQ.
        assert(*(vu32 *)0x0400410C == words);
        assert((*(vu32 *)0x0400411C & (BIT(30) | BIT(29))) == 0);
        status0 |= TMIO_STAT0_DATAEND;
        assert(my_sdmmc_continue_readsectors(&deviceSD) == 0x474E4950);
        for (unsigned i = 0; i < *(vu32 *)0x0400410C; ++i) data[i] = i ^ 0x12345678;
        *(vu32 *)0x0400411C &= ~BIT(31);
        assert(my_sdmmc_continue_readsectors(&deviceSD) == 0);
        for (unsigned i = 0; i < words; ++i) assert(data[i] == (i ^ 0x12345678));
        for (unsigned i = words; i < words + 16; ++i) assert(data[i] == 0xDEADBEEF);
        assert(*(vu32 *)0x0400411C == 0);
        request(0x47445353);
        assert(*(vu32 *)0x02FFFA08 == words);
    }
    // Unaligned callers use PIO. Completion flags alone must not return
    // success while the FIFO still has the final block to deliver.
    reset();
    enableSdmmc();
    my_sdmmc_controller_init(false);
    pioRead = true;
    fifoDelay = 3;
    *(vu32 *)(SDMMC_BASE + REG_SDFIFO32) = 0x76543210;
    u8 *unaligned = (u8 *)0x02000001;
    memset(unaligned - 1, 0xAA, 514);
    assert(my_sdmmc_readsectors(&deviceSD, 0, 1, unaligned) == 0);
    for (unsigned i = 0; i < 512; ++i) assert(unaligned[i] == ((0x76543210u >> (8 * (i % 4))) & 255));
    assert(unaligned[-1] == 0xAA && unaligned[512] == 0xAA);
    assert(fifoDelay == 0);

    reset();
    enableSdmmc();
    my_sdmmc_controller_init(false);
    noResponse = true;
    assert(my_sdmmc_readsectors(&deviceSD, 0, 1, (void *)0x02000000) == 0x474E4950);
    assert(registerReads < 32);
    for (unsigned frame = 0; frame < 120; ++frame) {
        testVcount = 200;
        assert(my_sdmmc_continue_readsectors(&deviceSD) == 0x474E4950);
        testVcount = 0;
        const u32 result = my_sdmmc_continue_readsectors(&deviceSD);
        assert(result == (frame == 119 ? 1 : 0x474E4950));
    }
    assert(*(vu32 *)0x0400411C == 0);
    assert(sdmmc_read16(REG_SDSTOP) == 0x101);
    reset();
    enableSdmmc();
    my_sdmmc_controller_init(false);
    assert(my_sdmmc_readsectors(&deviceSD, 0, 1, (void *)0x02000000) == 0x474E4950);
    status1 = TMIO_STAT1_CRCFAIL;
    assert(my_sdmmc_continue_readsectors(&deviceSD) == 1);
    assert(*(vu32 *)0x0400411C == 0);
    reset();
    enableSdmmc();
    my_sdmmc_controller_init(false);
    assert(my_sdmmc_readsectors(&deviceSD, 0, 1, (void *)0x02000000) == 0x474E4950);
    deviceSD.dmaPollsLeft = 1;
    assert(my_sdmmc_continue_readsectors(&deviceSD) == 1);
    assert(*(vu32 *)0x0400411C == 0);

    reset();
    assert(request(0x5453414E) == 0 && appAttempts == 3);
    reset();
    neverReady = true;
    assert(request(0x5453414E) != 0 && appAttempts == 200);
    puts("ARM7 SD gating, card presence, SDHC/SDSC/NAND startup, bounded failures, exact DMA lengths/completion, IRQ liveness and iEvolution fallback passed");
}
