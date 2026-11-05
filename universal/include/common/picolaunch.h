#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

#include "picoLoader7.h"

namespace pico {

// Each CPU receives two 128 KiB VRAM banks. These offsets describe the
// 32-bit loader ABI even when validation is compiled on a 64-bit host.
constexpr size_t loaderCapacity = 0x40000;
constexpr size_t headerV1Size = 12 + sizeof(pload_params_t);
constexpr size_t headerV2Size = headerV1Size + sizeof(pload_header7_v2_t);
constexpr size_t headerV3Size = headerV2Size + sizeof(uint32_t);

inline unsigned loader7ApiVersion(const uint8_t* data, size_t size) {
    if (!data || size < headerV1Size || size > loaderCapacity) return 0;
    const unsigned version = data[10] | (unsigned(data[11]) << 8);
    if (version == 0 || (version >= 2 && size < headerV2Size)
        || (version >= 3 && size < headerV3Size)) return 0;
    return version;
}

inline bool setLoadPaths(pload_params_t& params, std::string_view rom,
                         std::string_view save) {
    if (rom.empty() || rom.size() >= sizeof(params.romPath)
        || save.size() >= sizeof(params.savePath)
        || rom.find('\0') != rom.npos || save.find('\0') != save.npos) return false;
    params = {};
    std::memcpy(params.romPath, rom.data(), rom.size());
    if (!save.empty()) std::memcpy(params.savePath, save.data(), save.size());
    return true;
}

inline bool addCheatSize(size_t& tableBytes, size_t words) {
    // One length word precedes each cheat's opcode words. The table's
    // length field and each individual length field are both 32 bits.
    if (tableBytes > UINT32_MAX - sizeof(uint32_t)
        || words > (UINT32_MAX - tableBytes - sizeof(uint32_t)) / sizeof(uint32_t))
        return false;
    tableBytes += sizeof(uint32_t) + words * sizeof(uint32_t);
    return true;
}

} // namespace pico
