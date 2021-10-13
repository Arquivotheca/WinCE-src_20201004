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
; THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
;
    .include "kxshx.h"

; Status register, SR, related defines

; Mask for DSP specific bits in the status register (SR)
; DSP related bits are: RC(16-27), DSP(12), DMY(11), DMX(10), RF1(3), RF0(2), and S(1).
SR_DSP_MASK:    .equ    h'0FFF1C0E

; DSP option mode is on in SR
; There is the other copy defined in 
; %_WINCEROOT%\private\winceos\coreos\nk\kernel\shx\shx.h
; for used in *.C.  
; Please update both places should it be changed in the future!!!!!!
SR_DSP_ENABLED  .equ    h'1000

; FPU Disable bit is set to disable FPU
; There is the other copy defined in 
; %_WINCEROOT%\private\winceos\coreos\nk\inc\ksshx.h
; for used in *.C.  
; Please update both places should it be changed in the future!!!!!!!
SR_FPU_DISABLED .equ    h'8000


; API call defines: cloned from syscall.h

FIRST_METHOD: .equ h'FFFFFE01
SYSCALL_RETURN: .equ FIRST_METHOD-2

METHOD_MASK: .equ h'00FF
APISET_MASK: .equ h'007F
APISET_SHIFT: .equ 8

NUM_SYS_HANDLES: .equ 32
SYS_HANDLE_BASE: .equ 64
SH_WIN32: .equ 0
SH_CURTHREAD: .equ 1
SH_CURPROC: .equ 2

; a few Win32 error codes:
ERROR_INVALID_FUNCTION: .equ 1
ERROR_INVALID_HANDLE: .equ  6
ERROR_INVALID_ADDRESS: .equ 487

  .aif SH_CPU eq h'40
; SH4 MMU and exception registers

SH3CTL_BASE: .equ   h'ff000000
MMUPTEH: .equ   h'00    ; MMU Page Table Entry High
MMUPTEL: .equ   h'04    ; MMU Page Table Entry Low
MMUPTEA: .equ   h'34    ; MMU Page Table Entry Low
MMUTTB: .equ    h'08    ; MMU Translation Table Base
MMUTEA: .equ    h'0c    ; MMU Translation Effective Address
MMUCR:  .equ    h'10    ; MMU control register
CCR:    .equ    h'1C    ; Cache Control register
TRPA: .equ      h'20    ; TRAPA code
EXPEVT: .equ    h'24    ; general exception event code
INTEVT: .equ    h'28    ; interrupt exception event code

  .aendi

  .aif (SH_CPU/16) eq 3
; SH3 MMU and exception registers

SH3CTL_BASE: .equ   h'ffffffd0
TRPA: .equ      h'00    ; TRAPA code
EXPEVT: .equ    h'04    ; general exception event code
INTEVT: .equ    h'08    ; interrupt exception event code
MMUCR:  .equ    h'10    ; MMU control register
UBCASA: .equ    h'14    ; Breakpoint ASID A
UBCASB: .equ    h'18    ; Breakpoint ASID B
CCR:    .equ    h'1C    ; Cache Control register
MMUPTEH: .equ   h'20    ; MMU Page Table Entry High
MMUPTEL: .equ   h'24    ; MMU Page Table Entry Low
MMUTTB: .equ    h'28    ; MMU Translation Table Base
MMUTEA: .equ    h'2c    ; MMU Translation Effective Address

; SH3 User Breakpoint Unit registers

UBC_BASE: .equ  h'ffffff90
UBCBARA: .equ   h'20    ; address A (32 bit)
UBCBAMRA: .equ  h'24    ; address mask A (8 bit)
UBCBBRA: .equ   h'28    ; bus cycle A (16 bit)
; ASID registers in SH3 MMU set
UBCBARB: .equ   h'10    ; address B (32 bit)
UBCBAMRB: .equ  h'14    ; address mask B (8 bit)
UBCBBRB: .equ   h'18    ; bus cycle B (16 bit)
; ASID registers in SH3 MMU set
UBCBDRB: .equ   h'00    ; data register B (32 bit)
UBCBDMRB: .equ  h'04    ; data mask B (32 bit)
UBCBRCR: .equ   h'08    ; break control register (16 bit)
    .aendi

