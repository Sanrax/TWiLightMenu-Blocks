#include "common/menuImage.h"
#include <cassert>
#include <filesystem>

namespace fs = std::filesystem;

static bool decodeCache(const std::vector<unsigned char> &bytes, indexedImage::Image &image) {
    FILE *file = tmpfile();
    assert(file && fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size());
    rewind(file);
    const bool loaded = image.load(file, 256, 192);
    fclose(file);
    return loaded;
}

static void checkRuns() {
    const std::vector<unsigned char> valid = {
        'P','H','T','2',3,0,2,0,2,0,4,0,
        255,0,0,255,0,128,255,31, 3,0,3,1};
    indexedImage::Image image;
    assert(decodeCache(valid, image));
    assert((image.pixels == std::vector<uint8_t>{0,0,0,1,1,1}));
    auto raw = valid;
    raw[3] = '1'; raw[10] = 0; raw.resize(20);
    raw.insert(raw.end(), {0,0,0,1,1,1});
    assert(decodeCache(raw, image));
    assert((image.pixels == std::vector<uint8_t>{0,0,0,1,1,1}));
    for (const auto mutation : {std::pair<unsigned,unsigned>{20,0}, {20,4},
                               {20,2}, {21,2}, {10,0}, {10,3}, {10,8}, {4,0}}) {
        auto bad = valid; bad[mutation.first] = mutation.second;
        assert(!decodeCache(bad, image));
    }
    for (size_t length = 0; length < valid.size(); ++length) {
        auto bad = valid; bad.resize(length);
        assert(!decodeCache(bad, image));
    }
}

static void compare(const menuImage::Image &image, unsigned width, unsigned height,
                    const std::vector<unsigned char> &rgba, const uint16_t *lut) {
    assert(image.width == width && image.height == height && image.size() == rgba.size() / 4);
    for (size_t i = 0; i < image.size(); ++i) {
        uint16_t expected = 0x8000 | (rgba[i * 4] >> 3)
            | ((rgba[i * 4 + 1] >> 3) << 5) | ((rgba[i * 4 + 2] >> 3) << 10);
        if (lut) expected = lut[expected & 0x7fff] | 0x8000;
        assert(image.color(i) == expected && image.alpha(i) == rgba[i * 4 + 3]);
    }
}

int main(int argc, char **argv) {
    assert(argc == 2);
    checkRuns();
    fs::current_path(argv[1]);
    std::array<uint16_t, 32768> table;
    for (unsigned i = 0; i < table.size(); ++i) table[i] = (i * 7419u ^ 0x1357) & 0x7fff;
    unsigned assets = 0;
    for (const auto &entry : fs::recursive_directory_iterator("nitro:/graphics")) {
        const fs::path cache = entry.path();
        if (cache.extension() != ".pht") continue;
        fs::path png = cache; png.replace_extension(".png");
        std::vector<unsigned char> rgba, bytes;
        unsigned width, height;
        assert(lodepng::decode(rgba, width, height, png.string()) == 0);
        assert(lodepng::load_file(bytes, cache.string()) == 0);
        assert(bytes.size() > 12 && (bytes[8] || bytes[9]));
        indexedImage::Image decoded;
        assert(decodeCache(bytes, decoded));
        auto raw = bytes;
        raw[3] = '1'; raw[10] = raw[11] = 0;
        raw.resize(12 + decoded.palette.size() * 4);
        raw.insert(raw.end(), decoded.pixels.begin(), decoded.pixels.end());
        const uint16_t *tables[] = {nullptr, table.data()};
        for (const auto *lut : tables) {
            menuImage::Image image;
            // A valid cache must work with the PNG absent, including alpha/LUT.
            const fs::path hidden = png.string() + ".hidden";
            fs::rename(png, hidden);
            assert(image.load(png.string(), 256, 192, lut));
            compare(image, width, height, rgba, lut);
            assert(lodepng::save_file(raw, cache.string()) == 0);
            assert(image.load(png.string(), 256, 192, lut));
            compare(image, width, height, rgba, lut);
            assert(lodepng::save_file(bytes, cache.string()) == 0);
            fs::rename(hidden, png);

            // Malformed and unavailable caches must decode the original PNG.
            auto corrupt = bytes;
            corrupt[0] = '?';
            assert(lodepng::save_file(corrupt, cache.string()) == 0);
            assert(image.load(png.string(), 256, 192, lut));
            compare(image, width, height, rgba, lut);
            corrupt = bytes;
            corrupt.resize(corrupt.size() - 1);
            assert(lodepng::save_file(corrupt, cache.string()) == 0);
            assert(image.load(png.string(), 256, 192, lut));
            compare(image, width, height, rgba, lut);
            corrupt.assign(bytes.begin(), bytes.begin() + 12);
            corrupt[8] = corrupt[9] = 0;
            assert(lodepng::save_file(corrupt, cache.string()) == 0);
            assert(image.load(png.string(), 256, 192, lut));
            compare(image, width, height, rgba, lut);
            fs::remove(cache);
            assert(image.load(png.string(), 256, 192, lut));
            compare(image, width, height, rgba, lut);
            assert(lodepng::save_file(bytes, cache.string()) == 0);

            // Custom FAT PNGs must ignore a sidecar, even if it is valid.
            const fs::path fat = fs::path("fat:/skin") / png.filename();
            fs::create_directories(fat.parent_path());
            fs::copy_file(png, fat, fs::copy_options::overwrite_existing);
            fs::path fatCache = fat; fatCache.replace_extension(".pht");
            corrupt = bytes;
            for (size_t i = 12; i < 12 + 4 * (bytes[8] | unsigned(bytes[9]) << 8); ++i)
                corrupt[i] ^= 255;
            assert(lodepng::save_file(corrupt, fatCache.string()) == 0);
            assert(image.load(fat.string(), 256, 192, lut));
            compare(image, width, height, rgba, lut);

            assert(!image.load(png.string(), width - 1, height, lut));
            assert(image.size() == 0);
            assert(!image.load(png.string(), width, height - 1, lut));
            assert(image.size() == 0);
            assert(!image.load("missing.png", 256, 192, lut));
            assert(image.size() == 0);
        }
        assets++;
    }
    assert(assets == 13);
    puts("13 DS Classic images: cached/PNG pixels, alpha, LUTs, corrupt/missing fallback, FAT overrides and dimensions passed");
}
