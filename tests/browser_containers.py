#!/usr/bin/env python3
"""Exercise production sort, cached-page updates and typed directory iteration.

Only SDK/filesystem calls are mocked. The production functions are extracted
verbatim; libstdc++ checked iterators, ASan and UBSan check their container use.
"""
import argparse
import os
import resource
from pathlib import Path
import subprocess
import tempfile

p = argparse.ArgumentParser()
p.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1])
p.add_argument('--case', choices=['all', 'sort', 'cache', 'directory'], default='all')
a = p.parse_args()
root = a.source.resolve()
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))

def between(source, start, end):
    begin = source.index(start)
    return source[begin:source.index(end, begin)]

pre = r'''
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <map>
#include <random>
#include <string>
#include <string_view>
#include <vector>
#include <strings.h>
using namespace std;
'''

sort_test = r'''
int main() {
    vector<DirEntry> entries;
    for (bool directory : {false,true}) for (bool custom : {false,true})
        for (int position : {0,1}) for (const char *name : {"A","a","B"})
            entries.emplace_back(name,directory,position,custom);
    auto before=[](const DirEntry&a,const DirEntry&b){return dirEntryPredicate(a,b);};
    auto same=[&](const DirEntry&a,const DirEntry&b){return !before(a,b)&&!before(b,a);};
    for (const auto&x:entries) {
        assert(!before(x,x));
        for (const auto&y:entries) for (const auto&z:entries) {
            if(before(x,y)) assert(!before(y,x));
            if(before(x,y)&&before(y,z)) assert(before(x,z));
            if(same(x,y)&&same(y,z)) assert(same(x,z));
        }
    }
    vector<DirEntry> order={{"game.nds",false,0,true},{"old folder",true,1,true},
        {"new folder",true,0,false},{"new game.nds",false,0,false}};
    sort(order.begin(),order.end(),before);
    assert(order[0].name=="new folder"&&order[1].name=="game.nds"&&order[2].name=="old folder");
    mt19937 random(42);
    for(int run=0;run<100;run++) {
        vector<DirEntry> list;
        for(int i=0;i<1024;i++)list.push_back(entries[random()%entries.size()]);
        sort(list.begin(),list.end(),before);
        assert(is_sorted(list.begin(),list.end(),before));
    }
    puts("Sort: strict weak ordering, saved positions and 100 mixed 1024-entry lists passed");
}
'''

cache_pre = r'''
#define PAGENUM page
#define F_OK 0
int page, file_count, fileStartPos;
bool dirInfoIniFound, pageLoaded[100], dirContentBlankFilled[100], lockOutDirContentBlankFilling;
vector<string> names;
int access(const char*,int){return 0;}
void recalculateBoxesCount(){}
void startupTraceMark(const char*){}
struct CIniFile {
    CIniFile(const char*){}
    int GetInt(const char*,const char*,int){return names.size();}
    string GetString(const char*,const char*item,const char*) {
        size_t index=stoul(item);return index<names.size()?names[index]:string();
    }
};
'''
cache_test = r'''
int main() {
    vector<DirEntry> entries;
    const int counts[]={96,1,0,39,40,41,79,80,200,120,2};
    for(int run=0;run<30;run++)for(int length:counts) {
        names.clear();for(int i=0;i<length;i++)names.push_back(to_string(run)+"-"+to_string(i)+".nds");
        const int last=max((length-1)/40,0);
        page=last;getDirectoryContents(entries);updateDirectoryContents(entries);
        for(page=last;page>=0;page--) {
            updateDirectoryContents(entries);
            for(int i=page*40;i<min((page+1)*40,length);i++)assert(entries.at(i).name==names.at(i));
        }
        assert(entries.size()==names.size());
        for(int i=0;i<length;i++)assert(entries.at(i).name==names.at(i));
        // Also load a new cache sequentially from page zero.
        page=0;getDirectoryContents(entries);
        for(page=0;page<=last;page++)updateDirectoryContents(entries);
        assert(entries.size()==names.size());
        for(int i=0;i<length;i++)assert(entries.at(i).name==names.at(i));
    }
    puts("Cache: 660 directory rebuilds, empty/partial/full pages and forward/backward loading passed");
}
'''

