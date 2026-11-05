#include "common/ndsmetadata.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <vector>

int main() {
    constexpr size_t offset = 0x210;
    std::array<unsigned char, ndsmetadata::bannerAnimatedSize> banner;
    for (unsigned version : {1, 2, 3, 0x103}) {
        FILE* file = tmpfile();
        assert(file);
        std::vector<unsigned char> rom(offset + 0x3000, 0x5a);
        rom[offset] = version & 255;
        rom[offset + 1] = version >> 8;
        assert(fwrite(rom.data(), 1, rom.size(), file) == rom.size());
        const size_t expected = version == 1 ? 0x840 : version == 2 ? 0x940
                              : version == 3 ? 0xa40 : 0x23c0;
        assert(ndsmetadata::readBanner(file, offset, banner.data(), banner.size(), true) == expected);
        assert(ftell(file) == long(offset + expected));
        for (size_t i = 2; i < banner.size(); ++i)
            assert(banner[i] == (i < expected ? 0x5a : 0));
        if (version == 0x103) {
            assert(ndsmetadata::readBanner(file, offset, banner.data(), banner.size(), false) == 0xa40);
            assert(ftell(file) == long(offset + 0xa40));
            // All eight title languages survive with animation disabled.
            assert(banner[0xa3f] == 0x5a && banner[0xa40] == 0);
        }
        fclose(file);
    }
    FILE* truncated = tmpfile();
    assert(truncated);
    std::vector<unsigned char> partial(0x83f, 0x5a);
    partial[0] = 1;
    partial[1] = 0;
    assert(fwrite(partial.data(), 1, partial.size(), truncated) == partial.size());
    assert(ndsmetadata::readBanner(truncated, 0, banner.data(), banner.size(), true) < 0x840);
    assert(ndsmetadata::readBanner(truncated, 0x10000, banner.data(), banner.size(), true) == 0);
    assert(ndsmetadata::readBanner(truncated, 0, banner.data(), 0x100, true) == 0);
    assert(ndsmetadata::readBanner(nullptr, 0, banner.data(), banner.size(), true) == 0);
    fclose(truncated);
    std::puts("NDS banner sizes, language retention, animation and truncated reads passed");
}
