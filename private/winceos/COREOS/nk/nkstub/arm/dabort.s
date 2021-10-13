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

        TTL ARM stub for calling DataAbortHandler
;-------------------------------------------------------------------------------
;++
;
;
; Module Name:
;
;    dabort.s
;
; Abstract:
;
;    This module implements the code to jump to DataAbortHandler (a function pointer)
;    WITH ALL REGISTERS PRESERVED.
;
; Environment:
;
;    Kernel mode only.
;
;--
;-------------------------------------------------------------------------------
        OPT     2       ; disable listing
        INCLUDE ksarm.h
        OPT     1       ; reenable listing

;
; register offsets from sp after doing "stmfd sp!, {r0-r3, r12, lr}"
;
ofstR0  EQU     0
ofstR1  EQU     4
ofstR2  EQU     8
ofstR3  EQU     12
ofstR12 EQU     16
ofstLR  EQU     20

        TEXTAREA

        IMPORT  GetOriginalDAbortHandlerAddress

        LEAF_ENTRY DataAbortHandler

        stmfd   sp!, {r0-r3, r12, lr}

        bl      GetOriginalDAbortHandlerAddress
        ; (r0) - address for data abort handler
        
        ; restore lr
        ldr     lr, [sp, #ofstLR]

        ; put the address of the real DataAbortHandler onto stack
        str     r0, [sp, #ofstLR]

        ; restore registers and jump to the address of data abort handler
        ldmfd   sp!, {r0-r3, r12, pc}


        ENTRY_END DataAbortHandler
        
        END

