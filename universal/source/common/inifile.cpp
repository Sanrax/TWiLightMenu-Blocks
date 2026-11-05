/*
	inifile.cpp
	Copyright (C) 2007 Acekard, www.acekard.com
	Copyright (C) 2007-2009 somebody
	Copyright (C) 2009 yellow wood goblin

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "common/inifile.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <utility>

bool gbar2Fix = false;

static std::string_view trim(std::string_view text)
{
    const size_t first = text.find_first_not_of(" \t");
    if (first == text.npos) return {};
    return text.substr(first, text.find_last_not_of(" \t") - first + 1);
}

static std::string_view itemName(std::string_view line)
{
    const std::string_view key = line.substr(0, line.find('='));
    const size_t last = key.find_last_not_of(" \t");
    return last == key.npos ? std::string_view() : key.substr(0, last + 1);
}

CIniFile::CIniFile() : m_bLastResult(false), m_bModified(false), m_bReadOnly(false) {}

CIniFile::CIniFile(const std::string &filename) : CIniFile()
{
    LoadIniFile(filename);
}

CIniFile::~CIniFile() = default;

void CIniFile::SetString(const std::string &Section, const std::string &Item, const std::string &Value)
{
    const std::string previous = GetFileString(Section, Item);
    if (!m_bReadOnly && (!m_bLastResult || previous != Value)) {
        SetFileString(Section, Item, Value);
        m_bModified = true;
    }
}

void CIniFile::SetInt(const std::string &Section, const std::string &Item, int Value)
{
    SetString(Section, Item, std::to_string(Value));
}

std::string CIniFile::GetString(const std::string &Section, const std::string &Item)
{
    return GetFileString(Section, Item);
}

std::string CIniFile::GetString(const std::string &Section, const std::string &Item, const std::string &DefaultValue)
{
    std::string value = GetFileString(Section, Item);
    if (!m_bLastResult) {
        SetString(Section, Item, DefaultValue);
        return DefaultValue;
    }
    return value;
}

void CIniFile::GetStringVector(const std::string &Section, const std::string &Item, std::vector<std::string> &strings, char delimiter)
{
    const std::string value = GetFileString(Section, Item);
    strings.clear();
    size_t start = 0;
    while (start < value.size()) {
        const size_t end = value.find(delimiter, start);
        const size_t length = (end == value.npos ? value.size() : end) - start;
        if (length) strings.emplace_back(value, start, length);
        if (end == value.npos) break;
        start = end + 1;
    }
}

void CIniFile::SetStringVector(const std::string &Section, const std::string &Item, std::vector<std::string> &strings, char delimiter)
{
    std::string value;
    for (size_t i = 0; i < strings.size(); i++) {
        if (i) value += delimiter;
        value += strings[i];
    }
    SetString(Section, Item, value);
}

int CIniFile::GetInt(const std::string &Section, const std::string &Item)
{
    const std::string value = GetFileString(Section, Item);
    const int base = value.size() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X') ? 16 : 10;
    return strtol(value.c_str(), nullptr, base);
}

int CIniFile::GetInt(const std::string &Section, const std::string &Item, int DefaultValue)
{
    const int value = GetInt(Section, Item);
    if (!m_bLastResult) {
        SetInt(Section, Item, DefaultValue);
        return DefaultValue;
    }
    return value;
}

bool CIniFile::LoadIniFile(const std::string &FileName)
{
    if (!FileName.empty()) m_sFileName = FileName;
    FILE *file = fopen(m_sFileName.c_str(), "rb");
    if (!file) return false;

    StringArray lines;
    std::string line;
    auto finishLine = [&]() {
        const std::string_view text = trim(line);
        if (!text.empty() && text[0] != ';' && text[0] != '/' && text[0] != '!')
            lines.emplace_back(text);
        line.clear();
    };

    char buffer[4096];
    bool firstBlock = true;
    size_t count;
    while ((count = fread(buffer, 1, sizeof(buffer), file)) != 0) {
        const char *current = buffer;
        const char *end = buffer + count;
        if (firstBlock && count >= 3 && static_cast<unsigned char>(buffer[0]) == 0xEF
            && static_cast<unsigned char>(buffer[1]) == 0xBB && static_cast<unsigned char>(buffer[2]) == 0xBF)
            current += 3;
        firstBlock = false;
        while (current != end) {
            const char *newline = std::find_if(current, end, [](char c) { return c == '\r' || c == '\n'; });
            line.append(current, newline);
            if (newline == end) break;
            finishLine();
            current = newline + 1;
        }
    }
    const bool ok = !ferror(file);
    const bool closed = fclose(file) == 0;
    if (!ok || !closed) return false;
    if (!line.empty()) finishLine();
    m_FileContainer.swap(lines);
    m_Index.clear();
    m_IndexValid = false;
    m_bLastResult = false;
    m_bModified = false;
    return true;
}

bool CIniFile::SaveIniFileModified(const std::string &FileName)
{
    return !m_bModified || SaveIniFile(FileName);
}

bool CIniFile::SaveIniFile(const std::string &FileName)
{
    if (!FileName.empty()) m_sFileName = FileName;
    FILE *file = fopen(m_sFileName.c_str(), "wb");
    if (!file) return false;

    const std::string_view newline = gbar2Fix ? "\n" : "\r\n";
    bool ok = true;
    auto write = [&](std::string_view text) {
        if (ok && fwrite(text.data(), 1, text.size(), file) != text.size()) ok = false;
    };
    for (size_t i = 0; i < m_FileContainer.size() && ok; i++) {
        const std::string &line = m_FileContainer[i];
        const size_t start = line.find_first_not_of(' ');
        if (start == line.npos) continue;
        const std::string_view text(line.data() + start, line.size() - start);
        if (text[0] == '[' && i > 0 && !m_FileContainer[i - 1].empty()) write(newline);
        write(text);
        write(newline);
    }
    if (fclose(file) != 0) ok = false;
    if (ok) m_bModified = false;
    return ok;
}

void CIniFile::BuildIndex()
{
    if (m_IndexValid) return;
    m_Index.clear();
    for (size_t i = 0; i < m_FileContainer.size(); i++) {
        const std::string &line = m_FileContainer[i];
        if (line.empty() || line[0] != '[') continue;
        const size_t bracket = line.find(']');
        if (bracket == line.npos || bracket == 0) continue;
        const std::string_view name(line.data() + 1, bracket - 1);
        // As before, the first occurrence of a section and of each key wins.
        if (std::find_if(m_Index.begin(), m_Index.end(), [&](const SectionIndex &s) { return s.name == name; }) != m_Index.end())
            continue;
        SectionIndex section{std::string(name), i + 1, m_FileContainer.size(), {}};
        for (size_t j = i + 1; j < m_FileContainer.size(); j++) {
            const std::string &item = m_FileContainer[j];
            if (item.find('=') != item.npos) section.items.push_back(j);
            else if (!item.empty() && item[0] == '[') {
                section.end = j;
                break;
            }
        }
        std::stable_sort(section.items.begin(), section.items.end(), [&](size_t a, size_t b) {
            return itemName(m_FileContainer[a]) < itemName(m_FileContainer[b]);
        });
        m_Index.push_back(std::move(section));
    }
    m_IndexValid = true;
}

std::string CIniFile::GetFileString(const std::string &Section, const std::string &Item)
{
    BuildIndex();
    m_bLastResult = false;
    for (const SectionIndex &section : m_Index) {
        if (section.name != Section) continue;
        const auto found = std::lower_bound(section.items.begin(), section.items.end(), Item,
            [&](size_t line, const std::string &key) { return itemName(m_FileContainer[line]) < key; });
        if (found != section.items.end() && itemName(m_FileContainer[*found]) == Item) {
            const std::string &line = m_FileContainer[*found];
            const size_t first = line.find_first_not_of(" \t", line.find('=') + 1);
            m_bLastResult = true;
            return first == line.npos ? std::string() : line.substr(first);
        }
        break;
    }
    return {};
}

void CIniFile::SetFileString(const std::string &Section, const std::string &Item, const std::string &Value)
{
    if (m_bReadOnly) return;
    BuildIndex();
    const std::string replacement = Item + (gbar2Fix ? "=" : " = ") + Value;
    for (const SectionIndex &section : m_Index) {
        if (section.name != Section) continue;
        const auto found = std::lower_bound(section.items.begin(), section.items.end(), Item,
            [&](size_t line, const std::string &key) { return itemName(m_FileContainer[line]) < key; });
        if (found != section.items.end() && itemName(m_FileContainer[*found]) == Item) {
            ReplaceLine(*found, replacement);
        } else {
            InsertLine(section.end, replacement);
        }
        return;
    }
    const size_t end = m_FileContainer.size();
    InsertLine(end, "[" + Section + "]");
    InsertLine(end + 1, replacement);
}

bool CIniFile::InsertLine(size_t line, const std::string &str)
{
    m_FileContainer.insert(m_FileContainer.begin() + line, str);
    if (!str.empty() && str[0] == '[') m_IndexValid = false;
    if (m_IndexValid) {
        const bool hasKey = str.find('=') != str.npos;
        for (SectionIndex &section : m_Index) {
            const bool contains = section.begin <= line && line <= section.end;
            for (size_t &item : section.items) if (item >= line) item++;
            if (contains && hasKey) {
                const auto position = std::lower_bound(section.items.begin(), section.items.end(), line,
                    [&](size_t a, size_t b) {
                        const auto ka = itemName(m_FileContainer[a]), kb = itemName(m_FileContainer[b]);
                        return ka < kb || (ka == kb && a < b);
                    });
                section.items.insert(position, line);
            }
            if (section.begin > line) section.begin++;
            if (section.end >= line) section.end++;
        }
    }
    return true;
}

bool CIniFile::ReplaceLine(size_t line, const std::string &str)
{
    const std::string &previous = m_FileContainer[line];
    if ((!previous.empty() && previous[0] == '[') || (!str.empty() && str[0] == '[')
        || itemName(previous) != itemName(str)) m_IndexValid = false;
    m_FileContainer[line] = str;
    return true;
}
