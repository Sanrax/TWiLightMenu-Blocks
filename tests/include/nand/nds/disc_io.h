#pragma once
#include <nds.h>
#define FEATURE_MEDIUM_CANREAD 1
struct DISC_INTERFACE_STRUCT {
 unsigned long ioType,features;
 bool (*startup)(void),(*isInserted)(void);
 bool (*readSectors)(sec_t,sec_t,void*);
 bool (*writeSectors)(sec_t,sec_t,const void*);
 bool (*clearStatus)(void),(*shutdown)(void);
};
typedef struct DISC_INTERFACE_STRUCT DISC_INTERFACE;
