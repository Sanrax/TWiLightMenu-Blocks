// Predecode bundled photos and UI textures using the menu's PNG decoder.
#include "common/indexedImage.h"
#include "common/lodepng.h"
#include <map>
#include <cstring>

int main(int argc, char **argv) {
    if (argc != 3 && !(argc == 4 && std::strcmp(argv[3], "--screen") == 0)) {
        fprintf(stderr, "Usage: photo-index input.png output.pht [--screen]\n");
        return 1;
    }
    unsigned width = 0, height = 0;
    std::vector<unsigned char> rgba;
    if (lodepng::decode(rgba, width, height, argv[1]) || !width || !height) {
        fprintf(stderr, "Invalid PNG: %s\n", argv[1]);
        return 1;
    }
    std::map<indexedImage::Color, unsigned> indices;
    std::vector<indexedImage::Color> palette;
    std::vector<uint8_t> pixels;
    const unsigned maxWidth = argc == 4 ? 256 : 208, maxHeight = argc == 4 ? 192 : 156;
    const unsigned indexedPixels = width <= maxWidth && height <= maxHeight ? width * height : 0;
    for (unsigned i = 0; i < indexedPixels; i++) {
        indexedImage::Color color;
        for (unsigned j = 0; j < 4; j++) color[j] = rgba[i * 4 + j];
        auto [entry, inserted] = indices.emplace(color, palette.size());
        if (inserted) palette.push_back(color);
        pixels.push_back(entry->second);
    }
    // Larger images or images with more than 256 colors keep the PNG path.
    // Emit an explicit unavailable marker so incremental builds remain stable.
    if (palette.size() > 256) { palette.clear(); pixels.clear(); }
    // Flat screen art compresses well as short runs. Keep photos and any
    // image where this costs more bytes in the existing raw indexed format.
    std::vector<uint8_t> runs;
    if (argc == 4) {
        for (size_t i = 0; i < pixels.size();) {
            unsigned count = 1;
            while (count < 255 && i + count < pixels.size() && pixels[i + count] == pixels[i]) ++count;
            runs.push_back(count);
            runs.push_back(pixels[i]);
            i += count;
        }
    }
    const bool compressed = !runs.empty() && runs.size() < pixels.size();
    const unsigned payloadLength = compressed ? runs.size() : 0;
    const auto &payload = compressed ? runs : pixels;
    const unsigned count = palette.size();
    const uint8_t header[] = {'P', 'H', 'T', uint8_t(compressed ? '2' : '1'), uint8_t(width), uint8_t(width >> 8),
        uint8_t(height), uint8_t(height >> 8), uint8_t(count), uint8_t(count >> 8),
        uint8_t(payloadLength), uint8_t(payloadLength >> 8)};
    FILE *file = fopen(argv[2], "wb");
    if (!file) return 1;
    bool success = fwrite(header, 1, sizeof(header), file) == sizeof(header)
        && fwrite(palette.data(), sizeof(indexedImage::Color), count, file) == count
        && fwrite(payload.data(), 1, payload.size(), file) == payload.size();
    if (fclose(file)) success = false;
    return success ? 0 : 1;
}
