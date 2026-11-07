#include "common/pcmStream.h"
#include "streamingaudio.h"
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <deque>
#include <iostream>
#include <random>
#include <vector>

static bool splitAllocation = false;
extern "C" void* __real_malloc(size_t);
extern "C" void* __wrap_malloc(size_t bytes) {
    if (splitAllocation && bytes == 192000 * sizeof(int16_t)) return nullptr;
    return __real_malloc(bytes);
}

static FILE* pcm(const std::vector<int16_t>& data, unsigned skip) {
    FILE* file = std::tmpfile(); assert(file);
    std::vector<unsigned char> header(skip, 0xE7);
    assert(std::fwrite(header.data(), 1, skip, file) == skip);
    assert(std::fwrite(data.data(), sizeof(int16_t), data.size(), file) == data.size());
    return file;
}

static void loops(unsigned introLength, unsigned loopLength, unsigned skip) {
    std::vector<int16_t> intro(introLength), loop(loopLength), out(400001);
    for (unsigned i = 0; i < intro.size(); ++i) intro[i] = i * 123 + 1;
    for (unsigned i = 0; i < loop.size(); ++i) loop[i] = i * 1111 + 0x7E02;
    FILE* first = pcm(intro, skip); FILE* repeat = pcm(loop, skip);
    twlm::PcmStream stream;
    assert(stream.reset(repeat, first, skip));
    const auto got = stream.read(out.data(), out.size());
    assert(got == (loop.empty() ? intro.size() : out.size()));
    for (unsigned i = 0; i < got; ++i)
        assert(out[i] == (i < intro.size() ? intro[i] : loop[(i - intro.size()) % loop.size()]));
    assert(stream.reset(repeat, nullptr, skip));
    assert(stream.read(out.data(), 391) == (loop.empty() ? 0 : 391));
    for (unsigned i = 0; !loop.empty() && i < 391; ++i) assert(out[i] == loop[i % loop.size()]);
    assert(!stream.reset(nullptr, nullptr, skip));
    std::fclose(first); std::fclose(repeat);
}

static void callbacks(mm_stream_formats format) {
    assert(alloc_streaming_buf());
    resetStreamSettings();
    assert(streaming_ring.capacity == 192000);
    std::mt19937 random(42);
    std::deque<int16_t> expected;
    std::vector<int16_t> output(8192);
    unsigned next = 0;
    uint64_t consumed = 0;
    auto read = [&](unsigned count) {
        const unsigned delay = std::min<unsigned>(unsigned(sample_delay_count), count);
        assert(on_stream_request(count, output.data(), format) == count);
        for (unsigned i = 0; i < count; ++i) {
            if (i < delay || expected.empty()) assert(output[i] == 0);
            else { assert(output[i] == expected.front()); expected.pop_front(); ++consumed; }
        }
        assert(expected.size() == streaming_ring.queued);
        assert(streaming_ring.queued <= streaming_ring.capacity);
    };
    read(8192); // Empty/uninitialized storage becomes silence.
    for (unsigned iteration = 0; iteration < 30000; ++iteration) {
        unsigned count = std::min<unsigned>(twlmAudioRingWriteSpace(&streaming_ring), random() % 8192);
        std::vector<int16_t> pending(count);
        for (unsigned i = 0; i < count; ++i) {
            pending[i] = (next++ * 1237) ^ 0x417E;
            twlmAudioRingWriteData(&streaming_ring)[i] = pending[i];
        }
        // Simulate IRQs during a slow file read, before publication.
        if (random() & 1) read(random() % 8192);
        twlmAudioRingCommit(&streaming_ring, count);
        expected.insert(expected.end(), pending.begin(), pending.end());
        if (iteration % 199 == 0) sample_delay_count = random() % 20000;
        read(random() % 8192);
    }
    assert(consumed > 10000000); // Many wraps, mixed silence, publication and delays.
    sample_delay_count = 0;
    while (!expected.empty()) read(std::min<size_t>(expected.size(), output.size()));
    read(8192);
    free_streaming_buf();
    free_streaming_buf();
    read(8192);
}

