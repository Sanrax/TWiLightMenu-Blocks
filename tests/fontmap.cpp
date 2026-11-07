#include "common/nftrFontMap.h"
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

static unsigned readCalls;
extern "C" size_t __real_fread(void *, size_t, size_t, FILE *);
extern "C" size_t __wrap_fread(void *out, size_t size, size_t count, FILE *file) {
    readCalls++;
    return __real_fread(out, size, count, file);
}

// Frozen small-read implementation: compare every bundled font's map bytes.
static void legacyMap(FILE *file, uint16_t *fontMap, uint32_t fileSize) {
	fseek(file, 0x28, SEEK_SET);
	uint32_t locPAMC, mapType;
	fread(&locPAMC, 4, 1, file);

	while (locPAMC && locPAMC < fileSize) {
		uint16_t firstChar, lastChar;
		fseek(file, locPAMC, SEEK_SET);
		fread(&firstChar, 2, 1, file);
		fread(&lastChar, 2, 1, file);
		fread(&mapType, 4, 1, file);
		fread(&locPAMC, 4, 1, file);

		switch(mapType) {
			case 0: {
				uint16_t firstTile;
				fread(&firstTile, 2, 1, file);
				for (unsigned i=firstChar;i<=lastChar;i++) {
					fontMap[firstTile+(i-firstChar)] = i;
				}
				break;
			} case 1: {
				for (int i=firstChar;i<=lastChar;i++) {
					uint16_t tile;
					fread(&tile, 2, 1, file);
					fontMap[tile] = i;
				}
				break;
			} case 2: {
				uint16_t groupAmount;
				fread(&groupAmount, 2, 1, file);
				for (int i=0;i<groupAmount;i++) {
					uint16_t charNo, tileNo;
					fread(&charNo, 2, 1, file);
					fread(&tileNo, 2, 1, file);
					fontMap[tileNo] = charNo;
				}
				break;
			}
		}
	}
}

