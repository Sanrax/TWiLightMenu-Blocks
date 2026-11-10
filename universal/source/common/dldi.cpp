#include "common/storage.h"

#include <nds.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Bound both input and output, including back-references, before installing the
// compressed NRIO driver. Its compressed copy is also needed by later loaders.
static bool unpackDldi(const u8 *input, size_t size, u8 *output, size_t capacity, size_t &length) {
	if (size < 4 || input[0] != 0x10) return false;
	length = input[1] | (input[2] << 8) | (input[3] << 16);
	if (!length || length > capacity) return false;
	size_t src = 4, dst = 0;
	while (dst < length) {
		if (src == size) return false;
		unsigned flags = input[src++];
		for (unsigned bit = 0x80; bit && dst < length; bit >>= 1) {
			if (!(flags & bit)) {
				if (src == size) return false;
				output[dst++] = input[src++];
			} else {
				if (size - src < 2) return false;
				unsigned a = input[src++], b = input[src++];
				size_t offset = ((a & 15) << 8) + b + 1;
				size_t count = (a >> 4) + 3;
				if (offset > dst || count > length - dst) return false;
				while (count--) { output[dst] = output[dst - offset]; dst++; }
			}
		}
	}
	return true;
}

bool twlmLoadDldi(const char *path) {
	FILE *file = fopen(path, "rb");
	if (!file) return false;
	u8 *image = static_cast<u8 *>(calloc(1, 0x8000));
	if (!image) { fclose(file); return false; }
	size_t size = fread(image, 1, 0x8000, file);
	const int extra = fgetc(file);
	bool ok = extra == EOF && !ferror(file);
	fclose(file);
	u8 *unpacked = nullptr;
	const char *ext = strrchr(path, '.');
	const bool compressed = ext && strcmp(ext, ".lz77") == 0;
	if (ok && compressed) {
		unpacked = static_cast<u8 *>(calloc(1, 0x8000));
		size_t length = 0;
		ok = size <= 0x3FFC && unpacked && unpackDldi(image, size, unpacked, 0x8000, length)
			&& twlmInstallDldi(unpacked, length);
		if (ok) {
			memcpy(reinterpret_cast<void *>(0x02FF8004), image, size);
			*reinterpret_cast<u32 *>(0x02FF8000) = 0x53535A4C;
			DC_FlushRange(reinterpret_cast<void *>(0x02FF8000), size + 4);
		}
	} else if (ok) {
		ok = twlmInstallDldi(image, size);
	}
	free(unpacked);
	free(image);
	return ok;
}
