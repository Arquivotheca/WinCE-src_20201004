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
        OPT     1       ; reenable listing

; flag for unhandled exception, must match the value in winnt.h
EXCEPTION_FLAG_UNHANDLED    EQU     0x80000000

        TEXTAREA
        IMPORT xxx_RaiseException
        IMPORT DoRtlDispatchException

        EXPORT RtlDispatchReturn

;-------------------------------------------------------------------------------
;++
; EXCEPTION_DISPOSITION
; RtlpExecuteHandlerForException (
;    IN PEXCEPTION_RECORD ExceptionRecord,
;    IN ULONG EstablisherFrame,
;    IN OUT PCONTEXT ContextRecord,
;    IN OUT PDISPATCHER_CONTEXT DispatcherContext);
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
; Return Value:
;    The disposition value returned by the specified exception handler is
;    returned as the function value.
;--
;-------------------------------------------------------------------------------

        NESTED_ENTRY RtlpExecuteHandlerForException,, RtlpExceptionHandler

        PROLOG_PUSH     {r3, lr}                ; save ptr to DispatcherContext

        ldr     lr, [r3, #DcLanguageHandler]    ; lr = ExceptionRoutine
        blx     lr                              ; invoke handler

        mov     r1, r1                          ; effective NOP required to terminate unwinding
                                                ; note it must be different from the equivalent
                                                ; NOP in RtlpExecuteHandlerForUnwind, below
        EPILOG_POP      {r3, pc}

        NESTED_END RtlpExecuteHandlerForException


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

        ldr     r2, [r0, #ErExceptionFlags]     ; we don't care about r2; use it for scratch  
        movs    r0, #ExceptionContinueSearch    ; assume unwind in progress
        tst     r2, #EXCEPTION_UNWIND           ; unwind in progress?
        bxne    lr                              ;  Y: & return

        ldr     r2, [r1, #-8]                   ; (r2) = ptr to DispatcherContext
        ldr     r2, [r2, #DcEstablisherFrame]   ; (r2) = establisher frame pointer
        str     r2, [r3, #DcEstablisherFrame]   ; copy to current dispatcher ctx
        movs     r0, #ExceptionNestedException  ; set dispostion value

        bx      lr                              ; return to caller

        LEAF_END RtlpExeptionHandler




;-------------------------------------------------------------------------------
;++
; EXCEPTION_DISPOSITION
; RtlpExecuteHandlerForUnwind (
;    IN PEXCEPTION_RECORD ExceptionRecord,
;    IN PVOID EstablisherFrame,
;    IN OUT PCONTEXT ContextRecord,
;    IN OUT PVOID DispatcherContext
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
; Return Value:
;    The disposition value returned by the specified exception handler is
;    returned as the function value.
;--
;-------------------------------------------------------------------------------

        NESTED_ENTRY RtlpExecuteHandlerForUnwind,, RtlpUnwindHandler

        PROLOG_PUSH     {r3, lr}                ; save ptr to DispatcherContext

        ldr     lr, [r3, #DcLanguageHandler]    ; lr = ExceptionRoutine
        blx     lr                              ; invoke handler

        mov     r0, r0                          ; effective NOP required to terminate unwinding
                                                ; note it must be different from the equivalent
                                                ; NOP in RtlpExecuteHandlerForException, above
        EPILOG_POP      {r3, pc}


        NESTED_END RtlpExecuteHandlerForUnwind



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

        ldr     r0, [r1, #-8]                           ; get establisher context address  
                                                        ; (we don't care about r0; use it for scratch)  
        ldr     r2, [r0, #DcControlPc]                  ; copy control PC  
        str     r2, [r3, #DcControlPc]                  ;  
        ldr     r2, [r0, #DcImageBase]                  ; copy image base  
        str     r2, [r3, #DcImageBase]                  ;  
        ldr     r2, [r0, #DcFunctionEntry]              ; copy function entry  
        str     r2, [r3, #DcFunctionEntry]              ;  
        ldr     r2, [r0, #DcEstablisherFrame]           ; copy establisher frame  
        str     r2, [r3, #DcEstablisherFrame]           ;  
        ldr     r2, [r0, #DcContextRecord]              ; copy context record address  
        str     r2, [r3, #DcContextRecord]              ;  
        ldr     r2, [r0, #DcLanguageHandler]            ; copy language handler address  
        str     r2, [r3, #DcLanguageHandler]            ;  
        ldr     r2, [r0, #DcHandlerData]                ; copy handler data address  
        str     r2, [r3, #DcHandlerData]                ;  
        ldr     r2, [r0, #DcScopeIndex]                 ; copy scope table index  
        str     r2, [r3, #DcScopeIndex]                 ;   
        ldr     r2, [r0, #DcNonVolatileRegisters]       ; copy non-volatile register pointer  
        str     r2, [r3, #DcNonVolatileRegisters]       ;   
        
        movs    r0, #ExceptionCollidedUnwind            ; set dispostion value
        bx      lr                                      

        LEAF_END RtlpUnwindHandler

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
        vmsr    fpscr, r1
        
        add     r1, r0, #CtxRegs
        vldmia  r1!, {d0-d15}

        and     r2, r2, #CONTEXT_EXTENDED_FLOAT
        cmp     r2, #CONTEXT_EXTENDED_FLOAT
        bne     VfpDone

        vldmia  r1, {d16-d31}

VfpDone
        ldr     r2, [r0, #CtxPsr]       ; (r2) = PSR
        msr     CPSR_F, r2              ; restore bit 24:31 of PSR

        ldr     r1, [r0, #CtxSp]        ; (r1) = new SP
        ldr     r4, [r0, #CtxPc]        ; (r4) = new PC
        add     r0, r0, #CtxR0          ; (r0) = pCtx->R0
        ldmia   r0!, {r2, r3}           ; {r2-r3} to keep original {r0-r1}, (r0) = &pCtx->R2
        stmdb   r1!, {r2-r4}            ; save r0, r1, pc on stack, r1 = new sp with 3 register pushed
        ldr     lr, [r0, #CtxLr-CtxR2]  ; restore lr
        ldmia   r0, {r2-r12}            ; restore r2-r12
        mov     sp, r1                  ; (sp) = new sp with r0, r1, pc on stack
        pop     {r0-r1, pc}             ; restore r0, r1 and pc

        LEAF_END RtlResumeFromContext

;-------------------------------------------------------------------------------
;++
; void 
; RtlCaptureNonVolatileContext (
;       OUT PCONTEXT pCtx,
;       IN  BOOL fpuUsed
;       )
;
; Routine Description:
;       Capture non-volatile context
;
; Arguments:
;       pCtx (r0) - Supplies a pointer to a context record to receive non-volatile context
;       fpuUsed (r1) - indicate if FPU context should be captured
;
; Return Value:
;       None
;
;--
;-------------------------------------------------------------------------------

        LEAF_ENTRY RtlCaptureNonVolatileContext

        add     r2, r0, #CtxR4                  ; (r2) = &pCtx->R4
        stmia   r2, {r4-r11}                    ; save r4-r11
        adr     r3, DoneCapture
        orr     r3, r3, #1
        str     r3, [r0, #CtxPc]                ; save pc
        str     lr, [r0, #CtxLr]                ; save lr
        str     sp, [r0, #CtxSp]                ; save sp

        cbz     r1, DoneCapture                 ; capture FPU?

        ; capture FPU context
        add     r2, r0, #CtxRegs+64             ; r2 = &pCtx->D[8]
        vstmia  r2, {d8-d15}                    ; store d8-d15
        
DoneCapture
        bx      lr
        
        LEAF_END RtlCaptureNonVolatileContext

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

        mov     sp, r0
        subs    sp, sp, #CtxSizeof              ; pop off everything above pExr - sizeof(CONTEXT)

        ; call RaiseException
        mov     r3, r1                          ; (r3) = arg3 = pCtx
        mov     r2, #0                          ; (r2) = arg2 = 0
        ldr     r1, =EXCEPTION_FLAG_UNHANDLED   ; (r1) = arg1 = EXCEPTION_FLAG_UNHANDLED
        ldr     r0, [r0]                        ; (r0) = arg0 = pExr->ExceptionCode
        bl      xxx_RaiseException              ; call RaiseException

        ; never return
        ; debugchk here, just mov 0 to pc and we'll fault
        mov     lr, #1
        bx      lr

        LEAF_END RtlReportUnhandledException


;-------------------------------------------------------------------------------
;++
; void RtlDispatchException (
;    IN PEXCEPTION_RECORD pExr,
;    IN PCONTEXT pCtx
;    )
;
        ; Fake prolog for unwinding.
        NESTED_ENTRY xxRtlDispatchException

        PROLOG_STACK_ALLOC  ExrSizeof+CtxSizeof-CtxPsr  ; skip everyting before Pc
        PROLOG_PUSH         {lr}                        ; Restore pc at time of fault
        PROLOG_STACK_ALLOC  12                          ; skip r12, sp, lr
        PROLOG_PUSH         {r4-r11}                    ; Restore non-volatiles r4-r11
        PROLOG_STACK_ALLOC  (4 * 5)                     ; Unlink past flags, and r0-r3


        ; real entry point for RtlDispatchException
        ALTERNATE_ENTRY RtlDispatchException

        bl      DoRtlDispatchException
RtlDispatchReturn
        ; should never return
        mov     lr, #1
        bx      lr
        NESTED_END xxRtlDispatchException



        END
        
