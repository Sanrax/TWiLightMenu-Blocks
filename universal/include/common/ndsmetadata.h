#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace ndsmetadata {

constexpr size_t bannerOriginalSize = 0x840;
constexpr size_t bannerChineseSize = 0x940;
constexpr size_t bannerKoreanSize = 0xA40;
constexpr size_t bannerAnimatedSize = 0x23C0;

// Read only the banner version's actual data, rather than 9 KiB from every
// ROM. The caller supplies a full banner buffer so absent languages and
// animation frames cannot retain data from the previous directory entry.
inline size_t readBanner(FILE* file, uint32_t offset, void* output, size_t capacity,
                         bool animation) {
    if (!file || !output || capacity < bannerAnimatedSize) return 0;
    std::memset(output, 0, capacity);
    if (fseek(file, offset, SEEK_SET) != 0) return 0;
    auto* bytes = static_cast<uint8_t*>(output);
    constexpr size_t prefix = 0x20;
    if (fread(bytes, 1, prefix, file) != prefix) return 0;
    const unsigned version = bytes[0] | (unsigned(bytes[1]) << 8);
    const size_t size = version == 0x103 && animation ? bannerAnimatedSize
        : version == 0x103 || version == 3 ? bannerKoreanSize
        : version == 2 ? bannerChineseSize : bannerOriginalSize;
    return prefix + fread(bytes + prefix, 1, size - prefix, file);
}

} // namespace ndsmetadata
