#ifndef TWLM_CLOCK_TEXT_H
#define TWLM_CLOCK_TEXT_H

#include <ctime>
#include <string>

namespace twlm {
// Main-thread clock formatting. Reuse the calendar and text while the RTC
// second is unchanged; retain arbitrary strftime formats, including seconds.
class ClockTextCache {
    struct Text {
        std::string format;
        std::string value;
        bool dirty = true;
    };
    std::time_t second{};
    std::tm calendar{};
    bool haveSecond = false;
    bool calendarValid = false;
    Text dateText, timeText;

    void update(std::time_t now) {
        if (haveSecond && now == second) return;
        second = now;
        haveSecond = true;
        const std::tm *parts = std::localtime(&now);
        calendarValid = parts != nullptr;
        if (parts) calendar = *parts;
        dateText.dirty = timeText.dirty = true;
    }

    void format(Text &text, const std::string &pattern) {
        if (!text.dirty && text.format == pattern) return;
        text.format = pattern;
        char buffer[24] = {};
        const size_t size = calendarValid
            ? std::strftime(buffer, sizeof(buffer), pattern.c_str(), &calendar) : 0;
        text.value.assign(buffer, size);
        text.dirty = false;
    }

public:
    const std::string &date(std::time_t now, const std::string &pattern) {
        update(now);
        format(dateText, pattern);
        return dateText.value;
    }

    const std::string &time(std::time_t now, const std::string &pattern, bool colonVisible) {
        update(now);
        format(timeText, pattern);
        // Preserve the menu's leading-zero and blinking-colon presentation.
        // Empty/short custom formats must not index beyond their text.
        if (!timeText.value.empty() && timeText.value[0] == ' ') timeText.value[0] = '0';
        if (timeText.value.size() > 2) timeText.value[2] = colonVisible ? ':' : ' ';
        return timeText.value;
    }
};
} // namespace twlm

#endif
