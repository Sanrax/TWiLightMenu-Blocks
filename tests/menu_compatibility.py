#!/usr/bin/env python3
"""Exercise production driver installation, startup publication and IRQ palette copies.

Pass a separately built checkout with --source. No SDK/BIOS/ROMs are needed.
The small hardware shims compile extracted production functions, not copies of
those algorithms. Test files deliberately remain on diagnostics.
"""
import argparse, pathlib, subprocess, tempfile
p=argparse.ArgumentParser();p.add_argument('--source',type=pathlib.Path,default=pathlib.Path(__file__).resolve().parents[1]);a=p.parse_args();root=a.source.resolve()
def function(path,start,end):
 s=(root/path).read_text();return s[s.index(start):s.index(end,s.index(start))]
install=function('universal/sdmmc/arm9/source/blocksds_storage.c','static bool dldiSectionValid','static void syncNativeSdRead')
unpack=function('universal/source/common/dldi.cpp','static bool unpackDldi','bool twlmLoadDldi')
reveal=function('romsel_dsimenutheme/arm9/source/graphics/graphics.cpp','void endMenuContentUpdate','static void drawMenuIcon')
palette=function('romsel_aktheme/arm9/source/iconTitle.cpp','void glReloadIconPalette(int num)','void glLoadPalette')
pre=r'''
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
using u8=uint8_t;using u16=uint16_t;using u32=uint32_t;
#define FIX_ALL 1
#define FIX_GLUE 2
#define FIX_GOT 4
#define FIX_BSS 8
struct DLDI_INTERFACE {
 uint32_t magicNumber;char magicString[8];uint8_t versionNumber,driverSize,fixSectionsFlags,allocatedSize;
 char friendlyName[48];void *dldiStart,*dldiEnd,*interworkStart,*interworkEnd,*gotStart,*gotEnd,*bssStart,*bssEnd;
};
struct {int fs_type;} mounts[3];
alignas(32) unsigned char stub[32768];
const DLDI_INTERFACE*io_dldi_data=(DLDI_INTERFACE*)stub;
size_t reserved=sizeof stub,flushSize=576;std::vector<int> events;
void* dldiGetStubEnd(){return stub+reserved;}
bool dldiIsValid(const DLDI_INTERFACE*d){return d->magicNumber==0xBF8DA5ED;}
void cache_deinit(){assert(stub[1024]==0xA5);events.push_back(1);}
void dldiRelocate(DLDI_INTERFACE*d,void*t){assert(events==std::vector<int>{1});assert(d==t);events.push_back(2);}
void DC_FlushRange(void*p,size_t n){assert(p==stub&&n==flushSize);events.push_back(3);}
void IC_InvalidateRange(void*p,size_t n){assert(p==stub&&n==flushSize);events.push_back(4);}
#define DLDI_MODE_AUTODETECT -1
void dldiSetMode(int m){assert(m==-1);events.push_back(5);}
'''
fade=r'''
namespace TWLSettings {enum {EThemeDSi=0,ETheme3DS=1,EThemeSaturn=4,EThemeHBL=5};}
struct Settings {int theme;bool fastMenuFades;} settings;
Settings&ms(){return settings;}
int screenBrightness=31,fadeDelay=2;bool fadeSpeed,controlTopBright,fadeType,dropDown,displayGameIcons;
unsigned menuContentState=1;bool menuIconsAnimating=false;int critical;
int enterCriticalSection(){assert(!critical);critical=1;return 0;}
void leaveCriticalSection(int){assert(critical);assert(menuContentState==2);assert(displayGameIcons);critical=0;}
void menuTransitionFrame(unsigned){assert(false);}
#define BAD_ICON_IDX(n) ((n)<0||(n)>=8)
u16 colors[8][16],_paletteCache[8][16];int iconTexID[]={1,2,3,4,5,6,7,8};int lookups,copies,bankSwitches;
void*glGetColorTablePointer(int id){lookups++;return id?colors[id-1]:nullptr;}
#define VRAM_E_LCD 0
#define VRAM_F_LCD 0
#define VRAM_G_LCD 0
u32 vramSetBanks_EFG(int,int,int){bankSwitches++;return 42;}
void vramRestoreBanks_EFG(u32 b){assert(b==42);bankSwitches--;}
void tonccpy(void*d,const void*s,size_t n){assert(bankSwitches==1&&n==32);copies++;memcpy(d,s,n);}
'''
main=r'''
int main(int argc,char**argv){
 assert(argc>=2);
 alignas(32) unsigned char image[1024]={};auto*d=(DLDI_INTERFACE*)image;
 d->magicNumber=0xBF8DA5ED;d->driverSize=10;d->fixSectionsFlags=FIX_BSS;
 d->dldiStart=(void*)0xBF800000;d->dldiEnd=(void*)0xBF800200;
 d->bssStart=d->dldiEnd;d->bssEnd=(void*)0xBF800240;
 memset(image+sizeof(*d),0x3C,512-sizeof(*d));
 auto reset=[](){memset(stub,0xA5,sizeof stub);((DLDI_INTERFACE*)stub)->allocatedSize=15;events.clear();};
 auto rejects=[&](size_t n){reset();unsigned char old[sizeof stub];memcpy(old,stub,sizeof stub);assert(!twlmInstallDldi(image,n));assert(events.empty());assert(!memcmp(old,stub,sizeof stub));};
 rejects(sizeof(*d)-1);rejects(511);
 d->magicNumber=0;rejects(512);d->magicNumber=0xBF8DA5ED;
 d->driverSize=31;rejects(512);d->driverSize=10;
 d->bssStart=(void*)0xBF800100;rejects(512);d->bssStart=d->dldiEnd;
 d->bssEnd=(void*)0xBF801000;rejects(512);d->bssEnd=(void*)0xBF800240;
 mounts[0].fs_type=1;rejects(512);mounts[0].fs_type=0;
 reserved=128;rejects(512);reserved=sizeof stub;
 d->fixSectionsFlags|=FIX_GOT;d->gotStart=(void*)0xBF800201;d->gotEnd=(void*)0xBF800204;rejects(512);d->fixSectionsFlags=FIX_BSS;
 reset();assert(twlmInstallDldi(image,512));assert((events==std::vector<int>{1,2,3,4,5}));
 assert(((DLDI_INTERFACE*)stub)->allocatedSize==15);assert(!memcmp(stub+sizeof(*d),image+sizeof(*d),512-sizeof(*d)));
 for(int i=512;i<576;i++)assert(stub[i]==0);for(int i=576;i<32768;i++)assert(stub[i]==0xA5);
 std::ifstream f(argv[1],std::ios::binary);std::vector<u8> compressed((std::istreambuf_iterator<char>(f)),{}),decoded(32768);size_t length=0;
 assert(unpackDldi(compressed.data(),compressed.size(),decoded.data(),decoded.size(),length));assert(length==19724);
 uint32_t magic;memcpy(&magic,decoded.data(),4);assert(magic==0xBF8DA5ED);
 assert(!unpackDldi(compressed.data(),compressed.size()-100,decoded.data(),decoded.size(),length));
 assert(!unpackDldi(compressed.data(),compressed.size(),decoded.data(),128,length));
 u8 bad[]={0x10,3,0,0,0x80,0,0};assert(!unpackDldi(bad,sizeof bad,decoded.data(),decoded.size(),length));
 for(int theme:{0,1,4,5})for(bool fast:{false,true}){
  settings={theme,fast};screenBrightness=31;fadeDelay=2;dropDown=false;fadeType=false;controlTopBright=false;displayGameIcons=false;menuContentState=1;
  endMenuContentUpdate(true);assert(screenBrightness==16&&fadeDelay==0&&fadeType&&controlTopBright);
  assert(dropDown==(theme==0));assert(fadeSpeed==(theme!=0||fast));
 }
 for(int i=0;i<8;i++){for(int c=0;c<16;c++)_paletteCache[i][c]=i*16+c;glReloadIconPalette(i);}
 assert(copies==8&&lookups==8&&!bankSwitches);assert(!memcmp(colors,_paletteCache,sizeof colors));
 glReloadIconPalette(-1);glReloadIconPalette(8);assert(copies==8&&lookups==8);
 for(int arg=1;arg<argc;arg++){
  std::ifstream f(argv[arg],std::ios::binary);std::vector<u8> raw((std::istreambuf_iterator<char>(f)),{}),unpacked(32768);
  if(raw[0]==0x10){size_t len;assert(unpackDldi(raw.data(),raw.size(),unpacked.data(),unpacked.size(),len));raw.assign(unpacked.begin(),unpacked.begin()+len);}
  DLDI_INTERFACE header{};memcpy(&header,raw.data(),64);
  auto ptr=[&](int off){u32 v;memcpy(&v,raw.data()+off,4);return reinterpret_cast<void*>((uintptr_t)v);};
  header.dldiStart=ptr(64);header.dldiEnd=ptr(68);header.interworkStart=ptr(72);header.interworkEnd=ptr(76);
  header.gotStart=ptr(80);header.gotEnd=ptr(84);header.bssStart=ptr(88);header.bssEnd=ptr(92);
  static_assert(sizeof(header)==128);memcpy(raw.data(),&header,sizeof header);
  flushSize=((std::max((uintptr_t)header.dldiEnd,(uintptr_t)header.bssEnd)-(uintptr_t)header.dldiStart)+3)&~size_t(3);
  reset();assert(twlmInstallDldi(raw.data(),raw.size()));assert(events.size()==5);
 }
 puts("All bundled driver layouts, rejection/publication order, NRIO decoding, theme entry fades and allocation-free palette copies passed");
}
'''
with tempfile.TemporaryDirectory(prefix='twlm-menu-check-') as tmp:
 src=pathlib.Path(tmp)/'check.cpp';exe=pathlib.Path(tmp)/'check';src.write_text(pre+install+unpack+fade+reveal+palette+main)
 subprocess.run(['c++','-std=c++17','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation',str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe),str(root/'title/nitrofiles/dldi/nrio.lz77'),*map(str,sorted((root/'title/nitrofiles/dldi').glob('*.dldi')))],check=True)