directory_pre = r'''
// Native FatFS declarations use the ARM32 allocator ABI. Mock the typed
// directory API here; filename capacities are separately checked with ARM GCC.
struct FF_DIR { unsigned unused; };
struct FILINFO { char fname[256]; unsigned char fattrib; unsigned fclust; };
enum FRESULT { FR_OK, FR_NO_PATH, FR_DISK_ERR };
constexpr unsigned AM_DIR=0x10, AM_HID=0x02;
static_assert(sizeof(FILINFO::fname)<=sizeof(dirent::d_name));
struct Item{string name;unsigned attr,cluster;};
vector<Item> disk;map<FF_DIR*,size_t> cursors;bool failOpen,failRead;
FRESULT libnds_f_opendir(FF_DIR*dir,const char*) {
    if(failOpen)return FR_NO_PATH;cursors[dir]=0;return FR_OK;
}
FRESULT libnds_f_readdir(FF_DIR*dir,FILINFO*info) {
    if(failRead)return FR_DISK_ERR;
    *info={};size_t &i=cursors.at(dir);if(i==disk.size())return FR_OK;
    const auto &entry=disk[i++];strcpy(info->fname,entry.name.c_str());
    info->fattrib=entry.attr;info->fclust=entry.cluster;return FR_OK;
}
FRESULT libnds_f_closedir(FF_DIR*dir){assert(cursors.erase(dir)==1);return FR_OK;}
typedef struct TWLDirectory TWLDirectory;
// The production adapter is C. Give calloc its concrete return type only in
// this C++ test translation unit; allocation size and runtime behavior match C.
TWLDirectory* allocateDirectory(size_t n,size_t bytes){return static_cast<TWLDirectory*>(std::calloc(n,bytes));}
#define calloc allocateDirectory
'''
directory_test = r'''
int main() {
    disk={{"..",AM_DIR,1},{"DS Battery Timer.nds",0,2},{"hidden.nds",AM_HID,3},
        {"folder",AM_DIR|AM_HID,4},{string(255,'x'),0,5}};
    for(int run=0;run<1000;run++) {
        TWLDirectory *dir=twlmOpenDirectory(".");assert(dir);
        vector<string> copied;
        for(const auto &expected:disk) {
            uint8_t attributes=0xFF;const dirent*entry=twlmReadDirectory(dir,&attributes);assert(entry);
            assert(entry->d_name==expected.name&&attributes==expected.attr);
            assert(entry->d_type==((expected.attr&AM_DIR)?DT_DIR:DT_REG));
            assert(entry->d_ino==expected.cluster);copied.emplace_back(entry->d_name);
        }
        assert(!twlmReadDirectory(dir,nullptr));
        for(unsigned i=0;i<copied.size();i++)assert(copied[i]==disk[i].name);
        failRead=true;errno=0;assert(!twlmReadDirectory(dir,nullptr)&&errno==EIO);failRead=false;
        twlmCloseDirectory(dir);assert(cursors.empty());
    }
    failOpen=true;errno=0;assert(!twlmOpenDirectory("missing")&&errno==EIO);twlmCloseDirectory(nullptr);
    puts("Directory adapter: names/attributes/types, 255-byte names, errors and 1000 reopen cycles passed");
}
'''

with tempfile.TemporaryDirectory(prefix='twlm-browser-') as directory:
    work = Path(directory)
    cases = []
    for app in ['romsel_dsimenutheme','romsel_aktheme','romsel_r4theme']:
        source = (root/app/'arm9/source/fileBrowse.cpp').read_text()
        entry = between(source, 'struct DirEntry {', '\n};') + '\n};\n'
        if a.case in ['all','sort']:
            comparison = between(source,'bool dirEntryPredicate(', '\nvoid getDirectoryContents' if app!='romsel_dsimenutheme' else '\nvoid updateDirectoryContents')
            cases.append((app+'-sort',pre+entry+comparison+sort_test))
        if app=='romsel_dsimenutheme' and a.case in ['all','cache']:
            # This is the complete production cached-directory branch; the
            # FAT walk below it is unreachable with the mocked cache present.
            get = between(source,'void getDirectoryContents(', '\n\tTWLDirectory *pdir')+'\n}\n'
            update = between(source,'void updateDirectoryContents(', '\nvoid getDirectoryContents(')
            cases.append(('cache',pre+entry+cache_pre+get+update+cache_test))
    if a.case in ['all','directory']:
        source = (root/'universal/sdmmc/arm9/source/blocksds_storage.c').read_text()
        adapter = between(source,'struct TWLDirectory {','unsigned twlmStorageClusterSize(')
        cases.append(('directory',pre+directory_pre+adapter+directory_test))
    for name,source in cases:
        cpp=work/(name+'.cpp');exe=work/name;cpp.write_text(source)
        subprocess.run(['c++','-std=c++17','-O1','-g','-D_GLIBCXX_DEBUG',
            '-fsanitize=address,undefined','-fno-omit-frame-pointer',
            '-I'+str(root/'universal/include'),str(cpp),'-o',str(exe)],check=True)
        # LeakSanitizer cannot inspect threads under some ptrace sandboxes.
        # Address/undefined checks and checked iterators stay enabled.
        env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')
        subprocess.run([str(exe)],check=True,timeout=30,env=env)
