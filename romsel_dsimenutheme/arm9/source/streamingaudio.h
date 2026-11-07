#pragma once
#include <maxmod9.h>
#include <mm_types.h>
#include <nds.h>
#include "common/audioRing.h"

// Preserve the former two 96,000-frame buffers' total capacity in one queue.
#define STREAMING_BUF_LENGTH 192000
#define STREAMING_REFILL_FRAMES 4096

#ifdef __cplusplus
extern "C" {
#endif
extern TwlmAudioRing streaming_ring;
extern volatile u32 sample_delay_count;

bool alloc_streaming_buf(void);
void free_streaming_buf(void);
void resetStreamSettings(void);
mm_word on_stream_request(mm_word length, mm_addr dest, mm_stream_formats format);
#ifdef __cplusplus
}
#endif
