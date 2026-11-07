#pragma once

#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

// Build-time decoded indexed RGBA images. The PNG remains the fallback.
// PHT1 stores little-endian width, height, color count and reserved words,
// followed by RGBA colors and one byte per pixel in row order. PHT2 uses
// the reserved word for its payload length and stores (run length, index)
// byte pairs, with runs of 1..255 pixels. Its bounded
// dimensions cover bundled photos and menu screens. Callers retain their
// own bounds; the default remains the photo/small-texture limit.
namespace indexedImage {
using Color = std::array<uint8_t, 4>;
struct Image {
    unsigned width = 0, height = 0;
    std::vector<Color> palette;
    std::vector<uint8_t> pixels;

    bool load(FILE *file, unsigned maxWidth = 208, unsigned maxHeight = 156) {
        uint8_t header[12];
        if (!file || fread(header, 1, sizeof(header), file) != sizeof(header)
            || header[0] != 'P' || header[1] != 'H' || header[2] != 'T'
            || (header[3] != '1' && header[3] != '2')) return false;
        const bool runs = header[3] == '2';
        const unsigned payload = header[10] | (unsigned(header[11]) << 8);
        if (!runs && payload) return false;
        width = header[4] | (unsigned(header[5]) << 8);
        height = header[6] | (unsigned(header[7]) << 8);
        const unsigned colors = header[8] | (unsigned(header[9]) << 8);
        if (!width || width > maxWidth || width > 256 || !height
            || height > maxHeight || height > 192 || !colors || colors > 256) return false;
        palette.resize(colors);
        pixels.resize(width * height);
        if (fread(palette.data(), sizeof(Color), colors, file) != colors) return false;
        if (runs) {
            if (!payload || payload > pixels.size() || (payload & 1)) return false;
            std::vector<uint8_t> encoded(payload);
            if (fread(encoded.data(), 1, payload, file) != payload) return false;
            size_t position = 0;
            for (size_t i = 0; i < payload; i += 2) {
                const unsigned count = encoded[i], index = encoded[i + 1];
                if (!count || index >= colors || count > pixels.size() - position) return false;
                std::fill_n(pixels.data() + position, count, index);
                position += count;
            }
            if (position != pixels.size()) return false;
        } else {
            if (fread(pixels.data(), 1, pixels.size(), file) != pixels.size()) return false;
            for (uint8_t index : pixels)
                if (index >= colors) return false;
        }
        return true;
    }
};
}
