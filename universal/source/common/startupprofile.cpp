#include "common/startupprofile.h"

#if defined(TWLM_PROFILE_STARTUP) && (CURRENT_SCREEN_MODE == 0 || CURRENT_SCREEN_MODE == 1 || CURRENT_SCREEN_MODE == 2 || CURRENT_SCREEN_MODE == 3)
#include <nds.h>
#include <cstdio>
#include "common/systemdetails.h"

namespace {
struct Point {
    const char *phase;
    u32 ticks;
};
Point points[64];
unsigned count;
bool active;
const char *profileFilename;
// The title's GIF splash uses ARM9 timer 0. Maxmod streams use ARM7 timers.
constexpr int timer = CURRENT_SCREEN_MODE == 0 ? 2 : 0;
}

void startupProfileBegin(const char *filename)
{
    // Capture in RAM; write only after the measured interval ends.
    cpuStartTiming(timer);
    points[0] = {"main", 0};
    count = 1;
    active = true;
    profileFilename = filename;
}

void startupProfileMark(const char *phase)
{
    if (active && count < sizeof(points) / sizeof(points[0]))
        points[count++] = {phase, cpuGetTiming()};
}

void startupProfileEnd()
{
    if (!active) return;
    startupProfileMark("ready");
    cpuEndTiming();
    active = false;
    constexpr const char *filename = CURRENT_SCREEN_MODE == 0 ? "title-startup-profile.csv"
        : CURRENT_SCREEN_MODE == 1 ? "settings-startup-profile.csv"
        : CURRENT_SCREEN_MODE == 2 ? "classic-startup-profile.csv" : "startup-profile.csv";
    char path[96];
    snprintf(path, sizeof(path), "%s:/_nds/TWiLightMenu/%s", sys().isRunFromSD() ? "sd" : "fat", profileFilename ? profileFilename : filename);
    FILE *file = fopen(path, "wb");
    if (!file) return;
    fputs("phase,total_us,phase_us\n", file);
    for (unsigned i = 1; i < count; i++) {
        fprintf(file, "%s,%lu,%lu\n", points[i].phase,
                (unsigned long)timerTicks2usec(points[i].ticks),
                (unsigned long)timerTicks2usec(points[i].ticks - points[i - 1].ticks));
    }
    fclose(file);
}
#endif
