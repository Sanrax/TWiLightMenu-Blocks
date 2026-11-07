#include "common/inifile.h"
#include "common/languageEscape.h"
#include "common/languagePack.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

#define STRING(key, fallback) {#key, fallback, nullptr},
namespace menu { constexpr twlm::language::Entry entries[] = {
#include "../romsel_dsimenutheme/arm9/source/language.inl"
}; }
namespace settings { constexpr twlm::language::Entry entries[] = {
#include "../settings/arm9/source/language.inl"
}; }
namespace classic { constexpr twlm::language::Entry entries[] = {
#include "../quickmenu/arm9/source/language.inl"
}; }
namespace title { constexpr twlm::language::Entry entries[] = {
#include "../title/arm9/source/language.inl"
}; }
namespace manual { constexpr twlm::language::Entry entries[] = {
#include "../manual/arm9/source/language.inl"
}; }
namespace imageview { constexpr twlm::language::Entry entries[] = {
#include "../imageview/arm9/source/language.inl"
}; }
#undef STRING

static void write32(std::vector<unsigned char> &bytes, size_t at, uint32_t value) {
    for (int i = 0; i < 4; ++i) bytes[at + i] = value >> (i * 8);
}

int main(int argc, char **argv) {
    assert(argc >= 4 && argc % 2 == 0);
    std::vector<twlm::language::Entry> entries;
#define APP(name, ns) if (std::string(argv[1]) == name) entries.assign(std::begin(ns::entries), std::end(ns::entries))
    APP("romsel_dsimenutheme", menu);
    APP("settings", settings);
    APP("quickmenu", classic);
    APP("title", title);
    APP("manual", manual);
    APP("imageview", imageview);
#undef APP
    assert(!entries.empty());
    const uint32_t schema = twlm::language::schemaHash(entries.data(), entries.size());
    std::vector<std::string> outputs(entries.size());
    for (size_t i = 0; i < entries.size(); ++i) entries[i].output = &outputs[i];
    size_t strings = 0;
    for (int arg = 2; arg < argc; arg += 2) {
        CIniFile ini;
        assert(ini.LoadIniFile(argv[arg]));
        assert(twlm::language::loadPack(argv[arg + 1], entries.data(), entries.size(), schema));
        for (size_t i = 0; i < entries.size(); ++i) {
            std::string expected = ini.GetString("LANGUAGE", entries[i].key, entries[i].fallback);
            if (expected.empty()) expected = entries[i].fallback;
            assert(outputs[i] == decodeLanguageEscapes(std::move(expected)));
            ++strings;
        }
    }

    // Failed loads must leave every output untouched for the INI fallback.
    const auto original = outputs;
    const auto failed = [&](const char *path, uint32_t hash = 0) {
        assert(!twlm::language::loadPack(path, entries.data(), entries.size(), hash ? hash : schema));
        assert(outputs == original);
    };
    const char *goodPath = argv[3];
    failed(goodPath, schema ^ 1);
    std::ifstream file(goodPath, std::ios::binary);
    std::vector<unsigned char> good((std::istreambuf_iterator<char>(file)), {});
    const std::string badPath = std::string(goodPath) + ".bad";
    auto reject = [&](const std::vector<unsigned char> &bytes) {
        FILE *bad = std::fopen(badPath.c_str(), "wb"); assert(bad);
        assert(std::fwrite(bytes.data(), 1, bytes.size(), bad) == bytes.size());
        assert(std::fclose(bad) == 0);
        failed(badPath.c_str());
    };
    for (size_t end : {size_t(0), size_t(1), size_t(19), good.size() - 1})
        reject(std::vector<unsigned char>(good.begin(), good.begin() + end));
    auto bad = good; bad.push_back(0); reject(bad);
    bad = good; bad[0] ^= 1; reject(bad);
    bad = good; bad.back() ^= 1; reject(bad);
    bad = good; write32(bad, 4, entries.size() + 1); reject(bad);
    bad = good; write32(bad, 12, 0xFFFFFFFF); reject(bad);
    auto rejectOffsets = [&](size_t offset, uint32_t value) {
        bad = good; write32(bad, offset, value);
        write32(bad, 16, twlm::language::dataHash(bad.data() + 20, bad.size() - 20));
        reject(bad);
    };
    rejectOffsets(20, 1);
    rejectOffsets(24, 0xFFFFFFFF);
    rejectOffsets(20 + entries.size() * 4, 0);
    rejectOffsets(28, 0); // Descending offsets
    std::remove(badPath.c_str());
    failed(badPath.c_str()); // Missing pack

    // Exercise the program-facing loader with a host NitroFS directory.
    const auto cwd = std::filesystem::current_path();
    const auto fixture = std::filesystem::path(goodPath).parent_path() / (std::string("fallback-") + argv[1]);
    const auto language = fixture / "nitro:/languages/test";
    std::filesystem::create_directories(language);
    std::filesystem::copy_file(argv[argc - 2], language / "language.ini");
    std::filesystem::copy_file(argv[argc - 1], language / "language.lng");
    std::filesystem::current_path(fixture);
    twlm::language::loadTranslations("test", entries.data(), entries.size(), schema);
    assert(outputs == original);
    std::filesystem::remove("nitro:/languages/test/language.lng");
    for (auto &out : outputs) out = "uninitialized";
    twlm::language::loadTranslations("test", entries.data(), entries.size(), schema);
    assert(outputs == original);
    { std::ofstream invalid("nitro:/languages/test/language.lng"); invalid << "old or damaged pack"; }
    for (auto &out : outputs) out = "uninitialized";
    twlm::language::loadTranslations("test", entries.data(), entries.size(), schema);
    assert(outputs == original);
    twlm::language::loadTranslations("missing", entries.data(), entries.size(), schema);
    for (size_t i = 0; i < entries.size(); ++i)
        assert(outputs[i] == decodeLanguageEscapes(entries[i].fallback));
    std::filesystem::current_path(cwd);
    std::cout << argv[1] << ": " << strings << " prepared strings match INI/defaults; invalid packs leave outputs unchanged\n";
}
