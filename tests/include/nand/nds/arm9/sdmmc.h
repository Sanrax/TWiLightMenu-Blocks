#pragma once
#include <nds.h>
bool nand_Startup(void);
uint32_t nand_GetSectors(void);
bool nand_ReadSectors(sec_t, sec_t, void *);
