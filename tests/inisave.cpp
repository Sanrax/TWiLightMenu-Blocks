#include "common/inifile.h"
#include <cassert>
#include <cstdio>
#include <cerrno>
#include <fstream>
#include <iterator>
#include <string>
static bool shortWrite, failPublish;
extern "C" size_t __real_fwrite(const void *, size_t, size_t, FILE *);
extern "C" size_t __wrap_fwrite(const void *p,size_t s,size_t n,FILE *f) {
    if (shortWrite) { shortWrite=false; return __real_fwrite(p,s,n/2,f); }
    return __real_fwrite(p,s,n,f);
}
extern "C" int __real_rename(const char *,const char *);
extern "C" int __wrap_rename(const char *old,const char *next) {
    if (failPublish && std::string(old).compare(std::string(old).size()-4,4,".tmp")==0) {failPublish=false;errno=EIO;return -1;}
    return __real_rename(old,next);
}
static std::string read(const std::string &p) {
    std::ifstream f(p); return {std::istreambuf_iterator<char>(f),{}};
}
int main(int argc,char **argv) {
    assert(argc==2);
    const std::string p=argv[1];
    CIniFile ini(p); ini.SetString("SRLOADER","LANGUAGE","English"); assert(ini.SaveIniFile(p));
    const auto first=read(p);
    ini.SetString("SRLOADER","LANGUAGE","Japanese"); shortWrite=true;
    assert(!ini.SaveIniFile(p) && read(p)==first);
    assert(ini.SaveIniFile(p) && read(p+".bak")==first);
    const auto second=read(p);
    ini.SetString("SRLOADER","LANGUAGE","German"); failPublish=true;
    assert(!ini.SaveIniFile(p) && read(p)==second);
    assert(ini.SaveIniFile(p) && read(p+".bak")==second);
    assert(std::remove(p.c_str())==0);
    CIniFile recovered(p); assert(recovered.GetString("SRLOADER","LANGUAGE","missing")=="Japanese");
    assert(recovered.SaveIniFile(p) && read(p).find("Japanese")!=std::string::npos);
    puts("INI short writes, failed publish rollback and backup recovery passed");
}