static void put16(std::vector<uint8_t> &b, size_t p, uint16_t n) { b[p]=n; b[p+1]=n>>8; }
static void put32(std::vector<uint8_t> &b, size_t p, uint32_t n) { put16(b,p,n); put16(b,p+2,n>>16); }
static FILE *openData(const std::vector<uint8_t> &b) {
    FILE *f=tmpfile(); assert(f); assert(fwrite(b.data(),1,b.size(),f)==b.size()); return f;
}
static bool load(const std::vector<uint8_t> &b, std::vector<uint16_t> &map, uint32_t size=0) {
    FILE *f=openData(b); const bool ok=loadNftrCharacterMap(f,map.data(),map.size(),size?size:b.size());
    fclose(f); return ok;
}
static std::vector<uint8_t> block(unsigned type, unsigned first, unsigned last, const std::vector<uint16_t> &data) {
    std::vector<uint8_t> b(0x4c+data.size()*2);
    put32(b,0x28,0x40); put16(b,0x40,first); put16(b,0x42,last); put32(b,0x44,type);
    for (size_t i=0;i<data.size();i++) put16(b,0x4c+i*2,data[i]);
    return b;
}
static void rejected(const std::vector<uint8_t> &b, unsigned glyphs=8, uint32_t size=0) {
    std::vector<uint16_t> map(glyphs,0x1234); assert(!load(b,map,size));
    for (auto c:map) assert(c==0xffff);
}
int main(int argc,char **argv) {
    assert(argc>1); unsigned largeFonts=0;
    for (int arg=1;arg<argc;arg++) {
        std::ifstream in(argv[arg],std::ios::binary); assert(in.good());
        const std::vector<uint8_t> data{std::istreambuf_iterator<char>(in),{}};
        const unsigned tileOffset=0x14+data.at(0x14);
        const unsigned tileSize=twlm::nftr::read16(data.data()+tileOffset+6);
        assert(tileSize>0); const unsigned glyphs=(twlm::nftr::read32(data.data()+tileOffset)-0x10)/tileSize;
        assert(glyphs>0 && glyphs<=65536);
        std::vector<uint16_t> expected(glyphs,0xffff),actual(glyphs+2,0x1234);
        FILE *f=openData(data); readCalls=0; legacyMap(f,expected.data(),data.size()); const unsigned oldCalls=readCalls;
        readCalls=0; assert(loadNftrCharacterMap(f,actual.data()+1,glyphs,data.size())); const unsigned newCalls=readCalls; fclose(f);
        assert(actual.front()==0x1234 && actual.back()==0x1234);
        assert(!memcmp(expected.data(),actual.data()+1,glyphs*sizeof(uint16_t)));
        // Check every BMP character, including one beyond the largest mapping.
        // Fonts keep their existing ordered-map lookup semantics.
        if (std::is_sorted(expected.begin(),expected.end())) {
            for (uint32_t c=0;c<65536;c++) {
                const auto found=std::lower_bound(expected.begin(),expected.end(),c);
                const uint16_t fallback=0x1234;
                const uint16_t index=findNftrGlyph(actual.data()+1,glyphs,c,fallback);
                if (found!=expected.end() && *found==c) assert(index<glyphs && expected[index]==c);
                else assert(index==fallback);
            }
        }
        if (oldCalls>10000) {
            largeFonts++; assert(newCalls<oldCalls/20);
            std::cout<<argv[arg]<<": "<<oldCalls<<" -> "<<newCalls<<" map fread calls\n";
        }
    }
    assert(largeFonts>=2);
    std::vector<uint16_t> map(8);
    auto direct=block(0,32,39,{0}); assert(load(direct,map));
    for (unsigned i=0;i<8;i++) assert(map[i]==32+i);
    auto table=block(1,32,35,{2,0xffff,0,7}); assert(load(table,map));
    assert(map[2]==32 && map[0]==34 && map[7]==35 && map[1]==0xffff);
    auto pairs=block(2,0,65535,{4,0x5d0,0,0xe000,7,0x3042,2,0x200e,3}); assert(load(pairs,map));
    assert(map[0]==0x5d0 && map[7]==0xe000 && map[2]==0x3042 && map[3]==0x200e);
    // Type 1 and type 2 tables cross the 512-byte scratch-buffer boundary.
    std::vector<uint16_t> many(700);
    for (unsigned i=0;i<many.size();i++) many[i]=i;
    auto bigTable=block(1,1000,1699,many); map.resize(700); assert(load(bigTable,map));
    for (unsigned i=0;i<700;i++) assert(map[i]==1000+i);
    many={700}; for (unsigned i=0;i<700;i++) { many.push_back(i+1000); many.push_back(i); }
    auto bigPairs=block(2,0,65535,many); assert(load(bigPairs,map));
    for (unsigned i=0;i<700;i++) assert(map[i]==1000+i);
    // Linked maps may point backward while remaining acyclic.
    auto chain=block(0,32,35,{0}); chain.resize(0x80);
    put32(chain,0x28,0x60); put16(chain,0x60,36);put16(chain,0x62,39);put32(chain,0x64,0);put32(chain,0x68,0x40);put16(chain,0x6c,4);
    map.resize(8);assert(load(chain,map));for(unsigned i=0;i<8;i++)assert(map[i]==32+i);
    auto bad=direct;put16(bad,0x4c,1);rejected(bad); // consecutive range extends past allocation
    bad=table;put16(bad,0x4c,8);rejected(bad);
    bad=pairs;put16(bad,0x50,8);rejected(bad);
    bad=direct;put32(bad,0x48,0x40);rejected(bad); // self-link
    bad=chain;put32(bad,0x48,0x60);rejected(bad); // two-block cycle
    bad=direct;put32(bad,0x48,0xfffffffc);rejected(bad);
    bad=direct;put16(bad,0x40,40);rejected(bad); // reversed range
    bad=direct;put32(bad,0x44,3);rejected(bad);
    for (size_t length=0;length<direct.size();length++) {
        bad.assign(direct.begin(),direct.begin()+length);rejected(bad);
    }
    bad=bigTable;bad.resize(bad.size()-1);rejected(bad,700,bigTable.size()); // short device read
    bad=bigPairs;bad.resize(bad.size()-1);rejected(bad,700,bigPairs.size());
    const uint16_t ordered[]={32,63,126,0xe000};
    assert(findNftrGlyph(ordered,4,0xffff,1)==1);
    assert(findNftrGlyph(ordered,4,0,1)==1);
    assert(findNftrGlyph(ordered,4,0xe000,1)==3);
    assert(findNftrGlyph(nullptr,0,0xffff,1)==1);
    std::cout<<"NFTR maps: "<<argc-1<<" fonts match, all three map formats, missing glyphs, bounds, truncated reads and cycles passed\n";
}
