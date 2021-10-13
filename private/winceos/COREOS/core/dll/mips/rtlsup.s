//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
//------------------------------------------------------------------------------
//      TITLE("SEH Handling")
//------------------------------------------------------------------------------
//
//
// Module Name:
//
//    RtlSup.s
//
// Abstract:
//
//    This module implements the code necessary to handle SEH for MIPS
//
//------------------------------------------------------------------------------

#include "ksmips.h"

#define EXCEPTION_FLAG_UNHANDLED    0x80000000

//------------------------------------------------------------------------------
//
// Define call frame for calling exception handlers.
//
//------------------------------------------------------------------------------

        .struct 0
#ifdef  _MIPS64
CfArg:  .space  4 * 8                   // argument register save area
CfRa:   .space  8                       // saved return address
                                                                                // Must be 8 byte aligned
CfFrameLength:                          // length of stack frame
CfA0:   .space  8                       // caller argument save area
CfA1:   .space  8                       //
CfA2:   .space  8                       //
CfA3:   .space  8                       //
CfExr:  .space  8                       // address of exception routine

#else   //  _MIPS64
CfArg:  .space  4 * 4                   // argument register save area
CfRa:   .space  4                       // saved return address
_Pad_:  .space  4                       // pad stack frame to stack alignment (8 bytes)

CfFrameLength:                          // length of stack frame
CfA0:   .space  4                       // caller argument save area
CfA1:   .space  4                       //
CfA2:   .space  4                       //
CfA3:   .space  4                       //
CfExr:  .space  4                       // address of exception routine
#endif  //  _MIPS64

        SBTTL("Execute Handler for Exception")
//------------------------------------------------------------------------------
//
// EXCEPTION_DISPOSITION
// RtlpExecuteHandlerForException (
//    IN PEXCEPTION_RECORD ExceptionRecord,
//    IN ULONG EstablisherFrame,
//    IN OUT PCONTEXT ContextRecord,
//    IN OUT PDISPATCHER_CONTEXT DispatcherContext,
//    IN PEXCEPTION_ROUTINE ExceptionRoutine
//    )
//
// Routine Description:
//
//    This function allocates a call frame, stores the establisher frame
//    pointer in the frame, establishes an exception handler, and then calls
//    the specified exception handler as an exception handler. If a nested
//    exception occurs, then the exception handler of this function is called
//    and the establisher frame pointer is returned to the exception dispatcher
//    via the dispatcher context parameter. If control is returned to this
//    routine, then the frame is deallocated and the disposition status is
//    returned to the exception dispatcher.
//
// Arguments:
//
//    ExceptionRecord (a0) - Supplies a pointer to an exception record.
//
//    EstablisherFrame (a1) - Supplies the frame pointer of the establisher
//       of the exception handler that is to be called.
//
//    ContextRecord (a2) - Supplies a pointer to a context record.
//
//    DispatcherContext (a3) - Supplies a pointer to the dispatcher context
//       record.
//
//    ExceptionRoutine (4 * REG_SIZE(sp)) - supplies a pointer to the exception handler
//       that is to be called.
//
// Return Value:
//
//    The disposition value returned by the specified exception handler is
//    returned as the function value.
//
//------------------------------------------------------------------------------
EXCEPTION_HANDLER(RtlpExceptionHandler)

NESTED_ENTRY(RtlpExecuteHandlerForException, CfFrameLength, zero)

        subu    sp,sp,CfFrameLength     // allocate stack frame
        S_REG   ra,CfRa(sp)             // save return address
        PROLOGUE_END

        lw      t0, CfExr(sp)           // get address of exception routine
        S_REG   a3, CfA3(sp)            // save address of dispatcher context
        jal     t0                      // call exception exception handler
10:     L_REG   ra, CfRa(sp)            // restore return address
        addu    sp,sp,CfFrameLength     // deallocate stack frame
        j       ra                      // return

        .end    RtlpExecuteHandlerForException

        
        
        SBTTL("Local Exception Handler")
//------------------------------------------------------------------------------
//
// EXCEPTION_DISPOSITION
// RtlpExceptionHandler (
//    IN PEXCEPTION_RECORD ExceptionRecord,
//    IN ULONG EstablisherFrame,
//    IN OUT PCONTEXT ContextRecord,
//    IN OUT PDISPATCHER_CONTEXT DispatcherContext
//    )
//
// Routine Description:
//
//    This function is called when a nested exception occurs. Its function
//    is to retrieve the establisher frame pointer from its establisher's
//    call frame, store this information in the dispatcher context record,
//    and return a disposition value of nested exception.
//
// Arguments:
//
//    ExceptionRecord (a0) - Supplies a pointer to an exception record.
//
//    EstablisherFrame (a1) - Supplies the frame pointer of the establisher
//       of this exception handler.
//
//    ContextRecord (a2) - Supplies a pointer to a context record.
//
//    DispatcherContext (a3) - Supplies a pointer to the dispatcher context
//       record.
//
// Return Value:
//
//    A disposition value ExceptionNestedException is returned if an unwind
//    is not in progress. Otherwise a value of ExceptionContinueSearch is
//    returned.
//
//------------------------------------------------------------------------------
LEAF_ENTRY(RtlpExceptionHandler)
        .set noreorder
        
        lw      t0,ErExceptionFlags(a0)         // get exception flags
        nop
        and     t0,t0,EXCEPTION_UNWIND          // check if unwind in progress
        .set reorder
        bne     zero,t0,10f                     // if neq, unwind in progress

