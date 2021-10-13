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
/*
 *
 * Structure Exception Handling support.
 * Taken from NT sources.
 */

#include <windows.h>

#define Naked void __declspec (naked)

void xxx_RaiseException (DWORD dwExceptionCode, DWORD dwExceptionFlags, DWORD cArgs, CONST DWORD * lpArgs);

/*
 * The compiler cannot mark the exception handlers in this file as safe, so
 * force the inclusion of a MASM file with the appropriate markup.
 */
#pragma comment(linker, "/INCLUDE:__RTLSUP_MARK_HANDLERS_SAFE__")

Naked RtlResumeFromContext (IN PCONTEXT pCtx)
{
    // restore EIP, EAX, EBX, ECX, EDX, EDI, ESI, EBP, ESP, and EFLAGS.
    // DO NOT TOUCH ANY SEGMENT REGISTERS
    __asm {

        // restore everyting but esi/edi for temp registers
        mov     edi, [esp+4]        // EDI = pCtx

        // 'push' return address on ESP of context
        mov     eax, [edi].Eip      // eax = pCtx->Eip
        mov     ebx, [edi].Esp      // ebx = pCtx->Esp
        sub     ebx, 4              // room for 'push' on context Esp
        mov     [ebx], eax          // 'push' Eip on context stack
                                    // (possible overlap with SegSs)
        mov     [edi].Esp, ebx      // pCtx->Esp -= 4
                                    
        lea     esp, [edi].Edi      // esp = &pCtx->Edi
        pop     edi                 // restore edi
        pop     esi                 // restore esi
        pop     ebx                 // restore ebx
        pop     edx                 // restore edx
        pop     ecx                 // restore ecx
        pop     eax                 // restore eax
        pop     ebp                 // restore ebp
        add     esp, 8              // skip CS and EIP
        popfd                       // restore EFLAGS
        pop     esp                 // restore esp

        // return address is right at [esp]
        // just return
        ret
    }
}


// restore SP and raise an "Unhandle Exception"
Naked RtlReportUnhandledException (IN PEXCEPTION_RECORD pExr, IN PCONTEXT pCtx)
{
    // we'll not return from this funciton. No register needs to be preserved.
    // Stack space above pExr-sizeof(CONTEXT) is no longer needed.
    
    __asm {
        mov     ecx, [esp+4]        // (ecx) = pExr
        mov     eax, [esp+8]        // (eax) = pCtx
        mov     edx, ecx
        sub     edx, size CONTEXT   // (edx) = pExr - sizeof Context
        mov     esp, edx            // esp = pop everything above pCtx
        
        // RaiseException (pExr->ExceptionCode, EXCEPTION_FLAG_UNHANDLED, 0, (LPDWORD)pCtx);
        push    eax                 // arg3 = pCtx
        push    0
        push    EXCEPTION_FLAG_UNHANDLED
        push    [ecx].ExceptionCode
        call    xxx_RaiseException
        // never return
    }
    DEBUGCHK (0);
}



#pragma warning(disable:4035 4733)
//------------------------------------------------------------------------------
//
//   ExecuteHandler is the common tail for RtlpExecuteHandlerForException
//   and RtlpExecuteHandlerForUnwind.
//
//   (edx) = handler (Exception or Unwind) address
//
///ExceptionRecord     equ [ebp+8]
///EstablisherFrame    equ [ebp+12]
///ContextRecord       equ [ebp+16]
///DispatcherContext   equ [ebp+20]
///ExceptionRoutine    equ [ebp+24]
//
//------------------------------------------------------------------------------

EXCEPTION_DISPOSITION __declspec(naked) 
ExecuteHandler(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PVOID EstablisherFrame,
    IN OUT PCONTEXT ContextRecord,
    IN OUT PDISPATCHER_CONTEXT DispatcherContext,
    IN PEXCEPTION_ROUTINE ExceptionRoutine
    ) 
{
    __asm {
        push    ebp
        mov     ebp, esp
        push    EstablisherFrame        // Save context of exception handler
                                        // that we're about to call.                                        
        push    edx                     // Set Handler address
        push    dword ptr fs:[0]        // Set next pointer
        mov     dword ptr fs:[0], esp   // Link us on
        //
        // Call the specified exception handler.
        //
        push    DispatcherContext
        push    ContextRecord
        push    EstablisherFrame
        push    ExceptionRecord

        call    [ExceptionRoutine]

        // Don't clean stack here, code in front of ret will restore initial state
        // Disposition is in eax, so all we do is deregister handler and return
        mov     esp, dword ptr fs:[0]
        pop     dword ptr fs:[0]
        mov     esp, ebp
        pop     ebp
        ret
    }
}



