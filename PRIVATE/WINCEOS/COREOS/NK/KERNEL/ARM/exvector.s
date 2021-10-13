;
; Copyright (c) Microsoft Corporation.  All rights reserved.
;
;
; This source code is licensed under Microsoft Shared Source License
; Version 1.0 for Windows CE.
; For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

  IF :DEF:ARMV4T :LOR: :DEF:ARMV4I
        IMPORT  OEMDataAbortHandler
  ENDIF


VectorTable
        DCD     -1                              ; reset
        DCD     UndefException                  ; undefined instruction
        DCD     SWIHandler                      ; SVC
        DCD     PrefetchAbort                   ; Prefetch abort

        IF :DEF:ARMV4T :LOR: :DEF:ARMV4I
        DCD     OEMDataAbortHandler             ; data abort
        ELSE
        DCD     DataAbortHandler                ; data abort
        ENDIF

        DCD     -1                              ; unused vector
        DCD     IRQHandler                      ; IRQ
        DCD     FIQHandler                      ; FIQ

        END
