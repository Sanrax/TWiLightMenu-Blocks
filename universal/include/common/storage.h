#ifndef TWLM_STORAGE_H
#define TWLM_STORAGE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <dirent.h>
#include <nds/disc_io.h>

#ifdef __cplusplus
extern "C" {
#endif

// Unlike fatInit(), this supports late flashcard discovery and failed-mount retries.
bool twlmMountStorage(const char *name, const DISC_INTERFACE *interface);

// Install a late-discovered flashcard driver before its filesystem is mounted.
bool twlmInstallDldi(const void *image, size_t size);
bool twlmLoadDldi(const char *path);

// A single directory pass returns both names and FAT attributes.
typedef struct TWLDirectory TWLDirectory;
TWLDirectory *twlmOpenDirectory(const char *path);
const struct dirent *twlmReadDirectory(TWLDirectory *directory, uint8_t *attributes);
void twlmCloseDirectory(TWLDirectory *directory);

// Does not scan the FAT to calculate unused space.
unsigned twlmStorageClusterSize(bool secondaryDevice);

#ifdef __cplusplus
}
#endif
#endif
