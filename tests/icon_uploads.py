#!/usr/bin/env python3
"""Exercise production icon staging, scanline guards and palette selection.

Hardware calls are bounded shims; no SDK, BIOS or game files are required.
Use --source to check another checkout. Tests remain on diagnostics.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1])
root = p.parse_args().source.resolve()
base = root / 'romsel_dsimenutheme/arm9/source'
icons = (base / 'iconTitle.cpp').read_text()
handler = (base / 'graphics/iconHandler.cpp').read_text()
header = (base / 'graphics/iconHandler.h').read_text()

def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'

pre = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
using u8=uint8_t; using u16=uint16_t;
int REG_VCOUNT, currentBg, CURPOS=3, movingApp=-1;
bool displayGameIcons=true;
namespace TWLSettings { enum { EThemeSaturn=4 }; }
struct Settings { int theme=0; } settings;
Settings& ms() { return settings; }
#define NDS_ICON_BANK_COUNT 8
#define ICON_GRID_MAX_OFFSET 3
#define MOVING_APP_SLOT 40
#define ICON_MOVING_BANK 7
#define ICON_GRID_BANK(n) ((n)%7)
#define TWL_TEX_HEIGHT 256
#define BAD_ICON_IDX(n) ((n)<0||(n)>=8)
alignas(4) u8 pendingTiles[8][4096], pixels[8][4096];
alignas(4) u16 pendingPalette[8][16], paletteCache[8][16];
void* iconPalettes[8];
bool pendingTwl[8], bankLoaded[8], bnriconisDSi[41];
volatile bool pendingValid[8];
int bnriconPalLine[41], bnriconPalLoaded[41];
struct Banner { u16 dsi_palette[8][16]; } bnriconTile[41];
int uploads, paletteUploads, copyLines;
void tonccpy(void*d,const void*s,size_t n) { memcpy(d,s,n); }
void glLoadPalette(int bank,const u16* pal) {
    assert(REG_VCOUNT>=192 && REG_VCOUNT<=208);
    memcpy(paletteCache[bank],pal,32); paletteUploads++;
}
void glLoadIcon(int bank,const u16* pal,const u8* data,int height) {
    assert(REG_VCOUNT>=192 && REG_VCOUNT<=200);
    memcpy(pixels[bank],data,32*height/2);
    memcpy(paletteCache[bank],pal,32); uploads++; REG_VCOUNT+=copyLines;
}
'''
main = r'''
int main() {
    for(int line=0;line<263;line++) {
        REG_VCOUNT=line;
        assert(vramSafeToUnmap()==(line>=192&&line<=200));
        assert(vramSafeToUnmap(true)==(line>=192&&line<=208));
    }
    u8 source[4096];u16 pal[16];
    for(int i=0;i<4096;i++)source[i]=(i*7)&255;
    for(int i=0;i<16;i++)pal[i]=i*37;
    REG_VCOUNT=100;
    for(currentBg=0;currentBg<3;currentBg++)stageConverted(0,pal,source,256);
    assert(uploads==0&&pendingValid[0]&&hasDeferredIconUpdates());
    source[0]^=255;pal[0]^=255;
    for(int line:{0,191,201,214,250,262}) {
        REG_VCOUNT=line;assert(!execDeferredIconUpdates());
        assert(pendingValid[0]&&uploads==0);
    }
    REG_VCOUNT=192;assert(execDeferredIconUpdates());
    assert(uploads==1&&bankLoaded[0]&&!hasDeferredIconUpdates());
    assert(pixels[0][0]==0&&paletteCache[0][0]==0); // Staged data is owned.
    source[0]^=255;pal[0]^=255;
    stageConverted(0,pal,source,256);assert(!hasDeferredIconUpdates());
    source[0]^=1;stageConverted(0,pal,source,256);assert(hasDeferredIconUpdates());
    // Simulate copies crossing the deadline. Remaining banks must survive.
    for(int i=1;i<8;i++)stageConverted(i,pal,source,256);
    copyLines=3;REG_VCOUNT=192;assert(execDeferredIconUpdates());
    assert(uploads==4&&hasDeferredIconUpdates());
    assert(!pendingValid[0]&&!pendingValid[1]&&!pendingValid[2]&&pendingValid[3]);
    for(int frame=0;hasDeferredIconUpdates();frame++) {
        assert(frame<3);REG_VCOUNT=192;execDeferredIconUpdates();
    }
    for(int i=0;i<8;i++)assert(!memcmp(pixels[i],source,4096));
    // Recorded palette indices can be stale: compare actual uploaded colors.
    for(int i=0;i<8;i++)iconPalettes[i]=paletteCache[i];
    bnriconisDSi[2]=true;bnriconPalLine[2]=bnriconPalLoaded[2]=3;
    for(int i=0;i<16;i++)bnriconTile[2].dsi_palette[3][i]=i+123;
    REG_VCOUNT=209;assert(!loadDeferredIconPalettes()&&paletteUploads==0);
    REG_VCOUNT=192;assert(loadDeferredIconPalettes()&&paletteUploads==1);
    assert(!memcmp(paletteCache[2],bnriconTile[2].dsi_palette[3],32));
    assert(!loadDeferredIconPalettes()&&paletteUploads==1);
    bnriconPalLine[2]=4;REG_VCOUNT=208;
    assert(loadDeferredIconPalettes()&&bnriconPalLoaded[2]==4);
    // Off-window entries alias banks but must not replace visible palettes.
    bnriconisDSi[9]=true;bnriconPalLoaded[9]=-1;
    memset(bnriconTile[9].dsi_palette,0xAB,sizeof bnriconTile[9].dsi_palette);
    assert(!loadDeferredIconPalettes()&&paletteUploads==2);
    // The carried icon has its own bank; Saturn/dialogs update only selection.
    movingApp=17;bnriconisDSi[40]=true;bnriconPalLine[40]=2;
    bnriconTile[40].dsi_palette[2][0]=0x1234;
    assert(loadDeferredIconPalettes()&&paletteCache[7][0]==0x1234);
    settings.theme=4;CURPOS=3;
    bnriconPalLine[2]=5;assert(!loadDeferredIconPalettes());
    displayGameIcons=false;settings.theme=0;assert(!loadDeferredIconPalettes());
    puts("Icon uploads: late IRQ deferral, owned staging, palette/frame synchronization passed");
}
'''
guard = header[header.index('#define VRAM_UNMAP_LAST_SAFE_LINE'):header.index('// Checks if the icon')]
source = pre + guard
for src, signature in [
    (handler, 'bool iconPaletteMatches'),
    (icons, 'bool execDeferredIconUpdates'),
    (icons, 'bool hasDeferredIconUpdates'),
    (icons, 'static void stageConverted'),
    (icons, 'bool loadDeferredIconPalettes'),
]:
    source += function(src, signature)
with tempfile.TemporaryDirectory(prefix='twlm-icon-') as directory:
    cpp = Path(directory) / 'test.cpp'
    exe = Path(directory) / 'test'
    cpp.write_text(source + main)
    subprocess.run(['c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                    str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
