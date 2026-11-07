#include "common/languagePack.h"
#include "common/twlmenusettings.h"
#include "language.h"
#include "common/logging.h"

#define STRING(key, fallback) std::string STR_##key;
#include "language.inl"
#undef STRING

static constexpr twlm::language::Entry languageEntries[] = {
#define STRING(key, fallback) {#key, fallback, &STR_##key},
#include "language.inl"
#undef STRING
};
static constexpr size_t languageEntryCount = sizeof(languageEntries) / sizeof(languageEntries[0]);
static constexpr uint32_t languageSchema = twlm::language::schemaHash(languageEntries, languageEntryCount);

void langInit(void)
{
	logPrint("langInit() ");
	twlm::language::loadTranslations(ms().getGuiLanguageString().c_str(), languageEntries, languageEntryCount, languageSchema);
	logPrint("Language inited\n");
}
