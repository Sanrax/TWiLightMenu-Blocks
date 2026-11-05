#include "common/soundbank.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

static void word(std::vector<uint8_t>& bytes, size_t at, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[at + i] = value >> (i * 8);
}

int main(int argc, char** argv) {
    for (unsigned count : {7, 9}) {
        const size_t tableEnd = 12 + count * 4;
        std::vector<uint8_t> bytes(tableEnd + count * 32, 0);
        word(bytes, 0, count);
        std::memcpy(bytes.data() + 4, "*maxmod*", 8);
        for (unsigned id = 0; id < count; ++id) {
            const size_t offset = tableEnd + id * 32;
            word(bytes, 12 + id * 4, offset);
            word(bytes, offset, 24);
            bytes[offset + 4] = 2;
        }
        soundbank::Header header{};
        assert(soundbank::readHeader(bytes.data(), 20, header));
        assert(header.sampleCount == count);
        assert(soundbank::validEffects(bytes.data(), bytes.size()));
        auto bad = bytes;
        word(bad, 12, 0xfffffffcu);
        assert(!soundbank::validEffects(bad.data(), bad.size()));
        bad = bytes;
        word(bad, tableEnd, 0xffffffffu);
        assert(!soundbank::validEffects(bad.data(), bad.size()));
        bad = bytes;
        bad[tableEnd + 4] = 0; // A module cannot stand in for an NDS sample.
        assert(!soundbank::validEffects(bad.data(), bad.size()));
        bad = bytes;
        bad[2] = 1;
        assert(!soundbank::validEffects(bad.data(), bad.size()));
        assert(!soundbank::validEffects(bytes.data(), tableEnd - 1));
        assert(!soundbank::validEffects(bytes.data(), bytes.size() - 1));
    }
    soundbank::Header header{};
    assert(!soundbank::readHeader(nullptr, 20, header));
    uint8_t shortHeader[19] = {};
    assert(!soundbank::readHeader(shortHeader, sizeof(shortHeader), header));
    assert(argc == 3);
    for (int i = 1; i < argc; ++i) {
        std::ifstream file(argv[i], std::ios::binary);
        assert(file);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
        assert(soundbank::readHeader(bytes.data(), bytes.size(), header));
        assert(soundbank::validEffects(bytes.data(), bytes.size()));
    }
    std::puts("Legacy/current sound banks and malformed sample bounds passed");
}