;*
;* KDataStruct offsets
;*

USER_KPAGE: .equ h'5800
USER_KPAGE_SHR10: .equ   h'16
INTERLOCKED_START: .equ USER_KPAGE+h'380
INTERLOCKED_END: .equ USER_KPAGE+h'400

lpvTls: .equ h'000  ; Current thread local storage pointer
ahSys: .equ h'004 ; system handle array
hCurThread: .equ h'008   ; SH_CURTHREAD==1
hCurProc: .equ h'00c     ; SH_CURPROC==2
bResched: .equ h'084    ; reschedule flag
cNest: .equ h'085   ; kernel exception nesting
bPowerOff: .equ h'086
bProfileOn: .equ h'087
dwKCRes: .equ h'088   ; resched needed?
pVMPrc: .equ h'08c  ; current VM in effect
pCurPrc: .equ h'090 ; pointer to current PROCESS structure
pCurThd: .equ h'094 ; pointer to current THREAD structure
pAPIReturn: .equ h'098 ; direct API return address for kernel mode
nMemForPT: .equ h'09c   ; Memory used for PageTables
dwInDebugger: .equ h'0a0 ; !0 when in debugger
dwTOCAddr: .equ h'0a4 ; address of pTOC
dwStartUpAddr: .equ h'0a8 ; startup address (for soft reset handler)
pOem: .equ h'0ac ; ptr to OEM globals
pNk: .equ h'0b0 ; ptr to NK globals
g_CurFPUOwner: .equ h'0b4 ; current FPU owner
PendEvents1: .equ h'0b8 ; - low (int 0-31) dword of interrupts pending (must be 8-byte aligned)
PendEvents2: .equ h'0bc ; - high (int 32-63) dword of interrupts pending
unused: .equ h'0c0 ; unused
dwTlbMissCnt: .equ h'198 ; # of TLB misses
CeLogStatus: .equ h'19c ; celog status
alpeIntrEvents: .equ h'1a0

; trust level
KERN_TRUST_FULL: .equ   2

;*
;* Process structure fields
;*
PrcID:     .equ 0
PrcTrust:  .equ h'53       
PrcHandle: .equ h'0c
PrcVMBase: .equ h'2c

;*
;* IPC Call Stack structure fields
;*
CstkArgs: .equ 0
CstkNext: .equ h'38
CstkRa: .equ h'3c
CstkPrevSP: .equ h'40
CstkPrcInfo: .equ h'44
CstkPrcLast: .equ h'48
CstkPrcVM: .equ h'4c
CstkPhd: .equ h'50
CstkOldTls: .equ h'54
CstkMethod: .equ h'58
CstkNewSP: .equ h'5c
Cstk_REG_R8: .equ h'60
Cstk_REG_R9: .equ h'64
Cstk_REG_R10: .equ h'68
Cstk_REG_R11: .equ h'6c
Cstk_REG_R12: .equ h'70
Cstk_REG_R13: .equ h'74
Cstk_REG_R14: .equ h'78
CstkExtra: .equ h'7c
CstkSizeof: .equ h'80

;* Thread structure fields

ThwInfo:        .equ    h'0c
ThProc:         .equ    h'10
ThPcstkTop:     .equ    h'1c
ThTlsPtr:       .equ    h'28
ThTlsSecure:    .equ    h'2c
ThTlsNonSecure: .equ    h'30
ThHandle:       .equ    h'3c

THREAD_CONTEXT_OFFSET: .equ h'60

