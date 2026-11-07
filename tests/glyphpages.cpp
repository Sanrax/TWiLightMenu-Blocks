#include "common/nftrGlyphPages.h"

#include <cassert>
#include <iostream>
#include <random>
#include <vector>

static size_t reads, readBytes;
static bool shortRead, failSeek;
extern "C" size_t __real_fread(void*, size_t, size_t, FILE*);
extern "C" int __real_fseek(FILE*, long, int);
extern "C" size_t __wrap_fread(void* dst, size_t size, size_t count, FILE* file) {
    ++reads;
    if (shortRead) { count /= 2; shortRead = false; }
    const size_t got = __real_fread(dst, size, count, file);
    readBytes += got * size;
    return got;
}
extern "C" int __wrap_fseek(FILE* file, long offset, int origin) {
    if (failSeek) { failSeek = false; return -1; }
    return __real_fseek(file, offset, origin);
}

using twlm::NftrGlyphPages;

static void geometry(uint32_t offset, uint32_t cellSize, uint32_t count) {
    const uint32_t bytes = cellSize * count;
    const auto capacity = NftrGlyphPages::storageSize(offset, bytes, cellSize);
    assert(capacity);
    std::vector<uint32_t> storage((capacity + 3) / 4, 0xCCCCCCCC);
    std::vector<uint8_t> expected(offset + bytes + 512);
    for (size_t i = 0; i < expected.size(); ++i) expected[i] = (i * 37 + i / 17) % 256;
    FILE* file = std::tmpfile();
    assert(file);
    assert(std::fwrite(expected.data(), 1, expected.size(), file) == expected.size());
    assert(std::fflush(file) == 0);
    NftrGlyphPages pages;
    reads = readBytes = 0;
    assert(pages.attach(storage.data(), offset, bytes, cellSize, true));
    assert(reads == 0 && pages.active());
    assert(!pages.load(file, count) && !pages.load(file, UINT32_MAX));
    assert(!pages.load(nullptr, 0));
    std::vector<uint32_t> order;
    for (uint32_t i = 0; i < count; ++i) order.push_back(i);
    std::shuffle(order.begin(), order.end(), std::mt19937(42));
    for (auto i : order) {
        assert(pages.load(file, i));
        assert(std::memcmp(pages.data() + i * cellSize, expected.data() + offset + i * cellSize, cellSize) == 0);
        const auto prior = reads;
        assert(pages.load(file, i) && prior == reads);
    }
    assert(readBytes == bytes); // Each populated page is read once, without chunk headers/trailing bytes.
    assert(reads == (offset % 512 + bytes + 511) / 512);
    NftrGlyphPages nextMenu;
    assert(nextMenu.attach(storage.data(), offset, bytes, cellSize, false));
    const auto prior = reads;
    for (auto i : order) assert(nextMenu.load(file, i));
    assert(prior == reads);
    assert(!nextMenu.attach(storage.data(), offset + 1, bytes, cellSize, false));
    assert(!nextMenu.active());
    std::fclose(file);
}

static void retry() {
    constexpr uint32_t offset = 56, cell = 60, bytes = 60 * 30;
    std::vector<uint32_t> storage((NftrGlyphPages::storageSize(offset, bytes, cell) + 3) / 4);
    std::vector<uint8_t> expected(offset + bytes, 0x57);
    FILE* file = std::tmpfile(); assert(file);
    assert(std::fwrite(expected.data(), 1, expected.size(), file) == expected.size());
    NftrGlyphPages pages;
    assert(pages.attach(storage.data(), offset, bytes, cell, true));
    reads = 0;
    failSeek = true;
    assert(!pages.load(file, 7)); // Glyph spans both first and second pages.
    assert(reads == 0);
    shortRead = true;
    assert(!pages.load(file, 7) && reads == 1);
    assert(pages.load(file, 7) && reads == 3);
    assert(std::memcmp(pages.data() + 7 * cell, expected.data() + offset + 7 * cell, cell) == 0);
    shortRead = true;
    assert(!pages.load(file, 20));
    NftrGlyphPages nextMenu;
    assert(nextMenu.attach(storage.data(), offset, bytes, cell, false));
    assert(nextMenu.load(file, 20));
    assert(std::memcmp(nextMenu.data() + 20 * cell, expected.data() + offset + 20 * cell, cell) == 0);
    // Explicit cache reset discards validity, even when reusing the same RAM.
    assert(nextMenu.attach(storage.data(), offset, bytes, cell, true));
    const auto prior = reads;
    assert(nextMenu.load(file, 7) && reads == prior + 2);
    storage[0] ^= 1;
    assert(!nextMenu.attach(storage.data(), offset, bytes, cell, false));
    std::fclose(file);
}

static void font(const char* path) {
    FILE* file = std::fopen(path, "rb"); assert(file);
    assert(std::fseek(file, 0x14, SEEK_SET) == 0);
    uint32_t infoSize = 0;
    assert(std::fread(&infoSize, 4, 1, file) == 1);
    assert(std::fseek(file, 0x14 + infoSize, SEEK_SET) == 0);
    uint32_t chunkSize = 0;
    uint16_t cell = 0;
    assert(std::fread(&chunkSize, 4, 1, file) == 1);
    assert(std::fseek(file, 2, SEEK_CUR) == 0);
    assert(std::fread(&cell, 2, 1, file) == 1);
    const uint32_t offset = infoSize + 0x20, bytes = chunkSize - 16;
    const uint32_t count = bytes / cell;
    std::vector<uint8_t> expected(bytes);
    assert(std::fseek(file, offset, SEEK_SET) == 0);
    assert(std::fread(expected.data(), 1, bytes, file) == bytes);
    std::vector<uint32_t> storage((NftrGlyphPages::storageSize(offset, bytes, cell) + 3) / 4, 0xDDDDDDDD);
    NftrGlyphPages pages;
    assert(pages.attach(storage.data(), offset, bytes, cell, true));
    reads = readBytes = 0;
    for (uint32_t i = count; i-- > 0;) assert(pages.load(file, i));
    assert(std::memcmp(pages.data(), expected.data(), bytes) == 0);
    assert(readBytes == bytes && reads == (offset % 512 + bytes + 511) / 512);
    std::cout << path << ": " << count << " glyphs, " << reads << " pages verified\n";
    std::fclose(file);
}

int main(int argc, char** argv) {
    assert(!NftrGlyphPages::storageSize(0, 0, 1));
    assert(!NftrGlyphPages::storageSize(0, 100, 0));
    assert(!NftrGlyphPages::storageSize(0, 100, 3));
    assert(!NftrGlyphPages::storageSize(UINT32_MAX, 100, 1));
    assert(!NftrGlyphPages::storageSize(INT32_MAX, 100, 1));
    NftrGlyphPages empty;
    assert(!empty.attach(nullptr, 0, 100, 1, true) && !empty.active());
    for (uint32_t offset : {0, 1, 56, 511, 512, 1023})
        for (uint32_t cell : {1, 48, 60, 511, 512, 513, 1025})
            geometry(offset, cell, 45);
    retry();
    for (int i = 1; i < argc; ++i) font(argv[i]);
    std::cout << "Glyph paging: boundaries, read failures, reuse and complete bitmap identities passed\n";
}
