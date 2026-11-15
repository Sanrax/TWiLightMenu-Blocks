#include <dirent.h>

#define DIR FF_DIR
#include "blocksds/fatfs/ff.h"
#undef DIR

_Static_assert(sizeof(((FILINFO *)0)->fname) <= sizeof(((struct dirent *)0)->d_name),
               "FatFS directory names must fit the directory adapter buffer");
_Static_assert(sizeof(TCHAR) == 1, "The adapter expects UTF-8 filename bytes");
