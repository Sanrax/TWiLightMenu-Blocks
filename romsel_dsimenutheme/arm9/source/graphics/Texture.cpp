#include "Texture.h"
#include "paletteEffects.h"
#include "common/tonccpy.h"
#include "common/twlmenusettings.h"
#include "common/lodepng.h"
#include "common/indexedImage.h"
// #include "common/ColorLut.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>
#include <sys/stat.h>

extern bool useTwlCfg;
extern u16* colorTable;

Texture::Texture(const std::string &filePath, const std::string &fallback1, const std::string &fallback2) noexcept
    : _paletteLength(0), _texLength(0), _texCmpLength(0), _texHeight(0), _texWidth(0), _type(TextureType::Unknown) {
    const std::string *paths[] = {&filePath, &fallback1, &fallback2};
    for (const std::string *path : paths) {
        if (path->empty()) continue;
        for (const char *extension : extensions) {
            if (std::strcmp(extension, ".png") == 0 && path->compare(0, 7, "nitro:/") == 0 && loadIndexedPNG(*path)) {
                _type = TextureType::Png;
                return;
            }
            FILE *file = fopen((*path + extension).c_str(), "rb");
            if (!file) continue;
            u32 header[12];
            _type = findType(file, header);
            bool loaded = false;
            switch (_type) {
            case TextureType::PalettedGrf:
            case TextureType::CompressedGrf:
                loaded = loadGrf(file, header);
                break;
            case TextureType::Bmp:
            case TextureType::PalettedBmp:
                loaded = loadBitmap(file);
                break;
            case TextureType::Png:
                loaded = loadPNG(file);
                break;
            default:
                break;
            }
            fclose(file);
            if (loaded) return;
            _palette.reset();
            _texture.reset();
            _paletteLength = 0;
            _texLength = _texCmpLength = _texHeight = _texWidth = 0;
            _type = TextureType::Unknown;
        }
    }
}

TextureType Texture::findType(FILE *file, u32 (&magic)[12]) {
    // The stream was just opened. Retain this header for sequential GRF reads.
    if (fread(magic, sizeof(u32), 12, file) != 12) return TextureType::Unknown;

	if (((u16)magic[0] & 0xffff) == BMP_ID('B', 'M')) {
		// Only 4 and 16 bit are supported currently
		if ((magic[7] & 0xFFFF) == 4)
			return TextureType::PalettedBmp;
		else if ((magic[7] & 0xFFFF) == 16)
			return TextureType::Bmp;
		return TextureType::Unknown;
	} else if (magic[0] == CHUNK_ID('\x89', 'P', 'N', 'G')) {
		return TextureType::Png;
	} else if (magic[0] == CHUNK_ID('R', 'I', 'F', 'F') && magic[2] == CHUNK_ID('G', 'R', 'F', ' ') &&
		magic[3] == CHUNK_ID('H', 'D', 'R', ' ') && magic[9] == CHUNK_ID('G', 'F', 'X', ' ')) {
		switch (magic[11] & 0xF0) {
		// may not be the case for general GRF, but for our case
		// We're going to assume all paletted textures are not compressed.
		case 0x00:
			return TextureType::PalettedGrf;
		// LZ77 Compressed
		case 0x10:
			return TextureType::CompressedGrf;
		default:
			return TextureType::Unknown;
		}
	}

	return TextureType::Unknown;
}

namespace {
u16 read16(const u8 *p) { return p[0] | (u16(p[1]) << 8); }
u32 read32(const u8 *p) { return read16(p) | (u32(read16(p + 2)) << 16); }
}

