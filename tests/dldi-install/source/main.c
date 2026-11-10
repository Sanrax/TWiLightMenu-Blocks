#include <nds.h>
#include <nds/arm9/dldi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include TWLM_STORAGE_SOURCE
#include "drivers.h"

static bool checkDriver(const u8 *source,size_t size) {
    u8 *image=malloc(size);
    if(!image)return false;
    memcpy(image,source,size);image[0]^=0xFF;
    DLDI_INTERFACE original=*(DLDI_INTERFACE*)image;
    if(cache_init(40)||!twlmInstallDldi(image,size)){free(image);return false;}
    uintptr_t delta=(uintptr_t)io_dldi_data-(uintptr_t)original.dldiStart;
    bool ok=io_dldi_data->dldiStart==(void*)io_dldi_data;
#define CHECK_FN(fn) ok &= (uintptr_t)io_dldi_data->ioInterface.fn==(uintptr_t)original.ioInterface.fn+delta
    CHECK_FN(startup);CHECK_FN(isInserted);CHECK_FN(readSectors);CHECK_FN(writeSectors);CHECK_FN(clearStatus);CHECK_FN(shutdown);
#undef CHECK_FN
    if(original.fixSectionsFlags&FIX_BSS)
        for(u8*p=io_dldi_data->bssStart;p<(u8*)io_dldi_data->bssEnd;p++)if(*p)ok=false;
    free(image);return ok;
}

static bool executableInstall(unsigned result) {
    u8 image[256]={};DLDI_INTERFACE*d=(DLDI_INTERFACE*)image;
    d->magicNumber=DLDI_MAGIC_NUMBER;memcpy(d->magicString," Chishm",8);
    d->versionNumber=1;d->driverSize=8;d->dldiStart=(void*)0xBF800000;d->dldiEnd=(void*)0xBF800088;
    d->ioInterface.startup=(FN_MEDIUM_STARTUP)0xBF800080;
    const u32 code[]={0xE3A00000|result,0xE12FFF1E};memcpy(image+128,code,sizeof code);
    return twlmInstallDldi(image,136)&&io_dldi_data->ioInterface.startup()==(bool)result;
}

int main(void) {
    consoleDemoInit();puts("SDK DLDI install checks");bool ok=true;
    for(unsigned i=0;i<sizeof drivers/sizeof drivers[0];i++) {
        bool pass=checkDriver(drivers[i].data,drivers[i].size);printf("%s: %s\n",drivers[i].name,pass?"PASS":"FAIL");ok&=pass;
    }
    bool code=executableInstall(1)&&executableInstall(0);printf("Replace/execute: %s\n",code?"PASS":"FAIL");ok&=code;
    puts(ok?"ALL PASSED":"FAILED");while(1)swiWaitForVBlank();
}
