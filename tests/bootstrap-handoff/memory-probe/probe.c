// SPDX-License-Identifier: GPL-2.0-or-later
// Pinned bootstrap 2.17.0 ARM7 probe. No SDK, IRQs or storage initialization.
typedef unsigned int u32;
typedef unsigned short u16;
#define WORD(address) (*(volatile u32 *)(address))
#define HALF(address) (*(volatile u16 *)(address))
#define RECORDS ((volatile u32 *)0x06037000)
#define FIELDS 14

// Only code/constant data is compared; mutable ARM9 BSS starts at 0x023BFBC8.
void capture(unsigned stage) {
    volatile u32 *out = RECORDS + stage * FIELDS;
    u32 bad = 0, first = 0, expected = 0, actual = 0;
    u32 sourceHash = 2166136261u, loadedHash = 2166136261u;
    out[0] = WORD(0x023BFBD0);
    out[1] = WORD(0x04004008); // ARM7 SCFG_EXT
    out[2] = HALF(0x04004004); // SCFG_CLK
    out[3] = HALF(0x04004000); // SCFG_ROM
    for (u32 offset = 0; offset < 0xBC8; offset += 4) {
        u32 src = WORD(0x06000240 + offset);
        u32 dst = WORD(0x023BF000 + offset);
        sourceHash = (sourceHash ^ src) * 16777619u;
        loadedHash = (loadedHash ^ dst) * 16777619u;
        if (src != dst) {
            if (!bad) {
                first = 0x023BF000 + offset;
                expected = src;
                actual = dst;
            }
            bad++;
        }
    }
    out[4] = bad;
    out[5] = first;
    out[6] = expected;
    out[7] = actual;
    out[8] = sourceHash;
    out[9] = loadedHash;
    out[10] = WORD(0x023BFB20); // ARM9 entry instruction
    out[11] = WORD(0x027BFB20); // read-only mirror sample
    out[12] = WORD(0x04004100); // NDMA global control
    out[13] = WORD(0x023BFBD0); // state again after comparison
}

static void logText(const char *text) {
    ((void (*)(const char *))0x0601F195)(text);
}

static void logHex(u32 value) {
    ((void (*)(u32))0x0601F1D5)(value);
}

__attribute__((noreturn)) void finish(void) {
    capture(2);
    logText("\nr18 ARM9 memory snapshots\n");
    for (unsigned stage = 0; stage < 3; stage++) {
        logText("stage "); logHex(stage); logText("\n");
        for (unsigned field = 0; field < FIELDS; field++) {
            logHex(field); logText(": ");
            logHex(RECORDS[stage * FIELDS + field]); logText("\n");
        }
    }
    // Repeat the trailer so an emulator's buffered final image write cannot
    // hide the last useful field if the owned process must be force-stopped.
    logText("r18 STOP\n"); logText("r18 STOP\n");
    for (;;) __asm__ volatile ("nop");
}
