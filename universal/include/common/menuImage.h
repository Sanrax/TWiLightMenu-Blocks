#pragma once

#include "common/indexedImage.h"
#include "common/lodepng.h"
#include <string>

// RGB555/alpha access without inflating bundled PNGs or expanding an indexed
// image into a screen-sized RGBA buffer. FAT skin PNGs keep the PNG decoder.
namespace menuImage {
class Image {
    indexedImage::Image indexed;
    std::vector<unsigned char> rgba;
    std::array<uint16_t, 256> colors;
    const uint16_t *lut = nullptr;
    bool cached = false;

    uint16_t convert(const unsigned char *pixel) const {
        uint16_t value = 0x8000 | (pixel[0] >> 3) | ((pixel[1] >> 3) << 5) | ((pixel[2] >> 3) << 10);
        return lut ? lut[value & 0x7fff] | 0x8000 : value;
    }

public:
    unsigned width = 0, height = 0;

    bool load(const std::string &path, unsigned maxWidth, unsigned maxHeight,
              const uint16_t *colorTable = nullptr) {
        width = height = 0;
        cached = false;
        rgba.clear();
        indexed = indexedImage::Image();
        lut = colorTable;
        if (!maxWidth || maxWidth > 256 || !maxHeight || maxHeight > 192) return false;

        if (path.compare(0, 7, "nitro:/") == 0 && path.size() >= 4
            && path.compare(path.size() - 4, 4, ".png") == 0) {
            const std::string cache = path.substr(0, path.size() - 4) + ".pht";
            FILE *file = fopen(cache.c_str(), "rb");
            if (file) {
                cached = indexed.load(file, maxWidth, maxHeight);
                fclose(file);
            }
            if (cached) {
                width = indexed.width;
                height = indexed.height;
                for (unsigned i = 0; i < indexed.palette.size(); ++i)
                    colors[i] = convert(indexed.palette[i].data());
                return true;
            }
            indexed = indexedImage::Image();
        }

        std::vector<unsigned char> encoded;
        if (lodepng::load_file(encoded, path)) return false;
        LodePNGState state;
        lodepng_state_init(&state);
        unsigned w = 0, h = 0;
        const unsigned error = lodepng_inspect(&w, &h, &state, encoded.data(), encoded.size());
        lodepng_state_cleanup(&state);
        if (error || !w || w > maxWidth || !h || h > maxHeight
            || lodepng::decode(rgba, w, h, encoded)
            || rgba.size() != size_t(w) * h * 4) return false;
        width = w;
        height = h;
        return true;
    }

    size_t size() const { return size_t(width) * height; }
    uint16_t color(size_t pixel) const {
        return cached ? colors[indexed.pixels[pixel]] : convert(rgba.data() + pixel * 4);
    }
    uint8_t alpha(size_t pixel) const {
        return cached ? indexed.palette[indexed.pixels[pixel]][3] : rgba[pixel * 4 + 3];
    }
};
}
