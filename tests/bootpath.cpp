#include "common/bootpath.h"

#include <cassert>
#include <cstdio>
#include <cstring>

static void expect(bool flashcard, bool sd, bool primary, const char* path, const char* wanted) {
    const char* result = bootpath::mainMenu(flashcard, sd, primary, path);
    assert((!result && !wanted) || (result && wanted && std::strcmp(result, wanted) == 0));
}

int main() {
    const char* fat = bootpath::flashcardMenu;
    const char* sd = bootpath::sdMenu;
    expect(true, false, false, nullptr, fat);
    expect(false, true, false, nullptr, sd);
    expect(true, true, false, nullptr, fat);
    expect(true, true, true, nullptr, sd);
    expect(true, true, false, "fat:/BOOT.NDS", fat);
    expect(true, true, false, "sd:/BOOT.NDS", sd);
    expect(true, true, false, "sdmc:/BOOT.NDS", sd);
    expect(true, true, true, "fat:/BOOT.NDS", sd);
    expect(false, true, false, "fat:/BOOT.NDS", sd);
    expect(true, false, true, "sd:/BOOT.NDS", fat);
    expect(false, false, true, "sd:/BOOT.NDS", nullptr);
    expect(false, false, false, nullptr, nullptr);
    expect(true, true, false, "BOOT.NDS", fat);
    expect(true, true, false, "s", fat);
    expect(true, true, false, "", fat);
    std::puts("Boot device selection, explicit launch paths, primary marker and missing installations passed");
}
