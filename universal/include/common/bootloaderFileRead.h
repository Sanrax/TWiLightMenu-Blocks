/*
 * Small-loader FAT reader, derived from fat.c.
 * Copyright (C) 2005 Michael "Chishm" Chisholm
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef TWLM_BOOTLOADER_FILE_READ_H
#define TWLM_BOOTLOADER_FILE_READ_H

#include <stdint.h>

// Stay within the largest transfer already used for a 64 KiB FAT cluster.
// Drivers that reject a batch are retried one sector at a time. Unaligned
// destinations use the existing aligned scratch buffer for DMA-based drivers.
enum { BOOT_FAT_MAX_READ_SECTORS = 128 };

static bool bootFatValidCluster(u32 cluster) {
    return cluster >= CLUSTER_FIRST && cluster < CLUSTER_EOF;
}

static u32 bootFatReadSectors(char *destination, u32 sector, u32 count) {
    const bool aligned = ((uintptr_t)destination & 3) == 0;
    if (aligned && CARD_ReadSectors(sector, count, destination)) return count * BYTES_PER_SECTOR;
    u32 copied = 0;
    for (u32 i = 0; i < count; i++) {
        if (aligned) {
            if (!CARD_ReadSector(sector + i, destination + copied)) break;
        } else {
            if (!CARD_ReadSector(sector + i, globalBuffer)) break;
            for (u32 byte = 0; byte < BYTES_PER_SECTOR; byte++)
                destination[copied + byte] = globalBuffer[byte];
        }
        copied += BYTES_PER_SECTOR;
    }
    return copied;
}

u32 fileRead(char *buffer, u32 cluster, u32 startOffset, u32 length) {
    if (!length || !bootFatValidCluster(cluster) || discBytePerClus <= 0 || discSecPerClus <= 0) return 0;
    for (u32 skip = startOffset / discBytePerClus; skip; skip--) {
        cluster = FAT_NextCluster(cluster);
        if (!bootFatValidCluster(cluster)) return 0;
    }

    u32 copied = 0;
    u32 sectorInCluster = (startOffset % discBytePerClus) / BYTES_PER_SECTOR;
    const u32 firstByte = startOffset % BYTES_PER_SECTOR;
    if (firstByte) {
        if (!CARD_ReadSector(FAT_ClustToSect(cluster) + sectorInCluster, globalBuffer)) return 0;
        u32 bytes = BYTES_PER_SECTOR - firstByte;
        if (bytes > length) bytes = length;
        for (; copied < bytes; copied++) buffer[copied] = globalBuffer[firstByte + copied];
        sectorInCluster++;
    }

    while (copied < length) {
        if (sectorInCluster >= (u32)discSecPerClus) {
            cluster = FAT_NextCluster(cluster);
            if (!bootFatValidCluster(cluster)) return copied;
            sectorInCluster = 0;
        }
        const u32 firstSector = FAT_ClustToSect(cluster) + sectorInCluster;
        const u32 remaining = length - copied;
        if (remaining < BYTES_PER_SECTOR) {
            if (!CARD_ReadSector(firstSector, globalBuffer)) return copied;
            for (u32 byte = 0; byte < remaining; byte++) buffer[copied + byte] = globalBuffer[byte];
            return length;
        }

        u32 limit = remaining / BYTES_PER_SECTOR;
        if (limit > BOOT_FAT_MAX_READ_SECTORS) limit = BOOT_FAT_MAX_READ_SECTORS;
        u32 sectors = discSecPerClus - sectorInCluster;
        if (sectors > limit) sectors = limit;
        u32 lastCluster = cluster;
        u32 lastSector = sectorInCluster + sectors;
        // The separate FAT cache makes lookahead safe: it never overwrites
        // file data in globalBuffer. A discontinuity ends this transfer.
        while (sectors < limit && lastSector == (u32)discSecPerClus) {
            const u32 next = FAT_NextCluster(lastCluster);
            if (!bootFatValidCluster(next) || next != lastCluster + 1) break;
            u32 extend = limit - sectors;
            if (extend > (u32)discSecPerClus) extend = discSecPerClus;
            sectors += extend;
            lastCluster = next;
            lastSector = extend;
        }

        const u32 bytes = bootFatReadSectors(buffer + copied, firstSector, sectors);
        copied += bytes;
        if (bytes != sectors * BYTES_PER_SECTOR) return copied;
        cluster = lastCluster;
        sectorInCluster = lastSector;
    }
    return copied;
}

#endif
