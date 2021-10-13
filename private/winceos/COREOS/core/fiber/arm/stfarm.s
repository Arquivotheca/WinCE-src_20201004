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
;++
;
;
; Module Name:
;
;    stfarm.s
;
; Abstract:
;
;    This module implements the cpu dependent code for fiber switching
;
; Revision History:
;
;--


        OPT     2       ; disable listing
        INCLUDE ksarm.h
        OPT     1       ; reenable listing

;; where to find stack base;bound and cur-fiber from TLS pointer
;;  (must match the PRETLS definitions in pkfuncs.h)
PRETLS_BASE     EQU     -4
PRETLS_BOUND    EQU     -8
PRETLS_FIBER    EQU     -12
PRETLS_SIZE     EQU     -16

;; the fiber fields (offset in FIBERSTRUCT)
FBS_THRD        EQU     0
FBS_BASE        EQU     4
FBS_BOUND       EQU     8
FBS_SP          EQU     12
FBS_SIZE        EQU     16

SIZE_CTX        EQU     28*4    ; r0, r1, r3-r11, lr (12 regs), d8-d15 (8 64-bit regs).
                                ; Total == 12 + 16 = 28 registers.
SIZE_FP_REGS    EQU     8*8     ; d8-d15

        AREA |.data|, DATA
        EXPORT dwSizeCtx
        
dwSizeCtx       DCD     SIZE_CTX

;;------------------------------------------------------------------------------
;;
;; void
;; MDFiberSwitch (
;;    IN OUT PFIBERSTRUCT pFiberTo,
;;    IN DWORD dwThrdId,
;;    IN OUT LPDWORD pTlsPtr,
;;    IN BOOL fFpuUsed
;;    )
;;
;; Routine Description:
;;
;;    This routine is called to switch from a fiber to another fiber
;;
;; Arguments:
;;
;;    (r0) - pointer to the fiber to switch to
;;    (r1) - current thread id
;;    (r2) - TLS pointer
;;    (r3) - FPU used
;;
;; Return Value:
;;
;;    None
;;
;;------------------------------------------------------------------------------
        AREA |.text|, CODE
        LEAF_ENTRY MDFiberSwitch

        ; save registers
        sub     sp, sp, #SIZE_FP_REGS   ; room for FP registers
        mov     r12, sp                 ; (r12) = ptr to FP register save area
        stmdb   sp!, {r0, r1, r3-r11}   ; r0, r1, r3-r11
        stmdb   sp!, {lr}               ; lr must be 1st

        ; save non-volatile fp registers if needed
        cmp     r3, #0
        beq     nosavefp
        fstmiad r12, {d8-d15}           ; save d8-d15 if FPU used
nosavefp

        ; get current fiber
        ldr     r3, [r2, #PRETLS_FIBER] ; r3 = current fiber

        ; save SP, stack bound to current fiber
        ldr     r4, [r2, #PRETLS_BOUND] ; r4 = current stack bound
        str     r4, [r3, #FBS_BOUND]    ; pCurFiber->dwStkBound = r4
        str     sp, [r3, #FBS_SP]       ; pCurFiber->dwStkPtr = sp

        ; restore bound, size, and base from new fiber
        ldr     r4, [r0, #FBS_BASE]     ; r4 = pFiber->dwStkBase
        bic     r4, r4,  #1             ; clear LSB (main fiber indicator)
        ldr     r5, [r0, #FBS_BOUND]    ; r5 = pFiber->dwStkBound
        ldr     r6, [r0, #FBS_SIZE]     ; (r6) = pFiber->dwStkSize
        str     r4, [r2, #PRETLS_BASE]  ; update new stack base in TLS
        str     r5, [r2, #PRETLS_BOUND] ; update new stack bound in TLS
        str     r6, [r2, #PRETLS_SIZE]  ; update new stack size in TLS

        ; restore new sp from new fiber
        ldr     sp, [r0, #FBS_SP]       ; sp = pFiber->dwStkPtr

        ; update thread id field of the fibers
        str     r1, [r0, #FBS_THRD]
        mov     r1, #0
        str     r1, [r3, #FBS_THRD]

        ; update current fiber
        str     r0, [r2, #PRETLS_FIBER]
        
        ; restore registers from the new stack
        ldmia   sp!, {lr}               ; restore lr
        ldmia   sp!, {r0, r1, r3-r11}   ; restore r0, r1, r3-r11

        ; restore non-volatile fp reigsters if needed
        cmp     r3, #0
        beq     norestorefp
        fldmiad sp, {d8-d15}            ; restore d8-d15 if FPU used
norestorefp

        add     sp, sp, #SIZE_FP_REGS   ; update sp

  IF Interworking :LOR: Thumbing
        bx      lr
  ELSE
        mov     pc, lr                  ; return to the new fiber
  ENDIF        
        ENTRY_END MDFiberSwitch

        END