//
// Unwind is not in progress - return nested exception disposition.
//
        L_REG   t0,CfA3 - CfA0(a1)              // get dispatcher context address
        li      v0,ExceptionNestedException     // set disposition value
        .set noreorder
        lw      t1,DcEstablisherFrame(t0)       // copy the establisher frame pointer
        nop
        sw      t1,DcEstablisherFrame(a3)       // to current dispatcher context
        .set reorder
        j       ra                              // return

//
// Unwind is in progress - return continue search disposition.
//
10:     li      v0,ExceptionContinueSearch      // set disposition value
        j       ra                              // return

        .end    RtlpExceptionHandler)

        

        SBTTL("Execute Handler for Unwind")
        EXCEPTION_HANDLER(RtlpUnwindHandler)
//------------------------------------------------------------------------------
//
// EXCEPTION_DISPOSITION
// RtlpExecuteHandlerForUnwind (
//    IN PEXCEPTION_RECORD ExceptionRecord,
//    IN PVOID EstablisherFrame,
//    IN OUT PCONTEXT ContextRecord,
//    IN OUT PVOID DispatcherContext,
//    IN PEXCEPTION_ROUTINE ExceptionRoutine
//    )
//
// Routine Description:
//
//    This function allocates a call frame, stores the establisher frame
//    pointer and the context record address in the frame, establishes an
//    exception handler, and then calls the specified exception handler as
//    an unwind handler. If a collided unwind occurs, then the exception
//    handler of of this function is called and the establisher frame pointer
//    and context record address are returned to the unwind dispatcher via
//    the dispatcher context parameter. If control is returned to this routine,
//    then the frame is deallocated and the disposition status is returned to
//    the unwind dispatcher.
//
// Arguments:
//
//    ExceptionRecord (a0) - Supplies a pointer to an exception record.
//
//    EstablisherFrame (a1) - Supplies the frame pointer of the establisher
//       of the exception handler that is to be called.
//
//    ContextRecord (a2) - Supplies a pointer to a context record.
//
//    DispatcherContext (a3) - Supplies a pointer to the dispatcher context
//       record.
//
//    ExceptionRoutine (4 * REG_SIZE(sp)) - supplies a pointer to the exception handler
//       that is to be called.
//
// Return Value:
//
//    The disposition value returned by the specified exception handler is
//    returned as the function value.
//
//------------------------------------------------------------------------------
NESTED_ENTRY(RtlpExecuteHandlerForUnwind, CfFrameLength, zero)

        subu    sp,sp,CfFrameLength     // allocate stack frame
        S_REG   ra,CfRa(sp)             // save return address
        PROLOGUE_END

        lw      t0,CfExr(sp)            // get address of exception routine
        S_REG   a3,CfA3(sp)             // save address of dispatcher context
        jal     t0                      // call exception exception handler
10:     L_REG   ra,CfRa(sp)             // restore return address
        addu    sp,sp,CfFrameLength     // deallocate stack frame
        j       ra                      // return

        .end    RtlpExecuteHandlerForUnwind


        SBTTL("Local Unwind Handler")
//------------------------------------------------------------------------------
//
// EXCEPTION_DISPOSITION
// RtlpUnwindHandler (
//    IN PEXCEPTION_RECORD ExceptionRecord,
//    IN PVOID EstablisherFrame,
//    IN OUT PCONTEXT ContextRecord,
//    IN OUT PVOID DispatcherContext
//    )
//
// Routine Description:
//
//    This function is called when a collided unwind occurs. Its function
//    is to retrieve the establisher dispatcher context, copy it to the
//    current dispatcher context, and return a disposition value of nested
//    unwind.
//
// Arguments:
//
//    ExceptionRecord (a0) - Supplies a pointer to an exception record.
//
//    EstablisherFrame (a1) - Supplies the frame pointer of the establisher
//       of this exception handler.
//
//    ContextRecord (a2) - Supplies a pointer to a context record.
//
//    DispatcherContext (a3) - Supplies a pointer to the dispatcher context
//       record.
//
// Return Value:
//
//    A disposition value ExceptionCollidedUnwind is returned if an unwind is
//    in progress. Otherwise, a value of ExceptionContinueSearch is returned.
//
//------------------------------------------------------------------------------
LEAF_ENTRY(RtlpUnwindHandler)
        .set noreorder
        lw      t0,ErExceptionFlags(a0)         // get exception flags
        nop
        and     t0,t0,EXCEPTION_UNWIND          // check if unwind in progress
        .set reorder
        beq     zero,t0,10f                     // if eq, unwind not in progress