//------------------------------------------------------------------------------
//
// EXCEPTION_DISPOSITION
// ExceptionHandler (
//    IN PEXCEPTION_RECORD ExceptionRecord,
//    IN PVOID EstablisherFrame,
//    IN OUT PCONTEXT ContextRecord,
//    IN OUT PVOID DispatcherContext
//    )
//
// Routine Description:
//
//    This function is called when a nested exception occurs. Its function
//    is to retrieve the establisher frame pointer and handler address from
//    its establisher's call frame, store this information in the dispatcher
//    context record, and return a disposition value of nested exception.
//
// Arguments:
//
//    ExceptionRecord (exp+4) - Supplies a pointer to an exception record.
//
//    EstablisherFrame (esp+8) - Supplies the frame pointer of the establisher
//       of this exception handler.
//
//    ContextRecord (esp+12) - Supplies a pointer to a context record.
//
//    DispatcherContext (esp+16) - Supplies a pointer to the dispatcher context
//       record.
//
// Return Value:
//
//    A disposition value ExceptionNestedException is returned if an unwind
//    is not in progress. Otherwise a value of ExceptionContinueSearch is
//    returned.
//
//------------------------------------------------------------------------------
Naked 
ExceptionHandler(void) 
{
    __asm {
        mov     ecx, dword ptr [esp+4]          // (ecx) -> ExceptionRecord
        test    dword ptr [ecx.ExceptionFlags], EXCEPTION_UNWINDING
        mov     eax, ExceptionContinueSearch    // Assume unwind
        jnz     eh10                            // unwind, go return

        //
        // Unwind is not in progress - return nested exception disposition.
        //
        mov     ecx,[esp+8]             // (ecx) -> EstablisherFrame
        mov     edx,[esp+16]            // (edx) -> DispatcherContext
        mov     eax,[ecx+8]             // (eax) -> EstablisherFrame for the
                                        //          handler active when we
                                        //          nested.
        mov     [edx], eax              // Set DispatcherContext field.
        mov     eax, ExceptionNestedException
eh10:   ret
    }
}



//------------------------------------------------------------------------------
//
// EXCEPTION_DISPOSITION
// RtlpExecuteHandlerForException (
//    IN PEXCEPTION_RECORD ExceptionRecord,
//    IN PVOID EstablisherFrame,
//    IN OUT PCONTEXT ContextRecord,
//    IN OUT PVOID DispatcherContext,
//    IN PEXCEPTION_ROUTINE ExceptionRoutine
//    )
//
// Routine Description:
//
//    This function allocates a call frame, stores the handler address and
//    establisher frame pointer in the frame, establishes an exception
//    handler, and then calls the specified exception handler as an exception
//    handler. If a nested exception occurs, then the exception handler of
//    of this function is called and the handler address and establisher
//    frame pointer are returned to the exception dispatcher via the dispatcher
//    context parameter. If control is returned to this routine, then the
//    frame is deallocated and the disposition status is returned to the
//    exception dispatcher.
//
// Arguments:
//
//    ExceptionRecord (ebp+8) - Supplies a pointer to an exception record.
//
//    EstablisherFrame (ebp+12) - Supplies the frame pointer of the establisher
//       of the exception handler that is to be called.
//
//    ContextRecord (ebp+16) - Supplies a pointer to a context record.
//
//    DispatcherContext (ebp+20) - Supplies a pointer to the dispatcher context
//       record.
//
//    ExceptionRoutine (ebp+24) - supplies a pointer to the exception handler
//       that is to be called.
//
// Return Value:
//
//    The disposition value returned by the specified exception handler is
//    returned as the function value.
//
//------------------------------------------------------------------------------
EXCEPTION_DISPOSITION __declspec(naked) 
RtlpExecuteHandlerForException(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PVOID EstablisherFrame,
    IN OUT PCONTEXT ContextRecord,
    IN OUT PDISPATCHER_CONTEXT DispatcherContext,
    IN PEXCEPTION_ROUTINE ExceptionRoutine
    ) 
{
    __asm {
        mov     edx,offset ExceptionHandler     // Set who to register
        jmp     ExecuteHandler                  // jump to common code
    }
}



