#include "common/nitroFile.h"

#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct File { std::string name; uint64_t id; unsigned type = DT_REG; };
struct Cursor { std::vector<File> files; unsigned at = 0; dirent entry{}; };
static std::map<std::string, std::vector<File>> contents;
static std::map<std::string, std::string> fallback;
static std::map<uint16_t, std::string> payloads;
static std::map<DIR*, std::unique_ptr<Cursor>> cursors;
static unsigned scans, pathOpens, idOpens;
static bool failRead, failClose, failId, failStream, unavailable;
static bool alreadyTagged;
static unsigned descriptorCloses;

static FILE* payload(const std::string& text) {
    FILE* file = std::tmpfile(); assert(file);
    assert(std::fwrite(text.data(), 1, text.size(), file) == text.size());
    std::rewind(file); return file;
}
extern "C" DIR* __wrap_opendir(const char* path) {
    ++scans;
    const auto found = contents.find(path);
    if (found == contents.end()) { errno = unavailable ? ENODEV : ENOENT; return nullptr; }
    auto cursor = std::make_unique<Cursor>(); cursor->files = found->second;
    DIR* ptr = reinterpret_cast<DIR*>(cursor.get());
    cursors.emplace(ptr, std::move(cursor)); return ptr;
}
extern "C" dirent* __wrap_readdir(DIR* ptr) {
    auto& cursor = *cursors.at(ptr);
    if (failRead && cursor.at == 1) { failRead = false; errno = EIO; return nullptr; }
    if (cursor.at == cursor.files.size()) return nullptr;
    const auto& file = cursor.files[cursor.at++];
    cursor.entry = {};
    assert(file.name.size() < sizeof(cursor.entry.d_name));
    std::strcpy(cursor.entry.d_name, file.name.c_str());
    cursor.entry.d_ino = file.id; cursor.entry.d_type = file.type;
    return &cursor.entry;
}
extern "C" int __wrap_closedir(DIR* ptr) {
    assert(cursors.erase(ptr) == 1);
    if (failClose) { failClose = false; errno = EIO; return -1; }
    return 0;
}
extern "C" FILE* __wrap_fopen(const char* path, const char* mode) {
    ++pathOpens; assert(std::strcmp(mode, "rb") == 0);
    const auto found = fallback.find(path);
    if (found == fallback.end()) { errno = ENOENT; return nullptr; }
    return payload(found->second);
}
extern "C" int nitroFSOpenById(uint16_t id) {
    ++idOpens;
    if (failId) { failId = false; errno = EMFILE; return -1; }
    assert(payloads.count(id)); return (alreadyTagged ? 0x20000000 : 0) | (0x02000000 + id);
}
extern "C" FILE* __wrap_fdopen(int descriptor, const char* mode) {
    assert((uint32_t(descriptor) & 0xF0000000u) == 0x20000000u);
    assert(std::strcmp(mode, "rb") == 0);
    if (failStream) { failStream = false; errno = ENOMEM; return nullptr; }
    return payload(payloads.at(descriptor & 0xFFFF));
}
extern "C" int __wrap_close(int descriptor) {
    assert((uint32_t(descriptor) & 0xF0000000u) == 0x20000000u);
    ++descriptorCloses; errno = EIO; return 0;
}
static void reset() {
    assert(cursors.empty()); twlm::clearNitroFileCache();
    contents.clear(); fallback.clear(); payloads.clear();
    scans = pathOpens = idOpens = descriptorCloses = 0;
    failRead = failClose = failId = failStream = alreadyTagged = unavailable = false;
}
static void expect(const char* path, const std::string& text) {
    FILE* file = twlm::openNitroFile(path); assert(file);
    std::string actual;
    for (int c; (c = std::fgetc(file)) != EOF;) actual += char(c);
    std::fclose(file); assert(actual == text);
}

