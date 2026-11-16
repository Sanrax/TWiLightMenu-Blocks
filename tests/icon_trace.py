#!/usr/bin/env python3
"""Check the diagnostic recorder, not icon rendering or physical SD timing."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='twlm-icon-trace-') as directory:
    work = Path(directory)
    (work/'include/common').mkdir(parents=True)
    (work/'include/nds.h').write_text('''#pragma once
extern int REG_VCOUNT;
int enterCriticalSection();
void leaveCriticalSection(int);
void *memUncached(void *);
bool memBufferIsInMainRam(const void *, unsigned);
''')
    (work/'include/common/systemdetails.h').write_text('''#pragma once
struct TestSystem { bool isRunFromSD() { return false; } };
TestSystem& sys();
''')
    source = r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include "common/systemdetails.h"
int REG_VCOUNT=192, critical; bool inIrq;
int enterCriticalSection(){int old=critical;critical++;return old;}
void leaveCriticalSection(int old){assert(critical==old+1);critical=old;}
unsigned char diskBuffer[1536],ramBuffer[1536];
void*memUncached(void*p){assert(p==diskBuffer);return ramBuffer;}
bool memBufferIsInMainRam(const void*p,unsigned n){return p==diskBuffer&&n<=sizeof diskBuffer;}
TestSystem& sys(){static TestSystem s;return s;}
extern "C" FILE* __real_fopen(const char*,const char*);
extern "C" FILE* __wrap_fopen(const char*p,const char*m){assert(!inIrq&&!critical);return __real_fopen(p,m);}
#include "ICON_TRACE_SOURCE"
std::string log(){std::ifstream f("fat:/_nds/TWiLightMenu/icon-diagnostic.log");return std::string(std::istreambuf_iterator<char>(f),{});}
int main(){
 assert(crc("123456789",9)==0x4B37);
 unsigned char banner[0x840]={1},vram[512];unsigned short palette[16]={};
 for(unsigned i=0;i<512;i++)banner[32+i]=i*7;
 unsigned short expected=crc(banner+32,0x820);banner[2]=expected;banner[3]=expected>>8;
 iconTraceRead("Other.nds",1,banner,sizeof banner);assert(count==0);
 iconTraceRead("DS Battery Timer.nds",1,banner,sizeof banner);
 iconTraceSelect(1,1,"DS Battery Timer.nds");
 iconTraceStage(1,banner+32,palette,512,false);
 memcpy(vram,banner+32,512);inIrq=true;
 iconTraceUpload(1,banner+32,vram,palette,512);inIrq=false;
 assert(count==3);iconTraceFlush();
 auto text=log();assert(text.find("3 events, 0 dropped")!=std::string::npos);
 assert(text.find("differences=0")!=std::string::npos&&snapshot[0].storedCrc==snapshot[0].readCrc);
 banner[32+400]^=1;iconTraceRead("DS Battery Timer.nds",1,banner,sizeof banner);
 iconTraceStage(1,banner+32,palette,512,true);
 memcpy(vram,banner+32,512);vram[400]^=1;inIrq=true;
 iconTraceUpload(1,banner+32,vram,palette,512);inIrq=false;iconTraceFlush();
 assert(snapshot[0].storedCrc!=snapshot[0].readCrc);
 text=log();assert(text.find("differences=1")!=std::string::npos&&text.find("S slot=1")!=std::string::npos);
 for(int i=0;i<13;i++)iconTraceRead("DS Battery Timer.nds",1,banner,sizeof banner);
 assert(count==8&&dropped==5);iconTraceFlush();assert(log().find("8 events, 5 dropped")!=std::string::npos);
 iconTraceSelect(1,1,"Other.nds");iconTraceUpload(1,banner+32,vram,palette,512);assert(!count);
 // The observer records raw SDK results before production synchronizes them.
 memset(diskBuffer,0x3C,sizeof diskBuffer);memset(ramBuffer,0x3C,sizeof ramBuffer);
 twlmIconTraceDiskRead(123,3,diskBuffer,true);assert(!ioReads&&!count);
 {
  IconTraceIoScope io("DS Battery Timer.nds",1);
  twlmIconTraceDiskRead(123,3,diskBuffer,true);assert(ioReads==1&&!ioDifferent&&!count);
  diskBuffer[536]=0x55;diskBuffer[539]=0x66;
  twlmIconTraceDiskRead(123,3,diskBuffer,true);assert(ioReads==2&&ioDifferent==1&&count==1);
  const auto&e=events[0];assert(e.type=='D'&&e.firstDifference==536&&e.differences==2&&e.sampleOffset==512);
  assert(e.source[24]==0x55&&e.source[27]==0x66&&e.actual[24]==0x3C&&e.actual[27]==0x3C);
  assert(diskBuffer[536]==0x55&&ramBuffer[536]==0x3C); // The observer never repairs data.
  twlmIconTraceDiskRead(123,3,diskBuffer,false);assert(ioErrors==1&&count==1);
  {IconTraceIoScope other("Other.nds",2);assert(ioSlot==-1);}assert(ioSlot==1);
 }
 assert(ioSlot==-1);iconTraceFlush();
 text=log();assert(text.find("SD reads=3 pre-sync cache/RAM mismatches=1 errors=1")!=std::string::npos);
 assert(text.find("first=536 differences=2 sample=512")!=std::string::npos);
 FILE*f=fopen("fat:/_nds/TWiLightMenu/icon-diagnostic.log","ab");
 while(ftell(f)<65537)fputc('x',f);long before=ftell(f);fclose(f);
 iconTraceRead("DS Battery Timer.nds",1,banner,sizeof banner);iconTraceFlush();
 f=fopen("fat:/_nds/TWiLightMenu/icon-diagnostic.log","rb");fseek(f,0,SEEK_END);assert(ftell(f)==before);fclose(f);
 puts("Icon recorder: banner/VRAM errors, bounded logs, no IRQ I/O and pre-sync SD cache/RAM observation passed");
}
'''
    source = source.replace('ICON_TRACE_SOURCE',str(root/'romsel_dsimenutheme/arm9/source/graphics/iconTrace.cpp'))
    (work/'test.cpp').write_text(source)
    (work/'fat:/_nds/TWiLightMenu').mkdir(parents=True)
    exe = work/'test'
    subprocess.run(['c++','-std=c++17','-O2','-Wall','-Wextra','-Werror',
        '-Wno-misleading-indentation','-DTWLM_ICON_TRACE','-I'+str(work/'include'),
        str(work/'test.cpp'),'-Wl,--wrap=fopen','-o',str(exe)],check=True)
    subprocess.run([str(exe)],cwd=work,check=True)
