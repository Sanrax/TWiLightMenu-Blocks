@ SPDX-License-Identifier: GPL-2.0-or-later
.syntax unified
.cpu arm7tdmi
.thumb
.section .text.hooks,"ax",%progbits

@ Replace exactly two original instructions, preserving all other registers.
@ r2 and r3 were overwritten at each site anyway. Stack remains 8-byte aligned.
.global before_clear
.type before_clear,%function
.thumb_func
before_clear:
    push {r0,r1,r4,lr}
    mov r4,ip
    movs r0,#0
    bl capture
    mov ip,r4
    pop {r0,r1,r4}
    ldr r3,=0x06030A4C
    ldr r2,=0x04FFFA10
    pop {pc}
.size before_clear,.-before_clear

.global after_clear
.type after_clear,%function
.thumb_func
after_clear:
    push {r0,r1,r4,lr}
    mov r4,ip
    movs r0,#1
    bl capture
    mov ip,r4
    pop {r0,r1,r4}
    movs r3,#0
    ldr r2,=0x04000210
    pop {pc}
.size after_clear,.-after_clear
.pool
