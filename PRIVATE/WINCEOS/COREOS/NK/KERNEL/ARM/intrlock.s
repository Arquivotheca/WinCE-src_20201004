;
; Copyright (c) Microsoft Corporation.  All rights reserved.
;
;
; This source code is licensed under Microsoft Shared Source License
; Version 1.0 for Windows CE.
; For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

;;++
;;
;; ULONG
;; __C_ExecuteExceptionFilter (
;;    PEXCEPTION_POINTERS ExceptionPointers,
;;    EXCEPTION_FILTER ExceptionFilter,
;;    ULONG EstablisherFrame,
;;    PCONTEXT ContextRecord
;;    )
;;
;; Routine Description:
;;
;;    This function calls the specified exception filter routine with the
;;    establisher frame passed in r12
;;
;; Arguments:
;;
;;    ExceptionPointers (r0) - Supplies a pointer to the exception pointers
;;       structure.
;;
;;    ExceptionFilter (r1) - Supplies the address of the exception filter
;;       routine.
;;
;;    EstablisherFrame (r2) - Supplies the establisher frame pointer.
;;
;;    ContextRecord (r3) - Supplies the register context at the point of the
;;                         exception
;;
;; Return Value:
;;
;;    The value returned by the exception filter routine.
;;
;;--

;;++
;;
;; VOID
;; __C_ExecuteTerminationHandler (
;;    BOOLEAN AbnormalTermination,
;;    TERMINATION_HANDLER TerminationHandler,
;;    ULONG EstablisherFrame,
;;    PCONTEXT ContextRecord
;;    )
;;
;; Routine Description:
;;
;;    This function calls the specified termination handler routine with the
;;    establisher frame passed in r12.
;;
;; Arguments:
;;
;;    AbnormalTermination (r0) - Supplies a boolean value that determines
;;       whether the termination is abnormal.
;;
;;    TerminationHandler (r1) - Supplies the address of the termination
;;       handler routine.
;;
;;    EstablisherFrame (r2) - Supplies the establisher frame pointer.
;;
;;    ContextRecord (r3) - Supplies the register context at the point of the
;;                         exception
;; Return Value:
;;
;;    None.
;;
;;--
    NESTED_ENTRY __C_ExecuteExceptionFilter
    ALTERNATE_ENTRY __C_ExecuteTerminationHandler

    stmfd   sp!, {r4-r11, lr}

    PROLOG_END

  IF Interworking :LOR: Thumbing

;
;   This function is always entered in ARM mode. Switch to Thumb
;   mode if necessary
;

    tst     r1, #0x01                           ; Thumb mode handler ??
    beq     ARMDispatch                         ; no, execute ARM handler

;
;   Dispatch to a Thumb handler:
;   Enter Thumb mode here and call the handler. Return to ARM mode after
;   returning from the handler.
;
    
    mov     r11, r7                         ; preserve incoming R7 value
    mov     r7, r2                          ; Pass Establisher Frame in R7

    orr     r12, pc, #1                     ; Create Thumb mode address
    bx      r12                             ; switch to Thumb mode

 IF {FALSE}
    CODE16

    mov     lr, pc
    mov     pc, r1                          ; jump to Thumb mode handler

    bx      pc
    nop

    CODE32
 ELSE
    DCW 0x46FE                              ; mov lr, pc
    DCW 0x468F                              ; mov pc, r1

; This code when run in ARM mode does nothing but destroy the R4
; register (which is a permanent register and should be saved and
; restored in any function making use of this).  The trick
; is that this code if executed in Thumb mode is interpreted as
; a 'BX PC' instruction, that switches back into ARM mode and
; continues execution at the next 32-bit boundary.  Not pretty,
; but somewhat clever.
; When executed as arm - MOV r4,r8,ROR r7
; When executed as thumb - bx pc; higher(next) 16-bit will be ignored

    MAGIC_ARM_MODE_SWITCH                   ; magic arm mode switch
 ENDIF

    mov r7, r11                             ; restore incoming R7 value
    b   DispatchExit                        ; ARM mode. Branch to exit


  ENDIF                                     ; Interworking & Thumbing code

;
;   Dispatch to an ARM handler:
;

ARMDispatch

    mov    r11, r2                              ; (r11) = frame pointer
    mov    lr, pc
    mov    pc, r1                               ; jump to handler function

DispatchExit

  IF Interworking :LOR: Thumbing
    ldmfd   sp!, {r4-r11, lr}
    bx      lr
  ELSE
    ldmfd   sp!, {r4-r11, pc}
  ENDIF

    ENTRY_END __C_ExecuteExceptionFilter

    END

