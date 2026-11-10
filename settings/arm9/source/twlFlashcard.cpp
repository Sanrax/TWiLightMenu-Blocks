#include "common/storage.h"
#include <nds.h>
#include <nds/arm9/dldi.h>
#include <fat.h>
#include <sys/stat.h>
#include <stdio.h>

#include "common/flashcard.h"
#include "common/bootstrappaths.h"
#include "common/inifile.h"
#include "common/systemdetails.h"
#include "common/tonccpy.h"
#include "read_card.h"

TWL_CODE bool UpdateCardInfo(char* gameid, char* gamename) {
	cardReadHeader((uint8*)&ndsCardHeader);
	tonccpy(gameid, ndsCardHeader.gameCode, 4);
	gameid[4] = 0x00;
	tonccpy(gamename, ndsCardHeader.gameTitle, 12);
	gamename[12] = 0x00;
	return true;
}

/*TWL_CODE void ShowGameInfo(const char gameid[], const char gamename[]) {
	printf("Game id: %s\nName:    %s", gameid, gamename);
}*/

TWL_CODE void twl_flashcardInit(void) {
	if (REG_SCFG_MC == 0x11 || sys().arm7SCFGLocked()) {
		return;
	}

	sysSetCardOwner (BUS_OWNER_ARM9);

	// Reset Slot-1 to allow reading title name and ID
	disableSlot1();
	{
		CIniFile settingsini( DSIMENUPP_INI );

		if (settingsini.GetInt("SRLOADER", "SECONDARY_ACCESS", 0) == false) {
			return;
		}
	}
	for(int i = 0; i < 25; i++) { swiWaitForVBlank(); }

	if (REG_SCFG_MC == 0x11) {
		sysSetCardOwner (BUS_OWNER_ARM7);
		return;
	}

	enableSlot1();
	for(int i = 0; i < 15; i++) { swiWaitForVBlank(); }

	// Follow HBMenu Deluxe's ARM7-first initialization order, without a later slot reset:
	// https://github.com/ApacheThunder/ntr-hb-menu/blob/deluxe/arm7/source/main.c
	// https://github.com/ApacheThunder/ntr-hb-menu/blob/deluxe/arm9/include/read_card.c
	{
		sysSetCardOwner(BUS_OWNER_ARM7);
		if (!fifoSendValue32(FIFO_USER_02, 0x54494E49)) {
			sysSetCardOwner(BUS_OWNER_ARM9);
			// printf("Failed to request ARM7 card initialization.\n");
			return;
		}
		fifoWaitValue32(FIFO_USER_02);
		u32 cardInitResult = fifoGetValue32(FIFO_USER_02);
		sysSetCardOwner(BUS_OWNER_ARM9);
		if (cardInitResult != 0) {
			// printf("ARM7 card initialization failed: %lu\n", (unsigned long)cardInitResult);
			return;
		}
	}

	char gamename[13];
	char gameid[5];

	UpdateCardInfo(&gameid[0], &gamename[0]);

	/*consoleDemoInit();
	printf("REG_SCFG_MC: %x\n", REG_SCFG_MC);
	ShowGameInfo(gameid, gamename);

	for (int i = 0; i < 60*5; i++) {
		swiWaitForVBlank();
	}*/

	sysSetCardOwner (BUS_OWNER_ARM7);	// 3DS fix

	if (gameid[0] >= 0x00 && gameid[0] < 0x20) {
		return;
	}

	bool doCardInit = false;
	bool isDSPico = false;

	// Read a DLDI driver specific to the cart
	if (!memcmp(gameid, "DSPI", 4) || !memcmp(gameid, "NTRJ", 4)) { // DSpico
		doCardInit = true;
		isDSPico = true;
		if (!twlmLoadDldi("nitro:/dldi/pico.dldi")) return;
	} else if (!memcmp(gamename, "QMATETRIAL", 9) || !memcmp(gamename, "R4DSULTRA", 9) // R4iDSN/R4 Ultra
	 || !memcmp(gameid, "ACEK", 4) || !memcmp(gameid, "YCEP", 4) || !memcmp(gameid, "AHZH", 4) || !memcmp(gameid, "CHPJ", 4) || !memcmp(gameid, "ADLP", 4)) { // Acekard 2(i)
		if (!twlmLoadDldi("nitro:/dldi/ak2.dldi")) return;
	} else if (!memcmp(gameid, "ASMA", 4)) {
		doCardInit = true;
		if (!twlmLoadDldi((!memcmp(gamename, "MEDIAPLAYER", 11)) ? "nitro:/dldi/gmtf.dldi" : "nitro:/dldi/r4tf.dldi")) return;
	} else if (!memcmp(gameid, "ASME", 4)) {
		doCardInit = true;
		if (!twlmLoadDldi("nitro:/dldi/CycloEvo.dldi")) return;
	} else if (!memcmp(gameid, "ABJJ", 4)) {
		doCardInit = true;
		if (!twlmLoadDldi("nitro:/dldi/ez5n.dldi")) return;
	} else if (!memcmp(gameid, "TTDS", 4)) {
		if (!twlmLoadDldi("nitro:/dldi/ttio.dldi")) return;
	} else if (!memcmp(gamename, "20130628ver", 11)) { // Some DSTTi clones report 0x0 of their flashrom instead of their rom title with certain homebrew header reads.... lol
		doCardInit = true; // Demon clones require card init else they hang on DLDI init.
		if (!twlmLoadDldi("nitro:/dldi/ttio.dldi")) return;
	} else if (!memcmp(gameid, "DSGB", 4)) {
		if (!twlmLoadDldi("nitro:/dldi/nrio.lz77")) return;
	} /*else if (!memcmp(gameid, "ALXX", 4)) { // SuperCard DSTWO
		if (!twlmLoadDldi("nitro:/dldi/dstwo.dldi")) return;
	}*/

	if (doCardInit) { // Certain flashcarts require card init before DLDI will work.
		int result = cardInitWithoutSlotReset();
		if (result != 0) {
			// printf("ARM9 card initialization failed: %d\n", result);
			return;
		}
		for (int i = 0; i < 30; i++) swiWaitForVBlank();
		if (isDSPico) picoInit(false);
	}

	twlmMountStorage("fat", dldiGetInternal());

	flashcardFoundReset();
	if (!flashcardFound()) {
		disableSlot1();
	}
}

void flashcardInit(void) {
	if (isDSiMode() && !flashcardFound()) {
		twl_flashcardInit();
	}
}
