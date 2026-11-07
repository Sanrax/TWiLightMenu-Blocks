#include "streamingaudio.h"
#include <stdlib.h>

TwlmAudioRing streaming_ring = {0};
volatile u32 sample_delay_count = 0;

bool alloc_streaming_buf(void) {
    s16* data = malloc(STREAMING_BUF_LENGTH * sizeof(s16));
    s16* second = NULL;
    // Retain the old allocation sizes if a fragmented heap cannot provide one
    // larger block. The same queue can address two separately owned halves.
    if (!data) {
        data = malloc(STREAMING_BUF_LENGTH / 2 * sizeof(s16));
        second = malloc(STREAMING_BUF_LENGTH / 2 * sizeof(s16));
        if (!data || !second) { free(data); free(second); data = second = NULL; }
    }
    twlmAudioRingReset(&streaming_ring, data, data ? STREAMING_BUF_LENGTH : 0);
    streaming_ring.secondHalf = second;
    return data != NULL;
}

void free_streaming_buf(void) {
    free(streaming_ring.samples);
    free(streaming_ring.secondHalf);
    twlmAudioRingReset(&streaming_ring, NULL, 0);
}

void resetStreamSettings(void) {
    s16* second = streaming_ring.secondHalf;
    twlmAudioRingReset(&streaming_ring, streaming_ring.samples, streaming_ring.capacity);
    streaming_ring.secondHalf = second;
    sample_delay_count = 0;
}

// Maxmod calls this in an interrupt. Copy published PCM only; all filesystem
// reads and queue refills run on the main thread in SoundControl::updateStream.
mm_word on_stream_request(mm_word length, mm_addr dest, mm_stream_formats format) {
    (void)format;
    s16* target = dest;
    u32 delayed = sample_delay_count < length ? sample_delay_count : length;
    if (delayed) {
        memset(target, 0, delayed * sizeof(*target));
        sample_delay_count -= delayed;
        target += delayed;
    }
    twlmAudioRingRead(&streaming_ring, target, length - delayed);
    return length;
}
