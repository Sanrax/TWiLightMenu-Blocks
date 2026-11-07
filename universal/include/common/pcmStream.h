#pragma once

#include <cstdint>
#include <cstdio>

namespace twlm {

// Both built-in WAV data and the existing decoded ADPCM caches contain two
// bytes per frame (16-bit mono or paired 8-bit stereo). Handles any number of
// loop boundaries in a read, including an intro shorter than one buffer.
class PcmStream {
    FILE* intro = nullptr;
    FILE* loop = nullptr;
    long offset = 0;
    bool playingIntro = false;

public:
    bool reset(FILE* loopFile, FILE* introFile, long dataOffset) {
        intro = introFile;
        loop = loopFile;
        offset = dataOffset;
        playingIntro = intro != nullptr;
        if (!loop || offset < 0 || std::fseek(loop, offset, SEEK_SET)) return false;
        if (intro && std::fseek(intro, offset, SEEK_SET)) return false;
        return true;
    }

    uint32_t read(int16_t* dst, uint32_t count) {
        uint32_t done = 0;
        bool justRewound = false;
        while (done < count && loop) {
            FILE* file = playingIntro ? intro : loop;
            const uint32_t got = std::fread(dst + done, sizeof(*dst), count - done, file);
            done += got;
            if (done == count) break;
            if (std::ferror(file)) {
                std::clearerr(file); // Allow a later refill to retry a transient read failure.
                break;
            }
            if (playingIntro) {
                playingIntro = false;
                justRewound = true;
            } else {
                if (!got && justRewound) break; // Empty/truncated loop: avoid an endless refill.
                justRewound = true;
            }
            if (std::fseek(loop, offset, SEEK_SET)) break;
        }
        return done;
    }
};

} // namespace twlm