CtxContextFlags: .equ h'00
CtxPR: .equ h'04
CtxMACH: .equ h'08
CtxMACL: .equ h'0c
CtxGBR: .equ h'10
CtxR0: .equ h'14
CtxR1: .equ h'18
CtxR2: .equ h'1c
CtxR3: .equ h'20
CtxR4: .equ h'24
CtxR5: .equ h'28
CtxR6: .equ h'2c
CtxR7: .equ h'30
CtxR8: .equ h'34
CtxR9: .equ h'38
CtxR10: .equ h'3c
CtxR11: .equ h'40
CtxR12: .equ h'44
CtxR13: .equ h'48
CtxR14: .equ h'4c
CtxR15: .equ h'50
CtxFir: .equ h'54
CtxPsr: .equ h'58

  .aif SH_CPU eq h'30

;*
;* CONTEXT_FULL is also defined in \public\common\sdk\inc\winnt.h.
;* For SH3 DSP, it includes not only control registers, interger
;* registers, debug registers, but also DSP registers.
;*

CONTEXT_FULL: .equ h'5b



CtxOldStuff:  .equ h'5c
CtxDbgRegs:   .equ h'64

CtxDsr:       .equ h'80
CtxMod:       .equ h'84
CtxRS:        .equ h'88
CtxRE:        .equ h'8c
CtxDSPRegs:   .equ h'90

CtxDSPSizeof: .equ h'34
CtxSizeof:    .equ h'B4
  .aendi

  .aif SH_CPU eq h'40
;*
;* CONTEXT_FULL is also defined in \public\common\sdk\inc\winnt.h.
;* For SH3 DSP, it includes not only control registers, interger
;* registers, debug registers, but also floating point registers.
;*

CONTEXT_FULL: .equ h'cf

CtxFpscr: .equ h'5c
CtxFpul: .equ h'60
CtxFRegs: .equ h'64
CtxXFRegs: .equ h'A4

CtxFPSizeof: .equ h'88
CtxSizeof: .equ h'E4
  .aendi

; Dispatcher Context Structure Offset Definitions

DcControlPc: .equ h'0
DcFunctionEntry: .equ h'4
DcEstablisherFrame: .equ h'8
DcContextRecord: .equ h'c

; Exception Record Offset, Flag, and Enumerated Type Definitions

EXCEPTION_NONCONTINUABLE: .equ h'1
EXCEPTION_UNWINDING: .equ h'2
EXCEPTION_EXIT_UNWIND: .equ h'4
EXCEPTION_STACK_INVALID: .equ h'8
EXCEPTION_NESTED_CALL: .equ h'10
EXCEPTION_TARGET_UNWIND: .equ h'20
EXCEPTION_COLLIDED_UNWIND: .equ h'40
EXCEPTION_UNWIND: .equ h'66

ExceptionContinueExecution: .equ h'0
ExceptionContinueSearch: .equ h'1
ExceptionNestedException: .equ h'2
ExceptionCollidedUnwind: .equ h'3

; size of exception record, MUST MATCH sizeof(EXCEPTION_RECORD)
ExrSizeof:   .equ h'50

ErExceptionCode: .equ h'0
ErExceptionFlags: .equ h'4
ErExceptionRecord: .equ h'8
ErExceptionAddress: .equ h'c
ErNumberParameters: .equ h'10
ErExceptionInformation: .equ h'14
ExceptionRecordLength: .equ h'50

; Offsets of the components of a virtual address
VA_PAGE:    .equ 12 ;;  4K pages
VA_SECTION: .equ 25

BLOCK_MASK: .equ h'1ff

VERIFY_WRITE_FLAG: .equ   1
VERIFY_KERNEL_OK: .equ    2

;* NK interrupt defines

SYSINTR_NOP: .equ 0
SYSINTR_RESCHED: .equ 1
SYSINTR_BREAK: .equ 2
SYSINTR_DEVICES: .equ 8
SYSINTR_MAX_DEVICES: .equ   64

SYSINTR_FIRMWARE: .equ  SYSINTR_DEVICES+16
SYSINTR_MAXIMUM: .equ   SYSINTR_DEVICES+SYSINTR_MAX_DEVICES

REGSIZE: .equ 4

