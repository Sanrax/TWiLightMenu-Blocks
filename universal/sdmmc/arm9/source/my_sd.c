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

static bool waitStartupReply(u32 command) {
	// No timer ownership changes. Allow several seconds even at the DSi
	// CPU clock, and never reuse a mailbox whose ARM7 request is still live.
	for (unsigned polls = 0; polls < 20000; ++polls) {
		if (sharedAddr[3] != command) return true;
		swiDelay(12000);
	}
	startupTimedOut = true;
	return false;
}

//---------------------------------------------------------------------------------
bool my_sdio_Startup() {
//---------------------------------------------------------------------------------
	if (startupTimedOut) return false;
	// Some DS-mode launchers leave SCFG unlocked and the booter enables
	// 16 MiB after SDK startup. Its original DS-sized alias then differs
	// from the active RAM mapping. Avoid probing open bus with a write.
	sharedAddr = (REG_SCFG_EXT & SCFG_EXT_RAM_TWL) ? (vu32*)0x0CFFFA00
		: (vu32*)memUncached((void*)0x02FFFA00);

	// my_sdio_isNoSGba = (strncmp((const char*)0x04FFFA00, "no$gba", 6) == 0);

	int result = 0;

	if (sharedAddr[1] == 0x49444C44) {
		sharedAddr[0] = (u32)io_dldi_data;
		sysSetCardOwner(BUS_OWNER_ARM7);
	} else {
		sharedAddr[3] = 0x56484453;
		// if (my_sdio_isNoSGba) {
			IPC_SendSync(1);
		/* } else {
			fifoSendValue32(FIFO_SDMMC, 1);
		} */
		if (!waitStartupReply(0x56484453)) return false;
		result = sharedAddr[3];

		if (result==0) return false;
	}

	sharedAddr[3] = 0x54534453;
	// if (my_sdio_isNoSGba) {
		IPC_SendSync(1);
	/* } else {
		fifoSendValue32(FIFO_SDMMC, 1);
	} */
	if (!waitStartupReply(0x54534453)) return false;

	result = sharedAddr[3];

	return result == 0;
}

//---------------------------------------------------------------------------------
bool my_sdio_IsInserted() {
//---------------------------------------------------------------------------------
	if (startupTimedOut) return false;
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
    DC_FlushRange(buffer, numSectors * 512);
    sharedAddr[0] = sector;
    sharedAddr[1] = numSectors;
    sharedAddr[2] = (u32)buffer;
    sharedAddr[3] = 0x44524453;
    IPC_SendSync(4);
    if (!waitIoReply(0x44524453, &wait)) return false;
    while (sharedAddr[3] == 0x474E4950) {
        if (!ioDeadline(&wait)) return false;
        sharedAddr[3] = 0x52434453;
        IPC_SendSync(4);
        if (!waitIoReply(0x52434453, &wait)) return false;
    }
    const u32 result = sharedAddr[3];
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
