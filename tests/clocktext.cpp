#include "common/clockText.h"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <vector>

static unsigned calendarCalls, formatCalls;
extern "C" std::tm *__real_localtime(const std::time_t *);
extern "C" std::tm *__wrap_localtime(const std::time_t *value) {
    ++calendarCalls;
    return __real_localtime(value);
}
extern "C" size_t __real_strftime(char *, size_t, const char *, const std::tm *);
extern "C" size_t __wrap_strftime(char *out, size_t size, const char *pattern, const std::tm *value) {
    ++formatCalls;
    return __real_strftime(out, size, pattern, value);
}

static std::string reference(std::time_t now, const std::string &pattern, bool clock, bool colon) {
    char buffer[24] = {};
    const auto *parts = std::localtime(&now); assert(parts);
    const size_t size = std::strftime(buffer, sizeof(buffer), pattern.c_str(), parts);
    std::string out(buffer, size);
    if (clock) {
        if (!out.empty() && out[0] == ' ') out[0] = '0';
        if (out.size() > 2) out[2] = colon ? ':' : ' ';
    }
    return out;
}

int main() {
    setenv("TZ", "UTC0", 1); tzset();
    constexpr std::time_t midnight = 1704067200; // 2024-01-01 UTC
    const std::string dateFormat = "%m/%d", timeFormat = "%H:%M";
    twlm::ClockTextCache cache;
    calendarCalls = formatCalls = 0;
    for (int frame = 0; frame < 60; ++frame) {
        assert(cache.date(midnight, dateFormat) == "01/01");
        assert(cache.time(midnight, timeFormat, frame & 1) == (frame & 1 ? "00:00" : "00 00"));
    }
    assert(calendarCalls == 1 && formatCalls == 2);
    std::cout << "60 frames: one calendar conversion and two formatting calls\n";

    // Forward/backward clock changes, midnight, leap day, seconds in custom
    // formats, 12/24-hour changes and language-format changes in one second.
    const std::vector<std::time_t> times = {midnight - 1, midnight, midnight + 1,
        midnight + 59, midnight + 60, 1709251199, 1709251200, midnight, 0, -1};
    const std::vector<std::string> dates = {"%m/%d", "%d/%m", "%Y/%m/%d", "%d %B", "%S", ""};
    const std::vector<std::string> clocks = {"%H:%M", "%I:%M", "%I.%M", "%k:%M", "%H:%M:%S", "%p %I:%M", "%H", "", std::string(40, 'x')};
    for (const auto now : times) {
        for (const auto &pattern : dates) assert(cache.date(now, pattern) == reference(now, pattern, false, true));
        for (const auto &pattern : clocks)
            for (bool colon : {false, true})
                assert(cache.time(now, pattern, colon) == reference(now, pattern, true, colon));
    }
    setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1); tzset();
    for (std::time_t now : {1710053999, 1710054000, 1730613599, 1730613600}) {
        assert(cache.date(now, dateFormat) == reference(now, dateFormat, false, true));
        assert(cache.time(now, timeFormat, true) == reference(now, timeFormat, true, true));
    }
    std::cout << "Clock text, blinking, format changes, midnight/leap day, RTC jumps and DST passed\n";
}
