// Read-only NitroFS mount for nds-bootstrap, separate from the menu's NitroFS.
// BlocksDS provides the menu mount; device I/O provides the second "boot:" drive.
#ifdef __BLOCKSDS__
#include <stdbool.h>
#include <nds/arm9/device_io.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>

typedef struct {
	uint32_t start, size, pos;
} BootFile;

static FILE *rom;
static uint8_t *fnt;
static uint32_t fntSize, fatOffset, fatSize, romSize;
static unsigned openFiles;
static int deviceIndex = -1;

static uint16_t read16(const uint8_t *p) {
	return p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read32(const uint8_t *p) {
	return read16(p) | ((uint32_t)read16(p + 2) << 16);
}

static bool readAt(FILE *file, uint32_t offset, void *buffer, size_t size) {
	return fseek(file, offset, SEEK_SET) == 0 && fread(buffer, 1, size, file) == size;
}

static bool isBootDrive(const char *name) {
	return strcmp(name, "boot") == 0;
}

// Resolve FNT directory IDs and FAT file IDs without changing the current drive.
static int lookup(const char *path) {
	if (!rom) { errno = ENODEV; return -1; }
	if (strncmp(path, "boot:", 5) == 0) path += 5;
	unsigned dir = 0xf000;
	while (true) {
		while (*path == '/') path++;
		if (!*path) return dir;
		const char *end = strchr(path, '/');
		size_t length = end ? (size_t)(end - path) : strlen(path);
		uint32_t table = (dir & 0xfff) * 8;
		if (table > fntSize - 8) break;
		if (length == 1 && path[0] == '.') {
			path += length;
			continue;
		}
		if (length == 2 && path[0] == '.' && path[1] == '.') {
			if (dir != 0xf000) dir = read16(fnt + table + 6);
			path += length;
			continue;
		}
		uint32_t pos = read32(fnt + table);
		unsigned id = read16(fnt + table + 4);
		bool found = false;
		while (pos < fntSize && fnt[pos]) {
			unsigned type = fnt[pos++];
			unsigned len = type & 0x7f;
			if (len + ((type & 0x80) ? 2 : 0) > fntSize - pos) break;
			if (len == length && memcmp(fnt + pos, path, len) == 0) {
				if (type & 0x80) {
					dir = read16(fnt + pos + len);
					path += length;
					found = true;
					break;
				}
				if (end) { errno = ENOTDIR; return -1; }
				return id;
			}
			pos += len + ((type & 0x80) ? 2 : 0);
			if (!(type & 0x80)) id++;
		}
		if (!found) break;
	}
	errno = ENOENT;
	return -1;
}

static int getFile(int id, BootFile *file) {
	uint8_t entry[8];
	if (id < 0) return -1;
	if (id >= 0xf000) { errno = EISDIR; return -1; }
	if ((uint32_t)id >= fatSize / 8 || !readAt(rom, fatOffset + id * 8, entry, 8)) {
		errno = EIO;
		return -1;
	}
	uint32_t start = read32(entry), end = read32(entry + 4);
	if (end < start || end > romSize) { errno = EIO; return -1; }
	*file = (BootFile){start, end - start, 0};
	return 0;
}

static int bootOpen(const char *path, int flags, mode_t mode) {
	(void)mode;
	if ((flags & O_ACCMODE) != O_RDONLY || (flags & (O_CREAT | O_TRUNC | O_APPEND))) {
		errno = EROFS;
		return -1;
	}
	BootFile file;
	if (getFile(lookup(path), &file) < 0) return -1;
	BootFile *handle = malloc(sizeof(*handle));
	if (!handle) { errno = ENOMEM; return -1; }
	*handle = file;
	openFiles++;
	return (int)(uintptr_t)handle;
}

static int bootClose(int fd) {
	free((void *)(uintptr_t)fd);
	openFiles--;
	return 0;
}

static ssize_t bootRead(int fd, void *buffer, size_t size) {
	BootFile *file = (BootFile *)(uintptr_t)fd;
	if (size > file->size - file->pos) size = file->size - file->pos;
	if (!size) return 0;
	if (!readAt(rom, file->start + file->pos, buffer, size)) { errno = EIO; return -1; }
	file->pos += size;
	return size;
}

static off_t bootSeek(int fd, off_t offset, int whence) {
	BootFile *file = (BootFile *)(uintptr_t)fd;
	int64_t pos = offset;
	if (whence == SEEK_CUR) pos += file->pos;
	else if (whence == SEEK_END) pos += file->size;
	else if (whence != SEEK_SET) { errno = EINVAL; return -1; }
	if (pos < 0 || pos > file->size) { errno = EINVAL; return -1; }
	file->pos = pos;
	return pos;
}

static int fileStat(const BootFile *file, struct stat *st) {
	memset(st, 0, sizeof(*st));
	st->st_mode = S_IFREG | 0444;
	st->st_size = file->size;
	st->st_blksize = 512;
	return 0;
}

static int bootFstat(int fd, struct stat *st) {
	return fileStat((BootFile *)(uintptr_t)fd, st);
}

static int bootStat(const char *path, struct stat *st) {
	int id = lookup(path);
	if (id < 0) return -1;
	if (id >= 0xf000) {
		memset(st, 0, sizeof(*st));
		st->st_mode = S_IFDIR | 0555;
		return 0;
	}
	BootFile file;
	return getFile(id, &file) < 0 ? -1 : fileStat(&file, st);
}

static const device_io_t device = {
	.isdrive = isBootDrive, .open = bootOpen, .close = bootClose,
	.read = bootRead, .lseek = bootSeek, .fstat = bootFstat, .stat = bootStat
};

int bootFSInit(const char *path) {
	if (!path) { errno = EINVAL; return 0; }
	if (openFiles) { errno = EBUSY; return 0; }
	FILE *next = fopen(path, "rb");
	if (!next) return 0;
	uint8_t header[0x50];
	uint8_t *names = NULL;
	if (fseek(next, 0, SEEK_END) != 0) goto fail;
	long size = ftell(next);
	if (size < (long)sizeof(header) || !readAt(next, 0, header, sizeof(header))) goto fail;
	uint32_t offset = read32(header + 0x40), count = read32(header + 0x44);
	uint32_t fat = read32(header + 0x48), fatCount = read32(header + 0x4c);
	if (offset > (uint32_t)size || count < 8 || count > (uint32_t)size - offset ||
		fat > (uint32_t)size || fatCount > (uint32_t)size - fat || (fatCount & 7)) goto fail;
	names = malloc(count);
	if (!names || !readAt(next, offset, names, count)) goto fail;
	if (deviceIndex < 0) {
		deviceIndex = deviceIoAdd(&device);
		if (deviceIndex < 0) goto fail;
	}
	if (rom) fclose(rom);
	free(fnt);
	rom = next;
	fnt = names;
	fntSize = count;
	fatOffset = fat;
	fatSize = fatCount;
	romSize = size;
	return 1;
fail:
	free(names);
	fclose(next);
	errno = EIO;
	return 0;
}
#endif
