// Exercise production NAND setup with ARM7 replies separated from cached RAM.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <nds.h>
#ifndef NAND_SOURCE
#define NAND_SOURCE "../romsel_dsimenutheme/arm9/source/nand/nandio.c"
#endif
#include NAND_SOURCE

static const uint32_t expectedCid[4] = {0x12345678, 0x89abcdef, 0x11223344, 0x55667788};
static uint8_t keyReply[1024], diskSector[512];
static unsigned delays, reads, flushes, cryptoCalls;
static uint32_t stalledRequest;
static bool zeroCid, threeDS;
static const sec_t capacity = 10000;

void swiDelay(uint32_t duration) {
    assert(duration == 4096);
    ++delays;
    vu32 *command = (vu32*)0x0CFFFD0C;
    if (*command == stalledRequest) return;
    if (*command == 0x454D4D43) {
        if (!zeroCid) memcpy((void*)0x0CFFD7BC, expectedCid, 16);
    } else {
        assert(*command == 0x44494347 && flushes);
        memset(keyReply, 0x37, sizeof(keyReply));
    }
    *command = 0;
}
void DC_FlushRange(const void *buffer, unsigned length) {
    assert(buffer == crypt_buf && length == 1024); ++flushes;
}
void *memUncached(void *buffer) { assert(buffer == crypt_buf); return keyReply; }
bool nand_Startup(void) { return true; }
uint32_t nand_GetSectors(void) { return capacity; }
bool nand_ReadSectors(sec_t offset, sec_t length, void *buffer) {
    assert(offset < capacity && length && length <= capacity - offset);
    ++reads;
    memset(buffer, 0, length * 512);
    if (!offset) memcpy(buffer, diskSector, 512);
    return true;
}
void tonccpy(void *dst, const void *src, unsigned count) { memcpy(dst, src, count); }
void F_XY_reverse(uint32_t *key, uint32_t *out) {
    const uint8_t *bytes = (const uint8_t*)key;
    for (unsigned i = 0; i < 16; ++i) assert(bytes[i] == 0x37);
    memcpy(out, key, 16);
}
int parse_ncsd(const uint8_t sector[512], int verbose) {
    (void)sector; (void)verbose; return threeDS ? 0 : -1;
}
void dsi_crypt_init(const uint8_t *console, const uint8_t *cid, int is3DS_) {
    (void)console; assert(is3DS_ == threeDS);
    assert(memcmp(cid, expectedCid, 16) == 0); ++cryptoCalls;
}
void dsi_nand_crypt(uint8_t *out, const uint8_t *in, uint32_t offset, unsigned count) {
    (void)offset; memmove(out, in, count * 16);
}
static void reset(void) {
    memset((void*)0x02FFD000, 0, 0x3000); // Deliberately stale ARM9 cache view.
    memset((void*)0x0CFFD000, 0, 0x3000);
    memset(crypt_buf, 0, sizeof(crypt_buf));
    memset(diskSector, 0, sizeof(diskSector));
    mbr_t *m = (mbr_t*)diskSector;
    m->boot_signature_0 = 0x55; m->boot_signature_1 = 0xaa;
    m->partitions[0].offset = 100; m->partitions[0].length = 1000;
    reads = delays = flushes = cryptoCalls = 0;
    stalledRequest = 0; zeroCid = threeDS = false;
}
int main(void) {
    for (uintptr_t address = 0x02FFD000; address <= 0x0CFFD000; address += 0x0A000000)
        assert(mmap((void*)address, 0x3000, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0) == (void*)address);
    uint8_t data[512];
    reset(); assert(nandio_startup() && cryptoCalls == 1);
    assert(*(vu32*)0x02FFD7BC == 0); // Reply never came from this stale alias.
    assert(nandio_read_sectors(100, 1, data) && memcmp(data + 0x36, "FAT", 3) == 0);
    assert(nandio_read_sectors(capacity - 1, 1, data));
    unsigned before = reads;
    assert(!nandio_read_sectors(capacity - 1, 2, data));
    assert(!nandio_read_sectors(UINT32_MAX, 1, data) && reads == before);
    reset(); threeDS = true; assert(nandio_startup());
    assert(nandio_read_sectors(100, 1, data) && data[0x36] == 0);
    reset(); stalledRequest = 0x454D4D43;
    assert(!nandio_startup() && delays == 4096 && cryptoCalls == 0);
    assert(!twlmNandRequest(0x44494347, 0) && delays == 4096); // Busy mailbox.
    reset(); stalledRequest = 0x44494347;
    assert(!nandio_startup() && delays == 4097 && cryptoCalls == 0);
    reset(); zeroCid = true; assert(!nandio_startup() && !cryptoCalls);
    reset(); diskSector[511] = 0; assert(!nandio_startup());
    assert(!nandio_read_sectors(0, 1, data)); // Failed setup is not published.
    reset(); ((mbr_t*)diskSector)->partitions[0].offset = capacity;
    assert(!nandio_startup());
    reset(); ((mbr_t*)diskSector)->partitions[0].length = UINT32_MAX;
    assert(!nandio_startup());
    puts("NAND uncached replies, bounded waits, FAT fix and partition/read bounds passed");
}
