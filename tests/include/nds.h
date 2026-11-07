#ifndef TEST_NDS_H
#define TEST_NDS_H
#include <cstdio>
#include "nds/ndstypes.h"
#define BIT(n) (1u << (n))
struct TestPersonalData { u8 theme; };
extern TestPersonalData *PersonalData;
enum DecompressType { LZ77, LZ77Vram };
void decompress(const void *source, void *destination, DecompressType type);
inline void nocashMessage(const char *) {}
#endif
