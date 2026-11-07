#include "common/storage.h"
/*-----------------------------------------------------------------
 Copyright (C) 2005 - 2013
	Michael "Chishm" Chisholm
	Dave "WinterMute" Murphy
	Claudio "sverx"

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

------------------------------------------------------------------*/
#include <nds.h>
#include <nds/arm9/dldi.h>
#include <stdio.h>
#include <fat.h>
#include <sys/stat.h>
#include <limits.h>

#include <string.h>
#include <unistd.h>

#include "nds_loader_arm9.h"
#include "common/tonccpy.h"
#include "common/bootpath.h"

#ifdef TWLM_BOOT_DIAGNOSTICS
static const char* diagnosticLogPath = nullptr;

static void beginBootLog(const char* path) {
	diagnosticLogPath = path;
	FILE* file = fopen(path, "wb");
	if (!file) return;
	fprintf(file, "TWiLight boot diagnostics (Power/navigation r4)\nARM9 mode: %s\nSCFG_EXT: %08lX\nDLDI: %.48s\n",
		isDSiMode() ? "DSi" : "DS", (unsigned long)REG_SCFG_EXT, io_dldi_data->friendlyName);
	fclose(file);
}

extern "C" void bootDisableDiskLog(void) { diagnosticLogPath = nullptr; }
extern "C" void bootLogSdIo(void);
extern "C" bool bootSdIpcTimedOut(void);

extern "C" void bootStage(const char* stage) {
	printf("%s\n", stage);
	nocashMessage(stage);
	if (diagnosticLogPath) {
		FILE* file = fopen(diagnosticLogPath, "ab");
		if (file) {
			fprintf(file, "%s\n", stage);
			fclose(file);
		}
	}
}
#else
static inline void bootStage(const char* stage) {
	(void)stage;
}
#endif

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------

	//defaultExceptionHandler();
	fifoSendValue32(FIFO_PM, PM_REQ_SLEEP_DISABLE);
#ifdef TWLM_BOOT_DIAGNOSTICS
	consoleDemoInit();
	REG_MASTER_BRIGHT = 0;
	REG_MASTER_BRIGHT_SUB = 0;
	printf("TWiLight boot diagnostics\nARM9 mode: %s\nSCFG_EXT: %08lX\n",
		isDSiMode() ? "DSi" : "DS", (unsigned long)REG_SCFG_EXT);
#endif
	bootStage("ARM9 main entered");

	// overwrite reboot stub identifier
	extern char *fake_heap_end;
	*fake_heap_end = 0;

	REG_SCFG_CLK &= ~(BIT(1) | BIT(2));	// Disable DSP and Camera Interface clocks

	// Turn on screen backlights if they're disabled
	powerOn(PM_BACKLIGHT_TOP);
	powerOn(PM_BACKLIGHT_BOTTOM);
	
	REG_SCFG_CLK = 0x85;					// TWL clock speed
	REG_SCFG_EXT = 0x8307F100;				// Extended memory, 32-bit VRAM bus, etc.

	//consoleDemoInit();

	bootStage("Waiting for ARM7");
	bool isRegularDS = fifoGetValue32(FIFO_USER_07);
	bootStage("ARM7 ready");

	//iprintf("Initing flashcard...\n");
	bootStage("Mounting flashcard");
	bool flashcardFound = twlmMountStorage("fat", dldiGetInternal());
#ifdef TWLM_BOOT_DIAGNOSTICS
	if (flashcardFound) beginBootLog("fat:/_nds/TWiLightMenu/boot-diagnostic.log");
#endif
	bootStage(flashcardFound ? "Flashcard mounted" : "Flashcard unavailable");
	bool sdFound = false;
	if (isDSiMode() || (!isRegularDS && REG_SCFG_EXT != 0)) {
		extern const DISC_INTERFACE __my_io_dsisd;
		//iprintf("Initing SD...\n");
		bootStage("Mounting internal SD");
		sdFound = twlmMountStorage("sd", &__my_io_dsisd);
#ifdef TWLM_BOOT_DIAGNOSTICS
		if (sdFound && !flashcardFound) beginBootLog("sd:/_nds/TWiLightMenu/boot-diagnostic.log");
        bootLogSdIo();
        if (bootSdIpcTimedOut()) {
            printf("Restart console to retry.\n");
            while (1) swiWaitForVBlank();
        }
#endif
		bootStage(sdFound ? "Internal SD mounted" : "Internal SD unavailable");
	}
	const char* menuPath = bootpath::mainMenu(
		flashcardFound && access(bootpath::flashcardMenu, F_OK) == 0,
		sdFound && access(bootpath::sdMenu, F_OK) == 0,
		sdFound && access("sd:/_nds/primary", F_OK) == 0,
		argc > 0 && argv ? argv[0] : nullptr);

	if (!flashcardFound && !sdFound) {
		consoleDemoInit();
		printf ("FAT init failed!\n");
	} else if (!menuPath) {
		consoleDemoInit();
		printf("/_nds/TWiLightMenu/\nmain.srldr not found.\n");
	} else {
		u32* commonCacheTag = (u32*)0x02FF3FFC;
		u32* _lastUsedCommonCache = (u32*)0x02FF3FF4;

		//iprintf(sdFound ? "Running from SD...\n" : "Running from flashcard...\n");
		bootStage(menuPath);
		bootStage("Loading main.srldr");
		int err = runNdsFile(menuPath, 1, &menuPath,
			(*commonCacheTag == 0x414C5253) ? *_lastUsedCommonCache : 0);

		consoleDemoInit();

		if (err == 1) {
			printf ("fat:/_nds/TWiLightMenu/\nmain.srldr not found.\n");
		} else {
			printf ("Start failed. Error %i\n", err);
		}
	}

	printf(isRegularDS ? "\nPress B to power off." : "\nPress B to return to menu.");

	while (1) {
		scanKeys();
		if (keysDown() & KEY_B) {
			fifoSendValue32(FIFO_USER_01, 1);	// Tell ARM7 to reboot or poweroff depending on whether DS or DSi
			break;
		}
		swiWaitForVBlank();
	}

	for (int i = 0; i < 10; i++) {
		swiWaitForVBlank();
	}

	return 0;
}
