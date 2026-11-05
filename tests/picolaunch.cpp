#include "common/picolaunch.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <limits>
#include <string>

int main() {
    static_assert(sizeof(pload_params_t) == 772);
    static_assert(pico::headerV1Size == 0x310);
    static_assert(pico::headerV2Size == 0x410);
    static_assert(pico::headerV3Size == 0x414);

    std::array<uint8_t, pico::loaderCapacity + 1> loader{};
    assert(!pico::loader7ApiVersion(nullptr, loader.size()));
    assert(!pico::loader7ApiVersion(loader.data(), 0));
    assert(!pico::loader7ApiVersion(loader.data(), pico::headerV1Size));
    for (unsigned version : {1, 2, 3, 4, 65535}) {
        loader[10] = version & 255;
        loader[11] = version >> 8;
        const size_t size = version == 1 ? 0x310 : version == 2 ? 0x410 : 0x414;
        assert(pico::loader7ApiVersion(loader.data(), size) == version);
        assert(!pico::loader7ApiVersion(loader.data(), size - 1));
        assert(pico::loader7ApiVersion(loader.data(), pico::loaderCapacity) == version);
        assert(!pico::loader7ApiVersion(loader.data(), pico::loaderCapacity + 1));
    }

    pload_params_t params;
    std::memset(&params, 0xff, sizeof(params));
    assert(pico::setLoadPaths(params, "fat:/game.nds", "fat:/saves/game.sav"));
    assert(std::string(params.romPath) == "fat:/game.nds");
    assert(std::string(params.savePath) == "fat:/saves/game.sav");
    assert(params.argumentsLength == 0 && params.arguments[0] == 0);
    assert(pico::setLoadPaths(params, std::string(255, 'r'), std::string(255, 's')));
    assert(params.romPath[255] == 0 && params.savePath[255] == 0);
    assert(!pico::setLoadPaths(params, std::string(256, 'r'), ""));
    assert(!pico::setLoadPaths(params, "fat:/game.nds", std::string(256, 's')));
    assert(!pico::setLoadPaths(params, "", ""));
    assert(!pico::setLoadPaths(params, std::string("rom\0trailing", 12), ""));
    assert(pico::setLoadPaths(params, "fat:/game.nds", ""));
    assert(params.savePath[0] == 0);

    size_t bytes = 8;
    assert(pico::addCheatSize(bytes, 4) && bytes == 28);
    assert(pico::addCheatSize(bytes, 2) && bytes == 40);
    assert(!pico::addCheatSize(bytes, SIZE_MAX) && bytes == 40);
    bytes = UINT32_MAX - 3;
    assert(!pico::addCheatSize(bytes, 0) && bytes == UINT32_MAX - 3);
    bytes = 8;
    assert(!pico::addCheatSize(bytes, (UINT32_MAX - 8) / 4) && bytes == 8);
    assert(pico::addCheatSize(bytes, (UINT32_MAX - 12) / 4));
    assert(bytes == UINT32_MAX - 3);
    std::puts("Pico loader ABI versions, VRAM bounds, paths and cheat size checks passed");
}
