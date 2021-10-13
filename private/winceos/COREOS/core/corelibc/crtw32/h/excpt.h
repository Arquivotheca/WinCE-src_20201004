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
/***
*excpt.h - defines exception values, types and routines
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file contains the definitions and prototypes for the compiler-
*       dependent intrinsics, support functions and keywords which implement
*       the structured exception handling extensions.
*
*       [Public]
*
*Revision History:
*       11-01-91  GJF   Module created. Basically a synthesis of except.h
*                       and excpt.h and intended as a replacement for
*                       both.
*       12-13-91  GJF   Fixed build for Win32.
*       05-05-92  SRW   C8 wants C6 style names for now.
*       07-20-92  SRW   Moved from winxcpt.h to excpt.h
*       08-06-92  GJF   Function calling type and variable type macros. Also
*                       revised compiler/target processor macro usage.
*       11-09-92  GJF   Fixed preprocessing conditionals for MIPS. Also,
*                       fixed some compiler warning (fix from/for RichardS).
*       01-03-93  SRW   Fold in ALPHA changes
*       01-04-93  SRW   Add leave keyword for x86
*       01-09-93  SRW   Remove usage of MIPS and ALPHA to conform to ANSI
*                       Use _MIPS_ and _ALPHA_ instead.
*       01-21-93  GJF   Removed support for C6-386's _cdecl.
*       02-18-93  GJF   Changed _try to __try, etc.
*       03-31-93  CFW   Removed #define try, except, leave, finally for x86.
*       04-06-93  SKS   Replace _CRTAPI1/2 with __cdecl, _CRTVAR1 with nothing
*       07-29-93  GJF   Added declarations for _First_FPE_Indx and _Num_FPE.
*       09-01-93  GJF   Merged Cuda and NT SDK versions.
*       10-04-93  SRW   Fix ifdefs for MIPS and ALPHA to only check for _M_??????
*                       defines
*       10-12-93  GJF   Merged again.
*       10-19-93  GJF   MS/MIPS compiler gets most of the same SEH defs and
*                       decls as the MS compiler for the X86.
*       10-29-93  GJF   Don't #define try, et al, when compiling C++ app!
*       12-09-93  GJF   Alpha compiler now has MS front-end and implements
*                       the same SEH names.
*       11-03-94  GJF   Ensure 8 byte alignment.
*       02-11-95  CFW   Add _CRTBLD to avoid users getting wrong headers.
*       02-14-95  CFW   Clean up Mac merge.
*       12-14-95  JWM   Add "#pragma once".
*       02-21-97  GJF   Cleaned out obsolete support for _CRTAPI* and _NTSDK.
*                       Also, detab-ed.
*       09-30-97  JWM   Restored not-so-obsolete _CRTAPI1 support.
*       11-12-97  RDL   __C_specific_handler() prototype change from SC.
*       05-13-99  PML   Remove _CRTAPI1
*       05-17-99  PML   Remove all Macintosh support.
*       06-13-01  PML   Compile clean -Za -W4 -Tc (vs7#267063)
*       07-15-01  PML   Remove all ALPHA, MIPS, and PPC code
*       05-30-02  GB    Added __CppXcptFilter which is calls _XcptFilter only
*                       for C++ exceptions.
*       03-03-03  EAN   Added AMD64 declarations for _EXCEPTION_RECORD and _CONTEXT
*       10-10-04  ESC   Use _CRT_PACKING
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       01-14-05  AC    Fix SAL annotations (using prefast espx plug-in)
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*       07-18-07  ATC   ddb#125490.  Making _xcptActTabl const.
*
****/

#pragma once

#ifndef _INC_EXCPT
#define _INC_EXCPT

#include <crtdefs.h>

/*
 * Currently, all MS C compilers for Win32 platforms default to 8 byte
 * alignment.
 */
#pragma pack(push,_CRT_PACKING)

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * Exception disposition return values.
 */
typedef enum _EXCEPTION_DISPOSITION {
    ExceptionContinueExecution,
    ExceptionContinueSearch,
    ExceptionNestedException,
    ExceptionCollidedUnwind,
#ifdef _WIN32_WCE
    ExceptionExecuteHandler,
#endif
} EXCEPTION_DISPOSITION;


/*
 * Prototype for SEH support function.
 */

#ifdef _WIN32_WCE

#ifndef _DWORD_DEFINED
#define _DWORD_DEFINED
typedef unsigned long DWORD;
#endif  /* _DWORD_DEFINED */

#if !defined(MIDL_PASS)
typedef struct _CONTEXT *PCONTEXT;
typedef struct _EXCEPTION_RECORD *PEXCEPTION_RECORD;
typedef struct _EXCEPTION_POINTERS *PEXCEPTION_POINTERS;
typedef struct _EXCEPTION_REGISTRATION_RECORD *PEXCEPTION_REGISTRATION_RECORD;
typedef PCONTEXT LPCONTEXT;
typedef PEXCEPTION_RECORD LPEXCEPTION_RECORD;
typedef PEXCEPTION_POINTERS LPEXCEPTION_POINTERS;
typedef struct _DISPATCHER_CONTEXT DISPATCHER_CONTEXT, *PDISPATCHER_CONTEXT;
#endif

typedef
EXCEPTION_DISPOSITION
EXCEPTION_ROUTINE (
    _Inout_ struct _EXCEPTION_RECORD *ExceptionRecord,
    _In_ void *EstablisherFrame,
    _Inout_ struct _CONTEXT *ContextRecord,
    _In_ void *DispatcherContext
);
typedef EXCEPTION_ROUTINE *PEXCEPTION_ROUTINE;

