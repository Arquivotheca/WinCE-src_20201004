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
        TTL ARM SEH support
;-------------------------------------------------------------------------------
;++
;
;
; Module Name:
;
;    RtlSup.s
;
; Abstract:
;
;    This module implements the code necessary to field and process ARM SEH.
;
;--
;-------------------------------------------------------------------------------
        OPT     2       ; disable listing
        INCLUDE ksarm.h
        INCLUDE Armmacros.s
        OPT     1       ; reenable listing

; flag for unhandled exception, must match the value in winnt.h
EXCEPTION_FLAG_UNHANDLED    EQU     0x80000000

        TEXTAREA
        
        IMPORT xxx_RaiseException
        IMPORT DoRtlDispatchException

;-------------------------------------------------------------------------------
;++
; EXCEPTION_DISPOSITION
; RtlpExecuteHandlerForException (
;    IN PEXCEPTION_RECORD ExceptionRecord,
;    IN ULONG EstablisherFrame,
;    IN OUT PCONTEXT ContextRecord,
;    IN OUT PDISPATCHER_CONTEXT DispatcherContext,
;    IN PEXCEPTION_ROUTINE ExceptionRoutine);
;
; Routine Description:
;    This function allocates a call frame, stores the establisher frame
;    pointer in the frame, establishes an exception handler, and then calls
;    the specified exception handler as an exception handler. If a nested
;    exception occurs, then the exception handler of this function is called
;    and the establisher frame pointer is returned to the exception dispatcher
;    via the dispatcher context parameter. If control is returned to this
;    routine, then the frame is deallocated and the disposition status is
;    returned to the exception dispatcher.
;
; Arguments:
;    ExceptionRecord (r0) - Supplies a pointer to an exception record.
;
;    EstablisherFrame (r1) - Supplies the frame pointer of the establisher
;       of the exception handler that is to be called.
;
;    ContextRecord (r2) - Supplies a pointer to a context record.
;
;    DispatcherContext (r3) - Supplies a pointer to the dispatcher context
;       record.
;
;    ExceptionRoutine ((sp)) - supplies a pointer to the exception handler
;       that is to be called.
;
; Return Value:
;    The disposition value returned by the specified exception handler is
;    returned as the function value.
;--
;-------------------------------------------------------------------------------
        EXCEPTION_HANDLER RtlpExceptionHandler

        NESTED_ENTRY RtlpExecuteHandlerForException

        mov     r12, sp
        stmfd   sp!, {r3}                       ; save ptr to DispatcherContext
        stmfd   sp!, {r11, r12, lr}             ;   for RtlpExceptionHandler
        add     r11, sp, #12                    ; (r11) = frame pointer

        PROLOG_END

    IF Interworking :LOR: Thumbing
        ldr     r12, [r11, #4]
        mov     lr, pc
        bx      r12                             ; invoke handler

        sub     sp, r11, #12
        ldmfd   sp, {r11, sp, lr}
        bx      lr
    ELSE
        mov     lr, pc
        ldr     pc, [r11, #4]                   ; invoke handler

        sub     sp, r11, #12
        ldmfd   sp, {r11, sp, pc}
    ENDIF

        ENTRY_END RtlpExecuteHandlerForException


;-------------------------------------------------------------------------------
;++
; EXCEPTION_DISPOSITION
; RtlpExceptionHandler (
;    IN PEXCEPTION_RECORD ExceptionRecord,
;    IN ULONG EstablisherFrame,
;    IN OUT PCONTEXT ContextRecord,
;    IN OUT PDISPATCHER_CONTEXT DispatcherContext
;    )
;
; Routine Description:
;    This function is called when a nested exception occurs. Its function
;    is to retrieve the establisher frame pointer from its establisher's
;    call frame, store this information in the dispatcher context record,
;    and return a disposition value of nested exception.
;
; Arguments:
;    ExceptionRecord (r0) - Supplies a pointer to an exception record.
;
;    EstablisherFrame (r1) - Supplies the frame pointer of the establisher
;       of this exception handler.
;
;    ContextRecord (r2) - Supplies a pointer to a context record.
;
;    DispatcherContext (r3) - Supplies a pointer to the dispatcher context
;       record.
;
; Return Value:
;    A disposition value ExceptionNestedException is returned if an unwind
;    is not in progress. Otherwise a value of ExceptionContinueSearch is
;    returned.
;--
;-------------------------------------------------------------------------------
        LEAF_ENTRY RtlpExceptionHandler

        ldr     r12, [r0, #ErExceptionFlags]    ; (r12) = exception flags
        tst     r12, #EXCEPTION_UNWIND          ; unwind in progress?
        movne   r0, #ExceptionContinueSearch    ;  Y: set dispostion value

        RETURN_NE                               ;  Y: & return

        ldr     r12, [r1, #-4]                  ; (r12) = ptr to DispatcherContext
        ldr     r12, [r12, #DcEstablisherFrame] ; (r12) = establisher frame pointer
        str     r12, [r3, #DcEstablisherFrame]  ; copy to current dispatcher ctx
        mov     r0, #ExceptionNestedException   ; set dispostion value

        RETURN                                  ; return to caller

        ENTRY_END RtlpExeptionHandler




;-------------------------------------------------------------------------------
;++
; EXCEPTION_DISPOSITION
; RtlpExecuteHandlerForUnwind (
;    IN PEXCEPTION_RECORD ExceptionRecord,
;    IN PVOID EstablisherFrame,
;    IN OUT PCONTEXT ContextRecord,
;    IN OUT PVOID DispatcherContext,
;    IN PEXCEPTION_ROUTINE ExceptionRoutine
;    )
;
; Routine Description:
;    This function allocates a call frame, stores the establisher frame
;    pointer and the context record address in the frame, establishes an
;    exception handler, and then calls the specified exception handler as
;    an unwind handler. If a collided unwind occurs, then the exception
;    handler of of this function is called and the establisher frame pointer
;    and context record address are returned to the unwind dispatcher via
;    the dispatcher context parameter. If control is returned to this routine,
;    then the frame is deallocated and the disposition status is returned to
;    the unwind dispatcher.
;
; Arguments:
;    ExceptionRecord (r0) - Supplies a pointer to an exception record.
;
;    EstablisherFrame (r1) - Supplies the frame pointer of the establisher
;       of the exception handler that is to be called.
;
;    ContextRecord (r2) - Supplies a pointer to a context record.
;
;    DispatcherContext (r3) - Supplies a pointer to the dispatcher context
;       record.
;
;    ExceptionRoutine ((sp)) - supplies a pointer to the exception handler
;       that is to be called.
;
; Return Value:
;    The disposition value returned by the specified exception handler is
;    returned as the function value.
;--
;-------------------------------------------------------------------------------
        EXCEPTION_HANDLER RtlpUnwindHandler

        NESTED_ENTRY RtlpExecuteHandlerForUnwind

        mov     r12, sp
        stmfd   sp!, {r3}                       ; save ptr to DispatcherContext
        stmfd   sp!, {r11, r12, lr}             ;   for RtlpUnwindHandler
        add     r11, sp, #12                    ; (r11) = frame pointer

        PROLOG_END

    IF Interworking :LOR: Thumbing  
        ldr     r12, [r11, #4]
        mov     lr, pc
        bx      r12                             ; invoke handler

        sub     sp, r11, #12
        ldmfd   sp, {r11, sp, lr}
        bx      lr
    ELSE
        mov     lr, pc
        ldr     pc, [r11, #4]                   ; invoke handler

        sub     sp, r11, #12
        ldmfd   sp, {r11, sp, pc}
    ENDIF

        ENTRY_END RtlpExecuteHandlerForUnwind



;-------------------------------------------------------------------------------
;++
; EXCEPTION_DISPOSITION
; RtlpUnwindHandler (
;    IN PEXCEPTION_RECORD ExceptionRecord,
;    IN PVOID EstablisherFrame,
;    IN OUT PCONTEXT ContextRecord,
;    IN OUT PVOID DispatcherContext
;    )
;
; Routine Description:
;    This function is called when a collided unwind occurs. Its function
;    is to retrieve the establisher dispatcher context, copy it to the
;    current dispatcher context, and return a disposition value of nested
;    unwind.
;
; Arguments:
;    ExceptionRecord (r0) - Supplies a pointer to an exception record.
;
;    EstablisherFrame (r1) - Supplies the frame pointer of the establisher
;       of this exception handler.
;
;    ContextRecord (r2) - Supplies a pointer to a context record.
;
;    DispatcherContext (r3) - Supplies a pointer to the dispatcher context
;       record.
;
; Return Value:
;    A disposition value ExceptionCollidedUnwind is returned if an unwind is
;    in progress. Otherwise, a value of ExceptionContinueSearch is returned.
;--
;-------------------------------------------------------------------------------
        LEAF_ENTRY RtlpUnwindHandler

        ldr     r12, [r0, #ErExceptionFlags]    ; (r12) = exception flags
        tst     r12, #EXCEPTION_UNWIND          ; unwind in progress?
        moveq   r0, #ExceptionContinueSearch    ;  N: set dispostion value
        RETURN_EQ                               ;  N: & return

        ldr     r12, [r1, #-4]                  ; (r12) = ptr to DispatcherContext
        ldmia   r12, {r0-r2,r12}                ; copy establisher frames disp. ctx
        stmia   r3, {r0-r2,r12}                 ; to the current dispatcher context
        mov     r0, #ExceptionCollidedUnwind    ; set dispostion value

        RETURN                                  ; return to caller

        ENTRY_END RtlpUnwindHandler

;-------------------------------------------------------------------------------
;++
; void 
; RtlResumeFromContext (
;       IN PCONTEXT pCtx
;       )
;
; Routine Description:
;       Resume execution from the context given
;
; Arguments:
;       pCtx (r0) - Supplies a pointer to a context record to resume from
;
; Return Value:
;       None (never returned)
;
;--
;-------------------------------------------------------------------------------

        LEAF_ENTRY RtlResumeFromContext

        ldr     r2, [r0, #CtxFlags]
        and     r1, r2, #CONTEXT_FLOATING_POINT
        cmp     r1, #CONTEXT_FLOATING_POINT
        bne     VfpDone

        ldr     r1, [r0, #CtxFpscr]
        fmxr    fpscr, r1
        
        add     r1, r0, #CtxRegs
        fldmiad r1!, {d0-d15}

        and     r2, r2, #CONTEXT_EXTENDED_FLOAT
        cmp     r2, #CONTEXT_EXTENDED_FLOAT
        bne     VfpDone

        DCD     0xecd10b20              ; vldmia r1, {d16-d31}

VfpDone
        ldr     r2, [r0, #CtxPsr]       ; (r2) = PSR
        msr     CPSR_F, r2              ; restore bit 24:31 of PSR

    IF Interworking :LOR: Thumbing
        ldr     r3, [r0, #CtxPc]        ; (r3) = &pCtx->Pc
        tst     r3, #1                  ; is PC odd?
        bne     ResumeThumb             ; use special Thumb restore code if so
    ENDIF

        mov     sp, r0
        ldmib   sp, {r0-r15}            ; restore all registers, including PC.

        ENTRY_END RtlResumeFromContext

    IF Interworking :LOR: Thumbing

        NESTED_ENTRY RtlFinishResumeFromContextThumb

        sub     sp, sp, #8              ; fake prologue for unwinding

        PROLOG_END

ResumeThumb
        ; (r3) = pCtx->pc
        ldr     r5, [r0, #CtxSp]        ; (r5) = context SP

        ldmib   r0, {r1-r2}             ; r1-r2 keep original r0-r1
        stmdb   r5, {r1-r3}             ; save (r0-r1), and pc (in r3) 

        mov     r1, r5                  ; (r1) = context SP
        ldr     r14, [r0, #CtxLr]       ; restore r14
        add     r0, r0, #CtxR2          ; r0 = &pctx->R2
        ldmia   r0, {r2-r12}            ; restore r2-r12

        sub     sp, r1, #12             ; sp = context SP - 12 (room for r0, r1 and pc)

        ; need to restore r0-r2, pc, and pop 3 words form SP
        orr     r0, pc, #1              ; fix address so BX will switch
        bx      r0                      ; change to Thumb mode

        CODE16                          ; assemble Thumb instructions

        pop     {r0-r1, pc}             ; restore R0, R1 and continue in Thumb mode
        nop                             ; maintain 32-bit alignment

        CODE32                          ; assemble ARM instructions

        ENTRY_END RtlFinishResumeFromContextThumb
    ENDIF


;-------------------------------------------------------------------------------
;++
; void 
; RtlReportUnhandledException (
;       IN PEXCEPTION_RECORD pExr,
;       IN PCONTEXT pCtx
;       )
;
; Routine Description:
;       Report unhandled exception
;
; Arguments:
;       pExr (r0) - supplies a pointer to the exception record
;       pCtx (r1) - Supplies a pointer to a context record to resume from
;
; Return Value:
;       None (never returned)
;   
;--
;-------------------------------------------------------------------------------

        LEAF_ENTRY RtlReportUnhandledException

        sub     sp, r0, #CtxSizeof              ; pop off everything above pExr - sizeof(CONTEXT)

        ; call RaiseException
        mov     r3, r1                          ; (r3) = arg3 = pCtx
        mov     r2, #0                          ; (r2) = arg2 = 0
        ldr     r1, =EXCEPTION_FLAG_UNHANDLED   ; (r1) = arg1 = EXCEPTION_FLAG_UNHANDLED
        ldr     r0, [r0]                        ; (r0) = arg0 = pExr->ExceptionCode
        bl      xxx_RaiseException              ; call RaiseException

        ; never return
        ; debugchk here, just mov 0 to pc and we'll fault
        mov     pc, #0

        ENTRY_END RtlReportUnhandledException


;-------------------------------------------------------------------------------
;++
; void RtlDispatchException (
;    IN PEXCEPTION_RECORD pExr,
;    IN PCONTEXT pCtx
;    )
;
        ; Fake prolog for unwinding.
        NESTED_ENTRY xxRtlDispatchException

        sub     sp, sp, #ExrSizeof+CtxSizeof-CtxPsr ; skip everyting before PSR
        stmdb   sp!, {pc}                       ; Restore fault pc
        stmdb   sp!, {lr}                       ; Restore lr at time of fault
        sub     sp, sp, #8                      ; skip r12, sp
        stmdb   sp!, {r4-r11}                   ; Restore non-volatiles r4-r11
        sub     sp, sp, #(4 * 5)                ; Unlink past flags, and r0-r3

    PROLOG_END

        ; real entry point for RtlDispatchException
        ALTERNATE_ENTRY RtlDispatchException

        bl      DoRtlDispatchException

        ; should never return
        mov     pc, #0

        ENTRY_END xxRtlDispatchException



        END
        
