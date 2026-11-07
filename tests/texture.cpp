#include "Texture.h"
#include "common/lodepng.h"
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <utility>
#include <vector>

TestPersonalData personalData = {};
TestPersonalData *PersonalData = &personalData;
bool useTwlCfg = false;
u16 *colorTable = nullptr;
void effectColorModePalette(u16 *palette, u16 length) {
    if (colorTable)
        for (unsigned i = 0; i < length; i++) palette[i] = colorTable[palette[i] & 0x7FFF];
}
void effectColorModeBmpPalette(u16 *pixels, u16 length) {
    if (colorTable)
        for (unsigned i = 0; i < length; i++) pixels[i] = colorTable[pixels[i] & 0x7FFF] | BIT(15);
}
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
static u16 get16(const std::vector<u8> &b, size_t at) { return b.at(at) | (u16(b.at(at + 1)) << 8); }
static u32 get32(const std::vector<u8> &b, size_t at) { return get16(b, at) | (u32(get16(b, at + 2)) << 16); }
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
    if (texture.texWidth() != width || texture.texHeight() != height)
        fprintf(stderr, "Texture type %u: got %ux%u, expected %ux%u\n", unsigned(texture.type()),
                texture.texWidth(), texture.texHeight(), width, height);
    assert(texture.texWidth() == width && texture.texHeight() == height);
    std::vector<u16> out(pixels.size() + 1, 0xDEAD);
    texture.copy(out.data(), false);
    assert(std::equal(pixels.begin(), pixels.end(), out.begin()));
    assert(out.back() == 0xDEAD);
}

static void comparePNG(const std::string &original, const std::string &indexed) {
    Texture expected(original, ""), actual(indexed, "");
    assert(expected.type() == Png && actual.type() == Png);
    std::vector<u16> pixels(expected.texture(), expected.texture() + expected.pixelCount());
    expect(actual, pixels, expected.texWidth(), expected.texHeight());
}

static void checkGrf(const std::filesystem::path &file) {
    std::ifstream in(file, std::ios::binary);
    assert(in.good());
    const std::vector<u8> encoded{std::istreambuf_iterator<char>(in), {}};
    const u32 width = get32(encoded, 28), height = get32(encoded, 32);
    auto base = file; base.replace_extension();
    std::vector<u16> lut(0x8000);
    for (unsigned i = 0; i < lut.size(); i++) lut[i] = ((i * 12345) ^ (i >> 4)) & 0x7FFF;
    for (bool filtered : {false, true}) {
        colorTable = filtered ? lut.data() : nullptr;
        Texture texture(base.string(), "");
        assert(texture.texWidth() == width && texture.texHeight() == height);
        std::vector<u16> expected(width * height);
        if ((get32(encoded, 44) & 0xF0) == 0) {
            // Legacy reader: raw pixels start at 48; palette follows them.
            const unsigned bytes = get32(encoded, 44) >> 8, pal = 48 + bytes;
            const unsigned colors = get32(encoded, pal + 8) >> 9;
            assert(texture.type() == PalettedGrf && texture.texLength() == bytes / 2);
            assert(texture.paletteLength() == colors);
            assert(texture.paletteUploadLength() >= 16 && !(texture.paletteUploadLength() & 1));
            assert(!memcmp(texture.bytes(), encoded.data() + 48, bytes));
            for (unsigned i = 0; i < colors; i++) {
                u16 color = get16(encoded, pal + 12 + i * 2);
                if (colorTable) color = colorTable[color & 0x7FFF];
                assert(texture.palette()[i] == color);
            }
            for (unsigned i = colors; i < texture.paletteUploadLength(); i++) assert(texture.palette()[i] == 0);
            for (unsigned i = 0; i < expected.size(); i++) {
                const u8 index = (encoded[48 + i / 2] >> ((i & 1) * 4)) & 15;
                assert(index < colors);
                u16 color = get16(encoded, pal + 12 + index * 2);
                expected[i] = colorTable ? colorTable[color & 0x7FFF] : color;
            }
            // Match the SDK's power-of-two upload size, not just logical rows.
            auto gpuSize = [](unsigned n) { unsigned s = 8; while (s < n && s < 1024) s *= 2; return s; };
            const unsigned uploadBytes = gpuSize(width) * gpuSize(height) / 2;
            for (unsigned i = bytes; i < uploadBytes; i++) assert(texture.bytes()[i] == 0);
        } else {
            const unsigned bytes = get32(encoded, 40);
            assert(texture.type() == CompressedGrf && texture.texCmpLength() == bytes);
            assert(texture.texLength() == expected.size());
            assert(!memcmp(texture.bytes(), encoded.data() + 44, bytes));
            decompress(encoded.data() + 44, expected.data(), LZ77);
            if (colorTable)
                for (u16 &pixel : expected) pixel = colorTable[pixel & 0x7FFF] | BIT(15);
        }
        expect(texture, expected, width, height);
    }
    colorTable = nullptr;
}

