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
    INCLUDE kxarm.h

; API call defines: cloned from psyscall.h

FIRST_METHOD                EQU     0xF1020000
MAX_METHOD_NUM              EQU     0xF1020400      ; exactly value is 0xF10203FC. 
SYSCALL_RETURN_RAW          EQU     FIRST_METHOD-4

METHOD_MASK                 EQU     0x00FF
APISET_MASK                 EQU     0x007F
APISET_SHIFT                EQU     8

SYS_HANDLE_BASE             EQU     64
SH_WIN32                    EQU     0
SH_CURTHREAD                EQU     1
SH_CURPROC                  EQU     2

;
; The architecture version is read to decide whether to use the
; new page table format provided in V6 onwards.

; -- ARM Architectures Versions
;
; CP15:r0:bits[19:16]=architecture version
;
ARMv5                       EQU     0x5
ARMv6                       EQU     0x7
ARMv7                       EQU     0xF
;
; section mapped defs
;
BANK_SHIFT                  EQU     20      ; each section = 1M


; Page table bits
; Cacheable/Bufferable are always at the same bit position
CACHEABLE                   EQU     8
BUFFERABLE                  EQU     4

; Basic page table entry types
PTL1_2Y_TABLE               EQU     1       ; top-level entry points at second-level table
PTL1_SECTION                EQU     2       ; top-level entry maps a 1MB section directly
PTL1_XN                     EQU     0       ; No execution control on pre-V6

PTL2_LARGE_PAGE             EQU     1
PTL2_SMALL_PAGE             EQU     2

; Common access attribute settings
PTL1_KRW                    EQU     0x400   ; bits 10, 11
PTL1_KRW_URO                EQU     0x800
PTL1_RW                     EQU     0xc00
PTL2_KRW                    EQU     0x10
PTL2_KRW_URO                EQU     0x20
PTL2_RW                     EQU     0x30

; Values to use for V6 format tables
; eXecute-Never bits.
ARMV6_MMU_PTL1_XN           EQU     1 <<  4
ARMV6_MMU_PTL2_LARGE_XN     EQU     1 << 15
ARMV6_MMU_PTL2_SMALL_XN     EQU     1 <<  0

; V6 ro settings are explicit rather than using the CP15 R1 R bit.
; Note that these are more restrictive than the non-V6 settings: they deny user-mode access.
ARMV6_MMU_PTL1_KR0          EQU     0x8400  ; bits 11,10=01, APX(b15)=1
ARMV6_MMU_PTL2_KR0          EQU     0x210   ; bits  5, 4=01, APX(b9 )=1

ARMV6_MMU_ASID_ADDR_SHIFT   EQU     25      ; there are 128 slots covering the 32-bit address space.
                                            ; Therefore shift by 32-7=25 to convert between
                                            ; (simple slot number) ASID and process base address

; Values to use for old format page tables
PREARMV6_MMU_PTL1_XN        EQU     0       ; No execution control on pre-V6
PREARMV6_MMU_PTL2_SMALL_XN  EQU     0
PREARMV6_MMU_PTL1_KR0       EQU     0x000   ; bits 10,11=00 : R bit set in CP15
PREARMV6_MMU_PTL2_KR0       EQU     0x00    ; bits  5, 4=00 : R bit set in CP15

;
; V6 specific bits in control register
;
ARMV6_A_BIT                 EQU     1 << 1  ; bit 1 is A bit
ARMV6_U_BIT                 EQU     1 << 22 ; bit 22 is U bit

TTBR_CTRL_BIT_MASK          EQU     0xff    ; bottom 8 bits

INVALID_ASID                EQU     0xff    ; special ASID that is never used by any process

; ARM processor modes
USER_MODE                   EQU     2_10000
FIQ_MODE                    EQU     2_10001
IRQ_MODE                    EQU     2_10010
SVC_MODE                    EQU     2_10011
ABORT_MODE                  EQU     2_10111
UNDEF_MODE                  EQU     2_11011
SYSTEM_MODE                 EQU     2_11111
MODE_MASK                   EQU     2_11111
THUMB_STATE                 EQU     0x0020
EXEC_STATE_MASK3            EQU     0x07000000 ; ITSTATE_MASK_HIGH | JAZELLE_STATE
EXEC_STATE_MASK1            EQU     0x0000FE00 ; ITSTATE_MASK_LOW | BIG_ENDIAN_STATE
EXEC_STATE_MASK0            EQU     THUMB_STATE

; ID codes for HandleException
ID_RESCHEDULE               EQU     0
ID_UNDEF_INSTR              EQU     1
ID_SWI_INSTR                EQU     2
ID_PREFETCH_ABORT           EQU     3
ID_DATA_ABORT               EQU     4
ID_IRQ                      EQU     5

;*
;* KDataStruct offsets
;*

USER_KPAGE                  EQU     0xFFFFC800
ADDR_NUM_CPUS               EQU     USER_KPAGE+0x088

