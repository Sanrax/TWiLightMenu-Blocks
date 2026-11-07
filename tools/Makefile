HOSTCXX ?= c++

.PHONY: all clean
all: build/r4denc

build/r4denc: r4denc/r4denc.cpp
	@mkdir -p build
	$(HOSTCXX) -O2 $< -o $@

build/photo-index: photo_index.cpp ../universal/include/common/indexedImage.h ../universal/include/common/lodepng.h ../universal/source/lodepng/lodepng.cpp
	@mkdir -p build
	$(HOSTCXX) -std=c++17 -O2 -I../universal/include photo_index.cpp ../universal/source/lodepng/lodepng.cpp -o $@

build/language-pack-%: language_pack.cpp ../%/arm9/source/language.inl ../universal/include/common/languagePack.h ../universal/include/common/languageEscape.h ../universal/include/common/inifile.h ../universal/source/common/inifile.cpp
	@mkdir -p build
	$(HOSTCXX) -std=c++17 -O2 -I../universal/include -DTWLM_LANGUAGE_INL='"../$*/arm9/source/language.inl"' language_pack.cpp ../universal/source/common/inifile.cpp -o $@

clean:
	rm -rf build
