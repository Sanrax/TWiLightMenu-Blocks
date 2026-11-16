#include "common/parentDirectorySelection.h"

#include <cassert>
#include <cstdlib>
#include <new>
#include <string>
#include <vector>

static size_t allocations;
void *operator new(size_t bytes) {
    ++allocations;
    if (void *memory = std::malloc(bytes)) return memory;
    throw std::bad_alloc();
}
void operator delete(void *memory) noexcept { std::free(memory); }
void operator delete(void *memory, size_t) noexcept { std::free(memory); }

struct Entry { std::string name; bool isDirectory; };

int main() {
    twlm::ParentDirectorySelection selection;
    std::vector<Entry> entries = {{"..", true}, {"other", true}, {"Child", true}, {"game.nds", false}};
    assert(selection.restore(entries) == -1);
    std::string currentPath = "sd:/roms/Child/";
    selection.remember(currentPath);
    currentPath = "sd:/roms"; // The browser overwrites its saved path before restoring.
    assert(selection.restore(entries) == 2);
    assert(selection.restore(entries) == -1); // A later page refresh must not jump back.

    selection.remember("fat:/roms/cHiLd");
    assert(selection.restore(entries) == 2);
    selection.remember("sd:/roms/Child/Inner");
    std::vector<Entry> child = {{"..", true}, {"Inner", true}};
    assert(selection.restore(child) == 1);
    selection.remember("sd:/roms/Child");
    assert(selection.restore(entries) == 2);

    for (const char *root : {"sd:/", "fat:/", "nand:/", "/", "", "sd:////"}) {
        selection.remember(root);
        assert(selection.restore(entries) == -1);
    }
    selection.remember("sd:/roms/missing");
    assert(selection.restore(entries) == -1);
    entries[2].isDirectory = false;
    selection.remember("sd:/roms/Child");
    assert(selection.restore(entries) == -1); // Never select a replacement file.

    // Locate the name after sorting/inserting/removing entries, not its old index.
    std::vector<Entry> paged;
    for (int i = 0; i < 123; ++i) paged.push_back({"folder" + std::to_string(i), true});
    selection.remember("fat:/roms/folder85");
    int index = selection.restore(paged);
    assert(index == 85 && index / 40 == 2 && index % 40 == 5);
    paged.insert(paged.begin(), {"new folder", true});
    selection.remember("fat:/roms/folder85");
    assert(selection.restore(paged) == 86);
    selection.remember("fat:/roms/new folder");
    assert(selection.restore(paged) == 0);

    const std::string longName(255, 'a');
    const std::string longPath = "sd:/roms/" + longName;
    std::vector<Entry> longEntries = {{longName, true}};
    selection.remember(longPath);
    assert(selection.restore(longEntries) == 0);
    const size_t before = allocations;
    for (int i = 0; i < 10000; ++i) {
        selection.remember(longPath);
        assert(selection.restore(longEntries) == 0);
    }
    assert(allocations == before); // The filename allocation is reused.

    std::vector<Entry> unicode = {{"日本語", true}};
    selection.remember("sd:/roms/日本語");
    assert(selection.restore(unicode) == 0);
    selection.remember("sd:/roms/Child");
    assert(selection.restore(std::vector<Entry>{}) == -1);
}
