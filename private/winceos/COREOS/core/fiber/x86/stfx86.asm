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
;;
;;
;; Module Name:
;;
;;    stfx86.asm
;;
;; Abstract:
;;
;;    This module implements the code to switch to another fiber
;;
;; Environment:
;;
;;
.486p


;; where to find stack base;bound and cur-fiber from TLS pointer
;;  (must match the PRETLS definitions in pkfuncs.h)
        PRETLS_BASE  EQU -4
        PRETLS_BOUND EQU -8
        PRETLS_FIBER EQU -12
        PRETLS_SIZE  EQU -16

;; the fiber fields (offset in FIBERSTRUCT)
        FBS_THRD  EQU 0
        FBS_BASE  EQU 4
        FBS_BOUND EQU 8
        FBS_SP    EQU 12
        FBS_SIZE  EQU 16

CONST   SEGMENT
        public    _dwSizeCtx
_dwSizeCtx  DD    20        ; fs:[0], ebx, edi, esi, and ebp
CONST   ENDS

_TEXT segment para public 'TEXT'

;;------------------------------------------------------------------------------
;;
;; void
;; MDFiberSwitch (
;;    IN OUT PFIBERSTRUCT pFiberTo,
;;    IN DWORD dwThrdId,
;;    IN OUT LPDWORD pTlsPtr
;;    )
;;
;; Routine Description:
;;
;;    This routine is called to switch from a fiber to another fiber
;;
;; Return Value:
;;
;;    None
;;
;;------------------------------------------------------------------------------
        assume fs:nothing

_MDFiberSwitch PROC NEAR PUBLIC

        ;; get the arguments
        mov eax, DWORD PTR 4[esp]          ;; eax == pFiber
        mov edx, DWORD PTR 8[esp]          ;; edx == thread id
        mov ecx, DWORD PTR 12[esp]         ;; ecx == tlsPtr

        ;; save registers
        push DWORD PTR fs:[0]
        push ebx
        push edi
        push esi
        push ebp
 
        ;; get current fiber from TLS
        mov ebx, DWORD PTR PRETLS_FIBER[ecx] ;; ebx == current fiber

        ;; save SP and bound in current fiber
        mov DWORD PTR FBS_SP[ebx], esp      ;; save esp in current fiber
        mov edi, DWORD PTR PRETLS_BOUND[ecx];; edi == current stack bound
        mov DWORD PTR FBS_BOUND[ebx], edi   ;; save stack bound in current fiber
                                            ;; (stack base never change)

        ;; update TLS with new fiber info.
        ;; (eax) == new fiber
        ;; (ecx) == tls ptr
        mov edi, DWORD PTR FBS_BOUND[eax]    ;; edi = pFiber->dwStkBound
        mov esi, DWORD PTR FBS_BASE[eax]     ;; esi = pFiber->dwStkBase
        and esi, 0fffffffeh                  ;; mask the LSB (main fiber indicator)
        mov DWORD PTR PRETLS_FIBER[ecx], eax ;; UCurFiber() = eax (new fiber)
        mov DWORD PTR PRETLS_BOUND[ecx], edi ;; UStkBound() = edi
        mov DWORD PTR PRETLS_BASE[ecx], esi  ;; UStkBase() = esi
        mov edi, DWORD PTR FBS_SIZE[eax]     ;; edi = pFiber->dwStkSize
        mov DWORD PTR PRETLS_SIZE[ecx], edi  ;; UStkSize() = edi
        ;; restore SP from new fiber
        mov esp, DWORD PTR FBS_SP[eax]       ;; esp = pFiber->dwStkPtr

        ;; update thread id for the fibers
        mov DWORD PTR FBS_THRD[eax], edx     ;; pFiber->dwThrdId = edx (current thread id)
        xor  edx, edx
        mov DWORD PTR FBS_THRD[ebx], edx     ;; pCurFiber->dwThrdId = 0

        ;; now restore registers
        pop  ebp
        pop  esi
        pop  edi
        pop  ebx
        pop  DWORD PTR fs:[0]

        ret

_MDFiberSwitch ENDP
        assume fs:error
_TEXT ENDS 

        END
