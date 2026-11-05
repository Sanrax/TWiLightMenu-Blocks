#include "common/inifile.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

static void writeFile(const std::string &path, const std::string &text) {
    std::ofstream out(path, std::ios::binary);
    out << text;
    assert(out.good());
}

static std::string readFile(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}

int main(int argc, char **argv) {
    assert(argc == 2);
    const std::string path = argv[1];
    writeFile(path, "\xEF\xBB\xBF; comment\r\n / comment\n! comment\r"
        "[SRLOADER]\r\n KEY\t =  first  \r\nKEY = second\nEMPTY = \nHEX = 0x2A\n"
        "NEG = -17\nARGS = :one::two:three:\n[OTHER]\nKEY = other\n"
        "[SRLOADER]\nLATER = ignored\n");
    CIniFile ini(path);
    assert(ini.GetString("SRLOADER", "KEY", "missing") == "first");
    assert(ini.GetString("SRLOADER", "EMPTY", "fallback").empty());
    assert(ini.GetInt("SRLOADER", "HEX", 0) == 42);
    assert(ini.GetInt("SRLOADER", "NEG", 0) == -17);
    assert(ini.GetString("OTHER", "KEY", "") == "other");
    assert(ini.GetString("SRLOADER", "LATER", "default") == "default");
    assert(ini.GetString("SRLOADER", "key", "case-sensitive") == "case-sensitive");
    std::vector<std::string> args;
    ini.GetStringVector("SRLOADER", "ARGS", args, ':');
    assert((args == std::vector<std::string>{"one", "two", "three"}));
    ini.SetString("SRLOADER", "KEY", "changed");
    ini.SetString("SRLOADER", "NEW_EMPTY", "");
    ini.SetInt("NEW SECTION", "NUMBER", 123);
    ini.SetStringVector("RECENT", "sd:/folder", args, ':');
    ini.SetInt("sd:/folder", "game.nds", 2);
    assert(ini.GetString("SRLOADER", "KEY", "") == "changed");
    assert(ini.GetInt("NEW SECTION", "NUMBER", 0) == 123);
    assert(ini.SaveIniFileModified(path));
    const std::string saved = readFile(path);
    assert(saved.find("; comment") == saved.npos);
    assert(saved.find("\r\n\r\n[OTHER]") != saved.npos);
    assert(saved.find("NEW_EMPTY = \r\n") != saved.npos);
    CIniFile reload(path);
    assert(reload.GetString("SRLOADER", "KEY", "") == "changed");
    assert(reload.GetString("SRLOADER", "LATER", "wrong") == "default");
    assert(reload.GetString("SRLOADER", "NEW_EMPTY", "wrong").empty());
    assert(reload.GetString("RECENT", "sd:/folder", "") == "one:two:three");
    assert(reload.GetInt("sd:/folder", "game.nds", 0) == 2);
    // Reload discards every old index entry; an unsuccessful load keeps data.
    writeFile(path, "[NEW]\rLAST = no newline");
    assert(reload.LoadIniFile(path));
    assert(reload.GetString("NEW", "LAST", "") == "no newline");
    assert(!reload.LoadIniFile(path + "/absent"));
    assert(reload.GetString("NEW", "LAST", "") == "no newline");
    // Long values cross read blocks. Whitespace-only/short files are valid.
    const std::string longValue(10000, 'x');
    writeFile(path, "[LONG]\nVALUE = " + longValue + "\n[LAST]\nN = 7");
    assert(reload.LoadIniFile(path));
    assert(reload.GetString("LONG", "VALUE", "") == longValue);
    assert(reload.GetInt("LAST", "N", 0) == 7);
    writeFile(path, " \t\r\n");
    assert(reload.LoadIniFile(path));
    assert(reload.GetString("LONG", "VALUE", "gone") == "gone");
    writeFile(path, "x");
    assert(reload.LoadIniFile(path));
    // GBARunner settings require compact assignments and LF line endings.
    gbar2Fix = true;
    reload.SetInt("emulation", "enable", 1);
    assert(reload.SaveIniFile(path));
    const std::string compact = readFile(path);
    assert(compact.find("enable=1\n") != compact.npos);
    assert(compact.find('\r') == compact.npos);
    gbar2Fix = false;
    // Failed writes must not be reported as successful or clear dirty state.
    reload.SetInt("emulation", "enable", 0);
    assert(!reload.SaveIniFile("/dev/full"));
    assert(reload.SaveIniFileModified(path));
    assert(readFile(path).find("enable = 0") != std::string::npos);
    puts("INI settings, duplicate keys, history, reloads, long lines and write failures passed");
}
