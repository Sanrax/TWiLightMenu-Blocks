// Exercise the production native-card detector with card levels, not IRQ edges.
#include <assert.h>
#include <stdio.h>
#include "common/storage7.h"
#include "common/arm7status.h"

volatile u32 testScfg;
static bool dsiMode;
static u8 diskStatus;
static unsigned reads;

bool isDSiMode(void) { return dsiMode; }
u8 SDMMC_getDiskStatus(u8 device) {
    assert(device == SDMMC_DEV_CARD);
    ++reads;
    return diskStatus;
}

static unsigned poll(bool *wasInserted, u32 *status) {
    const unsigned event = twlmSdChange(wasInserted);
    if (event) *status = (*status & ~SD_MASK) | (event << SD_OFF);
    return event;
}

int main(void) {
    dsiMode = true;
    testScfg = SCFG_EXT_SDMMC;
    // Native card detect is valid before SD initialization, and a protected
    // card is still present. Neither condition is an insertion/removal event.
    diskStatus = SDMMC_STATUS_NOINIT;
    bool wasInserted = twlmSdInserted();
    assert(wasInserted);
    const u32 other = (19u << VOL_OFF) | 7u | (0x15u << INIT_OFF);
    u32 status = other;
    for (unsigned i = 0; i < 1000; ++i) {
        diskStatus = i & 1 ? SDMMC_STATUS_PROTECT : 0;
        assert(poll(&wasInserted, &status) == 0);
        assert(status == other); // ARM9 must see SDOk at browser entry.
    }

    diskStatus = SDMMC_STATUS_NODISK | SDMMC_STATUS_NOINIT;
    assert(poll(&wasInserted, &status) == 1);
    assert(!wasInserted && status == (other | (1u << SD_OFF)));
    for (unsigned i = 0; i < 30; ++i) {
        assert(poll(&wasInserted, &status) == 0);
        assert((status & SD_MASK) == (1u << SD_OFF));
    }
    diskStatus = 0;
    assert(poll(&wasInserted, &status) == 2);
    assert(wasInserted && status == (other | (2u << SD_OFF)));
    assert(poll(&wasInserted, &status) == 0);
    assert((status & SD_MASK) == (2u << SD_OFF)); // Latch a real reinsertion.

    // A new .srldr has a fresh baseline, even after the preceding program
    // reported insertion. Normal boot and Settings return both start at SDOk.
    wasInserted = twlmSdInserted();
    status = other;
    assert(poll(&wasInserted, &status) == 0 && status == other);

    diskStatus = SDMMC_STATUS_NODISK;
    wasInserted = twlmSdInserted();
    status = other;
    assert(!wasInserted && poll(&wasInserted, &status) == 0 && status == other);
    diskStatus = 0;
    assert(poll(&wasInserted, &status) == 2);

    // Unavailable SD hardware must not be accessed in DS mode or with locked
    // SCFG. The prepare hook only enables devices when SCFG itself is writable.
    reads = 0;
    dsiMode = false;
    assert(!twlmSdInserted() && reads == 0);
    dsiMode = true;
    testScfg = 0;
    assert(!twlmSdInserted() && reads == 0);
    twlmPrepareStorage();
    assert(testScfg == 0);
    testScfg = SCFG_EXT_SCFG_MBK_REG;
    twlmPrepareStorage();
    assert((testScfg & (SCFG_EXT_SDMMC | SCFG_EXT_NDMA))
        == (SCFG_EXT_SDMMC | SCFG_EXT_NDMA));

    puts("SD events: inserted boot, module return, removal/reinsertion, latched status and unavailable hardware passed");
    return 0;
}
