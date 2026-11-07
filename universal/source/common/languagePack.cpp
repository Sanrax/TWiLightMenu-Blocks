#include "common/languagePack.h"
#include "common/inifile.h"
#include "common/languageEscape.h"

#include <cstdio>
#include <memory>
#include <new>
#include <utility>

namespace twlm::language {
namespace {
uint32_t read32(const unsigned char *p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8)
        | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
}

bool loadPack(const char *path, const Entry *entries, size_t count, uint32_t schema) {
    if (!entries || !count || count > 4096) return false;
    FILE *file = std::fopen(path, "rb");
    if (!file) return false;
    unsigned char header[20];
    bool valid = std::fread(header, 1, sizeof(header), file) == sizeof(header);
    if (!valid || read32(header) != 0x31474E4C || read32(header + 4) != count
        || read32(header + 8) != schema) {
        std::fclose(file);
        return false;
    }

    const uint32_t textSize = read32(header + 12);
    // Bundled packs are much smaller. Bound allocation for damaged files.
    if (textSize > 1024 * 1024) {
        std::fclose(file);
        return false;
    }
    const size_t tableSize = (count + 1) * 4;
    const size_t payloadSize = tableSize + textSize;
    std::unique_ptr<unsigned char[]> payload(new (std::nothrow) unsigned char[payloadSize]);
    valid = payload && std::fread(payload.get(), 1, payloadSize, file) == payloadSize
        && std::fgetc(file) == EOF && !std::ferror(file);
    if (std::fclose(file) != 0) valid = false;
    if (!valid || dataHash(payload.get(), payloadSize) != read32(header + 16)
        || read32(payload.get()) != 0 || read32(payload.get() + count * 4) != textSize)
        return false;
    for (size_t i = 0; i < count; ++i) {
        const uint32_t start = read32(payload.get() + i * 4);
        const uint32_t end = read32(payload.get() + (i + 1) * 4);
        if (start > end || end > textSize || !entries[i].output) return false;
    }
    const char *text = reinterpret_cast<const char *>(payload.get() + tableSize);
    for (size_t i = 0; i < count; ++i) {
        const uint32_t start = read32(payload.get() + i * 4);
        const uint32_t end = read32(payload.get() + (i + 1) * 4);
        entries[i].output->assign(text + start, end - start);
    }
    return true;
}

void loadTranslations(const char *language, const Entry *entries, size_t count, uint32_t schema) {
    char path[96];
    std::snprintf(path, sizeof(path), "nitro:/languages/%s/language.lng", language);
    if (loadPack(path, entries, count, schema)) return;
    std::snprintf(path, sizeof(path), "nitro:/languages/%s/language.ini", language);
    CIniFile ini(path);
    for (size_t i = 0; i < count; ++i) {
        std::string value = ini.GetString("LANGUAGE", entries[i].key, entries[i].fallback);
        if (value.empty()) value = entries[i].fallback;
        *entries[i].output = decodeLanguageEscapes(std::move(value));
    }
}
} // namespace twlm::language