static std::vector<u8> palettedGrf() {
    std::vector<u8> bytes(68);
    put32(bytes, 0, CHUNK_ID('R','I','F','F')); put32(bytes, 4, bytes.size() - 8);
    put32(bytes, 8, CHUNK_ID('G','R','F',' ')); put32(bytes, 12, CHUNK_ID('H','D','R',' '));
    put32(bytes, 16, 16); put32(bytes, 20, 0x10000004); put32(bytes, 24, 0x101);
    put32(bytes, 28, 3); put32(bytes, 32, 1);
    put32(bytes, 36, CHUNK_ID('G','F','X',' ')); put32(bytes, 40, 6); put32(bytes, 44, 2 << 8);
    bytes[48] = 0x10; bytes[49] = 2;
    put32(bytes, 50, CHUNK_ID('P','A','L',' ')); put32(bytes, 54, 10); put32(bytes, 58, 6 << 8);
    put16(bytes, 62, 0x1234); put16(bytes, 64, 0x2345); put16(bytes, 66, 0x3456);
    return bytes;
}

static void checkGrfFailures(const std::string &base, const std::string &fallback) {
    const auto valid = palettedGrf();
    write(base + ".grf", valid);
    {
        Texture texture(base, ""); expect(texture, {0x1234, 0x2345, 0x3456}, 3, 1);
        assert(texture.paletteLength() == 3 && texture.paletteUploadLength() == 16);
        // Favorite-color files still use the logical palette stride.
        std::vector<u8> palettes(16 * 3 * 2);
        for (unsigned i = 0; i < 16 * 3; i++) { palettes[i * 2] = 0x12; palettes[i * 2 + 1] = i; }
        write(base + ".pal", palettes);
        personalData.theme = 5;
        texture.applyUserPaletteFile(base + ".pal", nullptr);
        for (unsigned i = 0; i < 3; i++) assert(texture.palette()[i] == 0x1200 + 15 + i);
        for (unsigned i = 3; i < texture.paletteUploadLength(); i++) assert(texture.palette()[i] == 0);
        personalData.theme = 0;
    }
    auto fallbackWorks = [&](const std::vector<u8> &bytes) {
        write(base + ".grf", bytes);
        Texture texture(base, fallback);
        if (texture.texWidth() != 3 || texture.texHeight() != 1)
            fprintf(stderr, "GRF fallback failed for %zu-byte input\n", bytes.size());
        expect(texture, {0x801F, 0x83E0, 0xFC00}, 3, 1);
    };
    for (size_t length = 0; length < valid.size(); length++)
        fallbackWorks(std::vector<u8>(valid.begin(), valid.begin() + length));
    for (auto [offset, value] : {std::pair<unsigned, u32>{28, 0}, {32, 0}, {44, 0xFFFFFF00},
                                {50, 0}, {58, 0}, {58, 0x100}, {58, 0x20000}, {58, 0x610}}) {
        auto invalid = valid; put32(invalid, offset, value); fallbackWorks(invalid);
    }
    auto invalid = valid; put32(invalid, 28, 0xFFFFFFFF); put32(invalid, 32, 0xFFFFFFFF);
    fallbackWorks(invalid);
    // A literal-only RGB16 LZ stream, with its four-byte header in GFX.
    auto compressed = valid; compressed.resize(66);
    put32(compressed, 4, compressed.size() - 8); put32(compressed, 20, 16);
    put32(compressed, 28, 4); put32(compressed, 32, 2); put32(compressed, 40, 22);
    put32(compressed, 44, (16 << 8) | 0x10);
    for (unsigned block = 0; block < 2; block++) {
        compressed[48 + block * 9] = 0;
        for (unsigned i = 0; i < 8; i++) compressed[49 + block * 9 + i] = block * 8 + i;
    }
    write(base + ".grf", compressed);
    { Texture texture(base, ""); expect(texture, {0x0100,0x0302,0x0504,0x0706,0x0908,0x0B0A,0x0D0C,0x0F0E}, 4, 2); }
    for (size_t length = 0; length < compressed.size(); length++)
        fallbackWorks(std::vector<u8>(compressed.begin(), compressed.begin() + length));
    for (auto [offset, value] : {std::pair<unsigned, u32>{40, 3}, {40, 0xFFFFFFFF}, {44, (18 << 8) | 0x10}}) {
        invalid = compressed; put32(invalid, offset, value); fallbackWorks(invalid);
    }
}

