#ifndef TEST_DEVICE_IO_H
#define TEST_DEVICE_IO_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
typedef struct {
    bool (*isdrive)(const char *);
    int (*open)(const char *, int, mode_t);
    int (*close)(int);
    ssize_t (*read)(int, void *, size_t);
    off_t (*lseek)(int, off_t, int);
    int (*fstat)(int, struct stat *);
    int (*stat)(const char *, struct stat *);
} device_io_t;
int deviceIoAdd(const device_io_t *device);
#endif
