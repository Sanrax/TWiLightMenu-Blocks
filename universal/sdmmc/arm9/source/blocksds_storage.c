// Mount native BlocksDS storage while retaining late discovery and FAT metadata.
#ifdef __BLOCKSDS__
#include <nds/arm9/dldi.h>
#include <nds/arm9/sdmmc.h>
#include <nds/disc_io.h>
#include <nds/system.h>
#include <string.h>
#include <errno.h>
#include <fat.h>
#include <blocksds_version.h>
#include "common/storage.h"

// FatFS names its directory type DIR too. Keep it local to this adapter.
#define DIR FF_DIR
#include "blocksds/fatfs/ff.h"
#undef DIR

#if !BLOCKSDS_VERSION_EQUALS(1, 24, 0)
#error Recheck the FatFS adapter ABI when upgrading BlocksDS (see fatfs/README.md)
#endif

extern FRESULT libnds_f_mount(FATFS *fs, const TCHAR *path, BYTE opt);
extern FRESULT libnds_f_opendir(FF_DIR *dir, const TCHAR *path);
extern FRESULT libnds_f_readdir(FF_DIR *dir, FILINFO *info);
extern FRESULT libnds_f_closedir(FF_DIR *dir);
extern int cache_init(int32_t sectors);
extern bool cache_initialized(void);

static FATFS mounts[3];
static const DISC_INTERFACE *interfaces[3];

bool __real_fatInit(int32_t cacheSize, bool setDefault);
bool __wrap_fatInit(int32_t cacheSize, bool setDefault) {
	// NitroFS calls fatInitDefault internally. Preserve our existing mounts.
	if (mounts[0].fs_type || mounts[1].fs_type) return true;
	return __real_fatInit(cacheSize, setDefault);
}

bool twlmMountStorage(const char *name, const DISC_INTERFACE *interface) {
	if (!name || !interface) { errno = EINVAL; return false; }
	int drive = strcmp(name, "fat") == 0 ? 0 : strcmp(name, "sd") == 0 ? 1 : strcmp(name, "nand") == 0 ? 2 : -1;
	if (drive < 0) { errno = EINVAL; return false; }
	// A newly installed DLDI driver can grow into memory previously used as
	// cache. Recompute those boundaries when the flashcard is (re)mounted.
	if ((!cache_initialized() || drive == 0) && cache_init(40) != 0) { errno = ENOMEM; return false; }
	interfaces[drive] = interface;
	const char *paths[] = {"fat:", "sd:", "nand:"};
	FRESULT result = libnds_f_mount(&mounts[drive], paths[drive], 1);
	if (result != FR_OK) { errno = EIO; return false; }
	return true;
}

struct TWLDirectory {
	FF_DIR handle;
	struct dirent entry;
};

TWLDirectory *twlmOpenDirectory(const char *path) {
	TWLDirectory *dir = calloc(1, sizeof(*dir));
	if (!dir) { errno = ENOMEM; return NULL; }
	if (libnds_f_opendir(&dir->handle, path) != FR_OK) {
		free(dir);
		errno = EIO;
		return NULL;
	}
	return dir;
}

const struct dirent *twlmReadDirectory(TWLDirectory *dir, uint8_t *attributes) {
	FILINFO info;
	FRESULT result = libnds_f_readdir(&dir->handle, &info);
	if (result != FR_OK) { errno = EIO; return NULL; }
	if (!info.fname[0]) return NULL;
	strcpy(dir->entry.d_name, info.fname);
	dir->entry.d_type = (info.fattrib & AM_DIR) ? DT_DIR : DT_REG;
	dir->entry.d_ino = info.fclust;
	if (attributes) *attributes = info.fattrib;
	return &dir->entry;
}

void twlmCloseDirectory(TWLDirectory *dir) {
	if (!dir) return;
	libnds_f_closedir(&dir->handle);
	free(dir);
}

unsigned twlmStorageClusterSize(bool secondaryDevice) {
	return mounts[secondaryDevice ? 0 : 1].csize * 512;
}

extern const DISC_INTERFACE io_dsi_nand __attribute__((weak));

const DISC_INTERFACE *__wrap_get_io_dsinand(void) {
	return interfaces[2] ? interfaces[2] : isDSiMode() ? &io_dsi_nand : NULL;
}

const DISC_INTERFACE *__real_dldiGetInternal(void);
const DISC_INTERFACE *__wrap_dldiGetInternal(void) {
	if (interfaces[0]) return interfaces[0];
	// iEvolution requires ARM7 execution; the SDK FIFO service supports it.
	if (isDSiMode() && strncmp(io_dldi_data->friendlyName, "CycloDS iEvolution", 18) == 0)
		dldiSetMode(DLDI_MODE_ARM7);
	return __real_dldiGetInternal();
}
#endif
