#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nds/arm9/device_io.h>

// The DS device I/O ABI uses 32-bit descriptors. A non-PIE host binary and
// static allocation let us exercise that ABI without truncating heap pointers.
static uint32_t arena[16384];
static size_t allocated;
static unsigned registrations;
static void *testAllocate(size_t size) {
    size = (size + 3) & ~(size_t)3;
    assert(allocated + size <= sizeof(arena));
    void *p = (char *)arena + allocated;
    allocated += size;
    return p;
}
static void testFree(void *p) { (void)p; }
int deviceIoAdd(const device_io_t *device) {
    assert(device->isdrive("boot"));
    assert(!device->isdrive("nitro"));
    registrations++;
    return 0;
}
#define malloc testAllocate
#define free testFree
#include "../universal/source/common/bootfs.c"
#undef malloc
#undef free

static void put16(uint8_t *p, unsigned value) { p[0] = value; p[1] = value >> 8; }
static void put32(uint8_t *p, unsigned value) { put16(p, value); put16(p + 2, value >> 16); }

static void fixture(const char *path, bool corruptFat) {
    uint8_t image[256] = {};
    put32(image + 0x40, 0x50);
    put32(image + 0x44, 42);
    put32(image + 0x48, 0x80);
    put32(image + 0x4c, 16);
    uint8_t *names = image + 0x50;
    put32(names, 16); put16(names + 4, 0); put16(names + 6, 2);
    put32(names + 8, 31); put16(names + 12, 1); put16(names + 14, 0xf000);
    names[16] = 8; memcpy(names + 17, "load.bin", 8);
    names[25] = 0x83; memcpy(names + 26, "sub", 3); put16(names + 29, 0xf001);
    // Root terminator at 31; subdirectory table starts at 32.
    put32(names + 8, 32);
    names[32] = 9; memcpy(names + 33, "child.bin", 9);
    put32(image + 0x44, 43);
    put32(image + 0x80, 0xa0); put32(image + 0x84, corruptFat ? 999 : 0xaa);
    put32(image + 0x88, 0xaa); put32(image + 0x8c, 0xb1);
    memcpy(image + 0xa0, "loaderdatasubdata", 17);
    FILE *file = fopen(path, "wb"); assert(file);
    assert(fwrite(image, 1, sizeof(image), file) == sizeof(image));
    assert(fclose(file) == 0);
}

int main(int argc, char **argv) {
    assert(argc == 2);
    fixture(argv[1], false);
    assert(bootFSInit(argv[1]) == 1);
    assert(registrations == 1);
    struct stat st;
    assert(bootStat("boot:/sub", &st) == 0 && S_ISDIR(st.st_mode));
    int first = bootOpen("boot:/load.bin", O_RDONLY, 0);
    int second = bootOpen("boot:/sub/./child.bin", O_RDONLY, 0);
    assert(first >= 0 && second >= 0 && first != second);
    char data[32] = {};
    assert(bootRead(first, data, 4) == 4 && memcmp(data, "load", 4) == 0);
    assert(bootRead(second, data, sizeof(data)) == 7 && memcmp(data, "subdata", 7) == 0);
    assert(bootRead(second, data, 1) == 0);
    assert(bootSeek(first, -4, SEEK_END) == 6);
    assert(bootRead(first, data, sizeof(data)) == 4 && memcmp(data, "data", 4) == 0);
    assert(bootFstat(first, &st) == 0 && st.st_size == 10);
    assert(bootSeek(first, -11, SEEK_END) == -1 && errno == EINVAL);
    assert(bootFSInit(argv[1]) == 0 && errno == EBUSY);
    assert(bootClose(first) == 0 && bootClose(second) == 0);
    first = bootOpen("boot:/sub/../load.bin", O_RDONLY, 0); assert(first >= 0); bootClose(first);
    assert(bootOpen("boot:/load.bin", O_WRONLY, 0) == -1 && errno == EROFS);
    assert(bootOpen("boot:/sub", O_RDONLY, 0) == -1 && errno == EISDIR);
    assert(bootOpen("boot:/missing", O_RDONLY, 0) == -1 && errno == ENOENT);
    assert(bootOpen("boot:/load.bin/child", O_RDONLY, 0) == -1 && errno == ENOTDIR);
    fixture(argv[1], true);
    assert(bootFSInit(argv[1]) == 1 && registrations == 1);
    assert(bootOpen("boot:/load.bin", O_RDONLY, 0) == -1 && errno == EIO);
    FILE *bad = fopen(argv[1], "wb"); assert(bad); fputs("bad", bad); fclose(bad);
    assert(bootFSInit(argv[1]) == 0);
    assert(bootFSInit(NULL) == 0 && errno == EINVAL);
    puts("boot: lookup, independent handles, seek, read-only, bounds and remount checks passed");
}