int main() {
    reset();
    contents["nitro:/ui/"] = {{".",0xF000,DT_DIR},{"z.pht",17},{"Foo.grf",0},
                              {"folder",0xF001,DT_DIR},{"é.pht",0xEFFF}};
    payloads = {{0,"zero"},{17,"last"},{0xEFFF,"utf8"}};
    expect("nitro:/ui/Foo.grf", "zero");
    expect("nitro:/ui/z.pht", "last");
    expect("nitro:/ui/é.pht", "utf8");
    expect("nitro:/ui/Foo.grf", "zero");
    assert(scans == 1 && idOpens == 4 && pathOpens == 0);
    errno = EIO; assert(!twlm::openNitroFile("nitro:/ui/foo.grf") && errno == ENOENT);
    assert(!twlm::openNitroFile("nitro:/ui/missing.bmp"));
    assert(scans == 1 && pathOpens == 0);
    failId = true; assert(!twlm::openNitroFile("nitro:/ui/Foo.grf") && errno == EMFILE);
    assert(pathOpens == 0);
    failStream = true; assert(!twlm::openNitroFile("nitro:/ui/Foo.grf") && errno == ENOMEM);
    assert(descriptorCloses == 1);
    alreadyTagged = true; expect("nitro:/ui/Foo.grf", "zero"); alreadyTagged = false;
    FILE* first = twlm::openNitroFile("nitro:/ui/Foo.grf");
    FILE* second = twlm::openNitroFile("nitro:/ui/Foo.grf");
    assert(first && second && first != second);
    assert(std::fgetc(first) == 'z' && std::fgetc(first) == 'e' && std::fgetc(second) == 'z');
    std::fclose(first); std::fclose(second);
    twlm::clearNitroFileCache(); contents["nitro:/ui/"] = {{"Foo.grf",10}}; payloads[10] = "remount";
    expect("nitro:/ui/Foo.grf", "remount"); assert(scans == 2);

    reset();
    contents["nitro:/"] = {{"root.grf",3}}; payloads[3] = "root";
    expect("nitro:/root.grf", "root"); assert(scans == 1 && idOpens == 1);
    for (const char* path : {"fat:/custom.grf", "sd:/skin.png", "relative.bmp", "nitro:/", "nitro:/ui/.."}) {
        fallback[path] = path; expect(path, path);
    }
    assert(scans == 1 && pathOpens == 5);

    reset();
    fallback["nitro:/late/icon.pht"] = "fallback";
    unavailable = true;
    expect("nitro:/late/icon.pht", "fallback");
    unavailable = false;
    contents["nitro:/late/"] = {{"icon.pht",4}}; payloads[4] = "mounted";
    expect("nitro:/late/icon.pht", "mounted"); assert(scans == 2 && pathOpens == 1);

    reset();
    assert(!twlm::openNitroFile("nitro:/missing/no.grf") && errno == ENOENT);
    assert(!twlm::openNitroFile("nitro:/missing/no.png"));
    assert(scans == 1 && pathOpens == 0);
    twlm::clearNitroFileCache(); contents["nitro:/missing/"] = {{"no.grf",8}}; payloads[8] = "new image";
    expect("nitro:/missing/no.grf", "new image"); assert(scans == 2);

    for (bool closeFailure : {false, true}) {
        reset(); contents["nitro:/error/"] = {{"a.grf",1},{"b.grf",2}};
        fallback["nitro:/error/b.grf"] = "fallback"; payloads[2] = "retry";
        failClose = closeFailure; failRead = !closeFailure;
        expect("nitro:/error/b.grf", "fallback");
        expect("nitro:/error/b.grf", "retry");
        assert(scans == 2 && pathOpens == 1 && idOpens == 1);
    }

    for (unsigned mode = 0; mode < 4; ++mode) {
        reset(); auto& files = contents["nitro:/large/"];
        if (mode == 0) for (unsigned i = 0; i < 129; ++i) files.push_back({std::to_string(i),i});
        if (mode == 1) for (unsigned i = 0; i < 40; ++i) files.push_back({std::string(120,'x')+std::to_string(i),i});
        if (mode == 2) files.push_back({"unknown",1,DT_UNKNOWN});
        if (mode == 3) files.push_back({"bad-id",0xF000});
        fallback["nitro:/large/icon.grf"] = "ordinary";
        expect("nitro:/large/icon.grf", "ordinary"); expect("nitro:/large/icon.grf", "ordinary");
        assert(scans == 1 && pathOpens == 2 && idOpens == 0);
    }

    reset();
    for (unsigned i = 0; i < 13; ++i) {
        const auto dir = "nitro:/dir" + std::to_string(i) + "/", path = dir + "icon.grf";
        contents[dir] = {{"icon.grf",i}}; payloads[i] = "id"; fallback[path] = "path";
        expect(path.c_str(), i < 12 ? "id" : "path");
    }
    assert(scans == 12 && idOpens == 12 && pathOpens == 1);
    const std::string longPath = "nitro:/" + std::string(260,'x') + "/icon.grf";
    fallback[longPath] = "long"; expect(longPath.c_str(), "long"); assert(scans == 12);
    reset(); contents["nitro:/empty/"] = {};
    assert(!twlm::openNitroFile("nitro:/empty/no.grf") && errno == ENOENT);
    assert(!twlm::openNitroFile("nitro:/empty/no.pht") && scans == 1);
    reset();
    std::cout << "Native NitroFS IDs, immutable misses, external paths, retry/remount, bounds and file ownership passed\n";
}
