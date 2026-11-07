#pragma once

#include <cstdio>

namespace twlm {

// Read-only asset access. External paths use normal stdio; immutable NitroFS
// directories are indexed once and files opened by the native BlocksDS ID.
// Call on the main thread. Clear before replacing the mounted NitroFS image.
FILE* openNitroFile(const char* path);
void clearNitroFileCache();

} // namespace twlm
