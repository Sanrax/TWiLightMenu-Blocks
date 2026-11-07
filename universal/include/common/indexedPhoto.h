#pragma once

#include "common/indexedImage.h"

namespace indexedPhoto {
using Color = indexedImage::Color;
using Image = indexedImage::Image;

inline uint16_t convertColor(const Color &rgba, bool dither4, bool dither2) {
    Color adjusted = rgba;
    for (unsigned i = 0; i < 4; i++) {
        if (dither4 && rgba[i] >= 4 && rgba[i] < 252) adjusted[i] += 4;
        if (dither2 && rgba[i] >= 2 && adjusted[i] < 254) adjusted[i] += 2;
    }
    uint16_t color = 0x8000;
    for (unsigned i = 0; i < 3; i++) {
        unsigned component = adjusted[i] >> 3;
        // Preserve the photo renderer's integer blend against black, including
        // its alpha dithering. Fully opaque pixels bypass blending.
        if (rgba[3] != 255) {
            const unsigned value = component * adjusted[3];
            component = (value + 1 + (value >> 8)) >> 8;
        }
        color |= component << (5 * i);
    }
    return color;
}

// Convert colors once per palette entry, rather than once per photo pixel.
// A second deband frame starts with opposite four-step/two-step phases. The
// row parity deliberately depends on width: odd-sized photos retain the same
// checkerboard sequence as the PNG renderer.
inline void render(const Image &image, uint16_t *destination, unsigned stride,
                   bool deband, bool secondFrame, const uint16_t *colorTable) {
    std::array<std::array<uint16_t, 256>, 2> colors;
    const bool phase = secondFrame && ((image.height * (image.width + 1)) & 1);
    for (unsigned i = 0; i < image.palette.size(); i++) {
        for (unsigned parity = 0; parity < 2; parity++) {
            const bool dither4 = bool(parity) ^ phase ^ secondFrame;
            const bool dither2 = deband && (bool(parity) ^ phase);
            uint16_t color = convertColor(image.palette[i], dither4, dither2);
            if (colorTable) color = colorTable[color & 0x7fff] | 0x8000;
            colors[parity][i] = color;
        }
    }
    const uint8_t *source = image.pixels.data();
    for (unsigned y = 0; y < image.height; y++) {
        const unsigned rowParity = (y * (image.width + 1)) & 1;
        for (unsigned x = 0; x < image.width; x++)
            destination[x] = colors[rowParity ^ (x & 1)][*source++];
        destination += stride;
    }
}
}