//
// Unwind is in progress - return collided unwind disposition.
//
        L_REG   t0,CfA3 - CfA0(a1)              // get dispatcher context address
        li      v0,ExceptionCollidedUnwind      // set disposition value
        lw      t1,DcControlPc(t0)              // Copy the establisher frames'
        lw      t2,DcFunctionEntry(t0)          // dispatcher context to the current
        lw      t3,DcEstablisherFrame(t0)       // dispatcher context
        lw      t4,DcContextRecord(t0)          //
        sw      t1,DcControlPc(a3)              //
        sw      t2,DcFunctionEntry(a3)          //
        sw      t3,DcEstablisherFrame(a3)       //
        sw      t4,DcContextRecord(a3)          //
        j       ra                              // return

//
// Unwind is not in progress - return continue search disposition.
//
10:     li      v0,ExceptionContinueSearch      // set disposition value
        j       ra                              // return
        .end    RtlpUnwindHandler



//------------------------------------------------------------------------------
//
//  void RtlResumeFromContext (
//      IN PCONTEXT pCtx
//      );
//
// Description:
//    Resume execution using the given context
//
// Arguments:
//
//    pCtx (a0) - Supplies a pointer to an context record.
//
//
// Return Value:
//
//    None. (Never returns)
//
//------------------------------------------------------------------------------
LEAF_ENTRY(RtlResumeFromContext)

        .set noreorder
        .set noat

#ifdef MIPS_HAS_FPU
        lw      t0, CxContextFlags(a0)
        li      t1, CONTEXT_FLOATING_POINT
        and     t0, t0, t1
        bne     t0, t1, FPUDone

        // need to restore FPU context
        lw      t0, CxFsr(a0)
        // For some reason unknown, bit 17 of FSR can be set, which
        // causes the ctc1 instruction to except. Mask bit 17 off the FSR
        // for now.
        li      t1, 0xfffdffff
        and     t0, t0, t1
        ctc1    t0, fsr
        
        // MIPSII - use sdc1 on all even number FP registers
        ldc1    f0, CxFltF0(a0)
        ldc1    f2, CxFltF2(a0)
        ldc1    f4, CxFltF4(a0)
        ldc1    f6, CxFltF6(a0)
        ldc1    f8, CxFltF8(a0)
        ldc1    f10, CxFltF10(a0)
        ldc1    f12, CxFltF12(a0)
        ldc1    f14, CxFltF14(a0)
        ldc1    f16, CxFltF16(a0)
        ldc1    f18, CxFltF18(a0)
        ldc1    f20, CxFltF20(a0)
        ldc1    f22, CxFltF22(a0)
        ldc1    f24, CxFltF24(a0)
        ldc1    f26, CxFltF26(a0)
        ldc1    f28, CxFltF28(a0)
        ldc1    f30, CxFltF30(a0)
#ifdef MIPSIV
        // MIPSIV - also save the odd number FP registers
        ldc1    f1, CxFltF1(a0)
        ldc1    f3, CxFltF3(a0)
        ldc1    f5, CxFltF5(a0)
        ldc1    f7, CxFltF7(a0)
        ldc1    f9, CxFltF9(a0)
        ldc1    f11, CxFltF11(a0)
        ldc1    f13, CxFltF13(a0)
        ldc1    f15, CxFltF15(a0)
        ldc1    f17, CxFltF17(a0)
        ldc1    f19, CxFltF19(a0)
        ldc1    f21, CxFltF21(a0)
        ldc1    f23, CxFltF23(a0)
        ldc1    f25, CxFltF25(a0)
        ldc1    f27, CxFltF27(a0)
        ldc1    f29, CxFltF29(a0)
        ldc1    f31, CxFltF31(a0)
#endif