#if !defined(MIDL_PASS)
#if defined(_M_IX86)
EXCEPTION_ROUTINE _except_handler3;
#else
EXCEPTION_ROUTINE __C_specific_handler;

//
// Define ARM exception handling structures and function prototypes.
//
// Function table entry structure definition.
//

typedef struct _RUNTIME_FUNCTION {
    DWORD BeginAddress;  
    DWORD UnwindData;  
} RUNTIME_FUNCTION, *PRUNTIME_FUNCTION;

//
// Scope table structure definition.
//

typedef struct _SCOPE_TABLE {
    DWORD Count;
    struct
    {
        DWORD BeginAddress;
        DWORD EndAddress;
        DWORD HandlerAddress;
        DWORD JumpTarget;
    } ScopeRecord[1];
} SCOPE_TABLE, *PSCOPE_TABLE;

#endif

//TODO: VS11_PORTING -- temporary copy from winnt.h

struct _DISPATCHER_CONTEXT {
#if defined(_M_IX86)
    struct _EXCEPTION_REGISTRATION_RECORD *RegistrationPointer;
    DWORD ControlPc;
#elif defined(_M_ARM)
    DWORD ControlPc;
    DWORD ImageBase;
    PRUNTIME_FUNCTION FunctionEntry;
    DWORD EstablisherFrame;
    DWORD TargetPc;
    PCONTEXT ContextRecord;
    PEXCEPTION_ROUTINE LanguageHandler;
    void *HandlerData;
    DWORD ScopeIndex;
    DWORD *NonVolatileRegisters;
#endif
};

#endif

#elif defined(_M_IX86)

/*
 * Declarations to keep MS C 8 (386/486) compiler happy
 */
struct _EXCEPTION_RECORD;
struct _CONTEXT;

EXCEPTION_DISPOSITION __cdecl _except_handler (
    _In_ struct _EXCEPTION_RECORD *_ExceptionRecord,
    _In_ void * _EstablisherFrame,
    _Inout_ struct _CONTEXT *_ContextRecord,
    _Inout_ void * _DispatcherContext
    );

#elif   defined(_M_X64) || defined(_M_ARM)

/*
 * Declarations to keep AMD64 compiler happy
 */
struct _EXCEPTION_RECORD;
struct _CONTEXT;
struct _DISPATCHER_CONTEXT;

#ifndef _M_CEE_PURE

_CRTIMP EXCEPTION_DISPOSITION __C_specific_handler (
    _In_ struct _EXCEPTION_RECORD * ExceptionRecord,
    _In_ void * EstablisherFrame,
    _Inout_ struct _CONTEXT * ContextRecord,
    _Inout_ struct _DISPATCHER_CONTEXT * DispatcherContext
);

#endif

#endif



/*
 * Keywords and intrinsics for SEH
 */

#define GetExceptionCode            _exception_code
#define exception_code              _exception_code
#define GetExceptionInformation     (struct _EXCEPTION_POINTERS *)_exception_info
#define exception_info              (struct _EXCEPTION_POINTERS *)_exception_info
#define AbnormalTermination         _abnormal_termination
#define abnormal_termination        _abnormal_termination

unsigned long __cdecl _exception_code(void);
void *        __cdecl _exception_info(void);
int           __cdecl _abnormal_termination(void);


/*
 * Legal values for expression in except().
 */

#define EXCEPTION_EXECUTE_HANDLER       1
#define EXCEPTION_CONTINUE_SEARCH       0
#define EXCEPTION_CONTINUE_EXECUTION    -1


#ifdef _WIN32_WCE
int __cdecl UnhandledExceptionFilter(struct _EXCEPTION_POINTERS*);
#endif

#ifndef _INTERNAL_IFSTRIP_
/*
 * for convenience, define a type name for a pointer to signal-handler
 */

typedef void (__cdecl * _PHNDLR)(int);

/*
 * Exception-action table used by the C runtime to identify and dispose of
 * exceptions corresponding to C runtime errors or C signals.
 */
struct _XCPT_ACTION {

    /*
     * exception code or number. defined by the host OS.
     */
    unsigned long XcptNum;

    /*
     * signal code or number. defined by the C runtime.
     */
    int SigNum;

    /*
     * exception action code. either a special code or the address of
     * a handler function. always determines how the exception filter
     * should dispose of the exception.
     */
    _PHNDLR XcptAction;
};

extern const struct _XCPT_ACTION _XcptActTab[];

/*
 * number of entries in the exception-action table
 */
extern const int _XcptActTabCount;

/*
 * size of exception-action table (in bytes)
 */
extern const int _XcptActTabSize;

/*
 * index of the first floating point exception entry
 */
extern const int _First_FPE_Indx;

/*
 * number of FPE entries
 */
extern const int _Num_FPE;

/*
 * return values and prototype for the exception filter function used in the
 * C startup
 */
int __cdecl __CppXcptFilter(_In_ unsigned long _ExceptionNum, _In_ struct _EXCEPTION_POINTERS * _ExceptionPtr);
int __cdecl _XcptFilter(_In_ unsigned long _ExceptionNum, _In_ struct _EXCEPTION_POINTERS * _ExceptionPtr);

#endif  /* _INTERNAL_IFSTRIP_ */

#ifdef  __cplusplus
}
#endif

#pragma pack(pop)

#endif  /* _INC_EXCPT */
