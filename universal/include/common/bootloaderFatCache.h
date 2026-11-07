#ifndef TWLM_BOOTLOADER_FAT_CACHE_H
#define TWLM_BOOTLOADER_FAT_CACHE_H
#include <stddef.h>

// Read-only FAT metadata cache for the small ARM7 handoff loaders. Keep it
// separate from their data/directory buffer: loading a file sector must not
// evict the FAT sector containing the next 128/256 cluster links.
static u32 bootFatCachedSector = (u32)-1;
static u32 bootFatSectorData[BYTES_PER_SECTOR / sizeof(u32)];

static void bootFatCacheInvalidate(void) {
    bootFatCachedSector = (u32)-1;
}

static const u8 *bootFatReadSector(u32 sector) {
    if (bootFatCachedSector != sector) {
        bootFatCacheInvalidate();
        if (!CARD_ReadSector(sector, bootFatSectorData)) return NULL;
        bootFatCachedSector = sector;
    }
    return (const u8 *)bootFatSectorData;
}

#endif
