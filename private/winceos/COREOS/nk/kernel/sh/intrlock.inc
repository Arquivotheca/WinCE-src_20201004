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
    .HEADING "Common Interlock includes"
    .FORM LINES=55
;++
;
;
; Module Name:
;
;    intrlock.inc
;
; Abstract:
;
;    This module implements the InterlockedIncrement, I...Decrement,
; I...Exchange and I...TestExchange APIs.
;
;    WARNING: This module jumps to well known code in the kernel data page to
; perform the interlocked operations.  The kernel exception handlers know the
; locations of those routines so that it can perform any necessary restarts in
; case of an interrupt or an exception.
;
; Environment:
;
;    Kernel mode or User mode.
;
; Revision History:
;
;--

        .section .data,data
        .align  4
        
InterlockedFuncitons:
        .data.l USER_KPAGE+h'3c0                ; InterlockedExchange
        .data.l USER_KPAGE+h'3d0                ; InterlockedCompareExchange
        .data.l USER_KPAGE+h'3e0                ; InterlockedIncrement
        .data.l USER_KPAGE+h'3b0                ; InterlockedDecrement
        .data.l USER_KPAGE+h'3f0                ; InterlockedExchangeAdd
        
OFST_ILOCK_EX:          .equ    0
OFST_ILOCK_CMPEX:       .equ    4
OFST_ILOCK_INC:         .equ    8
OFST_ILOCK_DEC:         .equ    12
OFST_ILOCK_XADD:        .equ    16

        .section .text,code,align=16


        LEAF_ENTRY InterlockAPIs
MultiCoreIlockEx:
        MOVLI_R4_R0                     ; (r0) = original value
        mov     r0, r1                  ; (r1) = original value
        mov     r5, r0                  ; (r0) = new value
        MOVCO_R0_R4                     ; store new value
        bf      MultiCoreIlockEx        ; retry if failed
        rts
        mov     r1, r0                  ; (delay slot) (r0) = old value

MultiCoreIlockCmpEx:
        MOVLI_R4_R0                     ; (r0) = original value
        cmp/eq  r0, r6
        bf      ICExEnd                 ; no match, skip store
        mov     r5, r0                  ; (r0) = new value
        MOVCO_R0_R4                     ; store new value
        bf/s    MultiCoreIlockCmpEx     ; retry if failed
        mov     r6, r0                  ; (delay slot) (r0) = old value
ICExEnd:
        rts
        nop

MultiCoreIlockInc:
        MOVLI_R4_R0                     ; (r0) = original value
        add     #1, r0                  ; increment
        MOVCO_R0_R4                     ; store new value
        bf      MultiCoreIlockInc       ; retry if failed
        rts
        nop
        
MultiCoreIlockDec:
        MOVLI_R4_R0                     ; (r0) = original value
        add     #-1, r0                 ; decrement
        MOVCO_R0_R4                     ; store new value
        bf      MultiCoreIlockDec       ; retry if failed
        rts
        nop
        
MultiCoreIlockXAdd:
        MOVLI_R4_R0                     ; (r0) = original value
        add     r5, r0                  ; increment by addent
        MOVCO_R0_R4                     ; store new value
        bf      MultiCoreIlockXAdd      ; retry if failed
        rts
        sub     r5, r0                  ; (delay slot) (r0) = old value

        ALTERNATE_ENTRY _InterlockedExchange
        mov     #InterlockedFuncitons, r0
        mov     @(OFST_ILOCK_EX, r0), r0
        jmp     @r0
        nop

        ALTERNATE_ENTRY _InterlockedTestExchange
        mov	r5, r1		;\ 
        mov	r6, r5		; > swap 2nd & 3rd arguments
        mov	r1, r6		;/
        ALTERNATE_ENTRY _InterlockedCompareExchange
        mov     #InterlockedFuncitons, r0
        mov     @(OFST_ILOCK_CMPEX, r0), r0
        jmp     @r0
        nop

        ALTERNATE_ENTRY _InterlockedIncrement
        mov     #InterlockedFuncitons, r0
        mov     @(OFST_ILOCK_INC, r0), r0
        jmp     @r0
        nop

        ALTERNATE_ENTRY _InterlockedExchangeAdd
        mov     #InterlockedFuncitons, r0
        mov     @(OFST_ILOCK_XADD, r0), r0
        jmp     @r0
        nop

        ALTERNATE_ENTRY _InterlockedDecrement
        mov     #InterlockedFuncitons, r0
        mov     @(OFST_ILOCK_DEC, r0), r0
        jmp     @r0
        nop

        ALTERNATE_ENTRY _InitInterlockedFunctions


        mov     #USER_KPAGE+nCpus, r0
        mov.l   @r0, r0
        cmp/eq  #1, r0
        bt      DoneInit

        ; multi-core, update function pointers
        mov     #InterlockedFuncitons, r0

        mov     #MultiCoreIlockEx, r1
        mov.l   r1, @(OFST_ILOCK_EX, r0)
        
        mov     #MultiCoreIlockCmpEx, r1
        mov.l   r1, @(OFST_ILOCK_CMPEX, r0)
        
        mov     #MultiCoreIlockInc, r1
        mov.l   r1, @(OFST_ILOCK_INC, r0)
        
        mov     #MultiCoreIlockDec, r1
        mov.l   r1, @(OFST_ILOCK_DEC, r0)
        
        mov     #MultiCoreIlockXAdd, r1
        mov.l   r1, @(OFST_ILOCK_XADD, r0)

        
DoneInit:
        rts
        nop
        
        .endf

