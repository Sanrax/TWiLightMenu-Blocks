#include "common/nitroFile.h"

#ifdef __BLOCKSDS__
#include <blocksds_version.h>
#include <filesystem.h>
#include <dirent.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
FILE* openById(uint16_t id) {
    int descriptor = nitroFSOpenById(id);
    if (descriptor < 0) return nullptr;
#if BLOCKSDS_VERSION_EQUALS(1, 24, 0)
    // 1.24's direct-ID API omits the device bits which open() adds to the
    // internal pointer. Without the NitroFS tag read/fstat/close dispatch to
    // sockets. Keep this ABI workaround confined to the affected SDK version.
    if ((uint32_t(descriptor) & 0xF0000000u) == 0) descriptor |= 0x20000000;
#endif
    FILE* file = fdopen(descriptor, "rb");
    if (!file) {
        const int error = errno;
        close(descriptor);
        errno = error;
    }
    return file;
}

struct Entry { uint16_t id, name; };
struct Directory {
    std::string path;
    std::vector<Entry> files;
    std::vector<char> names;
    bool indexed = false;
};
struct Cache {
    std::vector<Directory> directories;

    Directory* find(std::string_view path) {
        for (auto& dir : directories)
            if (std::string_view(dir.path) == path) return &dir;
        // Limits bound retained memory. Unusual directories keep normal stdio.
        if (directories.size() >= 12 || path.size() > 255) return nullptr;
        Directory result;
        result.path.assign(path.data(), path.size());
        DIR* source = opendir(result.path.c_str());
        if (!source) {
            // A missing directory stays missing in this immutable image.
            // Mount and I/O failures retain the normal, retryable path.
            if (errno != ENOENT) return nullptr;
            result.indexed = true;
            directories.push_back(std::move(result));
            return &directories.back();
        }
        bool supported = true;
        int error = 0;
        for (;;) {
            errno = 0;
            const dirent* file = readdir(source);
            if (!file) { error = errno; break; }
            if (file->d_type == DT_DIR) continue;
            const size_t length = std::strlen(file->d_name) + 1;
            if (file->d_type != DT_REG || file->d_ino > 0xEFFF ||
                result.files.size() >= 128 || result.names.size() + length > 4096) {
                supported = false;
                break;
            }
            result.files.push_back({uint16_t(file->d_ino), uint16_t(result.names.size())});
            result.names.insert(result.names.end(), file->d_name, file->d_name + length);
        }
        if (closedir(source) || error) return nullptr;
        if (supported) {
            const char* names = result.names.data();
            std::sort(result.files.begin(), result.files.end(), [names](Entry a, Entry b) {
                return std::strcmp(names + a.name, names + b.name) < 0;
            });
            result.indexed = true;
        } else {
            // Retain only the path so oversized/unsupported directories aren't
            // scanned again for every attempted extension.
            std::vector<Entry>().swap(result.files);
            std::vector<char>().swap(result.names);
        }
        directories.push_back(std::move(result));
        return &directories.back();
    }
};
Cache* existingCache = nullptr;
Cache& cache() {
    static Cache instance;
    existingCache = &instance;
    return instance;
}
} // namespace
#endif

FILE* twlm::openNitroFile(const char* path) {
#ifdef __BLOCKSDS__
    if (path && std::strncmp(path, "nitro:/", 7) == 0) {
        const char* slash = std::strrchr(path, '/');
        const char* name = slash + 1;
        if (*name && std::strcmp(name, ".") && std::strcmp(name, "..")) {
            Directory* dir = cache().find(std::string_view(path, slash - path + 1));
            if (dir && dir->indexed) {
                const char* names = dir->names.data();
                const auto entry = std::lower_bound(dir->files.begin(), dir->files.end(), name,
                    [names](Entry file, const char* key) { return std::strcmp(names + file.name, key) < 0; });
                if (entry != dir->files.end() && std::strcmp(names + entry->name, name) == 0)
                    return openById(entry->id);
                errno = ENOENT;
                return nullptr;
            }
        }
    }
#endif
    return std::fopen(path, "rb");
}

void twlm::clearNitroFileCache() {
#ifdef __BLOCKSDS__
    if (existingCache) std::vector<Directory>().swap(existingCache->directories);
#endif
}
