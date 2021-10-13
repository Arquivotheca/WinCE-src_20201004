;
; Copyright (c) Microsoft Corporation.  All rights reserved.
;
;
; This source code is licensed under Microsoft Shared Source License
; Version 1.0 for Windows CE.
; For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
HANDLE_MASK: .equ h'003F
HANDLE_SHIFT: .equ 8

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
cMsec: .equ h'088   ; # of milliseconds since boot
cDMsec: .equ h'08c  ; # of mSec since last TimerCallBack
pCurPrc: .equ h'090 ; pointer to current PROCESS structure
pCurThd: .equ h'094 ; pointer to current THREAD structure
dwKCRes: .equ h'098
hBase: .equ h'09c   ; handle table base address
aSections: .equ h'0a0   ; section table for virutal memory
alpeIntrEvents: .equ h'1a0
alpvIntrData: .equ h'220
bIntrIndexLow: .equ h'2a4
bIntrIndexHigh: .equ h'2a8
bIntrNumber: .equ h'2ac
g_CurFPUOwner: .equ h'2cc   ; SH4 only
g_CurDSPOwner: .equ h'2cc   ; SH3(DSP) only
PendEvents: .equ h'340
CeLogStatus: .equ h'368     ; offset of KInfoTable[KINX_CELOGSTATUS] from KData

; trust level
KERN_TRUST_FULL: .equ   2

;*
;* Process structure fields
;*
PrcID:     .equ 0
PrcTrust:  .equ 3       
PrcHandle: .equ h'08
PrcVMBase: .equ h'0c

;*
;* IPC Call Stack structure fields
;*
CstkNext:       .equ    0
CstkRa:         .equ    4
CstkPrcLast:    .equ    8
CstkAkyLast:    .equ    12
CstkExtra:      .equ    16
CstkPrevSP:     .equ    20
CstkPrcInfo:    .equ    24
CstkSizeof:     .equ    28

;* Thread structure fields

ThwInfo:        .equ    h'0
ThProc:         .equ    h'0c
ThAKey:         .equ    h'14
ThPcstkTop:     .equ    h'18
ThTlsPtr:       .equ    h'24
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

ErExceptionCode: .equ h'0
ErExceptionFlags: .equ h'4
ErExceptionRecord: .equ h'8
ErExceptionAddress: .equ h'c
ErNumberParameters: .equ h'10
ErExceptionInformation: .equ h'14
ExceptionRecordLength: .equ h'50

; Offsets of the components of a virtual address
VA_PAGE:    .equ 12 ;;  4K pages
VA_BLOCK:   .equ 16
VA_SECTION: .equ 25

PAGE_MASK: .equ h'00F
PAGE_MASK4: .equ h'03C

BLOCK_MASK: .equ h'1ff
SECTION_MASK: .equ h'03F

; MemBlock structure layout
mb_lock: .equ 0
mb_uses: .equ 4
mb_ixBase: .equ 6
mb_hPf: .equ    8
mb_pages: .equ 12

VERIFY_WRITE_FLAG: .equ   1
VERIFY_KERNEL_OK: .equ    2

;* NK interrupt defines

SYSINTR_NOP: .equ 0
SYSINTR_RESCHED: .equ 1
SYSINTR_BREAK: .equ 2
SYSINTR_DEVICES: .equ 8
SYSINTR_MAX_DEVICES: .equ   32

SYSINTR_FIRMWARE: .equ  SYSINTR_DEVICES+16
SYSINTR_MAXIMUM: .equ   SYSINTR_DEVICES+SYSINTR_MAX_DEVICES

