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
; THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
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
    INCLUDE    ksarm.h

    MACRO
    MAGIC_ARM_MODE_SWITCH
    ; ARM   ->  mov r4, r8, ror r7
    ;
    ; Thumb ->  bx pc
    ;           next two bytes ignored
    DCD 0xe1a04778
    MEND    


    TEXTAREA
    LEAF_ENTRY InterlockedExchange
    mov    r12, #USER_KPAGE+0x380-1
    add    pc, r12, #0x54+1    ; jump to 0xFFFFCBD4
;;;    swp    r0, r1, [r0]
;;;    mov    pc, lr

    ALTERNATE_ENTRY InterlockedTestExchange
    mov    r12, r1
    mov    r1, r2
    mov    r2, r12
    ALTERNATE_ENTRY InterlockedCompareExchange
    mov    r12, #USER_KPAGE+0x380-1
    add    pc, r12, #0x2C+1    ; jump to 0xFFFFCBAC

    ALTERNATE_ENTRY    InterlockedExchangeAdd
    mov    r12, #USER_KPAGE+0x380-1
    add    pc, r12, #0x40+1    ; jump to 0xFFFFCBC0

    NESTED_ENTRY InterlockedIncrement
    stmfd    sp!, {lr}
    PROLOG_END
    mov    r1, #1
    mov    r12, #USER_KPAGE+0x380-1
    mov    lr, pc        ; (lr) = Pc+8
    add    pc, r12, #0x40+1    ; jump to 0xFFFFCBC0
    add    r0, r0, #1    ; (r0) = incremented value
  IF Interworking :LOR: Thumbing
    ldmfd   sp!, {lr}
    bx      lr
  ELSE
    ldmfd    sp!, {pc}
  ENDIF
    ENTRY_END InterlockedIncrement

    NESTED_ENTRY InterlockedDecrement
    stmfd    sp!, {lr}
    PROLOG_END
    mov    r1, #-1
    mov    r12, #USER_KPAGE+0x380-1
    mov    lr, pc        ; (lr) = Pc+8
    add    pc, r12, #0x40+1    ; jump to 0xFFFFCBC0
    sub    r0, r0, #1    ; (r0) = decremented value
  IF Interworking :LOR: Thumbing
    ldmfd   sp!, {lr}
    bx      lr
  ELSE
    ldmfd    sp!, {pc}
  ENDIF
    ENTRY_END InterlockedDecrement

;;
;;--------------------------------------------------------------------------------------
;;
        ALTERNATE_ENTRY V6_WriteBarrier
        mov     r3, #0
WriteBarrier
        mcr     p15, 0, r3, c7, c10, 4  ; DSB
        bx      lr

    END

