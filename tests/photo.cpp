#include "common/indexedPhoto.h"
#include "common/lodepng.h"
#include <cassert>
#include <filesystem>

using indexedPhoto::Image;
using indexedPhoto::Color;

// Reference the original PNG renderer's pixel order, dithering and blend.
// This deliberately keeps its two running phases across rows and frames.
static void comparePixels(const Image &image) {
    std::array<uint16_t, 32768> table;
    for (unsigned i = 0; i < table.size(); i++) table[i] = (i * 7419u ^ 0x1357) & 0x7fff;
    for (bool deband : {false, true}) {
        const uint16_t *tables[] = {nullptr, table.data()};
        for (const uint16_t *lut : tables) {
            bool alternate = false, alternate2 = false;
            for (unsigned frame = 0; frame < unsigned(deband) + 1; frame++) {
                const unsigned stride = image.width + 3;
                std::vector<uint16_t> result(stride * (image.height + 2), 0xdead);
                indexedPhoto::render(image, result.data() + stride, stride, deband, frame != 0, lut);
                for (unsigned y = 0; y < image.height; y++) {
                    for (unsigned x = 0; x < image.width; x++) {
                        const Color &original = image.palette[image.pixels[y * image.width + x]];
                        Color adjusted = original;
                        for (unsigned c = 0; c < 4; c++) {
                            if (alternate && original[c] >= 4 && original[c] < 252) adjusted[c] += 4;
                            if (alternate2 && deband && original[c] >= 2 && adjusted[c] < 254) adjusted[c] += 2;
                        }
                        uint16_t expected = 0x8000;
                        for (unsigned c = 0; c < 3; c++) {
                            unsigned value = adjusted[c] >> 3;
                            if (original[3] != 255) {
                                value *= adjusted[3];
                                value = (value + 1 + (value >> 8)) >> 8;
                            }
                            expected |= value << (5 * c);
                        }
                        if (lut) expected = lut[expected & 0x7fff] | 0x8000;
                        assert(result[(y + 1) * stride + x] == expected);
                        alternate = !alternate;
                        alternate2 = !alternate2;
                    }
                    alternate = !alternate;
                    alternate2 = !alternate2;
                    for (unsigned x = image.width; x < stride; x++)
                        assert(result[(y + 1) * stride + x] == 0xdead);
                }
                alternate = !alternate;
                for (unsigned x = 0; x < stride; x++) {
                    assert(result[x] == 0xdead);
                    assert(result[(image.height + 1) * stride + x] == 0xdead);
                }
            }
        }
    }
}

static bool loadBytes(const std::vector<uint8_t> &bytes) {
    FILE *file = tmpfile();
    assert(file);
    assert(fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size());
    rewind(file);
    Image image;
    const bool result = image.load(file);
    fclose(file);
    return result;
}

int main(int argc, char **argv) {
    assert(argc == 3);
    unsigned assets = 0;
    for (const auto &entry : std::filesystem::directory_iterator(argv[2])) {
        const auto png = entry.path() / "photo_default.png";
        if (!std::filesystem::is_regular_file(png)) continue;
        std::vector<unsigned char> rgba;
        unsigned width, height;
        assert(lodepng::decode(rgba, width, height, png.string()) == 0);
        const auto encoded = std::filesystem::path(argv[1]) / (entry.path().filename().string() + ".pht");
        FILE *file = fopen(encoded.c_str(), "rb");
        assert(file);
        Image image;
        assert(image.load(file));
        fclose(file);
        assert(image.width == width && image.height == height);
        for (unsigned i = 0; i < width * height; i++)
            for (unsigned c = 0; c < 4; c++)
                assert(image.palette[image.pixels[i]][c] == rgba[i * 4 + c]);
        comparePixels(image);
        assets++;
    }
    assert(assets == 34);

    // Odd/even dimensions and alpha/dither thresholds, including transparent
    // pixels transformed by a nontrivial color LUT.
    for (unsigned w : {1, 2, 3, 208}) {
        for (unsigned h : {1, 2, 5, 156}) {
            Image image;
            image.width = w; image.height = h;
            for (unsigned c = 0; c < 256; c++)
                image.palette.push_back({uint8_t(c), uint8_t(c ^ 0x55), uint8_t(255 - c), uint8_t(c)});
            for (unsigned i = 0; i < w * h; i++) image.pixels.push_back(uint8_t(i * 19));
            comparePixels(image);
        }
    }
    std::vector<uint8_t> valid = {'P','H','T','1',1,0,1,0,1,0,0,0,255,0,255,255,0};
    assert(loadBytes(valid));
    for (size_t size = 0; size < valid.size(); size++)
        assert(!loadBytes(std::vector<uint8_t>(valid.begin(), valid.begin() + size)));
    for (unsigned offset : {0, 4, 6, 8, 10, 16}) {
        auto invalid = valid; invalid[offset] = offset == 16 ? 1 : 0;
        if (offset == 10) invalid[offset] = 1;
        assert(!loadBytes(invalid));
    }
    auto large = valid; large[5] = 1; assert(!loadBytes(large));
    large = valid; large[7] = 1; assert(!loadBytes(large));
    large = valid; large[9] = 2; assert(!loadBytes(large));
    printf("Indexed photos: %u PNG round trips, pixel-equivalent debanding/LUTs, alpha thresholds and malformed assets passed\n", assets);
}
