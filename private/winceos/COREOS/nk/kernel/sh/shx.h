//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#ifndef __SHX_H__
#define __SHX_H__

// Status register, SR, related defines

// DSP option mode(bit 12 of SR) is set to enable DSP
// There is the other copy defined in 
// %_WINCEROOT%\private\winceos\coreos\nk\inc\ksshx.h
// for used in *.SRC.  
// Please update both places should it be changed in the future!!!!!!
#define SR_DSP_ENABLED  0x1000


#define REG32BIT(x) (*(volatile unsigned long *)(x))
#define REG16BIT(x) (*(volatile unsigned short *)(x))
#define REG8BIT(x)  (*(volatile unsigned char *)(x))

// SH4 MMU control & exception registers
#define SH3CTL_BASE 0xff000000
#define MMUPTEH REG32BIT(SH3CTL_BASE+0x00)
#define MMUPTEL REG32BIT(SH3CTL_BASE+0x04)
#define MMUPTEA REG32BIT(SH3CTL_BASE+0x34)
#define MMUTTB  REG32BIT(SH3CTL_BASE+0x08)
#define MMUTEA  REG32BIT(SH3CTL_BASE+0x0c)
#define MMUCR   REG32BIT(SH3CTL_BASE+0x10)
#define CCR     REG32BIT(SH3CTL_BASE+0x1C)

#define TRPA    REG32BIT(SH3CTL_BASE+0x20)
#define EXPEVT  REG32BIT(SH3CTL_BASE+0x24)
#define INTEVT  REG32BIT(SH3CTL_BASE+0x28)

#define CACHE_OENABLE       0x001   /* enable operand cache */
#define CACHE_WRITETHRU     0x002   /* write-thru mode default for mapped spaces */
#define CACHE_P1WB          0x004   /* enable write-back mode for P1 unmapped space */
#define CACHE_OFLUSH        0x008   /* invalidate entire operand cache (no write-backs) */
#define CACHE_IENABLE       0x100   /* enable instruction cache */
#define CACHE_IFLUSH        0x800   /* invalidate entire instruction cache */

#define CACHE_ENABLE    (CACHE_OENABLE | CACHE_IENABLE | CACHE_P1WB)
#define CACHE_FLUSH     (CACHE_OFLUSH | CACHE_IFLUSH)

#define TLB_ENABLE      0x00000001  /* set to enable address translation */
#define TLB_FLUSH       0x00000004  /* set to flush the TLB */
#define TLB_URC         0x0000FC00  /* random counter (way selector) */
#define TLB_URB         0x00FC0000  /* replacement boundary */

// SH4 interrupt controller registers
#define INTC_CR     REG16BIT(0xffd00000)    // interrupt control register
#define INTC_PRA    REG16BIT(0xffd00004)    // intr priority level A
#define INTC_PRB    REG16BIT(0xffd00008)    // intr priority level B
#define INTC_PRC    REG16BIT(0xffd0000C)    // intr priority level C

// SH4 Timer unit registers

#define TMUADDR ((volatile struct TMU_REGS *)0xffd80000)
#define TMUPHYS ((void *)0x1fd80000)

struct TMU_REGS {
    BYTE    tocr;       // timer output control (8 bit)
    BYTE    pad1[3];
    BYTE    tstr;       // timer start register (8 bit)
    BYTE    pad2[3];
    DWORD   tcor0;      // timer constant 0 (32 bit)
    DWORD   tcnt0;      // timer count 0 (32 bit)
    WORD    tcr0;       // timer control 0 (16 bit)
    WORD    pad3;
    DWORD   tcor1;      // timer constant 1 (32 bit)
    DWORD   tcnt1;      // timer count 1 (32 bit)
    WORD    tcr1;       // timer control 1 (16 bit)
    WORD    pad4;
    DWORD   tcor2;      // timer constant 2 (32 bit)
    DWORD   tcnt2;      // timer count 2 (32 bit)
    WORD    tcr2;       // timer control 2 (16 bit)
    WORD    pad5;
    DWORD   tcpr2;      // input capture 2 (32 bit)
};

// Bits for TSTR:
#define TMU_START0  0x01
#define TMU_START1  0x02
#define TMU_START2  0x04

// Bits for TCRn:
#define TMUCR_UNF   0x100   // counter underflowed
#define TMUCR_UNIE  0x20    // underflow interrupt enable

#define TMUCR_RISE  0x00    // count on rising edge of clock
#define TMUCR_FALL  0x08    // count on falling edge of clock
#define TMUCR_BOTH  0x10    // count on both edges of clock

#define TMUCR_D4    0x00    // PERIPHERAL clock / 4
#define TMUCR_D16   0x01    // PERIPHERAL clock / 16
#define TMUCR_D64   0x02    // PERIPHERAL clock / 64
#define TMUCR_D256  0x03    // PERIPHERAL clock / 256
#define TMUCR_D1024 0x04    // PERIPHERAL clock / 1024
#define TMUCR_RTC   0x06    // real time clock output (16 kHz)
#define TMUCR_EXT   0x07    // external clock input

#endif

