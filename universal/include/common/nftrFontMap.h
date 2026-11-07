#ifndef TWLM_NFTR_FONT_MAP_H
#define TWLM_NFTR_FONT_MAP_H

#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace twlm::nftr {
inline uint16_t read16(const uint8_t *p) { return p[0] | (uint16_t(p[1]) << 8); }
inline uint32_t read32(const uint8_t *p) { return read16(p) | (uint32_t(read16(p + 2)) << 16); }

inline bool readMap(FILE *file, uint16_t *map, uint32_t glyphs, uint32_t fileSize) {
    uint8_t buffer[512];
    if (fileSize < 0x2C || std::fseek(file, 0x28, SEEK_SET)
        || std::fread(buffer, 1, 4, file) != 4) return false;
    uint32_t offset = read32(buffer);
    // A valid chain cannot contain more map headers than fit in the file.
    // This also bounds cycles without allocating a visited-block table.
    uint32_t blocksLeft = fileSize / 12;
    while (offset) {
        if (!blocksLeft-- || offset > fileSize - 12 || std::fseek(file, offset, SEEK_SET)
            || std::fread(buffer, 1, 12, file) != 12) return false;
        const uint32_t first = read16(buffer), last = read16(buffer + 2);
        const uint32_t type = read32(buffer + 4), next = read32(buffer + 8);
        const uint32_t remaining = fileSize - offset - 12;
        if (next == offset) return false;
        if (type == 0) {
            if (first > last || remaining < 2 || std::fread(buffer, 1, 2, file) != 2) return false;
            const uint32_t base = read16(buffer), count = last - first + 1;
            if (base >= glyphs || count > glyphs - base) return false;
            for (uint32_t i = 0; i < count; i++) map[base + i] = first + i;
        } else if (type == 1) {
            if (first > last) return false;
            const uint32_t count = last - first + 1;
            if (count > remaining / 2) return false;
            for (uint32_t i = 0; i < count;) {
                const uint32_t batch = std::min<uint32_t>(count - i, sizeof(buffer) / 2);
                if (std::fread(buffer, 2, batch, file) != batch) return false;
                for (uint32_t j = 0; j < batch; j++) {
                    const uint32_t tile = read16(buffer + j * 2);
                    if (tile == 0xFFFF) continue; // NFTR's unmapped-character marker
                    if (tile >= glyphs) return false;
                    map[tile] = first + i + j;
                }
                i += batch;
            }
        } else if (type == 2) {
            if (remaining < 2 || std::fread(buffer, 1, 2, file) != 2) return false;
            const uint32_t count = read16(buffer);
            if (count > (remaining - 2) / 4) return false;
            for (uint32_t i = 0; i < count;) {
                const uint32_t batch = std::min<uint32_t>(count - i, sizeof(buffer) / 4);
                if (std::fread(buffer, 4, batch, file) != batch) return false;
                for (uint32_t j = 0; j < batch; j++) {
                    const uint32_t tile = read16(buffer + j * 4 + 2);
                    if (tile >= glyphs) return false;
                    map[tile] = read16(buffer + j * 4);
                }
                i += batch;
            }
        } else return false;
        offset = next;
    }
    return true;
}
} // namespace twlm::nftr

// Keep only one sector of scratch space, including for large custom fonts.
// Failed/truncated maps leave a deterministic fallback instead of stale data.
inline bool loadNftrCharacterMap(FILE *file, uint16_t *map, uint32_t glyphs, uint32_t fileSize) {
    if (!map || !glyphs || glyphs > 65536) return false;
    std::fill_n(map, glyphs, uint16_t(0xFFFF));
    if (twlm::nftr::readMap(file, map, glyphs, fileSize)) return true;
    std::fill_n(map, glyphs, uint16_t(0xFFFF));
    return false;
}

inline uint16_t findNftrGlyph(const uint16_t *map, uint32_t glyphs, char16_t character, uint16_t fallback) {
    uint32_t first = 0, last = glyphs;
    while (first < last) {
        const uint32_t mid = first + (last - first) / 2;
        if (map[mid] == character) return mid;
        if (map[mid] < character) first = mid + 1;
        else last = mid;
    }
    return fallback;
}

#endif
