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
    TTL    Interlock memory operations
;++
;
; Module Name:
;    intrlock.s
;
; Abstract:
;    This module implements the InterlockedIncrement, I...Decrement,
; I...Exchange and I...TestExchange APIs.
;
; Environment:
;    Kernel mode or User mode.
;
;--
        OPT     2           ; disable listing
        INCLUDE ksarm.h
        OPT     1           ; reenable listing

;
; Depenedeing on the number of CPUs, interlocked operations are implemented differently.
; - if there are more than 1 CPUs, it's guaranteed that sync instructions (ldrex/strex) are present, and
;   we'll handle it locally with ldrex/strex.
; - if there is only 1 CPU, we always jump to know address, where kernel can restart instruction when interrupted.
;   NOTE: (r3) must be 0 before the jump - it'll be used as restart indicator.
;
;-----------------------------------------------------------------------
; Globals
        AREA |.data|, DATA

InterlockedFunctions
                DCD     INTLK_EXCHG
                DCD     INTLK_CMPCHG
                DCD     INTLK_XCHADD
                DCD     NopFunc
                DCD     NopFunc

ILOCKEX         EQU     0
ILOCKCMPEX      EQU     4
ILOCKXADD       EQU     8
READBARRIER     EQU     12
WRITEBARRIER    EQU     16


        TEXTAREA

        LEAF_ENTRY InterlockedFunctionArea

;;
;;--------------------------------------------------------------------------------------
;; Interlocked operation for multi-core (private)
;; (r3) == 0 at entrance
;;--------------------------------------------------------------------------------------
;;
MultiCoreInterlockedExchange
        mcr     p15, 0, r3, c7, c10, 5  ; DMB
        mov     r12, r0                 ; (r12) = address to exchange
ILEXCH_Retry
        ldrex   r0, [r12]               ; (r0) = original value
        strex   r3, r1, [r12]           ; perform the exchange, (r3) = 0 iff successful
        cmp     r3, #0
        bne     ILEXCH_Retry
ReadBarrier
        mcr     p15, 0, r3, c7, c10, 5  ; DMB
NopFunc
        bx      lr

;;
;;--------------------------------------------------------------------------------------
;;
MultiCoreInterlockedCompareExchange
        mcr     p15, 0, r3, c7, c10, 5  ; DMB
        mov     r12, r0                 ; (r12) = address to compare-exchange

ILCMPEXCH_Retry
        ldrex   r0, [r12]               ; (r0) = original value
        cmp     r0, r2                  ; (orignal == comparand)?
        strexeq r3, r1, [r12]           ; store exclusive if equal
        cmpeq   r3, #1                  ; (r3) == 1 iff failed.
        beq     ILCMPEXCH_Retry         ; retry if strex failed.

        mcr     p15, 0, r3, c7, c10, 5  ; DMB
        bx      lr

;;
;;--------------------------------------------------------------------------------------
;;
MultiCoreInterlockedExchangeAdd
        ; (r0) = address to memory
        ; (r1) = addent
        ; (r2) = value to substract from return value (return value = original + (r1) - (r2))
        ; (r3) = 0
        mcr     p15, 0, r3, c7, c10, 5  ; DMB
        mov     r12, r0                 ; (r12) = address to exchange

ILEXCHADD_Retry
        ldrex   r0, [r12]               ; (r0) = original value
        add     r0, r0, r1              ; (r0) = original + addent
        strex   r3, r0, [r12]           ; store exclusive, (r3) = 0 iff successful
        cmp     r3, #0
        bne     ILEXCHADD_Retry

        mcr     p15, 0, r3, c7, c10, 5  ; DMB
        sub     r0, r0, r2
        bx      lr

;;
;;--------------------------------------------------------------------------------------
;;
        ALTERNATE_ENTRY V6_WriteBarrier
        mov     r3, #0
WriteBarrier
        mcr     p15, 0, r3, c7, c10, 4  ; DSB
        bx      lr


;;
;;--------------------------------------------------------------------------------------
;;  Exported Interlocked functions
;;--------------------------------------------------------------------------------------
;;
        ALTERNATE_ENTRY InterlockedExchange

        ldr     r12, =InterlockedFunctions
        mov     r3, #0
        ldr     pc, [r12, #ILOCKEX]

;;
;;--------------------------------------------------------------------------------------
;;
        ALTERNATE_ENTRY InterlockedTestExchange
        mov    r12, r1
        mov    r1, r2
        mov    r2, r12
    
        ALTERNATE_ENTRY InterlockedCompareExchange

        ldr     r12, =InterlockedFunctions
        mov     r3, #0
        ldr     pc, [r12, #ILOCKCMPEX]

;;
;;--------------------------------------------------------------------------------------
;;
        ALTERNATE_ENTRY InterlockedExchangeAdd

        ldr     r12, =InterlockedFunctions
        mov     r2, r1                  ; return value for InterlockedExchangeAdd is (orignal)+(r1)-(r2) == (original)
        mov     r3, #0
        ldr     pc, [r12, #ILOCKXADD]
    
;;
;;--------------------------------------------------------------------------------------
;;

        ALTERNATE_ENTRY InterlockedIncrement

        ldr     r12, =InterlockedFunctions
        mov     r1, #1
        mov     r2, #0                  ; return value for InterlockedIncrement is (orignal)+(r1)-(r2) == (original)+1
        mov     r3, #0
        ldr     pc, [r12, #ILOCKXADD]
    
;;
;;--------------------------------------------------------------------------------------
;;

        ALTERNATE_ENTRY InterlockedDecrement

        ldr     r12, =InterlockedFunctions
        mov     r1, #-1
        mov     r2, #0                  ; return value for InterlockedIncrement is (orignal)+(r1)-(r2) == (original)-1
        mov     r3, #0
        ldr     pc, [r12, #ILOCKXADD]

;-------------------------------------------------------------------------------
; DataMemoryBarrier - Issue a DMB instruction if > 1 CPU
;-------------------------------------------------------------------------------

        ALTERNATE_ENTRY DataMemoryBarrier
        
        ldr     r12, =InterlockedFunctions
        mov     r3, #0
        ldr     pc, [r12, #READBARRIER]


;-------------------------------------------------------------------------------
; DataSycBarrier - Issue a DSB instruction if > 1 CPU
;-------------------------------------------------------------------------------
        ALTERNATE_ENTRY DataSyncBarrier

        ldr     r12, =InterlockedFunctions
        mov     r3, #0
        ldr     pc, [r12, #WRITEBARRIER]

;
;;--------------------------------------------------------------------------------------
;;
        ALTERNATE_ENTRY InitInterlockedFunctions

        ldr     r12, =USER_KPAGE
        ldr     r3, [r12, #ADDR_NUM_CPUS-USER_KPAGE]
        cmp     r3, #1
        bxeq    lr

        ;
        ; more than one core
        ;
        ldr     r12, =InterlockedFunctions
        ldr     r0,  =MultiCoreInterlockedExchange
        ldr     r1,  =MultiCoreInterlockedCompareExchange
        ldr     r2,  =MultiCoreInterlockedExchangeAdd
        ldr     r3,  =ReadBarrier

        str     r0, [r12, #ILOCKEX]
        str     r1, [r12, #ILOCKCMPEX]
        str     r2, [r12, #ILOCKXADD]
        str     r3, [r12, #READBARRIER]
        ldr     r0,  =WriteBarrier
        str     r0, [r12, #WRITEBARRIER]

        bx      lr

        ENTRY_END


        END

