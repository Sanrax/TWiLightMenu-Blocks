/*
    Copyright (C) 2024 lifehackerhansol
    Additional modifications Copyright (C) 2025-2026 coderkei & Rocket Robz

    SPDX-License-Identifier: GPL-3.0-or-later
*/

#ifndef NO_PICO_LAUNCHER

#include <unistd.h>
#include <cstdio>
#include <malloc.h>
#include <memory>
#include <cstddef>
#include <string>
#include <nds/arm9/dldi.h>

#include <nds.h>

#include "common/picolaunch.h"
#include "common/tonccpy.h"
#include "DSpicoLauncher.h"
#include "cheat.h"

typedef void (*pico_loader_9_func_t)(void);

static_assert(offsetof(pload_header7_t, apiVersion) == 10);
static_assert(offsetof(pload_header7_t, loadParams) == 12);
static_assert(offsetof(pload_header7_t, v2) == pico::headerV1Size);
static_assert(offsetof(pload_header7_t, v3) == pico::headerV2Size);
static_assert(sizeof(pload_header7_t) == pico::headerV3Size);

struct FreeBuffer {
    void operator()(void* data) const { free(data); }
};
struct CloseFile {
    void operator()(FILE* file) const { fclose(file); }
};
using Cheats = std::unique_ptr<pload_cheats_t, FreeBuffer>;

struct Loader {
    std::unique_ptr<u8, FreeBuffer> data;
    size_t size = 0;
};

static Loader readLoader(const char* path, size_t minimumSize) {
    std::unique_ptr<FILE, CloseFile> file(fopen(path, "rb"));
    if (!file || fseek(file.get(), 0, SEEK_END) != 0) return {};
    const long size = ftell(file.get());
    if (size < 0 || size_t(size) < minimumSize || size_t(size) > pico::loaderCapacity
        || fseek(file.get(), 0, SEEK_SET) != 0) return {};

    // VRAM accepts halfword writes. Reserve and initialize the padding byte
    // instead of reading past the allocation for odd-sized loader files.
    const size_t paddedSize = (size_t(size) + 1) & ~size_t(1);
    Loader loader;
    loader.data.reset(static_cast<u8*>(memalign(32, paddedSize)));
    if (!loader.data || fread(loader.data.get(), 1, size, file.get()) != size_t(size))
        return {};
    if (size_t(size) != paddedSize) loader.data.get()[size] = 0;
    loader.size = size;
    return loader;
}

static Cheats prepareCheats(const std::string& romPath) {
    // The table has two header words, then a length word and opcode words
    // for each enabled cheat. Check every addition before allocating it.
    size_t tableBytes = 8;
    CheatCodelist cheat;
    if (!cheat.parse(romPath)) return {};
    const auto entries = cheat.getCheatsAlt();
    if (entries.empty() || entries.size() > UINT32_MAX) return {};
    for (const auto& entry : entries) {
        if (!pico::addCheatSize(tableBytes, entry._cheat.size())) return {};
    }

    Cheats cheats(static_cast<pload_cheats_t*>(memalign(4, tableBytes)));
    if (!cheats) return {};
    cheats->length = tableBytes;
    cheats->numberOfCheats = entries.size();
    auto* output = reinterpret_cast<u32*>(cheats.get()) + 2;
    for (const auto& entry : entries) {
        *output++ = entry._cheat.size() * sizeof(u32);
        for (const u32 word : entry._cheat) *output++ = word;
    }
    return cheats;
}

int picoLaunchRom(std::string romPath, std::string savePath, bool useCheats) {
    const char picoLoader7Path[] = "fat:/_pico/picoLoader7.bin";
    const char picoLoader9Path[] = "fat:/_pico/picoLoader9.bin";

    if (access(picoLoader7Path, F_OK) != 0 || access(picoLoader9Path, F_OK) != 0)
        return 1;

    pload_params_t sLoadParams{};
    if (!pico::setLoadPaths(sLoadParams, romPath, savePath)) return 2;

    auto loader9 = readLoader(picoLoader9Path, sizeof(u32));
    if (!loader9.data) return 9;
    auto loader7 = readLoader(picoLoader7Path, pico::headerV1Size);
    if (!loader7.data) return 7;
    const unsigned apiVersion = pico::loader7ApiVersion(loader7.data.get(), loader7.size);
    if (!apiVersion) return 7;
    auto cheats = useCheats && apiVersion >= 3 ? prepareCheats(romPath) : Cheats{};

	// soundDisable();
	irqDisable(IRQ_ALL & ~(IRQ_FIFO_EMPTY |IRQ_FIFO_NOT_EMPTY));

	vramSetBankA(VRAM_A_LCD);
	vramSetBankB(VRAM_B_LCD);
	vramSetBankC(VRAM_C_LCD);
	vramSetBankD(VRAM_D_LCD);

	tonccpy((void*)0x06800000, loader9.data.get(), (loader9.size + 1) & ~size_t(1));
	tonccpy((void*)0x06840000, loader7.data.get(), (loader7.size + 1) & ~size_t(1));
	loader9.data.reset();
	loader7.data.reset();

	//clear out ARM9 DMA channels
	for (int i = 0; i < 4; i++) {
		DMA_CR(i) = 0;
		DMA_SRC(i) = 0;
		DMA_DEST(i) = 0;
		TIMER_CR(i) = 0;
		TIMER_DATA(i) = 0;
	}

	((pload_header7_t*)0x06840000)->bootDrive = PLOAD_BOOT_DRIVE_DLDI;
	((pload_header7_t*)0x06840000)->dldiDriver = (void*)io_dldi_data;
	tonccpy(&((pload_header7_t*)0x06840000)->loadParams, &sLoadParams, sizeof(pload_params_t));
	if (apiVersion >= 3) {
		((pload_header7_t*)0x06840000)->v3.cheats = cheats.get();
	}

	DC_FlushAll();
	DC_InvalidateAll();
	IC_InvalidateAll();
	sysSetBusOwners(false, false);

	vramSetBankC(VRAM_C_ARM7_0x06000000);
	vramSetBankD(VRAM_D_ARM7_0x06020000);
	fifoSendValue32(FIFO_USER_02, 0x4F434950); // 'PICO'
	irqDisable(IRQ_ALL);

	((pico_loader_9_func_t)0x06800000)();
	return 0;
}

#endif
