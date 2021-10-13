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
;***
;exsup.inc
;
;Purpose:
;       Common data structures & definitions for exsup.asm and other
;       Structured Exception Handling support modules.
;
;******************************************************************************


;handler dispositions
DISPOSITION_DISMISS             EQU     0
DISPOSITION_CONTINUE_SEARCH     EQU     1
DISPOSITION_NESTED_EXCEPTION    EQU     2
DISPOSITION_COLLIDED_UNWIND     EQU     3

;filter return codes
FILTER_ACCEPT                   EQU     1
FILTER_DISMISS                  EQU     -1
FILTER_CONTINUE_SEARCH          EQU     0

;handler flags settings..
EXCEPTION_UNWINDING             EQU     2
EXCEPTION_EXIT_UNWIND           EQU     4
EXCEPTION_UNWIND_CONTEXT        EQU EXCEPTION_UNWINDING OR EXCEPTION_EXIT_UNWIND
EXCEPTION_STACK_INVALID         EQU     8

TRYLEVEL_NONE                   EQU     -1
TRYLEVEL_INVALID                EQU     -2

;callback interface codes (mimimal rEQUired set)
CB_GET_MAX_CODE                 EQU     0
CB_DO_LOCAL_UNWIND              EQU     1
CB_GET_FRAME_EBP                EQU     2
CB_GET_SCOPE_INDEX              EQU     3
CB_GET_SCOPE_DATA               EQU     4
MAX_CALLBACK_CODE               EQU     4

;typedef struct _EXCEPTION_REGISTRATION PEXCEPTION_REGISTRATION;
;struct _EXCEPTION_REGISTRATION{
;     struct _EXCEPTION_REGISTRATION *prev;
;     void (*handler)(PEXCEPTION_RECORD, PEXCEPTION_REGISTRATION, PCONTEXT, PEXCEPTION_RECORD);
;     struct scopetable_entry *scopetable;
;     int trylevel;
;     int _ebp;
;     PEXCEPTION_POINTERS xpointers;
;};
_EXCEPTION_REGISTRATION STRUCT
    prev                dd      ?
    handler             dd      ?
_EXCEPTION_REGISTRATION ENDS

;
; setjmp/longjmp buffer
;
_JMP_BUF STRUCT
    saved_ebp           dd      ?
    saved_ebx           dd      ?
    saved_edi           dd      ?
    saved_esi           dd      ?
    saved_esp           dd      ?
    saved_return        dd      ?
    saved_xregistration dd      ?
    saved_trylevel      dd      ?
    ; following only found in C9.0 or later jmp_buf
    version_cookie      dd      ?
    unwind_func         dd      ?
    unwind_data         dd      6 dup(?)
_JMP_BUF ENDS

; Cookie placed in the jmp_buf to identify the new, longer form
JMPBUF_COOKIE                   EQU     'VC20'

; Offset of TryLevel in a C8.0 SEH registration node
C8_TRYLEVEL                     EQU     12

