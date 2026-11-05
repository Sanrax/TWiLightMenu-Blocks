HOSTCXX ?= c++

.PHONY: all clean
all: build/r4denc

build/r4denc: r4denc/r4denc.cpp
	@mkdir -p build
	$(HOSTCXX) -O2 $< -o $@

clean:
	rm -rf build