static void checkIndexedPNGs(const std::filesystem::path &temporary, const std::filesystem::path &assets) {
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(temporary);
    std::vector<u16> lut(0x8000);
    for (unsigned i = 0; i < lut.size(); i++) lut[i] = ((i * 12345) ^ (i >> 4)) & 0x7FFF;
    unsigned images = 0;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(assets / "themes")) {
        if (entry.path().extension() != ".png") continue;
        auto original = entry.path(); original.replace_extension();
        auto indexed = std::filesystem::path("nitro:") / entry.path().lexically_relative(assets);
        indexed.replace_extension();
        // Only indexed data was staged here. Loading must succeed without PNG.
        assert(!std::filesystem::exists(indexed.string() + ".png"));
        for (bool filtered : {false, true}) {
            colorTable = filtered ? lut.data() : nullptr;
            comparePNG(original.string(), indexed.string());
        }
        images++;
    }
    assert(images == 84);
    colorTable = nullptr;

    const std::vector<u8> rgba = {
        255,0,0,255, 0,0,255,255, 0,255,0,128, 255,0,255,255,
        248,7,248,255, 0,0,0,255, 20,40,60,254, 200,100,150,0,
        1,2,3,255, 128,129,130,255, 255,255,255,255, 0,255,255,255
    };
    const std::string original = (temporary / "indexed-reference").string();
    const std::string indexed = "nitro:/indexed-test";
    std::vector<u8> png;
    assert(lodepng::encode(png, rgba, 6, 2) == 0);
    write(original + ".png", png); write(indexed + ".png", png);
    std::vector<u8> cache = {'P','H','T','1',6,0,2,0,12,0,0,0};
    cache.insert(cache.end(), rgba.begin(), rgba.end());
    for (unsigned i = 0; i < 12; i++) cache.push_back(i);
    write(indexed + ".pht", cache);
    for (bool filtered : {false, true}) {
        colorTable = filtered ? lut.data() : nullptr;
        comparePNG(original, indexed);
    }
    colorTable = nullptr;
    for (size_t size = 0; size < cache.size(); size++) {
        write(indexed + ".pht", std::vector<u8>(cache.begin(), cache.begin() + size));
        comparePNG(original, indexed); // Truncated cache falls back to PNG.
    }
    for (unsigned offset : {0u, 8u, 10u, unsigned(cache.size() - 1)}) {
        auto invalid = cache; invalid[offset] = offset == 8 ? 0 : 255;
        write(indexed + ".pht", invalid); comparePNG(original, indexed);
    }
    // GRF retains priority, and external PNG overrides ignore indexed files.
    write(indexed + ".pht", cache);
    std::filesystem::copy_file(assets / "themes/dsi/white/grf/bips.grf", indexed + ".grf");
    { Texture texture(indexed, ""); assert(texture.type() == PalettedGrf); }
    std::filesystem::remove(indexed + ".grf");
    auto external = cache; external[12] = 0; external[14] = 255;
    write(original + ".pht", external);
    comparePNG(original, indexed);
    std::filesystem::remove(indexed + ".pht");
    comparePNG(original, indexed);
    std::filesystem::remove(indexed + ".png");
    { Texture fallback(indexed, original); assert(fallback.type() == Png && fallback.pixelCount() == 12); }

    // Converter inputs used by the runner to check unavailable-cache markers.
    for (bool large : {false, true}) {
        const unsigned width = large ? 209 : 129, height = large ? 1 : 2;
        std::vector<u8> colors(width * height * 4);
        for (unsigned i = 0; i < width * height; i++) {
            colors[i * 4] = i; colors[i * 4 + 1] = i >> 8; colors[i * 4 + 3] = 255;
        }
        png.clear(); assert(lodepng::encode(png, colors, width, height) == 0);
        const std::string name = large ? "unsupported-large" : "unsupported-colors";
        const std::string source = (temporary / name).string(), native = "nitro:/" + name;
        write(source + ".png", png); write(native + ".png", png);
        auto unavailable = cache; unavailable[8] = 0;
        write(native + ".pht", unavailable); comparePNG(source, native);
    }
    std::filesystem::current_path(previous);
    printf("Indexed UI textures: %u PNG pixel comparisons with LUTs, alpha, cache fallbacks and GRF/override priority passed\n", images);
}

int main(int argc, char **argv) {
    assert(argc == 3);
    const auto assetsPath = std::filesystem::absolute(argv[2]);
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
        checkGrf(entry.path());
        assets++;
    }
    printf("Texture BMP/PNG colors, orientation, padding, fallbacks and %u GRF assets passed\n", assets);
    checkGrfFailures(std::string(argv[1]) + "/broken-grf", base);
    checkIndexedPNGs(std::filesystem::absolute(argv[1]), assetsPath);
}
