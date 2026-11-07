#include <nds/disc_io.h>
#include <nds/fifocommon.h>
#include <nds/fifomessages.h>
#include <nds/ipc.h>
#include <nds/bios.h>
#include <nds/system.h>
#include <nds/memory.h>
#include <nds/arm9/cache.h>
#include <nds/arm9/dldi.h>
// #include <string.h>

vu32* sharedAddr = (vu32*)0x02FFFA00;
// bool my_sdio_isNoSGba = false;

#ifdef TWLM_BOOT_DIAGNOSTICS
#include <stdio.h>
extern void bootStage(const char* stage);
static void logSdReply(const char* stage, u32 result) {
	char message[96];
	snprintf(message, sizeof(message), "%s: %08lX", stage, (unsigned long)result);
	bootStage(message);
}
#else
#define bootStage(stage) ((void)0)
#define logSdReply(stage, result) ((void)0)
#endif

static bool startupTimedOut = false;

typedef struct {
    u32 polls;
    u16 line, frames;
} SdIoWait;

static SdIoWait beginIoWait(void) {
    SdIoWait wait = {1u << 22, REG_VCOUNT, 300};
    return wait;
}

static bool ioDeadline(SdIoWait *wait) {
    const u16 line = REG_VCOUNT;
    const bool expired = line < wait->line && --wait->frames == 0;
    wait->line = line;
    if (expired || --wait->polls == 0) {
        startupTimedOut = true;
        return false;
    }
    return true;
}

static bool waitIoReply(u32 command, SdIoWait *wait) {
    while (sharedAddr[3] == command) {
        if (!ioDeadline(wait)) return false;
        swiDelay(100);
    }
    return true;
}

#ifdef TWLM_BOOT_DIAGNOSTICS
extern void bootDisableDiskLog(void);
static struct SdReadTrace { u32 sector, count, address, result, polls; } sdReads[12];
static unsigned sdReadCount, sdStatusCount;

void bootLogSdIo(void) {
    // Never perform filesystem logging from inside a disk callback: libnds
    // shares its sector cache between the flashcard and internal SD.
    const unsigned count = sdReadCount < 12 ? sdReadCount : 12;
    struct SdReadTrace reads[12];
    for (unsigned i = 0; i < count; ++i) reads[i] = sdReads[i];
    u32 status = 0, dma = 0, length = 0, fifo = 0;
    if (!startupTimedOut && count) {
        SdIoWait wait = beginIoWait();
        sharedAddr[3] = 0x47445353;
        IPC_SendSync(3);
        if (waitIoReply(0x47445353, &wait)) {
            status = sharedAddr[0]; dma = sharedAddr[1];
            length = sharedAddr[2]; fifo = sharedAddr[3];
        }
    }
    if (startupTimedOut) {
        // An unacknowledged ARM7 request could still own a cache buffer.
        // Display the trace, but don't issue another filesystem operation.
        bootDisableDiskLog();
        bootStage("SD IPC reply timed out; disk logging stopped");
    }
    char message[144];
    snprintf(message, sizeof(message), "SD status checks: %u; read requests: %u", sdStatusCount, sdReadCount);
    bootStage(message);
    for (unsigned i = 0; i < count; ++i) {
        snprintf(message, sizeof(message), "SD read %lu +%lu -> %08lX: result %08lX, polls %lu",
            (unsigned long)reads[i].sector, (unsigned long)reads[i].count,
            (unsigned long)reads[i].address, (unsigned long)reads[i].result, (unsigned long)reads[i].polls);
        bootStage(message);
    }
    logSdReply("Last SD status", status);
    logSdReply("Last NDMA control", dma);
    logSdReply("Last NDMA word count", length);
    logSdReply("Last SD FIFO/block count", fifo);
}

bool bootSdIpcTimedOut(void) { return startupTimedOut; }
#endif


static bool waitStartupReply(u32 command) {
	// No timer ownership changes. Allow several seconds even at the DSi
	// CPU clock, and never reuse a mailbox whose ARM7 request is still live.
	for (unsigned polls = 0; polls < 20000; ++polls) {
		if (sharedAddr[3] != command) return true;
		swiDelay(12000);
	}
	startupTimedOut = true;
	bootStage("SD ARM7 reply timed out");
	return false;
}

