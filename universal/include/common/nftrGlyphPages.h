#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace twlm {

// The reserved glyph capacity stays in the common cache across menu handoffs.
// Only file pages needed by displayed text are populated. Call from the main
// thread, like the smaller DS glyph cache; a cache miss performs filesystem I/O.
class NftrGlyphPages {
    static constexpr uint32_t pageSize = 512;
    static constexpr uint32_t magic = 0x31475046; // FPG1
    struct Header {
        uint32_t signature, offset, bytes, cellSize;
    };
    Header* header = nullptr;
    uint8_t* valid = nullptr;
    uint8_t* tiles = nullptr;

    static uint32_t pageCount(uint32_t offset, uint32_t bytes) {
        return (uint64_t(offset % pageSize) + bytes + pageSize - 1) / pageSize;
    }
    static uint32_t bitmapSize(uint32_t offset, uint32_t bytes) {
        return ((pageCount(offset, bytes) + 31) / 32) * 4;
    }

public:
    static uint32_t storageSize(uint32_t offset, uint32_t bytes, uint32_t cellSize) {
        // fseek uses a signed 32-bit long on ARM9. Reject invalid geometry before
        // allocation or arithmetic, including a partial final glyph.
        if (!bytes || !cellSize || bytes % cellSize || uint64_t(offset) + bytes > INT32_MAX)
            return 0;
        return sizeof(Header) + bitmapSize(offset, bytes) + bytes;
    }

    bool attach(void* storage, uint32_t offset, uint32_t bytes, uint32_t cellSize,
                bool initialize) {
        header = nullptr;
        valid = tiles = nullptr;
        if (!storage || !storageSize(offset, bytes, cellSize))
            return false;
        auto* h = static_cast<Header*>(storage);
        if (initialize) {
            *h = {magic, offset, bytes, cellSize};
            std::memset(h + 1, 0, bitmapSize(offset, bytes));
        } else if (h->signature != magic || h->offset != offset ||
                   h->bytes != bytes || h->cellSize != cellSize) {
            return false;
        }
        header = h;
        valid = reinterpret_cast<uint8_t*>(h + 1);
        tiles = valid + bitmapSize(offset, bytes);
        return true;
    }

    bool active() const { return header != nullptr; }
    uint8_t* data() const { return tiles; }

    bool load(FILE* file, uint32_t glyph) {
        if (!header || !file || glyph >= header->bytes / header->cellSize)
            return false;
        const uint32_t begin = header->offset + glyph * header->cellSize;
        const uint32_t end = begin + header->cellSize;
        for (uint32_t page = begin / pageSize; page <= (end - 1) / pageSize; ++page) {
            const uint32_t bit = page - header->offset / pageSize;
            const uint8_t mask = 1u << (bit % 8);
            if (valid[bit / 8] & mask)
                continue;
            const uint32_t first = std::max(page * pageSize, header->offset);
            const uint32_t last = std::min((page + 1) * pageSize, header->offset + header->bytes);
            uint8_t* dst = tiles + first - header->offset;
            std::memset(dst, 0, last - first);
            if (std::fseek(file, first, SEEK_SET) != 0 ||
                std::fread(dst, 1, last - first, file) != last - first)
                return false; // Leave the page invalid so a later draw can retry.
            valid[bit / 8] |= mask;
        }
        return true;
    }
};

} // namespace twlm
