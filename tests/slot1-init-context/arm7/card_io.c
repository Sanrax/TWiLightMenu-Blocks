// Test only: emulate card I/O, retaining the real menu ARM7, card setup,
// encryption, timers, libc rand/TLS and FIFO request/reply path.
#include <nds.h>
#include <string.h>

extern volatile u16 irq_nesting_level;
static unsigned transfers;

void __wrap_libndsCrash(const char *message) {
    fifoSendDatamsg(FIFO_USER_08, strlen(message) + 1, (u8 *)message);
    for (;;) swiWaitForVBlank();
}

void __wrap_cardReadHeader(u8 *data) {
    memset(data, 0, 512);
    tNDSHeader *header = (tNDSHeader *)data;
    memcpy(header->gameCode, "DSPI", 4);
    header->cardControl13 = CARD_nRESET | CARD_CLK_SLOW;
    header->cardControlBF = CARD_CLK_SLOW;
    header->headerCRC16 = swiCRC16(0xFFFF, data, 0x15E);
    REG_ROMCTRL = CARD_nRESET;
    transfers = 0;
    fifoSendValue32(FIFO_USER_07, 0x10000000 | irq_nesting_level);
}

u32 __wrap_cardReadID(u32 flags) {
    (void)flags;
    return 0xC00000C2;
}

void __wrap_cardPolledTransfer(u32 flags, u32 *destination, u32 length, const u8 *command) {
    (void)flags; (void)destination; (void)length; (void)command;
    transfers++;
    if (transfers == 1 || transfers == 43)
        fifoSendValue32(FIFO_USER_07, 0x20000000 | (transfers << 16) | irq_nesting_level);
}
