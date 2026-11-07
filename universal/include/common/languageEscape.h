#ifndef TWLM_LANGUAGE_ESCAPE_H
#define TWLM_LANGUAGE_ESCAPE_H

#include <string>

// Translation INIs use button names and direction marks alongside ordinary
// UTF-8. Decode without repeatedly copying the prefix/suffix of the string.
inline std::string decodeLanguageEscapes(std::string input) {
    const size_t first = input.find_first_of("\\&");
    if (first == input.npos) return input;

    std::string output;
    // The largest expansion is a two-byte button name becoming three UTF-8
    // bytes. Strings without escape markers keep their original allocation.
    output.reserve(input.size() + input.size() / 2);
    output.append(input.data(), first);
    auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? char(c + ('a' - 'A')) : c; };
    for (size_t i = first; i < input.size();) {
        unsigned codepoint = 0;
        size_t consumed = 0;
        if (input[i] == '\\' && i + 1 < input.size()) {
            consumed = 2;
            switch (lower(input[i + 1])) {
            case 'n': codepoint = '\n'; break;
            case 'a': codepoint = 0xE000; break;
            case 'b': codepoint = 0xE001; break;
            case 'x': codepoint = 0xE002; break;
            case 'y': codepoint = 0xE003; break;
            case 'l': codepoint = 0xE004; break;
            case 'r': codepoint = 0xE005; break;
            case 'd':
                codepoint = 0xE006;
                if (i + 2 < input.size()) {
                    switch (lower(input[i + 2])) {
                    case 'u': codepoint = 0xE079; break;
                    case 'd': codepoint = 0xE07A; break;
                    case 'l': codepoint = 0xE07B; break;
                    case 'r': codepoint = 0xE07C; break;
                    case 'v': codepoint = 0xE07D; break;
                    case 'h': codepoint = 0xE07E; break;
                    }
                    if (codepoint != 0xE006) consumed = 3;
                }
                break;
            default: consumed = 0; break;
            }
        } else if (input[i] == '&') {
            if (input.compare(i, 4, "&lrm") == 0) codepoint = 0x200E;
            else if (input.compare(i, 4, "&rlm") == 0) codepoint = 0x200F;
            if (codepoint) consumed = 4;
        }

        if (!consumed) {
            output.push_back(input[i++]);
            continue;
        }
        if (codepoint == '\n') output.push_back('\n');
        else {
            output.push_back(char(0xE0 | (codepoint >> 12)));
            output.push_back(char(0x80 | ((codepoint >> 6) & 0x3F)));
            output.push_back(char(0x80 | (codepoint & 0x3F)));
        }
        i += consumed;
    }
    return output;
}

#endif
