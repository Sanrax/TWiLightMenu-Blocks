#include "common/startupprofile.h"

#if defined(TWLM_PROFILE_STARTUP) && CURRENT_SCREEN_MODE == 3
#include <nds.h>
#include <cstdio>
#include "common/systemdetails.h"

namespace {
struct Point {
    const char *phase;
    u32 ticks;
};
Point points[32];
unsigned count;
bool active;
}

void startupProfileBegin()
{
    // ARM9 timers 0/1 are unused by this menu. Maxmod's stream timer is ARM7.
    // Capture in memory; no filesystem writes until the first menu frame.
    cpuStartTiming(0);
    points[0] = {"main", 0};
    count = 1;
    active = true;
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
    const char *path = sys().isRunFromSD() ? "sd:/_nds/TWiLightMenu/startup-profile.csv"
                                         : "fat:/_nds/TWiLightMenu/startup-profile.csv";
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
