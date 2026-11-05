#include <fat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fat_ext.h"

bool fatGetAliasPath(const char *path, char *alias, size_t capacity) {
	if (!alias || !capacity) return false;
	alias[0] = '\0';
	if (!path) return false;
	char *full = realpath(path, NULL);
	if (!full) return false;
	char *component = strchr(full, ':');
	if (!component || component[1] != '/') goto fail;
	component += 2;
	size_t used = component - full;
	if (used >= capacity) goto fail;
	memcpy(alias, full, used);
	alias[used] = '\0';
	while (*component) {
		char *end = strchr(component, '/');
		if (end) *end = '\0';
		char shortName[FAT_SHORT_FILE_NAME_MAX + 1];
		bool ok = FAT_getShortNameFor(full, shortName);
		if (end) *end = '/';
		if (!ok) goto fail;
		size_t length = strlen(shortName);
		if (length + (end ? 1 : 0) >= capacity - used) goto fail;
		memcpy(alias + used, shortName, length);
		used += length;
		if (end) alias[used++] = '/';
		alias[used] = '\0';
		if (!end) break;
		component = end + 1;
	}
	free(full);
	return true;
fail:
	free(full);
	alias[0] = '\0';
	return false;
}
