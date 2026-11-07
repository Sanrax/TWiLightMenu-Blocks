#pragma once

#include <stdint.h>
#include <string.h>

// One main-thread producer and one interrupt consumer. File reads can take
// place with interrupts enabled: their destination is unpublished free space.
// The producer must mask interrupts only while committing the completed write.
typedef struct {
    int16_t* samples;
    int16_t* secondHalf;
    uint32_t capacity;
    uint32_t readPosition, writePosition;
    volatile uint32_t queued;
} TwlmAudioRing;

static inline void twlmAudioRingReset(TwlmAudioRing* ring, int16_t* samples, uint32_t capacity) {
    ring->samples = samples;
    ring->secondHalf = 0;
    ring->capacity = capacity;
    ring->readPosition = ring->writePosition = ring->queued = 0;
}

static inline uint32_t twlmAudioRingWriteSpace(const TwlmAudioRing* ring) {
    uint32_t free = ring->capacity - ring->queued;
    uint32_t contiguous = ring->capacity - ring->writePosition;
    if (ring->secondHalf && ring->writePosition < ring->capacity / 2)
        contiguous = ring->capacity / 2 - ring->writePosition;
    return free < contiguous ? free : contiguous;
}

static inline int16_t* twlmAudioRingWriteData(const TwlmAudioRing* ring) {
    if (ring->secondHalf && ring->writePosition >= ring->capacity / 2)
        return ring->secondHalf + (ring->writePosition - ring->capacity / 2);
    return ring->samples + ring->writePosition;
}

static inline void twlmAudioRingCommit(TwlmAudioRing* ring, uint32_t count) {
    ring->writePosition += count;
    if (ring->writePosition == ring->capacity) ring->writePosition = 0;
    ring->queued += count;
}

// Called in the consumer interrupt. Missing frames become silence; unpublished
// storage is never read. Return the number of real frames supplied.
static inline uint32_t twlmAudioRingRead(TwlmAudioRing* ring, int16_t* dst, uint32_t count) {
    uint32_t available = ring->queued;
    uint32_t copied = count < available ? count : available;
    uint32_t remaining = copied;
    while (remaining) {
        uint32_t end = ring->capacity;
        const int16_t* src;
        if (ring->secondHalf) {
            if (ring->readPosition < ring->capacity / 2) {
                end = ring->capacity / 2;
                src = ring->samples + ring->readPosition;
            }
            else src = ring->secondHalf + (ring->readPosition - ring->capacity / 2);
        } else src = ring->samples + ring->readPosition;
        uint32_t part = end - ring->readPosition;
        if (part > remaining) part = remaining;
        memcpy(dst + copied - remaining, src, part * sizeof(*dst));
        ring->readPosition += part;
        if (ring->readPosition == ring->capacity) ring->readPosition = 0;
        remaining -= part;
    }
    if (copied < count) memset(dst + copied, 0, (count - copied) * sizeof(*dst));
    ring->queued -= copied;
    return copied;
}
