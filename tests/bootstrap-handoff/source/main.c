#include <nds.h>
#include <fat.h>
#include <stdio.h>
#include <sys/stat.h>
#include "common/nds_loader_arm9.h"

static bool unlaunchSettings;
bool *unlaunchSettingsPtr = &unlaunchSettings;
int *removeLauncherPatchesPtr;
const char *charUnlaunchBg = "";

int main(void) {
    consoleDemoInit();
    puts("Bootstrap handoff control r17\n");
    puts("Default SDK ARM7 + menu loader\nNo menu UI or settings writes\n");
    if (!isDSiMode()) {
        puts("DSi mode required.\nLaunch from console SD\nthrough Unlaunch.");
        goto done;
    }
    if (!fatInitDefault()) { puts("Mount failed"); goto done; }
    const char *path;
    struct stat st;
    if (stat("sd:/_nds/nds-bootstrap.ini", &st)) {
        puts("nds-bootstrap.ini missing.\nSelect a game in TWiLight first.");
        goto done;
    }
    puts("Uses the existing bootstrap INI.\n");
    puts("A: Official release (unchanged)\nX: CPU SD-read experiment\nY: ARM9-state log, then STOP\n");
    puts("Y deliberately stays white.\nRead NDSBTSRP.LOG afterwards.");
    for (;;) {
        swiWaitForVBlank();
        scanKeys();
        unsigned keys = keysDown();
        if (keys & KEY_A) { path = "sd:/_nds/nds-bootstrap-release.nds"; break; }
        if (keys & KEY_X) { path = "sd:/_nds/twlm-diag/bootstrap-cpu.nds"; break; }
        if (keys & KEY_Y) { path = "sd:/_nds/twlm-diag/bootstrap-state.nds"; break; }
    }
    if (stat(path, &st)) { printf("Missing: %s\n", path); goto done; }
    int error = runNdsFile(path, 1, &path, true, false, true, false, true, true, false, -1, 0);
    printf("Launch returned: %d\n", error);
done:
    for (;;) swiWaitForVBlank();
}
