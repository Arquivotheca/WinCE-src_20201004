;
; Copyright (c) Microsoft Corporation.  All rights reserved.
;
;
; Use of this source code is subject to the terms of the Microsoft shared
; source or premium shared source license agreement under which you licensed
; this source code. If you did not accept the terms of the license agreement,
; you are not authorized to use this source code. For the terms of the license,
; please see the license agreement between you and Microsoft or, if applicable,
; see the SOURCE.RTF on your install media or the root of your tools installation.
; THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
;
    .include "kxshx.h"

; Status register, SR, related defines

; FPU Disable bit is set to disable FPU
; There is the other copy defined in 
; %_WINCEROOT%\private\winceos\coreos\nk\inc\ksshx.h
; for used in *.C.  
; Please update both places should it be changed in the future!!!!!!!
SR_FPU_DISABLED             .equ        h'8000


; API call defines: cloned from syscall.h

FIRST_METHOD:               .equ        h'FFFFFE01
SYSCALL_RETURN_RAW:         .equ        FIRST_METHOD-2

METHOD_MASK:                .equ        h'00FF
APISET_MASK:                .equ        h'007F
APISET_SHIFT:               .equ        8

SYS_HANDLE_BASE:            .equ        64
SH_WIN32:                   .equ        0
SH_CURTHREAD:               .equ        1
SH_CURPROC:                 .equ        2

; SH4 MMU and exception registers

SH3CTL_BASE:                .equ        h'ff000000
MMUPTEH:                    .equ        h'00    ; MMU Page Table Entry High
MMUPTEL:                    .equ        h'04    ; MMU Page Table Entry Low
MMUPTEA:                    .equ        h'34    ; MMU Page Table Entry Low
MMUTTB:                     .equ        h'08    ; MMU Translation Table Base
MMUTEA:                     .equ        h'0c    ; MMU Translation Effective Address
MMUCR:                      .equ        h'10    ; MMU control register
CCR:                        .equ        h'1C    ; Cache Control register
TRPA:                       .equ        h'20    ; TRAPA code
EXPEVT:                     .equ        h'24    ; general exception event code
INTEVT:                     .equ        h'28    ; interrupt exception event code

;
; CCR bits
;
CACHE_OENABLE:              .equ        h'0001  ; enable operand cache
CACHE_P1WB:                 .equ        h'0004  ; enable write-back mode for P1 unmapped space 
CACHE_OFLUSH:               .equ        h'0008  ; invalidate entire operand cache (no write-backs)
CACHE_IENABLE:              .equ        h'0100  ; enable instruction cache
CACHE_IFLUSH:               .equ        h'0800  ; invalidate entire instruction cache
CACHE_ENABLE:               .equ        CACHE_OENABLE+CACHE_IENABLE+CACHE_P1WB
CACHE_FLUSH:                .equ        CACHE_OFLUSH+CACHE_IFLUSH

;
; MMUCR bits 
;
TLB_ENABLE:                 .equ        h'0001  ; set to enable address translation
TLB_FLUSH:                  .equ        h'0004  ; set to flush the TLB

;*
;* KDataStruct offsets
;*
USER_PPCB:                  .equ        h'd800
USER_KPAGE:                 .equ        h'5800
UPCB_SHR12:                 .equ        h'd     ; USER_PPCB >> 12 = 0xd
INTERLOCKED_START:          .equ        USER_KPAGE+h'380
INTERLOCKED_END:            .equ        USER_KPAGE+h'400

