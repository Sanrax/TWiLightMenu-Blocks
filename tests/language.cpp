#include "common/languageEscape.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <vector>

// Frozen pre-refactor decoder from the DSi menu. Empty input is guarded
// because the old length()-1 loop underflowed for a blank default string.
static std::string legacyDecode(std::string out) {
    if (out.empty()) return out;
	// Convert "\n" to actual newlines
	for (size_t i = 0; i < out.length() - 1; i++) {
		if (out[i] == '\\') {
			switch(out[i + 1]) {
				case 'n':
				case 'N':
					out = out.substr(0, i) + '\n' + out.substr(i + 2);
					break;
				case 'a':
				case 'A':
					out = out.substr(0, i) + "" + out.substr(i + 2); // U+E000
					break;
				case 'b':
				case 'B':
					out = out.substr(0, i) + "" + out.substr(i + 2); // U+E001
					break;
				case 'x':
				case 'X':
					out = out.substr(0, i) + "" + out.substr(i + 2); // U+E002
					break;
				case 'y':
				case 'Y':
					out = out.substr(0, i) + "" + out.substr(i + 2); // U+E003
					break;
				case 'l':
				case 'L':
					out = out.substr(0, i) + "" + out.substr(i + 2); // U+E004
					break;
				case 'r':
				case 'R':
					out = out.substr(0, i) + "" + out.substr(i + 2); // U+E005
					break;
				case 'd':
				case 'D':
					switch(out[i + 2]) {
						default:
							out = out.substr(0, i) + "" + out.substr(i + 2); // U+E006
							break;
						case 'u':
						case 'U':
							out = out.substr(0, i) + "" + out.substr(i + 3); // U+E079
							break;
						case 'd':
						case 'D':
							out = out.substr(0, i) + "" + out.substr(i + 3); // U+E07A
							break;
						case 'l':
						case 'L':
							out = out.substr(0, i) + "" + out.substr(i + 3); // U+E07B
							break;
						case 'r':
						case 'R':
							out = out.substr(0, i) + "" + out.substr(i + 3); // U+E07C
							break;
						case 'v':
						case 'V':
							out = out.substr(0, i) + "" + out.substr(i + 3); // U+E07D
							break;
						case 'h':
						case 'H':
							out = out.substr(0, i) + "" + out.substr(i + 3); // U+E07E
							break;
					}
				default:
					break;
			}
		} else if (out[i] == '&') {
			if (out.substr(i + 1, 3) == "lrm") {
				out = out.substr(0, i) + "\u200E" + out.substr(i + 4); // Left-to-Right mark
			} else if (out.substr(i + 1, 3) == "rlm") {
				out = out.substr(0, i) + "\u200F" + out.substr(i + 4); // Right-to-Left mark
			}
		}
	}

    return out;
}

static size_t comparisons;
static void compare(const std::string &value) {
    assert(decodeLanguageEscapes(value) == legacyDecode(value));
    comparisons++;
}

static const char *defaults[] = {
#define STRING(key, value) value,
#include "../romsel_dsimenutheme/arm9/source/language.inl"
#include "../quickmenu/arm9/source/language.inl"
#include "../settings/arm9/source/language.inl"
#include "../title/arm9/source/language.inl"
#include "../3dssplash/arm9/source/language.inl"
#include "../manual/arm9/source/language.inl"
#include "../imageview/arm9/source/language.inl"
#undef STRING
};

int main(int argc, char **argv) {
    assert(decodeLanguageEscapes("").empty());
    assert(decodeLanguageEscapes("\\")=="\\");
    assert(decodeLanguageEscapes("\\n\\N")=="\n\n");
    assert(decodeLanguageEscapes("&lrm&rlm;")=="\u200e\u200f;");
    assert(decodeLanguageEscapes("&LRM &lr \\z")=="&LRM &lr \\z");
    const std::vector<std::pair<std::string, std::string>> symbols = {
        {"a", "\ue000"}, {"b", "\ue001"}, {"x", "\ue002"}, {"y", "\ue003"},
        {"l", "\ue004"}, {"r", "\ue005"}, {"d", "\ue006"},
        {"du", "\ue079"}, {"dd", "\ue07a"}, {"dl", "\ue07b"},
        {"dr", "\ue07c"}, {"dv", "\ue07d"}, {"dh", "\ue07e"}
    };
    for (auto [token, expected] : symbols) {
        assert(decodeLanguageEscapes("\\" + token) == expected);
        for (char &c : token) c -= 'a' - 'A';
        assert(decodeLanguageEscapes("\\" + token) == expected);
    }
    for (auto value : defaults) compare(value);
    size_t files = 0;
    for (int i = 1; i < argc; i++) {
        for (const auto &entry : std::filesystem::recursive_directory_iterator(argv[i])) {
            if (entry.path().filename() != "language.ini") continue;
            std::ifstream file(entry.path()); assert(file.good()); files++;
            for (std::string line; std::getline(file, line);) {
                const size_t equal = line.find('=');
                if (equal != line.npos) compare(line.substr(equal + 1));
            }
        }
    }
    assert(files > 0);
    std::mt19937 random(0x54574c4d);
    const char alphabet[] = "aAbBxXyYlLrRdDuUvVhHnN\\&;lrm \n\t\xE2\x80\x8E\xEF\x80\x80";
    for (unsigned sample = 0; sample < 20000; sample++) {
        std::string input;
        for (unsigned len = random() % 256; len; len--) input.push_back(alphabet[random() % (sizeof(alphabet) - 1)]);
        compare(input);
    }
    std::string repeated;
    for (int i = 0; i < 1000; i++) repeated += "prefix\\A\\n&rlm\\du\\N";
    compare(repeated);
    const char embedded[] = "\0\\A\0&lrm\\D";
    compare(std::string(embedded, sizeof(embedded) - 1));
    std::cout << "Translation escapes: " << comparisons << " comparisons across " << files
              << " INIs, defaults, button/direction marks and malformed/UTF-8 inputs passed\n";
}
