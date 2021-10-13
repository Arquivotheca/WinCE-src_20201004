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
        TTL ARM Interrupt and Exception Processing
;-------------------------------------------------------------------------------
;++
;
;
; Module Name:
;
;    ExVector.s
;
; Abstract:
;
;    This module specifies what goes into the exception vector
;
; Environment:
;
;    Kernel mode only.
;
;--
;-------------------------------------------------------------------------------
        OPT     2       ; disable listing
        INCLUDE kxarm.h
        OPT     1       ; reenable listing

        ;-----------------------------------------------------------------------
        ; NOTE: VectorTable MUST BE IN Code (Read-only TEXT section)
        TEXTAREA

        IMPORT  UndefException
        IMPORT  SWIHandler
        IMPORT  PrefetchAbort
        IMPORT  DataAbortHandler
        IMPORT  IRQHandler
        IMPORT  FIQHandler
        EXPORT  VectorTable

VectorTable
        DCD     -1                              ; reset
        DCD     UndefException                  ; undefined instruction
        DCD     SWIHandler                      ; SVC
        DCD     PrefetchAbort                   ; Prefetch abort
        DCD     DataAbortHandler                ; data abort
        DCD     -1                              ; unused vector
        DCD     IRQHandler                      ; IRQ
        DCD     FIQHandler                      ; FIQ

        END
