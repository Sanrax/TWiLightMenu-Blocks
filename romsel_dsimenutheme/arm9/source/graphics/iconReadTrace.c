// Diagnostics only: keep the SDK driver and observe completed native SD reads.
#ifdef TWLM_ICON_TRACE
#include <stddef.h>
#include <nds/disc_io.h>

extern void twlmIconTraceDiskRead(uint32_t sector, uint32_t sectors, void *buffer, bool ok);
extern const DISC_INTERFACE *__real_get_io_dsisd(void);
static const DISC_INTERFACE *native;
static DISC_INTERFACE traced;

static bool traceRead(sec_t sector, sec_t sectors, void *buffer) {
    const bool ok = native->readSectors(sector, sectors, buffer);
    twlmIconTraceDiskRead(sector, sectors, buffer, ok);
    return ok;
}

const DISC_INTERFACE *__wrap_get_io_dsisd(void) {
    native = __real_get_io_dsisd();
    if (!native) return NULL;
    traced = *native;
    traced.readSectors = traceRead;
    return &traced;
}
#endif
