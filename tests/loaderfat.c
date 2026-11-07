// Run the real small-loader FAT implementation against an in-memory card.
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nds/ndstypes.h"

#define BYTES_PER_SECTOR 512
#define CARD_H
enum { diskSectors = 32768 };
static unsigned char disk[diskSectors][512];
static unsigned reads[diskSectors];
static int failedSector = -1;
static unsigned fatSectors, dataStart, dataCommands, largestDataCommand;
static unsigned readCommands;
static unsigned driverLimit, alignmentErrors;
static bool CARD_StartUp(void) { return true; }
static bool CARD_ReadSectors(u32 sector, int count, void *buffer) {
    if (count <= 0 || sector >= diskSectors || (unsigned)count > diskSectors - sector) return false;
    readCommands++;
    if (sector >= dataStart) {
        dataCommands++;
        if ((unsigned)count > largestDataCommand) largestDataCommand = count;
    }
    if ((uintptr_t)buffer & 3) { alignmentErrors++; return false; }
    if (driverLimit && (unsigned)count > driverLimit) return false;
    for (int i = 0; i < count; i++) {
        reads[sector + i]++;
        if ((int)(sector + i) == failedSector) return false;
        memcpy((unsigned char *)buffer + i * 512, (unsigned char *)disk + (sector + i) * 512, 512);
    }
    return true;
}
static bool CARD_ReadSector(u32 sector, void *buffer) { return CARD_ReadSectors(sector, 1, buffer); }
#include BOOT_FAT_SOURCE

static void put16(unsigned char *p, unsigned n) { p[0] = n; p[1] = n >> 8; }
static void put32(unsigned char *p, unsigned n) { put16(p, n); put16(p + 2, n >> 16); }
static void linkCluster(unsigned bits, unsigned cluster, unsigned next) {
    unsigned char *fat = (unsigned char *)disk + 512;
    if (bits == 12) {
        const unsigned pos = cluster * 3 / 2;
        unsigned value = fat[pos] | (fat[pos + 1] << 8);
        value = cluster & 1 ? (value & 0xf) | (next << 4) : (value & 0xf000) | next;
        put16(fat + pos, value);
    } else if (bits == 16) put16(fat + cluster * 2, next);
    else put32(fat + cluster * 4, next | 0xa0000000u);
}
static unsigned endCluster(unsigned bits) { return bits == 12 ? 0xfff : bits == 16 ? 0xffff : 0x0fffffff; }
static void mount(unsigned bits, unsigned sectorsPerCluster) {
    memset(disk, 0, sizeof(disk));
    memset(reads, 0, sizeof(reads));
    failedSector = -1;
    driverLimit = 0;
    alignmentErrors = 0;
    unsigned char *boot = disk[0];
    put16(boot + 11, 512); boot[13] = sectorsPerCluster;
    put16(boot + 14, 1); boot[16] = 1; put16(boot + 17, bits == 32 ? 0 : 16);
    const unsigned clusters = bits == 12 ? 4000 : bits == 16 ? 6000 : 66000;
    fatSectors = ((clusters + 2) * bits / 8 + 511) / 512;
    dataStart = 1 + fatSectors + (bits == 32 ? 0 : 1);
    put32(boot + 32, dataStart + clusters * sectorsPerCluster);
    if (bits == 32) { put32(boot + 36, fatSectors); put32(boot + 44, 2); memcpy(boot + 82, "FAT32", 5); }
    else { put16(boot + 22, fatSectors); memcpy(boot + 54, bits == 12 ? "FAT12" : "FAT16", 5); }
    assert(FAT_InitFiles(true));
    assert(discFileSystem == (bits == 12 ? FS_FAT12 : bits == 16 ? FS_FAT16 : FS_FAT32));
    dataCommands = largestDataCommand = 0;
}
static unsigned fatReads(void) {
    unsigned total = 0;
    for (unsigned i = 1; i <= fatSectors; i++) total += reads[i];
    return total;
}
static void checkRead(const unsigned char *expected, unsigned first, unsigned offset, unsigned length, bool unaligned) {
    unsigned char *out = malloc(length + 32);
    assert(out); memset(out, 0xcd, length + 32);
    unsigned char *destination = out + 16 + unaligned;
    assert(fileRead((char *)destination, first, offset, length) == length);
    assert(!memcmp(destination, expected + offset, length));
    assert(destination[-1] == 0xcd && destination[length] == 0xcd);
    assert(alignmentErrors == 0);
    free(out);
}

