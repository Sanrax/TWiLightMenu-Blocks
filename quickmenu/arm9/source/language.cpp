#include <nds.h>
#include <stdio.h>
#include <fat.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <string>

#include "common/twlmenusettings.h"
#include "common/inifile.h"
#include "common/languageEscape.h"
#include <utility>
// #include "common/logging.h"

#define STRING(what,def) std::string STR_##what;
#include "language.inl"
#undef STRING

/**
 * Get strings from the ini with special processing
 */
std::string getString(CIniFile &ini, const std::string &item, const std::string &defaultValue) {
	std::string out = ini.GetString("LANGUAGE", item, defaultValue);
	// logPrint("%s: %s\n", item.c_str(), out.c_str());
	if (out == "") {
		// Fallback to default string if translated string is blank
		out = defaultValue;
	}

	return decodeLanguageEscapes(std::move(out));
}

/**
 * Initialize translations.
 * Uses the language ID specified in settings.ui.language.
 */
void langInit(void)
{
	char languageIniPath[64];
	snprintf(languageIniPath, sizeof(languageIniPath), "nitro:/languages/%s/language.ini", ms().getGuiLanguageString().c_str());

	CIniFile languageini(languageIniPath);

#define STRING(what,def) STR_##what = getString(languageini, ""#what, def);
#include "language.inl"
#undef STRING
}