static void playback() {
    // Refill the actual callback queue from an intro and loop, then simulate
    // a five-second main-thread I/O stall after the queue has warmed up.
    std::vector<int16_t> intro(137), loop(17003);
    for (unsigned i = 0; i < intro.size(); ++i) intro[i] = i * 3 + 1;
    for (unsigned i = 0; i < loop.size(); ++i) loop[i] = i * 117 + 5;
    FILE* first = pcm(intro, 44); FILE* repeat = pcm(loop, 44);
    twlm::PcmStream stream; assert(stream.reset(repeat, first, 44));
    assert(alloc_streaming_buf()); resetStreamSettings();
    auto fill = [&](unsigned limit) {
        const auto count = std::min<unsigned>(twlmAudioRingWriteSpace(&streaming_ring), limit);
        const auto got = stream.read(twlmAudioRingWriteData(&streaming_ring), count);
        assert(got == count); twlmAudioRingCommit(&streaming_ring, got);
    };
    fill(48000);
    std::vector<int16_t> output(1600);
    unsigned position = 0;
    for (unsigned frame = 0; frame < 600; ++frame) {
        on_stream_request(output.size(), output.data(), MM_STREAM_16BIT_MONO);
        for (auto value : output) {
            const auto i = position++;
            assert(value == (i < intro.size() ? intro[i] : loop[(i - intro.size()) % loop.size()]));
        }
        if (frame < 100 || frame >= 150) fill(4096);
    }
    assert(position == 960000);
    free_streaming_buf(); std::fclose(first); std::fclose(repeat);
}

static void readFailure() {
    struct Cookie { std::vector<unsigned char> bytes; off64_t position = 0; bool fail = true; } cookie;
    for (int16_t i = 0; i < 64; ++i) { cookie.bytes.push_back(i); cookie.bytes.push_back(0); }
    cookie_io_functions_t functions{};
    functions.read = [](void* ptr, char* dst, size_t bytes) -> ssize_t {
        auto& c = *static_cast<Cookie*>(ptr);
        if (c.fail) { c.fail = false; errno = EIO; return -1; }
        const auto n = std::min<size_t>(bytes, c.bytes.size() - c.position);
        std::memcpy(dst, c.bytes.data() + c.position, n); c.position += n;
        return n;
    };
    functions.seek = [](void* ptr, off64_t* offset, int origin) -> int {
        auto& c = *static_cast<Cookie*>(ptr);
        const auto next = *offset + (origin == SEEK_SET ? 0 : origin == SEEK_CUR ? c.position : off64_t(c.bytes.size()));
        if (next < 0 || uint64_t(next) > c.bytes.size()) return -1;
        c.position = *offset = next; return 0;
    };
    FILE* file = fopencookie(&cookie, "rb", functions); assert(file);
    twlm::PcmStream stream; assert(stream.reset(file, nullptr, 0));
    int16_t out[128];
    assert(stream.read(out, 128) == 0); // No publication after a failed read.
    assert(!std::ferror(file));
    assert(stream.read(out, 128) == 128);
    for (unsigned i = 0; i < 128; ++i) assert(out[i] == int16_t(i % 64));
    std::fclose(file);
}

int main() {
    for (unsigned skip : {0, 44})
        for (const auto lengths : {std::pair{0u, 0u}, {0u, 1u}, {1u, 7u},
                                   {137u, 17003u}, {200003u, 1u}, {1u, 0u}})
            loops(lengths.first, lengths.second, skip);
    callbacks(MM_STREAM_16BIT_MONO);
    callbacks(MM_STREAM_8BIT_STEREO);
    playback();
    splitAllocation = true;
    callbacks(MM_STREAM_16BIT_MONO);
    playback();
    splitAllocation = false;
    readFailure();
    std::cout << "PCM intro/loop transitions, stereo bits, queue wraps, IRQ publication, delays, underruns and I/O stalls passed\n";
}