int main(int argc, char **argv) {
    (void)argv;
    const bool measureOnly = argc > 1;
    for (unsigned bits = 12; bits <= 32; bits = bits == 12 ? 16 : bits == 16 ? 32 : 33) {
        const unsigned clusterSizes[] = {1, 4, 64, 128};
        for (unsigned size = 0; size < sizeof(clusterSizes) / sizeof(clusterSizes[0]); size++) {
            const unsigned spc = clusterSizes[size];
            for (unsigned fragmented = 0; fragmented < 3; fragmented++) {
                mount(bits, spc);
                enum {clusters = 200};
                unsigned chain[clusters];
                const unsigned length = clusters * spc * 512;
                unsigned char *expected = malloc(length); assert(expected);
                for (unsigned i = 0; i < length; i++) expected[i] = (i * 29 + i / 512 * 17) & 255;
                for (unsigned i = 0; i < clusters; i++) {
                    chain[i] = 4 + (fragmented == 0 ? i : fragmented == 1 ? i * 37 % 211 : (i / 8 * 7 % 29) * 8 + i % 8);
                    memcpy((unsigned char *)disk + FAT_ClustToSect(chain[i]) * 512, expected + i * spc * 512, spc * 512);
                }
                for (unsigned i = 0; i < clusters; i++) linkCluster(bits, chain[i], i + 1 < clusters ? chain[i + 1] : endCluster(bits));
                checkRead(expected, chain[0], 0, length, false);
                const unsigned linksRead = fatReads();
                if (!fragmented && spc == 1) {
                    printf("FAT%u contiguous 100 KiB: %u FAT sector reads, %u data commands\n", bits, linksRead, dataCommands);
                    if (!measureOnly) { assert(linksRead <= 3); assert(dataCommands <= 2); }
                }
                if (!measureOnly) assert(largestDataCommand <= 128);
                for (unsigned offset = 0; offset < 1500; offset += 127) {
                    checkRead(expected, chain[0], offset, 7503, false);
                    if (!measureOnly) checkRead(expected, chain[0], offset, 7503, true);
                }
                checkRead(expected, chain[0], length - 1027, 1027, false);
                checkRead(expected, chain[0], spc * 512 - 1, 2, false);
                unsigned longRead = 96 * 1024 + 37;
                if (longRead > length - 777) longRead = length - 777;
                checkRead(expected, chain[0], 777, longRead, false);
                if (!measureOnly) checkRead(expected, chain[0], 777, longRead, true);
                if (!measureOnly) {
                    driverLimit = 1;
                    checkRead(expected, chain[0], 0, 7503, false);
                    driverLimit = 0;
                    unsigned char out[1024]; memset(out, 0xcd, sizeof(out));
                    const unsigned beforeEmptyRead = readCommands;
                    assert(fileRead((char *)out, chain[0], length, 0) == 0);
                    assert(readCommands == beforeEmptyRead);
                    assert(fileRead((char *)out, chain[0], length, 1) == 0);
                    assert(out[0] == 0xcd);
                    assert(fileRead((char *)out, chain[0], length - 512, 1024) == 512);
                    assert(out[512] == 0xcd);
                    assert(fileRead((char *)out, 1, 0, 1) == 0);
                    assert(fileRead((char *)out, CLUSTER_FREE, 0, 1) == 0);
                    assert(fileRead((char *)out, CLUSTER_EOF, 0, 1) == 0);
                    failedSector = FAT_ClustToSect(chain[0]);
                    assert(fileRead((char *)out, chain[0], 0, 1) == 0);
                    // Whole-sector and final partial-sector failures return
                    // only the prefix that was actually copied.
                    failedSector = spc == 1 ? FAT_ClustToSect(chain[1]) : FAT_ClustToSect(chain[0]) + 1;
                    memset(out, 0xcd, sizeof(out));
                    assert(fileRead((char *)out, chain[0], 0, 1024) == 512);
                    assert(out[512] == 0xcd);
                    memset(out, 0xcd, sizeof(out));
                    assert(fileRead((char *)out, chain[0], 0, 513) == 512);
                    assert(out[512] == 0xcd);
                    failedSector = -1;
                }
                free(expected);
            }
        }
        if (measureOnly) continue;
        mount(bits, 1);
        // Directory reads must not overwrite cached FAT links, and vice versa.
        const unsigned first = bits == 12 ? 340 : bits == 16 ? 254 : 126;
        linkCluster(bits, first, first + 1);
        linkCluster(bits, first + 1, first + 2);
        linkCluster(bits, first + 2, endCluster(bits));
        assert(FAT_NextCluster(first) == first + 1);
        assert(CARD_ReadSector(discRootDir, globalBuffer));
        assert(FAT_NextCluster(first + 1) == first + 2);
        assert(FAT_NextCluster(first + 2) == CLUSTER_EOF);
        // FAT12's odd cluster 341 spans two FAT sectors.
        if (bits == 12) assert(FAT_NextCluster(341) == 342);
        // Remount the same sector number with different contents.
        mount(bits, 1); linkCluster(bits, first, first + 7);
        assert(FAT_NextCluster(first) == first + 7);
        // A failed FAT read cannot return an old cache entry; retry may recover.
        assert(FAT_InitFiles(false)); failedSector = 1 + first * bits / 8 / 512;
        assert(FAT_NextCluster(first) == CLUSTER_FREE);
        failedSector = -1; assert(FAT_NextCluster(first) == first + 7);
    }
    puts(measureOnly ? "Loader positive file comparisons passed (measurement mode)"
         : "Loader FAT12/16/32: contiguous/fragmented reads, partial sectors, EOF, remounts and I/O failures passed");
}
