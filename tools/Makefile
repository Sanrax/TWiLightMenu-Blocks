HOSTCXX ?= c++

.PHONY: all clean
all: build/r4denc

build/r4denc: r4denc/r4denc.cpp
	@mkdir -p build
	$(HOSTCXX) -O2 $< -o $@

build/photo-index: photo_index.cpp ../universal/include/common/indexedImage.h ../universal/include/common/lodepng.h ../universal/source/lodepng/lodepng.cpp
	@mkdir -p build
	$(HOSTCXX) -std=c++17 -O2 -I../universal/include photo_index.cpp ../universal/source/lodepng/lodepng.cpp -o $@

clean:
	rm -rf build