INTERLOCKED_START           EQU     USER_KPAGE+0x380

PCB_GETDWORD                EQU     USER_KPAGE+0x380
INTLK_EXCHG                 EQU     USER_KPAGE+0x3a0
INTLK_CMPCHG                EQU     USER_KPAGE+0x3C0
INTLK_XCHADD                EQU     USER_KPAGE+0x3E0

INTERLOCKED_END             EQU     USER_KPAGE+0x400

;
; CPU independent offsets
;
lpvTls                      EQU     0x000           ; Current thread local storage pointer
bResched                    EQU     0x004           ; reschedule flag
cNest                       EQU     0x005           ; kernel exception nesting
dwCurThId                   EQU     0x008           ; 
dwCurProcId                 EQU     0x00c           ; 
dwKCRes                     EQU     0x018
pVMPrc                      EQU     0x01c
pCurPrc                     EQU     0x020           ; pointer to current PROCESS structure
pCurThd                     EQU     0x024           ; pointer to current THREAD structure
ownspinlock                 EQU     0x028
CurFPUOwner                 EQU     0x02c
PendEvents1                 EQU     0x030
PendEvents2                 EQU     0x034
dwCpuId                     EQU     0x040
pSavedContext               EQU     0x048
dwSyscallReturnTrap         EQU     0x050           ; randomized return address for PSL return
dwPslTrapSeed               EQU     0x054           ; random seed for PSL Trap address

bPowerOff                   EQU     0x086
bProfileOn                  EQU     0x087
pAPIReturn                  EQU     0x098
bInDebugger                 EQU     0x0a0
CeLogStatus                 EQU     0x0b4
alpeIntrEvents              EQU     0x1a0

;
; CPU dependent offsets (range 0x2a0 - 0x300)
;
architectureId              EQU     0x2a0
pAddrMap                    EQU     0x2a4
dwTTBRCtrlBits              EQU     0x2c0


;*
;* Process structure fields
;*
dwPrcID                     EQU     0xC             ; process id

;*
;* Call Stack structure fields
;*
cstk_args                   EQU     0               ; args
cstk_pcstkNext              EQU     0x38            ; pcstkNext
cstk_retAddr                EQU     0x3C            ; retAddr
cstk_dwPrevSP               EQU     0x40            ; dwPrevSP
cstk_dwPrcInfo              EQU     0x44            ; dwPrcInfo
cstk_pprcLast               EQU     0x48            ; pprcLast
cstk_pprcVM                 EQU     0x4C            ; pprcVM
cstk_phd                    EQU     0x50            ; phd
cstk_pOldTls                EQU     0x54            ; pOldTls
cstk_iMethod                EQU     0x58            ; iMethod
cstk_dwNewSP                EQU     0x5C            ; dwNewSP
cstk_regs                   EQU     0x60            ; regs

cstk_sizeof                 EQU     0x90            ; sizeof (CALLSTACK) 

; bit fields of dePrcInfo
CST_MODE_FROM_USER          EQU     0x1

;* Thread structure fields
ThPcstkTop                  EQU     0x18            ; pcstkTop
ThTlsPtr                    EQU     0x1c            ; tlsPtr
ThTlsSecure                 EQU     0x20            ; tlsSecure
ThTlsNonSecure              EQU     0x24            ; tlsNonSecure

THREAD_CONTEXT_OFFSET       EQU     0x60

TcxPsr                      EQU     THREAD_CONTEXT_OFFSET+0x000
TcxR0                       EQU     THREAD_CONTEXT_OFFSET+0x004
TcxR1                       EQU     THREAD_CONTEXT_OFFSET+0x008
TcxR2                       EQU     THREAD_CONTEXT_OFFSET+0x00c
TcxR3                       EQU     THREAD_CONTEXT_OFFSET+0x010
TcxR4                       EQU     THREAD_CONTEXT_OFFSET+0x014
TcxR5                       EQU     THREAD_CONTEXT_OFFSET+0x018
TcxR6                       EQU     THREAD_CONTEXT_OFFSET+0x01c
TcxR7                       EQU     THREAD_CONTEXT_OFFSET+0x020
TcxR8                       EQU     THREAD_CONTEXT_OFFSET+0x024
TcxR9                       EQU     THREAD_CONTEXT_OFFSET+0x028
TcxR10                      EQU     THREAD_CONTEXT_OFFSET+0x02c
TcxR11                      EQU     THREAD_CONTEXT_OFFSET+0x030
TcxR12                      EQU     THREAD_CONTEXT_OFFSET+0x034
TcxSp                       EQU     THREAD_CONTEXT_OFFSET+0x038
TcxLr                       EQU     THREAD_CONTEXT_OFFSET+0x03c
TcxPc                       EQU     THREAD_CONTEXT_OFFSET+0x040

