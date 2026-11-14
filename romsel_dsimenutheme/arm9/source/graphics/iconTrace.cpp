#include "iconTrace.h"
#ifdef TWLM_ICON_TRACE
#include <nds.h>
#include <cstdio>
#include <cstring>
#include "common/systemdetails.h"

namespace {
// A hardware-only investigation of the supplied static Battery Timer banner.
// IRQ hooks only copy bytes to bounded RAM. Checksums and all I/O run later.
struct Event {
    char type;
    int slot, bank, line;
    unsigned bytes;
    uint16_t storedCrc, readCrc, palette[16];
    alignas(4) uint8_t source[512], actual[512];
};
Event events[8], snapshot[8];
volatile unsigned count, dropped;
int slots[8];
bool tracked[8], started;

bool target(const char *name) { return name && std::strstr(name, "Battery Timer"); }

Event *reserve(char type, int slot, int bank, unsigned bytes) {
    const int irq = enterCriticalSection();
    Event *e = nullptr;
    if (count < 8) {
        e = &events[count++];
        *e = {};
        e->type = type; e->slot = slot; e->bank = bank;
        e->line = REG_VCOUNT; e->bytes = bytes;
    } else ++dropped;
    leaveCriticalSection(irq);
    return e;
}

uint16_t crc(const void *data, size_t bytes) {
    const auto *p = static_cast<const uint8_t *>(data);
    uint16_t value = 0xffff;
    while (bytes--) {
        value ^= *p++;
        for (int bit = 0; bit < 8; ++bit)
            value = (value >> 1) ^ ((value & 1) ? 0xa001 : 0);
    }
    return value;
}
}

void iconTraceRead(const char *name, int slot, const void *banner, size_t bytes) {
    if (!target(name) || bytes < 0x840) return;
    const auto *p = static_cast<const uint8_t *>(banner);
    if (p[0] != 1 || p[1] != 0) return;
    if (Event *e = reserve('R', slot, slot == 40 ? 7 : slot % 7, bytes)) {
        e->storedCrc = p[2] | (p[3] << 8);
        e->readCrc = crc(p + 0x20, 0x820);
        std::memcpy(e->source, p + 0x20, 512);
        std::memcpy(e->palette, p + 0x220, 32);
    }
}

void iconTraceSelect(int bank, int slot, const char *name) {
    if (bank < 0 || bank >= 8) return;
    tracked[bank] = target(name);
    slots[bank] = slot;
}

void iconTraceStage(int bank, const uint8_t *pixels, const uint16_t *palette, size_t bytes, bool skipped) {
    if (bank < 0 || bank >= 8 || !tracked[bank] || bytes != 512) return;
    if (Event *e = reserve(skipped ? 'S' : 'C', slots[bank], bank, bytes)) {
        std::memcpy(e->source, pixels, 512);
        std::memcpy(e->palette, palette, 32);
    }
}

void iconTraceUpload(int bank, const uint8_t *source, const void *vram, const uint16_t *palette, size_t bytes) {
    if (bank < 0 || bank >= 8 || !tracked[bank] || bytes != 512) return;
    if (Event *e = reserve('U', slots[bank], bank, bytes)) {
        std::memcpy(e->source, source, 512);
        std::memcpy(e->actual, vram, 512);
        std::memcpy(e->palette, palette, 32);
    }
}

void iconTraceFlush() {
    const int irq = enterCriticalSection();
    const unsigned n = count, lost = dropped;
    if (n) std::memcpy(snapshot, events, n * sizeof(Event));
    count = dropped = 0;
    leaveCriticalSection(irq);
    if (!n) return;
    char path[96];
    std::snprintf(path, sizeof path, "%s:/_nds/TWiLightMenu/icon-diagnostic.log", sys().isRunFromSD() ? "sd" : "fat");
    FILE *file = std::fopen(path, started ? "ab" : "wb");
    if (!file) return;
    started = true;
    std::fseek(file, 0, SEEK_END);
    if (std::ftell(file) > 65536) { std::fclose(file); return; }
    std::fprintf(file, "r25 page: %u events, %u dropped\n", n, lost);
    for (unsigned i = 0; i < n; ++i) {
        const Event &e = snapshot[i];
        std::fprintf(file, "%c slot=%d bank=%d line=%d bytes=%u source=%04X palette=%04X",
            e.type, e.slot, e.bank, e.line, e.bytes, crc(e.source, 512), crc(e.palette, 32));
        if (e.type == 'R')
            std::fprintf(file, " banner-stored=%04X banner-read=%04X", e.storedCrc, e.readCrc);
        if (e.type == 'U') {
            unsigned differences = 0;
            for (unsigned j = 0; j < 512; ++j) differences += e.source[j] != e.actual[j];
            std::fprintf(file, " vram=%04X differences=%u", crc(e.actual, 512), differences);
        }
        std::fputc('\n', file);
        // Preserve source pixels to identify which tiles changed, even when
        // a valid banner was uploaded but a later rendered icon looks wrong.
        for (uint8_t value : e.source) std::fprintf(file, "%02X", value);
        std::fputc('\n', file);
        if (e.type == 'U' && std::memcmp(e.source, e.actual, 512)) {
            for (uint8_t value : e.actual) std::fprintf(file, "%02X", value);
            std::fputc('\n', file);
        }
    }
    std::fclose(file);
}
#endif
