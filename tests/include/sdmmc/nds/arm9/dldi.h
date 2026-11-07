#pragma once
#include <nds/system.h>
typedef struct {
    struct {
        bool (*startup)(void);
        bool (*isInserted)(void);
        bool (*readSectors)(u32, u32, void *);
        bool (*writeSectors)(u32, u32, void *);
    } ioInterface;
} DLDI_INTERFACE;
extern DLDI_INTERFACE *io_dldi_data;
