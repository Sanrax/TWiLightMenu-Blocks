#!/usr/bin/env python3
"""Replay hardware cache/RAM differences against the actual SD read adapter.

The host models cache invalidation, not ARM cache timing or real SD transfers.
Tests stay on diagnostics; --source can check the production master checkout.
"""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1])
args = parser.parse_args()
source = (args.source/'universal/sdmmc/arm9/source/blocksds_storage.c').read_text()
source = source[source.index('static void syncNativeSdRead('):source.index('bool __real_fatInit(')]
cases = json.loads((Path(__file__).parent/'fixtures/sd-cache-r27.json').read_text())

pre = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t sec_t;
typedef struct {
 unsigned long ioType,features;
 bool (*startup)(void),(*isInserted)(void);
 bool (*readSectors)(sec_t,sec_t,void*);
 bool (*writeSectors)(sec_t,sec_t,const void*);
 bool (*clearStatus)(void),(*shutdown)(void);
} DISC_INTERFACE;
_Alignas(32) uint8_t cached[160*1024],physical[160*1024];
int critical,invalidations,calls;bool failRead,noDriver,mainRam=true;
sec_t expectedSector,expectedSectors;void*expectedBuffer;
const uint8_t*replayCache,*replayRam;
int enterCriticalSection(void){assert(!critical);critical=1;return 0;}
void leaveCriticalSection(int old){assert(critical&&old==0);critical=old;}
bool memBufferIsInMainRam(const void*p,size_t n){return mainRam&&p>=(void*)cached&&(const uint8_t*)p+n<=cached+sizeof cached;}
void*memUncached(void*p){assert(critical==0);return physical+((uint8_t*)p-cached);}
void DC_InvalidateRange(void*p,size_t n){
 assert(critical&&((uintptr_t)p&31)==0&&(n&31)==0);invalidations++;
 memcpy(p,physical+((uint8_t*)p-cached),n);
}
bool sdmmc_ReadSectors(sec_t sector,sec_t sectors,void*buffer){
 assert(!critical&&sector==expectedSector&&sectors==expectedSectors&&buffer==expectedBuffer);calls++;
 size_t offset=(uint8_t*)buffer-cached;
 if(replayRam){assert(sectors==1);memcpy(physical+offset,replayRam,512);memcpy(buffer,replayCache,512);}
 else for(size_t i=0;i<sectors*512u;i++)physical[offset+i]=(uint8_t)(sector+i);
 return !failRead;
}
bool status(void){return true;}
bool writeSectors(sec_t s,sec_t n,const void*b){(void)s;(void)n;(void)b;return true;}
DISC_INTERFACE sdk={0x12345678,3,status,status,sdmmc_ReadSectors,writeSectors,status,status};
const DISC_INTERFACE*__real_get_io_dsisd(void){return noDriver?NULL:&sdk;}
'''

replays = []
for i,case in enumerate(cases):
    for field in ['cached','ram']:
        values = ','.join(str(x) for x in bytes.fromhex(case[field]))
        replays.append(f'static const uint8_t sample_{field}_{i}[512]={{{values}}};')

main = r'''
static void reset(unsigned alignment,unsigned sectors){
 memset(cached,0xA5,sizeof cached);memset(physical,0x18,sizeof physical);
 expectedBuffer=cached+64+alignment;expectedSector=123;expectedSectors=sectors;
 invalidations=calls=0;replayCache=replayRam=NULL;
}
static void check(const DISC_INTERFACE*io){
 assert(io->readSectors(expectedSector,expectedSectors,expectedBuffer));
 assert(calls==1&&invalidations==1&&!critical); // Preserve batched SDK transfers.
 size_t first=(uint8_t*)expectedBuffer-cached,last=first+expectedSectors*512u;
 for(size_t i=0;i<sizeof cached;i++){
  if(i>=first&&i<last)assert(cached[i]==physical[i]);
  else {assert(cached[i]==0xA5);assert(physical[i]==0x18);} // Dirty neighbors survive.
 }
}
int main(void){
 noDriver=true;assert(!__wrap_get_io_dsisd());noDriver=false;
 const DISC_INTERFACE*io=__wrap_get_io_dsisd();assert(io&&io!=&sdk);
 assert(io->ioType==sdk.ioType&&io->features==sdk.features);
 assert(io->startup==sdk.startup&&io->isInserted==sdk.isInserted);
 assert(io->writeSectors==sdk.writeSectors&&io->clearStatus==sdk.clearStatus&&io->shutdown==sdk.shutdown);
 for(unsigned align=0;align<32;align++)for(unsigned s=0;s<4;s++){
  unsigned lengths[]={1,2,17,257};reset(align,lengths[s]);check(io);
 }
 reset(4,1);failRead=true;assert(!io->readSectors(expectedSector,expectedSectors,expectedBuffer));
 assert(calls==1&&!invalidations);for(unsigned i=0;i<sizeof cached;i++)assert(cached[i]==0xA5);failRead=false;
 reset(20,1);mainRam=false;assert(io->readSectors(expectedSector,expectedSectors,expectedBuffer));
 assert(calls==1&&!invalidations);mainRam=true;
'''
for i,case in enumerate(cases):
    main += f'''
 reset({int(case['address'],16)&31},1);
 replayCache=sample_cached_{i};replayRam=sample_ram_{i};
 assert(sdmmc_ReadSectors(expectedSector,1,expectedBuffer));
 assert(memcmp(expectedBuffer,memUncached(expectedBuffer),512)!=0); // Captured failure before sync.
 calls=0;check(io);
'''
main += r'''
 puts("SD coherency: 32 alignments, 1/2/17/257-sector batches, dirty neighbors, errors, driver callbacks and three hardware replays passed");
}
'''

with tempfile.TemporaryDirectory(prefix='twlm-sd-cache-') as directory:
    path = Path(directory)
    (path/'test.c').write_text(pre+'\n'+source+'\n'+'\n'.join(replays)+main)
    subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror',
                    '-fsanitize=address,undefined','-fno-omit-frame-pointer',
                    str(path/'test.c'),'-o',str(path/'test')],check=True)
    # LeakSanitizer cannot inspect threads under the execution sandbox.
    import os
    subprocess.run([str(path/'test')],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'),check=True)
