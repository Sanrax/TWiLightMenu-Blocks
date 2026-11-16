#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef TWLM_ICON_TRACE
void iconTraceRead(const char *name, int slot, const void *banner, size_t bytes);
void iconTraceSelect(int bank, int slot, const char *name);
void iconTraceStage(int bank, const uint8_t *pixels, const uint16_t *palette, size_t bytes, bool skipped);
void iconTraceUpload(int bank, const uint8_t *source, const void *vram, const uint16_t *palette, size_t bytes);
void iconTraceFlush();
int iconTraceIoSelect(const char *name, int slot);
void iconTraceIoRestore(int slot);
struct IconTraceIoScope {
    int previous;
    IconTraceIoScope(const char *name, int slot) : previous(iconTraceIoSelect(name, slot)) {}
    ~IconTraceIoScope() { iconTraceIoRestore(previous); }
};
#else
inline void iconTraceRead(const char *, int, const void *, size_t) {}
inline void iconTraceSelect(int, int, const char *) {}
inline void iconTraceStage(int, const uint8_t *, const uint16_t *, size_t, bool) {}
inline void iconTraceUpload(int, const uint8_t *, const void *, const uint16_t *, size_t) {}
inline void iconTraceFlush() {}
struct IconTraceIoScope { IconTraceIoScope(const char *, int) {} };
#endif
