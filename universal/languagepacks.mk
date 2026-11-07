# Build prepared translations from the existing INIs and C++ defaults. Keep
# the INIs in NitroFS as the compatibility fallback; translators edit only INI.
.DEFAULT_GOAL := all
LANGUAGE_PACK_TOOL := $(TWLM_ROOT)/tools/build/language-pack-$(notdir $(CURDIR))
LANGUAGE_PACK_INPUTS := $(wildcard $(NITRODATA)/languages/*/language.ini)
LANGUAGE_PACK_OUTPUTS := $(LANGUAGE_PACK_INPUTS:.ini=.lng)

$(LANGUAGE_PACK_TOOL): $(TWLM_ROOT)/tools/language_pack.cpp arm9/source/language.inl $(TWLM_ROOT)/universal/include/common/languagePack.h $(TWLM_ROOT)/universal/include/common/languageEscape.h $(TWLM_ROOT)/universal/include/common/inifile.h $(TWLM_ROOT)/universal/source/common/inifile.cpp
	$(MAKE) -C $(TWLM_ROOT)/tools build/language-pack-$(notdir $(CURDIR))

$(NITRODATA)/languages/%/language.lng: $(NITRODATA)/languages/%/language.ini $(LANGUAGE_PACK_TOOL)
	$(LANGUAGE_PACK_TOOL) $< $@

$(TARGET).nds: $(LANGUAGE_PACK_OUTPUTS)

.PHONY: clean-language-packs
clean: clean-language-packs
clean-language-packs:
	rm -f $(LANGUAGE_PACK_OUTPUTS)
