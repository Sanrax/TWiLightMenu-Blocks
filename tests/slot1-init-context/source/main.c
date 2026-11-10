#include <nds.h>
#include <stdio.h>

static volatile u32 entered, first, last;
static char crash[64];
static volatile bool crashed;
static void discard(u32 value, void *context) { (void)value; (void)context; }
static void onCrash(int bytes, void *context) {
    (void)context;
    fifoGetDatamsg(FIFO_USER_08, sizeof(crash) - 1, (u8 *)crash);
    crashed = true;
    (void)bytes;
}
static void progress(u32 value, void *context) {
    (void)context;
    if ((value >> 28) == 1) entered = value;
    else if (((value >> 16) & 0xFFF) == 1) first = value;
    else last = value;
}

int main(void) {
    consoleDemoInit();
    fifoSetValue32Handler(FIFO_USER_03, discard, NULL);
    fifoSetValue32Handler(FIFO_USER_07, progress, NULL);
    fifoSetDatamsgHandler(FIFO_USER_08, onCrash, NULL);
    sysSetCardOwner(BUS_OWNER_ARM7);
    puts("Slot-1 init context regression\nReal menu ARM7 + real rand/TLS\nOnly cartridge I/O is simulated\n");
    for (unsigned run = 0; run < 8; run++) {
        entered = first = last = 0;
        fifoSendValue32(FIFO_USER_02, 0x54494E49);
        unsigned frames;
        for (frames = 0; frames < 180 && !crashed && !fifoCheckValue32(FIFO_USER_02); frames++)
            swiWaitForVBlank();
        if (crashed) {
            printf("ARM7 libc failure:\n%s\nHeader context: %08lX\n", crash, entered);
            break;
        }
        if (frames == 180) {
            printf("TIMEOUT run %u\nHeader context: %08lX\nFirst transfer: %08lX\nLast transfer: %08lX\n", run, entered, first, last);
            break;
        }
        u32 result = fifoGetValue32(FIFO_USER_02);
        bool pass = result == 0 && entered == 0x10000000 && first == 0x20010000 && last == 0x202B0000;
        printf("Run %u: %s / reply %lu\n", run + 1, pass ? "PASS" : "FAIL", result);
        if (!pass) {
            printf("Context %08lX %08lX %08lX\n", entered, first, last);
            break;
        }
        if (run == 7) puts("\nALL PASSED\n8 replies, 43 transfers each\nIRQ nesting zero throughout");
    }
    for (;;) swiWaitForVBlank();
}