bool Texture::loadBitmap(FILE *file) noexcept {
    u8 header[54];
    if (fseek(file, 0, SEEK_SET) || fread(header, 1, sizeof(header), file) != sizeof(header)) return false;
    // BMP offsets and DIB sizes are 32-bit fields, including on ARM.
    const u32 offset = read32(header + 10), headerSize = read32(header + 14);
    const u32 width = read32(header + 18);
    const int32_t height = int32_t(read32(header + 22));
    const u16 bitDepth = read16(header + 28);
    const u32 compression = read32(header + 30);
    if (headerSize < 40 || !width || width > INT32_MAX || !height || height == INT32_MIN
        || read16(header + 26) != 1 || (bitDepth != 4 && bitDepth != 16)
        || (compression != 0 && (bitDepth != 16 || compression != 3))) return false;
    const u32 rows = height < 0 ? -height : height;
    const uint64_t pixels = uint64_t(width) * rows;
    const uint64_t stride = ((uint64_t(width) * bitDepth + 31) / 32) * 4;
    if (pixels > UINT32_MAX / 2 || fseek(file, 0, SEEK_END)) return false;
    const long size = ftell(file);
    if (size < 0 || uint64_t(offset) + stride * rows > uint64_t(size)
        || uint64_t(headerSize) + 14 > offset) return false;

    bool rgb565 = false;
    if (compression == 3) {
        u8 masks[12];
        if (offset < 66 || fseek(file, 54, SEEK_SET) || fread(masks, 1, 12, file) != 12) return false;
        rgb565 = read32(masks) == 0xF800 && read32(masks + 4) == 0x07E0 && read32(masks + 8) == 0x001F;
        const bool rgb555 = read32(masks) == 0x7C00 && read32(masks + 4) == 0x03E0 && read32(masks + 8) == 0x001F;
        if (!rgb565 && !rgb555) return false;
    }
    if (bitDepth == 4) {
        const u32 colors = read32(header + 46) ? read32(header + 46) : 16;
        if (colors > 16 || uint64_t(headerSize) + 14 + colors * 4 > offset) return false;
        _paletteLength = colors;
        _palette = std::make_unique<u16[]>(paletteUploadLength());
        if (fseek(file, 14 + headerSize, SEEK_SET)) return false;
        for (u32 i = 0; i < colors; i++) {
            u8 color[4];
            if (fread(color, 1, 4, file) != 4) return false;
            const u16 r = (color[2] * 31 + 127) / 255;
            const u16 g = (color[1] * 31 + 127) / 255;
            const u16 b = (color[0] * 31 + 127) / 255;
            _palette[i] = BIT(15) | (b << 10) | (g << 5) | r;
            if (colorTable) _palette[i] = colorTable[_palette[i] & 0x7FFF] | BIT(15);
        }
    }
    _texWidth = width;
    _texHeight = rows;
    _texLength = bitDepth == 16 ? pixels : (pixels + 3) / 4;
    _texture = std::make_unique<u16[]>(_texLength);
    std::vector<u8> row(stride);
    if (fseek(file, offset, SEEK_SET)) return false;
    // Read in file order: no backward seek for every bottom-up row.
    for (u32 fileRow = 0; fileRow < rows; fileRow++) {
        if (fread(row.data(), 1, row.size(), file) != row.size()) return false;
        const u32 y = height < 0 ? fileRow : rows - fileRow - 1;
        for (u32 x = 0; x < width; x++) {
            const u32 pixel = y * width + x;
            if (bitDepth == 16) {
                u16 color = read16(row.data() + x * 2);
                if (rgb565) color = ((color >> 11) << 10) | (((color >> 6) & 31) << 5) | (color & 31);
                _texture[pixel] = bmpToDS(color);
            } else {
                const u8 index = (row[x / 2] >> ((x & 1) ? 0 : 4)) & 15;
                if (index >= _paletteLength) return false;
                reinterpret_cast<u8 *>(_texture.get())[pixel / 2] |= index << ((pixel & 1) * 4);
            }
        }
    }
    return true;
}

bool Texture::loadIndexedPNG(const std::string &basePath) noexcept {
    FILE *file = fopen((basePath + ".pht").c_str(), "rb");
    if (!file) return false;
    indexedImage::Image image;
    const bool loaded = image.load(file);
    fclose(file);
    if (!loaded) return false;

    std::array<u16, 256> colors{};
    for (unsigned i = 0; i < image.palette.size(); i++) {
        const auto &rgba = image.palette[i];
        if (rgba[3] == 255)
            colors[i] = bmpToDS((rgba[0] >> 3) << 10 | (rgba[1] >> 3) << 5 | (rgba[2] >> 3));
    }
    _texWidth = image.width;
    _texHeight = image.height;
    _texLength = image.pixels.size();
    _texture.reset(new u16[_texLength]);
    for (unsigned i = 0; i < _texLength; i++) _texture[i] = colors[image.pixels[i]];
    return true;
}

bool Texture::loadPNG(FILE *file) noexcept {
    if (fseek(file, 0, SEEK_END)) return false;
    const long size = ftell(file);
    if (size <= 0 || fseek(file, 0, SEEK_SET)) return false;
    std::vector<unsigned char> encoded(size), buffer;
    if (fread(encoded.data(), 1, encoded.size(), file) != encoded.size()) return false;
    unsigned width = 0, height = 0;
    if (lodepng::decode(buffer, width, height, encoded.data(), encoded.size()) || !width || !height
        || uint64_t(width) * height > UINT32_MAX / 4 || buffer.size() != uint64_t(width) * height * 4) return false;
    _texWidth = width;
    _texHeight = height;
    _texLength = width * height;
    _texture = std::make_unique<u16[]>(_texLength);
    for (u32 i = 0; i < _texLength; i++) {
        if (buffer[i * 4 + 3] == 0xFF)
            _texture[i] = bmpToDS((buffer[i * 4] >> 3) << 10 | (buffer[i * 4 + 1] >> 3) << 5 | (buffer[i * 4 + 2] >> 3));
    }
    return true;
}

