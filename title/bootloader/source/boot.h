#ifndef _BOOT_H_
#define _BOOT_H_

#define resetMemory2_ARM9_size 0x400
void __attribute__ ((long_call)) __attribute__((naked)) __attribute__((noreturn)) resetMemory2_ARM9();
#define clearMasterBright_ARM9_size 0x200
void __attribute__ ((long_call)) __attribute__((naked)) __attribute__((noreturn)) clearMasterBright_ARM9();
void __attribute__ ((long_call)) __attribute__((naked)) __attribute__((noreturn)) limitedModeMemoryPit_ARM9();
#define holdMasterWhite_ARM9_size 0x80
void __attribute__((long_call, naked, noreturn)) holdMasterWhite_ARM9(void);
#define startBinary_ARM9_size 0x200
void __attribute__ ((long_call)) __attribute__((noreturn)) __attribute__((naked)) startBinary_ARM9 ();
#define ARM9_START_FLAG (*(vu8*)0x02FFFDFB)

#endif // _BOOT_H_
