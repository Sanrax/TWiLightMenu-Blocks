#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <nds/ndstypes.h>
typedef uint32_t sec_t;
typedef volatile uint32_t vu32;
#define ALIGN(n) __attribute__((aligned(n)))
void swiDelay(uint32_t);
void DC_FlushRange(const void *, unsigned);
void *memUncached(void *);