lpvTls:                     .equ        h'000   ; Current thread local storage pointer
bResched:                   .equ        h'004   ; reschedule flag
cNest:                      .equ        h'005   ; kernel exception nesting
fIdle:                      .equ        h'006   ; is the CPU idle?
hCurThread:                 .equ        h'008   ; SH_CURTHREAD==1
hCurProc:                   .equ        h'00c   ; SH_CURPROC==2
dwKCRes:                    .equ        h'018   ; resched needed?
pVMPrc:                     .equ        h'01c   ; current VM in effect
pCurPrc:                    .equ        h'020   ; pointer to current PROCESS structure
pCurThd:                    .equ        h'024   ; pointer to current THREAD structure
ownspinlock:                .equ        h'028   ; - ownspinlock
pCurFPUOwner:               .equ        h'02c   ; current FPU owner
PendEvents1:                .equ        h'030   ; - low (int 0-31) dword of interrupts pending (must be 8-byte aligned)
PendEvents2:                .equ        h'034   ; - high (int 32-63) dword of interrupts pending
pKStk:                      .equ        h'038   ; ptr to per CPUKStack
pthSched:                   .equ        h'03c   ; - new thread scheduled.
dwTlbMissCnt:               .equ        h'04c   ; # of TLB misses
dwSyscallReturnTrap:        .equ        h'050   ; randomized return address for PSL return
dwPslTrapSeed:              .equ        h'054   ; random seed for PSL Trap address

bPowerOff:                  .equ        h'086
bProfileOn:                 .equ        h'087
nCpus:                      .equ        h'088
pAPIReturn:                 .equ        h'098   ; direct API return address for kernel mode
dwInDebugger:               .equ        h'0a0   ; !0 when in debugger
dwTOCAddr:                  .equ        h'0a4   ; address of pTOC
dwStartUpAddr:              .equ        h'0a8   ; startup address (for soft reset handler)
pOem:                       .equ        h'0ac   ; ptr to OEM globals
pNk:                        .equ        h'0b0   ; ptr to NK globals
CeLogStatus:                .equ        h'0b4   ; celog status
alpeIntrEvents:             .equ        h'1a0

;
; spinlock fields
;
owner_cpu:                  .equ        0       ; owner_cpu
nestcount:                  .equ        4       ; nestcount
next_owner:                 .equ        8       ; next_owner

;*
;* Process structure fields
;*
PrcID:                      .equ        0
PrcHandle:                  .equ        h'0c
PrcVMBase:                  .equ        h'2c

;*
;* IPC Call Stack structure fields
;*
CstkArgs:                   .equ        0
CstkNext:                   .equ        h'38
CstkRa:                     .equ        h'3c
CstkPrevSP:                 .equ        h'40
CstkPrcInfo:                .equ        h'44
CstkPrcLast:                .equ        h'48
CstkPrcVM:                  .equ        h'4c
CstkPhd:                    .equ        h'50
CstkOldTls:                 .equ        h'54
CstkMethod:                 .equ        h'58
CstkNewSP:                  .equ        h'5c
Cstk_REG_R8:                .equ        h'60
Cstk_REG_R9:                .equ        h'64
Cstk_REG_R10:               .equ        h'68
Cstk_REG_R11:               .equ        h'6c
Cstk_REG_R12:               .equ        h'70
Cstk_REG_R13:               .equ        h'74
Cstk_REG_R14:               .equ        h'78
CstkExtra:                  .equ        h'7c
CstkSizeof:                 .equ        h'80

;* Thread structure fields

ThwInfo:                    .equ        h'0c
ThPcstkTop:                 .equ        h'18
ThTlsPtr:                   .equ        h'1c
ThTlsSecure:                .equ        h'20
ThTlsNonSecure:             .equ        h'24
ThHandle:                   .equ        h'28

THREAD_CONTEXT_OFFSET:      .equ        h'60

CtxContextFlags:            .equ        h'00
CtxPR:                      .equ        h'04
CtxMACH:                    .equ        h'08
CtxMACL:                    .equ        h'0c
CtxGBR:                     .equ        h'10
CtxR0:                      .equ        h'14
CtxR1:                      .equ        h'18
CtxR2:                      .equ        h'1c
CtxR3:                      .equ        h'20
CtxR4:                      .equ        h'24
CtxR5:                      .equ        h'28
CtxR6:                      .equ        h'2c
CtxR7:                      .equ        h'30
CtxR8:                      .equ        h'34
CtxR9:                      .equ        h'38
CtxR10:                     .equ        h'3c
CtxR11:                     .equ        h'40
CtxR12:                     .equ        h'44
CtxR13:                     .equ        h'48
CtxR14:                     .equ        h'4c
CtxR15:                     .equ        h'50
CtxFir:                     .equ        h'54
CtxPsr:                     .equ        h'58