//---------------------------------------------------------------------------------
bool my_sdio_Startup() {
//---------------------------------------------------------------------------------
	if (startupTimedOut) return false;
	bootStage("SD selecting IPC mailbox");
	// Some DS-mode launchers leave SCFG unlocked and the booter enables
	// 16 MiB after SDK startup. Its original DS-sized alias then differs
	// from the active RAM mapping. Avoid probing open bus with a write.
	sharedAddr = (REG_SCFG_EXT & SCFG_EXT_RAM_TWL) ? (vu32*)0x0CFFFA00
		: (vu32*)memUncached((void*)0x02FFFA00);
	logSdReply("SD IPC mailbox", (u32)sharedAddr);

	// my_sdio_isNoSGba = (strncmp((const char*)0x04FFFA00, "no$gba", 6) == 0);

	int result = 0;

	if (sharedAddr[1] == 0x49444C44) {
		sharedAddr[0] = (u32)io_dldi_data;
		sysSetCardOwner(BUS_OWNER_ARM7);
	} else {
		bootStage("SD checking card presence through ARM7");
		sharedAddr[3] = 0x56484453;
		// if (my_sdio_isNoSGba) {
			IPC_SendSync(1);
		/* } else {
			fifoSendValue32(FIFO_SDMMC, 1);
		} */
		if (!waitStartupReply(0x56484453)) return false;
		result = sharedAddr[3];
		logSdReply("SD card present", result);
		logSdReply("ARM7 SCFG_EXT", sharedAddr[0]);
		logSdReply("ARM7 SD status", sharedAddr[2]);

		if (result==0) return false;
	}

	bootStage("SD initializing card through ARM7");
	sharedAddr[3] = 0x54534453;
	// if (my_sdio_isNoSGba) {
		IPC_SendSync(1);
	/* } else {
		fifoSendValue32(FIFO_SDMMC, 1);
	} */
	if (!waitStartupReply(0x54534453)) return false;

	result = sharedAddr[3];
	logSdReply("SD initialization result", result);

	return result == 0;
}

//---------------------------------------------------------------------------------
bool my_sdio_IsInserted() {
//---------------------------------------------------------------------------------
	if (startupTimedOut) return false;
#ifdef TWLM_BOOT_DIAGNOSTICS
    sdStatusCount++;
#endif
    SdIoWait wait = beginIoWait();
	sharedAddr[3] = 0x4E494453;
	// if (my_sdio_isNoSGba) {
		IPC_SendSync(3);
	/* } else {
		fifoSendValue32(FIFO_SDMMC, 3);
	} */
	if (!waitIoReply(0x4E494453, &wait)) return false;
	int result = sharedAddr[3];

	return result == 1;
}

//---------------------------------------------------------------------------------
bool my_sdio_ReadSectors(sec_t sector, sec_t numSectors, void* buffer) {
    if (startupTimedOut) return false;
    if (!numSectors) return true;
    SdIoWait wait = beginIoWait();
#ifdef TWLM_BOOT_DIAGNOSTICS
    const unsigned index = sdReadCount < 12 ? sdReadCount : 11;
    sdReadCount++;
    sdReads[index] = (struct SdReadTrace){sector, numSectors, (u32)buffer, 0xFFFFFFFF, 0};
#endif
    DC_FlushRange(buffer, numSectors * 512);
    sharedAddr[0] = sector;
    sharedAddr[1] = numSectors;
    sharedAddr[2] = (u32)buffer;
    sharedAddr[3] = 0x44524453;
    IPC_SendSync(4);
    if (!waitIoReply(0x44524453, &wait)) return false;
    while (sharedAddr[3] == 0x474E4950) {
#ifdef TWLM_BOOT_DIAGNOSTICS
        sdReads[index].polls++;
#endif
        if (!ioDeadline(&wait)) return false;
        sharedAddr[3] = 0x52434453;
        IPC_SendSync(4);
        if (!waitIoReply(0x52434453, &wait)) return false;
    }
    const u32 result = sharedAddr[3];
#ifdef TWLM_BOOT_DIAGNOSTICS
    sdReads[index].result = result;
#endif
    return result == 0;
}

//---------------------------------------------------------------------------------
bool my_sdio_WriteSectors(sec_t sector, sec_t numSectors,const void* buffer) {
//---------------------------------------------------------------------------------
	if (startupTimedOut) return false;
    if (!numSectors) return true;
    SdIoWait wait = beginIoWait();
	DC_FlushRange(buffer,numSectors * 512);

	sharedAddr[0] = sector;
	sharedAddr[1] = numSectors;
	sharedAddr[2] = (vu32)buffer;

	sharedAddr[3] = 0x52574453; // 'SDWR'
	// if (my_sdio_isNoSGba) {
		IPC_SendSync(5);
	/* } else {
		fifoSendValue32(FIFO_SDMMC, 5);
	} */
	if (!waitIoReply(0x52574453, &wait)) return false;

	int result = sharedAddr[3];

	return result == 0;
}


//---------------------------------------------------------------------------------
bool my_sdio_ClearStatus() {
//---------------------------------------------------------------------------------
	return true;
}

//---------------------------------------------------------------------------------
bool my_sdio_Shutdown() {
//---------------------------------------------------------------------------------
	return true;
}

const DISC_INTERFACE __my_io_dsisd = {
	DEVICE_TYPE_DSI_SD,
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE,
	(FN_MEDIUM_STARTUP)&my_sdio_Startup,
	(FN_MEDIUM_ISINSERTED)&my_sdio_IsInserted,
	(FN_MEDIUM_READSECTORS)&my_sdio_ReadSectors,
	(FN_MEDIUM_WRITESECTORS)&my_sdio_WriteSectors,
	(FN_MEDIUM_CLEARSTATUS)&my_sdio_ClearStatus,
	(FN_MEDIUM_SHUTDOWN)&my_sdio_Shutdown
};