FPUDone:
#endif

        L_REG   t0, CxIntHi(a0)         // (t0) = HI mul/div register
        L_REG   t1, CxIntLo(a0)         // (t1) = LO mul/div register
        lw      t2, CxFir(a0)           // (t2) = Excepiton PC
        lw      t3, CxIntSp(a0)         // (t3) = SP at the point of exception
        L_REG   t4, CxIntT3(a0)         // (t4) = original value of T3
        L_REG   t5, CxIntA0(a0)         // (t5) = pCtx->a0
        L_REG   s0, CxIntS0(a0)         // Restore thread's permanent registers
        L_REG   s1, CxIntS1(a0)
        L_REG   s2, CxIntS2(a0)
        L_REG   s3, CxIntS3(a0)
        L_REG   s4, CxIntS4(a0)
        L_REG   s5, CxIntS5(a0)
        L_REG   s6, CxIntS6(a0)
        L_REG   s7, CxIntS7(a0)
        L_REG   s8, CxIntS8(a0)

        // (t3) = SP of context
        S_REG   t5, -REG_SIZE(t3)       // save pCtx->a0
        S_REG   t2, -2*REG_SIZE(t3)     // save PC above context SP
        S_REG   t4, -3*REG_SIZE(t3)     // save original t3

        mthi    t0
        mtlo    t1

        L_REG   v1, CxIntV1(a0)
        L_REG   t0, CxIntT0(a0)
        L_REG   t1, CxIntT1(a0)
        L_REG   t2, CxIntT2(a0)
        L_REG   t4, CxIntT4(a0)
        L_REG   t5, CxIntT5(a0)
        L_REG   t6, CxIntT6(a0)
        L_REG   t7, CxIntT7(a0)
        L_REG   t8, CxIntT8(a0)
        L_REG   t9, CxIntT9(a0)
        L_REG   AT, CxIntAt(a0)
        L_REG   a3, CxIntA3(a0) 
        L_REG   a2, CxIntA2(a0) 
        L_REG   a1, CxIntA1(a0) 
        L_REG   gp, CxIntGp(a0)
        L_REG   ra, CxIntRa(a0)
        L_REG   v0, CxIntV0(a0)


        // all but a0, t3, sp, and pc restored
        // (t3) = context SP
        subu    sp, t3, 4*REG_SIZE      // sp = context SP - 4*REG_SIZE (room for t3, a0, pc, and pad for 8-byte alignment)
        L_REG   t3, REG_SIZE(sp)        // restore t3
        L_REG   a0, 2*REG_SIZE(sp)      // load a0 with Context PC

        // only a0, SP, PC left to be restored
        // (a0) = PC to jump to
        add     sp, sp, 4*REG_SIZE      // restore sp

        j       a0
        L_REG   a0, -REG_SIZE(sp)       // (delay slot), restore a0
        
        .set at
        .set reorder

        .end RtlResumeFromContext

//------------------------------------------------------------------------------
//
// void RtlReportUnhandledException (
//    IN PEXCEPTION_RECORD pExr,
//    IN PCONTEXT pCtx
//    );
//
// Description:
//    Report a second chance exception.
//
// Arguments:
//
//    pExr (a0) - supplies a pointer to exception record
//    pCtx (a1) - Supplies a pointer to an context record.
//
//
// Return Value:
//
//    None. (Never returns)
//
//------------------------------------------------------------------------------
LEAF_ENTRY(RtlReportUnhandledException)

        // pop off everything above pExr-sizeof(CONTEXT)
        subu    sp, a0, ContextFrameLength

        // call RaiseException
        move    a3, a1                          // (a3) = pCtx
        move    a2, zero                        // (a2) = 0
        li      a1, EXCEPTION_FLAG_UNHANDLED    // (a1) = EXCEPTION_FLAG_UNHANDLED
        lw      a0, (a0)                        // (a0) = pExr->ExceptionCode

        jal     xxx_RaiseException              // call RaiseException

        .end RtlReportUnhandledException


///////////////////////////////////////////////////////////////////////////////
//
//  xxDispatchException - provide prolog for unwinder
//
//

// the following code is never executed, it's sole purpose is to help unwinder
// unwind through RtlDispatchException
NESTED_ENTRY(xxDispatchException, ContextFrameLength+ExceptionRecordLength, zero)

        .set noreorder
        subu    sp, ContextFrameLength+ExceptionRecordLength  // room for CONEXT and EXCEPTION_RECORD

        // save all callee saved registers
        S_REG   gp, CxIntGp(sp)
        S_REG   s0, CxIntS0(sp)
        S_REG   s1, CxIntS1(sp)
        S_REG   s2, CxIntS2(sp)
        S_REG   s3, CxIntS3(sp)
        S_REG   s4, CxIntS4(sp)
        S_REG   s5, CxIntS5(sp)
        S_REG   s6, CxIntS6(sp)
        S_REG   s7, CxIntS7(sp)
        S_REG   s8, CxIntS8(sp)
        S_REG   ra, CxIntRa(sp) // unwinder will treat next RA store as EPC
        sw      ra, CxFir(sp)
        PROLOGUE_END

// this is the real entry point of RtlDispatchException        
ALTERNATE_ENTRY(RtlDispatchException)

        jal     DoRtlDispatchException
        nop
        
        .set reorder
        .end xxDispatchException


