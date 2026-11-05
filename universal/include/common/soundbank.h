#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace soundbank {

struct Header {
    unsigned sampleCount;
    uint32_t startupSamples;
};

inline uint32_t word(const uint8_t* bytes) {
    return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8)
        | (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24);
}

inline bool readHeader(const void* data, size_t size, Header& header) {
    if (!data || size < 20) return false;
    const auto* bytes = static_cast<const uint8_t*>(data);
    const unsigned count = bytes[0] | (unsigned(bytes[1]) << 8);
    if (count < 7 || bytes[2] || bytes[3] || std::memcmp(bytes + 4, "*maxmod*", 8)
        || word(bytes + 16) < 20) return false;
    header.sampleCount = count;
    // Retain the existing jingle delay calculation based on sample 1's offset.
    header.startupSamples = (word(bytes + 16) - 20) >> 1;
    return true;
}

// Custom banks are kept in memory for arbitrarily long skin paths. Check the
// offsets and MAS lengths before Maxmod receives pointers into that allocation.
inline bool validEffects(const void* data, size_t size) {
    Header header{};
    if (!readHeader(data, size, header)) return false;
    const auto* bytes = static_cast<const uint8_t*>(data);
    const size_t tableEnd = 12 + header.sampleCount * 4;
    if (tableEnd > size) return false;
    for (unsigned id = 0; id < header.sampleCount && id < 9; ++id) {
        const uint32_t offset = word(bytes + 12 + id * 4);
        if ((offset & 3) || offset < tableEnd || offset > size || size - offset < 28)
            return false;
        const uint32_t length = word(bytes + offset);
        if (length < 20 || length > size - offset - 8 || bytes[offset + 4] != 2)
            return false;
    }
    return true;
}

} // namespace soundbank
