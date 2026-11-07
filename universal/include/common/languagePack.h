#ifndef TWLM_LANGUAGE_PACK_H
#define TWLM_LANGUAGE_PACK_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace twlm::language {
struct Entry {
    const char *key;
    const char *fallback;
    std::string *output;
};

constexpr uint32_t hashByte(uint32_t hash, unsigned char byte) {
    return (hash ^ byte) * 16777619u;
}

// Include names, ordering and defaults, so a pack from a different program
// or an older language.inl cannot silently assign the wrong strings.
constexpr uint32_t schemaHash(const Entry *entries, size_t count) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < count; ++i) {
        for (const char *s = entries[i].key; *s; ++s) hash = hashByte(hash, *s);
        hash = hashByte(hash, 0);
        for (const char *s = entries[i].fallback; *s; ++s) hash = hashByte(hash, *s);
        hash = hashByte(hash, 0);
    }
    return hash;
}

inline uint32_t dataHash(const unsigned char *bytes, size_t size) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; ++i) hash = hashByte(hash, bytes[i]);
    return hash;
}

// No outputs change unless the complete pack passes bounds, schema and data
// checks. Callers retain their INI loader for missing, old or invalid packs.
bool loadPack(const char *path, const Entry *entries, size_t count, uint32_t schema);

// Keep the original INI/default/escape behavior when no compatible prepared
// translation is available. The packed and fallback paths share one schema.
void loadTranslations(const char *language, const Entry *entries, size_t count, uint32_t schema);
} // namespace twlm::language

#endif