bool Texture::loadGrf(FILE *file, const u32 (&header)[12]) noexcept {
    // findType() consumed the RIFF/HDR/GFX headers and the first GFX word.
    // fstat reads the open file's stored size; it does not seek through it.
    struct stat status;
    if (fstat(fileno(file), &status) || status.st_size < 48) return false;
    const uint64_t fileBytes = status.st_size;
    const uint64_t pixels = uint64_t(header[7]) * header[8];
    if (!header[7] || !header[8] || pixels > UINT32_MAX / 2) return false;

    _texWidth = header[7];
    _texHeight = header[8];
    if (_type == TextureType::PalettedGrf) {
        const u32 bytes = header[11] >> 8;
        if (bytes < (pixels + 1) / 2 || bytes > fileBytes - 48
            || fileBytes - 48 - bytes < 12) return false;
        _texLength = (bytes + 1) / 2;
        // glLoadTileSet uploads a power-of-two texture, even for a 192-row
        // dialog or 96-row sprite sheet. Give it owned, transparent padding.
        auto gpuSize = [](u32 size) {
            u32 result = 8;
            while (result < size && result < 1024) result *= 2;
            return result;
        };
        const u32 uploadWords = gpuSize(_texWidth) * gpuSize(_texHeight) / 4;
        _texture = std::make_unique<u16[]>(std::max(_texLength, uploadWords));
        if (fread(_texture.get(), 1, bytes, file) != bytes) return false;

        u32 paletteHeader[3];
        if (fread(paletteHeader, sizeof(u32), 3, file) != 3
            || paletteHeader[0] != CHUNK_ID('P', 'A', 'L', ' ')
            || (paletteHeader[2] & 0xFF)) return false;
        const u32 paletteBytes = paletteHeader[2] >> 8;
        const u32 colors = paletteBytes / 2;
        if ((paletteBytes & 1) || !colors || colors > UINT8_MAX
            || paletteBytes > fileBytes - 60 - bytes) return false;
        _paletteLength = colors;
        _palette = std::make_unique<u16[]>(paletteUploadLength());
        if (fread(_palette.get(), 1, paletteBytes, file) != paletteBytes) return false;
        effectColorModePalette(_palette.get(), _paletteLength);
    } else {
        _texCmpLength = header[10]; // Includes the LZ header already read.
        const u32 decodedBytes = header[11] >> 8;
        if (_texCmpLength < 4 || _texCmpLength > fileBytes - 44
            || decodedBytes != pixels * 2) return false;
        _texLength = decodedBytes / 2;
        _texture = std::make_unique<u16[]>((_texCmpLength + 1) / 2);
        std::memcpy(_texture.get(), &header[11], sizeof(u32));
        if (fread(reinterpret_cast<u8 *>(_texture.get()) + 4, 1, _texCmpLength - 4, file)
            != _texCmpLength - 4) return false;
    }
    return true;
}

void Texture::applyPaletteEffect(Texture::PaletteEffect effect) {
	if (_type & TextureType::Paletted) {
		effect(_palette.get(), _paletteLength);
	}
}

void Texture::applyBitmapEffect(Texture::BitmapEffect effect) {
	if (_type & TextureType::Bitmap) {
		effect(_texture.get(), _texLength);
	}
}

void Texture::applyUserPaletteFile(const std::string &filePath, Texture::PaletteEffect fallbackEffect) {
	if (_type & TextureType::Paletted) {
		FILE *file = fopen(filePath.c_str(), "rb");
		if (file) {
			u16 *pal = _palette.get();
			int offset = ((useTwlCfg ? *(u8*)0x02000444 : PersonalData->theme) * _paletteLength);
			fseek(file, sizeof(u16) * offset, SEEK_SET);
			fread(pal, sizeof(u16), _paletteLength, file);
			fclose(file);
			// swap palette bytes
			for (int i = 0; i < _paletteLength; i++) {
				pal[i] = (pal[i] << 8 & 0xFF00) | pal[i] >> 8;
			}
		}
		else {
			fallbackEffect(_palette.get(), _paletteLength);
		}
	}
}

u16 Texture::bmpToDS(u16 val) {
	// Return 0 for #ff00ff
	if ((val & 0x7FFF) == 0x7C1F)
		return 0;

	val = ((val >> 10) & 31) | (val & (31 << 5)) | ((val & 31) << 10) | BIT(15);
	if (colorTable) {
		return colorTable[val % 0x8000] | BIT(15);
	}
	return val;
}

void Texture::copy(u16 *dst, bool vram) const {
	switch(_type) {
		case TextureType::PalettedGrf:
		case TextureType::PalettedBmp:
			for (u32 i = 0; i < _texWidth * _texHeight / 2; i++) {
				const u8 byte = reinterpret_cast<const u8 *>(_texture.get())[i];
				*(dst++) = _palette[byte & 15];
				*(dst++) = _palette[byte >> 4];
			}
			if ((_texWidth * _texHeight) & 1)
				*dst = _palette[reinterpret_cast<const u8 *>(_texture.get())[_texWidth * _texHeight / 2] & 15];
			break;
		case TextureType::Bmp:
		case TextureType::Png:
			tonccpy(dst, _texture.get(), _texLength * sizeof(u16));
			break;
		case TextureType::CompressedGrf:
			decompress((u8 *)_texture.get(), (u8 *)dst, vram ? LZ77Vram : LZ77);
			effectColorModeBmpPalette(dst, _texLength);
			break;
		case TextureType::Unknown:
		case TextureType::Bitmap: // ingore the bitfields
		case TextureType::Paletted:
		case TextureType::Compressed:
			break;
	}
}