;*
;* CONTEXT_FULL is also defined in \public\common\sdk\inc\winnt.h.
;* For SH3 DSP, it includes not only control registers, interger
;* registers, debug registers, but also floating point registers.
;*

CONTEXT_SEH:                .equ        h'c3    ; CONTEXT_INTEGER|CONTEXT_CONTROL
CONTEXT_FLOATING_POINT:     .equ        h'c4    ; (CONTEXT_SH4 | 0x00000004L)
CONTEXT_FULL:               .equ        h'cf

CtxFpscr:                   .equ        h'5c
CtxFpul:                    .equ        h'60
CtxFRegs:                   .equ        h'64
CtxXFRegs:                  .equ        h'A4

CtxFPSizeof:                .equ        h'88
CtxSizeof:                  .equ        h'E4


; Dispatcher Context Structure Offset Definitions

DcControlPc:                .equ        h'0
DcFunctionEntry:            .equ        h'4
DcEstablisherFrame:         .equ        h'8
DcContextRecord:            .equ        h'c

; Exception Record Offset, Flag, and Enumerated Type Definitions

EXCEPTION_NONCONTINUABLE:   .equ        h'1
EXCEPTION_UNWINDING:        .equ        h'2
EXCEPTION_EXIT_UNWIND:      .equ        h'4
EXCEPTION_STACK_INVALID:    .equ        h'8
EXCEPTION_NESTED_CALL:      .equ        h'10
EXCEPTION_TARGET_UNWIND:    .equ        h'20
EXCEPTION_COLLIDED_UNWIND:  .equ        h'40
EXCEPTION_UNWIND:           .equ        h'66

ExceptionContinueExecution: .equ        h'0
ExceptionContinueSearch:    .equ        h'1
ExceptionNestedException:   .equ        h'2
ExceptionCollidedUnwind:    .equ        h'3

; size of exception record, MUST MATCH sizeof(EXCEPTION_RECORD)
ExrSizeof:                  .equ        h'50

ErExceptionCode:            .equ        h'0
ErExceptionFlags:           .equ        h'4
ErExceptionRecord:          .equ        h'8
ErExceptionAddress:         .equ        h'c
ErNumberParameters:         .equ        h'10
ErExceptionInformation:     .equ        h'14
ExceptionRecordLength:      .equ        h'50

; Offsets of the components of a virtual address
VM_PAGE_SHIFT:              .equ        12      ; 4K pages
;-------------------------------------------------------------------------------
; PTEL register bits (for 4K page, bit 7 = 0, bit 4 = 1 -> 4K page)
; 31 30 29 28 -------------------------- 11 10 9  8  7  6+5 4 3 2  1  0
;                   PPN                   0  0 0  V  0  PR  1 C D SH WT
;-------------------------------------------------------------------------------
PG_VALID_MASK:              .equ        h'100   ; PTE valid bit
USERKPAGE_PROTECTION:       .equ        h'15a   

;* NK interrupt defines

SYSINTR_NOP:                .equ        0
SYSINTR_RESCHED:            .equ        1
SYSINTR_BREAK:              .equ        2
SYSINTR_IPI:                .equ        4
SYSINTR_DEVICES:            .equ        8
SYSINTR_MAX_DEVICES:        .equ        64

SYSINTR_FIRMWARE:           .equ        SYSINTR_DEVICES+16
SYSINTR_MAXIMUM:            .equ        SYSINTR_DEVICES+SYSINTR_MAX_DEVICES

REGSIZE:                    .equ 4

;
; macros
;
        .macro  MOVLI_R4_R0
        .data.w h'0463                  ; movli.l @r4, r0
        .endm

        .macro  MOVCO_R0_R4
        .data.w h'0473                  ; movco.l r0, @r4
        .endm


