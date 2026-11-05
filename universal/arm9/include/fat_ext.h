#ifndef _LIBFAT_EXT_H
#define _LIBFAT_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <nds/disc_io.h>

/*
Get Alias Name
*/
// extern void fatGetAliasName (const char* drive, const char* name, char *alias);

/*
Get Alias Path
*/
// Returns false and clears alias on lookup failure or insufficient capacity.
extern bool fatGetAliasPath(const char *path, char *alias, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif // _LIBFAT_EXT_H
