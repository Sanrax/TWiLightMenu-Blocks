// Homebrew smoke-test target for direct launch and .argv handoff.
#include <nds.h>
#include <fat.h>
#include <filesystem.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include "fat_ext.h"

int bootFSInit(const char *path);

static void reportStorage(const char *device, const char *text) {
    char path[64];
    snprintf(path, sizeof(path), "%s/argv-result.txt", device);
    printf("%s", text);
    FILE *out = fopen(path, "a");
    if (out) { fputs(text, out); fclose(out); }
}

static void checkStorage(const char *device) {
    char bootstrap[128], menuPath[64], longPath[128];
    snprintf(bootstrap, sizeof(bootstrap), "%s/_nds/nds-bootstrap-hb-release.nds", device);
    snprintf(menuPath, sizeof(menuPath), "%s/menu.nds", device);
    snprintf(longPath, sizeof(longPath), "%s/Long Directory/Example file.txt", device);
    if (access(bootstrap, F_OK) == 0) {
        struct stat st;
        printf("Checking NitroFS...\n");
        bool menu = nitroFSInit(menuPath);
        printf("Checking boot filesystem...\n");
        bool boot = bootFSInit(bootstrap);
        bool loader = boot && stat("boot:/load.bin", &st) == 0 && st.st_size > 0;
        FILE *file = loader ? fopen("boot:/load.bin", "rb") : NULL;
        unsigned char bytes[4];
        bool read = file && fread(bytes, 1, sizeof(bytes), file) == sizeof(bytes);
        if (file) fclose(file);
        bool coexist = menu && stat("nitro:/", &st) == 0;
        char result[64];
        snprintf(result, sizeof(result), "boot: read=%d nitro:=%d\n", read, coexist);
        reportStorage(device, result);
    }
    if (access(longPath, F_OK) == 0) {
        char alias[256] = {0};
        bool ok = fatGetAliasPath(longPath, alias, sizeof(alias));
        FILE *file = ok ? fopen(alias, "rb") : NULL;
        bool readable = file && fgetc(file) == 'T';
        if (file) fclose(file);
        char result[300];
        snprintf(result, sizeof(result), "FAT alias: %d %s\n", readable, alias);
        reportStorage(device, result);
    }
}

int main(int argc, char **argv) {
    consoleDemoInit();
    bool mounted = fatInitDefault();
    const char *device = access("sd:/", F_OK) == 0 ? "sd:" : "fat:";
    char resultPath[64];
    snprintf(resultPath, sizeof(resultPath), "%s/argv-result.txt", device);
    printf("Launch probe\nargc=%d FAT=%d DSi=%d\n", argc, mounted, isDSiMode());
    FILE *out = mounted ? fopen(resultPath, "w") : NULL;
    if (out) fprintf(out, "argc=%d FAT=%d DSi=%d\n", argc, mounted, isDSiMode());
    for (int i = 0; i < argc; i++) {
        printf("%d: %s\n", i, argv[i]);
        if (out) fprintf(out, "%d: %s\n", i, argv[i]);
    }
    if (out) fclose(out);
    if (mounted) checkStorage(device);
    printf("Press B to return to the menu.\n");
    while (1) {
        swiWaitForVBlank();
        scanKeys();
        if (keysDown() & KEY_B) return 0;
    }
}