; floating point registers
TcxFpscr                    EQU     THREAD_CONTEXT_OFFSET+0x044
TcxFpExc                    EQU     THREAD_CONTEXT_OFFSET+0x048
TcxRegs                     EQU     THREAD_CONTEXT_OFFSET+0x04c

TcxSizeof                   EQU     THREAD_CONTEXT_OFFSET+0x16c   ; TcxRegs + (NUM_VFP_REGS+NUM_ExTRA_CONTROL_REGS) * 4

; size of exception record, MUST MATCH sizeof(EXCEPTION_RECORD)
ExrSizeof                   EQU     0x50

; Dispatcher Context Structure Offset Definitions

DcControlPc                 EQU     0x0
DcFunctionEntry             EQU     0x4
DcEstablisherFrame          EQU     0x8
DcContextRecord             EQU     0xc

; Exception Record Offset, Flag, and Enumerated Type Definitions

EXCEPTION_NONCONTINUABLE    EQU     0x1
EXCEPTION_UNWINDING         EQU     0x2
EXCEPTION_EXIT_UNWIND       EQU     0x4
EXCEPTION_STACK_INVALID     EQU     0x8
EXCEPTION_NESTED_CALL       EQU     0x10
EXCEPTION_TARGET_UNWIND     EQU     0x20
EXCEPTION_COLLIDED_UNWIND   EQU     0x40
EXCEPTION_UNWIND            EQU     0x66

ExceptionContinueExecution  EQU     0x0
ExceptionContinueSearch     EQU     0x1
ExceptionNestedException    EQU     0x2
ExceptionCollidedUnwind     EQU     0x3
ExceptionExecuteHandler     EQU     0x4

ErExceptionCode             EQU     0x0
ErExceptionFlags            EQU     0x4
ErExceptionRecord           EQU     0x8
ErExceptionAddress          EQU     0xc
ErNumberParameters          EQU     0x10
ErExceptionInformation      EQU     0x14
ExceptionRecordLength       EQU     0x50

;
; Context Frame Offset and Flag Definitions
;

CONTEXT_FULL                EQU     0x047
CONTEXT_CONTROL             EQU     0x041
CONTEXT_INTEGER             EQU     0x042
CONTEXT_FLOATING_POINT      EQU     0x044
CONTEXT_EXTENDED_FLOAT      EQU     0x048
CONTEXT_SEH                 EQU     0x043       ; CONTEXT_CONTROL|CONTEXT_INTEGER

CtxFlags                    EQU     0x000
CtxR0                       EQU     0x004
CtxR1                       EQU     0x008
CtxR2                       EQU     0x00c
CtxR3                       EQU     0x010
CtxR4                       EQU     0x014
CtxR5                       EQU     0x018
CtxR6                       EQU     0x01c
CtxR7                       EQU     0x020
CtxR8                       EQU     0x024
CtxR9                       EQU     0x028
CtxR10                      EQU     0x02c
CtxR11                      EQU     0x030
CtxR12                      EQU     0x034
CtxSp                       EQU     0x038
CtxLr                       EQU     0x03c
CtxPc                       EQU     0x040
CtxPsr                      EQU     0x044

; floating point registers
CtxFpscr                    EQU     0x048
CtxFpExc                    EQU     0x04c
CtxRegs                     EQU     0x050

CtxSizeof                   EQU     0x170       ; CtxRegs + (NUM_VFP_REGS+NUM_ExTRA_CONTROL_REGS) * 4

;
; spinlock offsets
;
owner_cpu                   EQU     0
nestcount                   EQU     4
next_owner                  EQU     8


;* NK interrupt defines

SYSINTR_NOP                 EQU     0
SYSINTR_RESCHED             EQU     1
SYSINTR_BREAK               EQU     2
SYSINTR_CHAIN               EQU     3
SYSINTR_IPI                 EQU     4
; OEM defined SYSINTR starts from SYSINTR_DEVICE
SYSINTR_DEVICES             EQU     8
SYSINTR_MAX_DEVICES         EQU     64

SYSINTR_FIRMWARE            EQU     SYSINTR_DEVICES+16
SYSINTR_MAXIMUM             EQU     SYSINTR_DEVICES+SYSINTR_MAX_DEVICES

;
; MP related constants
;
MP_PCB_BASE                 EQU     0xFFFD8000  ;// where PCB resides
FIQ_STACK_SIZE              EQU     0x100       ;// FIQ stack     128 bytes
IRQ_STACK_SIZE              EQU     0x100       ;// IRQ stack     256 bytes
ABORT_STACK_SIZE            EQU     0x400       ;// Abort stack   512 bytes
REGISTER_SAVE_AREA          EQU     0x020       ;// register save area 32 bytes
KSTACK_SIZE                 EQU     (0x1800 - FIQ_STACK_SIZE - IRQ_STACK_SIZE - ABORT_STACK_SIZE - REGISTER_SAVE_AREA)

;
; IPI commands
;
IPI_STOP_CPU                EQU     1           ; must match the value in kernel.h


    END
