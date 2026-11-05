#include "Texture.h"
#include "common/lodepng.h"
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

TestPersonalData personalData = {};
TestPersonalData *PersonalData = &personalData;
bool useTwlCfg = false;
u16 *colorTable = nullptr;
void effectColorModePalette(u16 *, u16) {}
void effectColorModeBmpPalette(u16 *, u16) {}
extern "C" void tonccpy(void *out, const void *in, unsigned size) { memcpy(out, in, size); }
void decompress(const void *source, void *destination, DecompressType) {
    const u8 *in = static_cast<const u8 *>(source);
    u8 *out = static_cast<u8 *>(destination);
    assert(in[0] == 0x10);
    const size_t size = in[1] | (size_t(in[2]) << 8) | (size_t(in[3]) << 16);
    size_t pos = 0; in += 4;
    while (pos < size) {
        const u8 flags = *in++;
        for (int bit = 7; bit >= 0 && pos < size; bit--) {
            if (flags & (1 << bit)) {
                const u8 a = *in++, b = *in++;
                const size_t length = (a >> 4) + 3, distance = ((a & 15) << 8) + b + 1;
                assert(distance <= pos && pos + length <= size);
                for (size_t i = 0; i < length; i++, pos++) out[pos] = out[pos - distance];
            } else out[pos++] = *in++;
        }
    }
}

static void put16(std::vector<u8> &b, size_t at, u16 n) { b[at] = n; b[at + 1] = n >> 8; }
static void put32(std::vector<u8> &b, size_t at, u32 n) { put16(b, at, n); put16(b, at + 2, n >> 16); }
static std::vector<u8> bmp(int width, int height, int depth, u32 offset = 54) {
    const size_t stride = ((width * depth + 31) / 32) * 4;
    std::vector<u8> b(offset + stride * std::abs(height));
    b[0] = 'B'; b[1] = 'M'; put32(b, 2, b.size()); put32(b, 10, offset);
    put32(b, 14, 40); put32(b, 18, width); put32(b, 22, height);
    put16(b, 26, 1); put16(b, 28, depth);
    // BI_RGB is permitted to omit the image size; derive it from dimensions.
    return b;
}
static void write(const std::string &path, const std::vector<u8> &bytes) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    assert(file.good());
}
static void expect(Texture &texture, const std::vector<u16> &pixels, u32 width, u32 height) {
    assert(texture.texWidth() == width && texture.texHeight() == height);
    std::vector<u16> out(pixels.size() + 1, 0xDEAD);
    texture.copy(out.data(), false);
    assert(std::equal(pixels.begin(), pixels.end(), out.begin()));
    assert(out.back() == 0xDEAD);
}

int main(int argc, char **argv) {
    assert(argc == 3);
    const std::string base = std::string(argv[1]) + "/texture";
    auto b = bmp(2, 2, 16, 0x10040);
    put16(b, 0x10040, 0x001F); put16(b, 0x10042, 0x03E0);
    put16(b, 0x10044, 0x7C00); put16(b, 0x10046, 0x7FFF);
    write(base + ".bmp", b);
    { Texture t(base, ""); assert(t.type() == Bmp); expect(t, {0x801F, 0xFFFF, 0xFC00, 0x83E0}, 2, 2); }
    b = bmp(3, -2, 16);
    put16(b, 54, 0x7C00); put16(b, 56, 0x03E0); put16(b, 58, 0x001F);
    put16(b, 62, 0x7FFF); put16(b, 64, 0x7C1F); put16(b, 66, 0);
    write(base + ".bmp", b);
    { Texture t(base, ""); expect(t, {0x801F, 0x83E0, 0xFC00, 0xFFFF, 0, 0x8000}, 3, 2); }
    b = bmp(2, 1, 16, 66); put32(b, 30, 3);
    put32(b, 54, 0xF800); put32(b, 58, 0x07E0); put32(b, 62, 0x001F);
    put16(b, 66, 0xF800); put16(b, 68, 0x07E0); write(base + ".bmp", b);
    { Texture t(base, ""); expect(t, {0x801F, 0x83E0}, 2, 1); }
    // Odd-width 4-bit images: row padding and nibble order must both survive.
    b = bmp(3, 1, 4, 70); put32(b, 46, 4);
    put32(b, 54, 0); put32(b, 58, 0xFF0000); put32(b, 62, 0x00FF00); put32(b, 66, 0x0000FF);
    b[70] = 0x12; b[71] = 0x30; write(base + ".bmp", b);
    { Texture t(base, ""); assert(t.type() == PalettedBmp); expect(t, {0x801F, 0x83E0, 0xFC00}, 3, 1); }
    // An invalid final extension used to be closed twice and block fallback.
    const std::string bad = std::string(argv[1]) + "/invalid";
    write(bad + ".bmp", std::vector<u8>(64, 'x'));
    { Texture t(bad, base); expect(t, {0x801F, 0x83E0, 0xFC00}, 3, 1); }
    // A full PNG signature with corrupt contents must also fall back.
    std::vector<u8> png(64);
    const u8 signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    memcpy(png.data(), signature, sizeof(signature)); write(bad + ".png", png);
    { Texture t(bad, base); expect(t, {0x801F, 0x83E0, 0xFC00}, 3, 1); }
    const std::vector<u8> rgba = {255,0,0,255, 0,0,255,255, 0,255,0,128, 255,0,255,255};
    png.clear();
    assert(lodepng::encode(png, rgba, 2, 2) == 0); write(bad + ".png", png);
    { Texture t(bad, ""); assert(t.type() == Png); expect(t, {0x801F, 0xFC00, 0, 0}, 2, 2); }
    { Texture t(base + "-missing", ""); assert(t.type() == Unknown && !t.texture()); }
    // Exercise the complete repository's built-in skin assets too.
    unsigned assets = 0;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(argv[2])) {
        if (entry.path().extension() != ".grf") continue;
        auto path = entry.path(); path.replace_extension();
        Texture t(path.string(), "");
        assert(t.type() == PalettedGrf || t.type() == CompressedGrf);
        assert(t.pixelCount() > 0);
        std::vector<u16> out(t.pixelCount() + 1, 0xDEAD);
        t.copy(out.data(), false); assert(out.back() == 0xDEAD);
        assets++;
    }
    printf("Texture BMP/PNG colors, orientation, padding, fallbacks and %u GRF assets passed\n", assets);
}
