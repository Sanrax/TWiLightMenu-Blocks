// Build the same final strings as langInit(), in language.inl order.
#include "common/inifile.h"
#include "common/languageEscape.h"
#include "common/languagePack.h"

#include <cstdio>
#include <utility>
#include <vector>

static constexpr twlm::language::Entry entries[] = {
#define STRING(key, fallback) {#key, fallback, nullptr},
#include TWLM_LANGUAGE_INL
#undef STRING
};

static void append32(std::vector<unsigned char> &out, uint32_t value) {
    for (int i = 0; i < 4; ++i) out.push_back(value >> (8 * i));
}

int main(int argc, char **argv) {
    if (argc != 3) return 1;
    CIniFile ini;
    if (!ini.LoadIniFile(argv[1])) {
        std::fprintf(stderr, "Cannot read translation: %s\n", argv[1]);
        return 1;
    }
    std::vector<unsigned char> text, payload, pack;
    for (const auto &entry : entries) {
        std::string value = ini.GetString("LANGUAGE", entry.key, entry.fallback);
        if (value.empty()) value = entry.fallback;
        value = decodeLanguageEscapes(std::move(value));
        append32(payload, text.size());
        text.insert(text.end(), value.begin(), value.end());
    }
    if (text.size() > 1024 * 1024) return 1;
    append32(payload, text.size());
    payload.insert(payload.end(), text.begin(), text.end());
    append32(pack, 0x31474E4C); // LNG1
    append32(pack, sizeof(entries) / sizeof(entries[0]));
    append32(pack, twlm::language::schemaHash(entries, sizeof(entries) / sizeof(entries[0])));
    append32(pack, text.size());
    append32(pack, twlm::language::dataHash(payload.data(), payload.size()));
    pack.insert(pack.end(), payload.begin(), payload.end());
    FILE *file = std::fopen(argv[2], "wb");
    if (!file) return 1;
    bool ok = std::fwrite(pack.data(), 1, pack.size(), file) == pack.size();
    if (std::fclose(file) != 0) ok = false;
    if (!ok) std::remove(argv[2]);
    return ok ? 0 : 1;
}
