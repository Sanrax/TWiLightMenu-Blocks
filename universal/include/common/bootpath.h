#pragma once

#include <cstring>

namespace bootpath {

constexpr const char* flashcardMenu = "fat:/_nds/TWiLightMenu/main.srldr";
constexpr const char* sdMenu = "sd:/_nds/TWiLightMenu/main.srldr";

// Ready means both the filesystem and its main.srldr are available. Preserve
// the SD primary marker, then prefer the explicitly supplied launch device.
inline const char* mainMenu(bool flashcardReady, bool sdReady, bool sdPrimary,
                            const char* runningPath) {
    const bool launchedFromSd = runningPath &&
        (std::strncmp(runningPath, "sd:/", 4) == 0 || std::strncmp(runningPath, "sdmc:/", 6) == 0);
    if (sdReady && (sdPrimary || launchedFromSd || !flashcardReady)) return sdMenu;
    return flashcardReady ? flashcardMenu : nullptr;
}

} // namespace bootpath