//------------------------------------------------------------------------------
//
// EXCEPTION_DISPOSITION
// UnwindHandler(
//    IN PEXCEPTION_RECORD ExceptionRecord,
//    IN PVOID EstablisherFrame,
//    IN OUT PCONTEXT ContextRecord,
//    IN OUT PVOID DispatcherContext)
//
// Routine Description:
//    This function is called when a collided unwind occurs. Its function
//    is to retrieve the establisher frame pointer and handler address from
//    its establisher's call frame, store this information in the dispatcher
//    context record, and return a disposition value of nested unwind.
//
// Arguments:
//    ExceptionRecord (esp+4) - Supplies a pointer to an exception record.
//
//    EstablisherFrame (esp+8) - Supplies the frame pointer of the establisher
//       of this exception handler.
//
//    ContextRecord (esp+12) - Supplies a pointer to a context record.
//
//    DispatcherContext (esp+16) - Supplies a pointer to the dispatcher context
//       record.
//
// Return Value:
//    A disposition value ExceptionCollidedUnwind is returned if an unwind is
//    in progress. Otherwise a value of ExceptionContinueSearch is returned.
//
//------------------------------------------------------------------------------
Naked 
UnwindHandler(void) 
{
    __asm {
        mov     ecx,dword ptr [esp+4]           // (ecx) -> ExceptionRecord
        test    dword ptr [ecx.ExceptionFlags], EXCEPTION_UNWINDING
        mov     eax,ExceptionContinueSearch     // Assume NOT unwind
        jz      uh10                            // not unwind, go return

// Unwind is in progress - return collided unwind disposition.
        mov     ecx,[esp+8]             // (ecx) -> EstablisherFrame
        mov     edx,[esp+16]            // (edx) -> DispatcherContext
        mov     eax,[ecx+8]             // (eax) -> EstablisherFrame for the
                                        //          handler active when we
                                        //          nested.
        mov     [edx],eax               // Set DispatcherContext field.
        mov     eax,ExceptionCollidedUnwind
uh10:   ret
    }
}



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
//    This function allocates a call frame, stores the handler address and
//    establisher frame pointer in the frame, establishes an exception
//    handler, and then calls the specified exception handler as an unwind
//    handler. If a collided unwind occurs, then the exception handler of
//    of this function is called and the handler address and establisher
//    frame pointer are returned to the unwind dispatcher via the dispatcher
//    context parameter. If control is returned to this routine, then the
//    frame is deallocated and the disposition status is returned to the
//    unwind dispatcher.
//
// Arguments:
//
//    ExceptionRecord (ebp+8) - Supplies a pointer to an exception record.
//
//    EstablisherFrame (ebp+12) - Supplies the frame pointer of the establisher
//       of the exception handler that is to be called.
//
//    ContextRecord (ebp+16) - Supplies a pointer to a context record.
//
//    DispatcherContext (ebp+20) - Supplies a pointer to the dispatcher context
//       record.
//
//    ExceptionRoutine (ebp+24) - supplies a pointer to the exception handler
//       that is to be called.
//
// Return Value:
//
//    The disposition value returned by the specified exception handler is
//    returned as the function value.
//
//------------------------------------------------------------------------------
EXCEPTION_DISPOSITION __declspec(naked) 
RtlpExecuteHandlerForUnwind(
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN PVOID EstablisherFrame,
    IN OUT PCONTEXT ContextRecord,
    IN OUT PDISPATCHER_CONTEXT DispatcherContext,
    IN PEXCEPTION_ROUTINE ExceptionRoutine
    ) 
{
    __asm {
        mov     edx,offset UnwindHandler
        jmp     ExecuteHandler                      // jump to common code
    }
}

#pragma warning(default:4035 4733)


