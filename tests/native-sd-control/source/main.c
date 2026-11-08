#include <nds.h>
#include <fat.h>
#include <stdio.h>
#include <string.h>
static unsigned char source[4096],check[4096];
int main(void) {
    consoleDemoInit();
    printf("Native SDK SD control\nDSi mode: %d\nMounting...\n",isDSiMode());
    if(!fatInitDefault()) {puts("Mount failed");goto done;}
    puts("Mounted; writing...");
    for(unsigned i=0;i<sizeof(source);i++)source[i]=(i*17)^0x53;
    for(unsigned loop=0;loop<16;loop++) {
        FILE *f=fopen("sd:/native-sd-probe.bin","wb");
        if(!f) {puts("Open write failed");goto done;}
        if(fwrite(source,1,sizeof(source),f)!=sizeof(source)||fclose(f)) {puts("Write failed");goto done;}
        f=fopen("sd:/native-sd-probe.bin","rb");
        if(!f || fread(check,1,sizeof(check),f)!=sizeof(check) || fclose(f) || memcmp(source,check,sizeof(source))) {puts("Readback failed");goto done;}
        printf("Read/write pass %u\n",loop+1);
    }
    puts("PASSED");
 done:while(1)swiWaitForVBlank();
}
