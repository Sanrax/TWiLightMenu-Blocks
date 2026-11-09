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
    puts("Bootstrap handoff control r16\n");
    puts("Default SDK ARM7 + menu loader\nNo menu UI or settings writes\n");
    if (!isDSiMode()) {
        puts("DSi mode required.\nLaunch from console SD\nthrough Unlaunch.");
        goto done;
    }
    if (!fatInitDefault()) { puts("Mount failed"); goto done; }
    const char *path = "sd:/_nds/nds-bootstrap-release.nds";
    struct stat st;
    if (stat(path, &st)) { puts("Bootstrap missing"); goto done; }
    printf("Bootstrap: %lu bytes\nCluster: %lu\n", (unsigned long)st.st_size,
           (unsigned long)st.st_ino);
    if (stat("sd:/_nds/nds-bootstrap.ini", &st)) {
        puts("nds-bootstrap.ini missing.\nSelect a game in TWiLight first.");
        goto done;
    }
    puts("\nUses the existing bootstrap INI.\nPress A to launch.");
    do {
        swiWaitForVBlank();
        scanKeys();
    } while (!(keysDown() & KEY_A));
    int error = runNdsFile(path, 1, &path, true, false, true, false, true, true, false, -1, 0);
    printf("Launch returned: %d\n", error);
done:
    for (;;) swiWaitForVBlank();
}
