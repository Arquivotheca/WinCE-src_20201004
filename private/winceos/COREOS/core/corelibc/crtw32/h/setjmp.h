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
*setjmp.h - definitions/declarations for setjmp/longjmp routines
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file defines the machine-dependent buffer used by
*       setjmp/longjmp to save and restore the program state, and
*       declarations for those routines.
*       [ANSI/System V]
*
*       [Public]
*
*Revision History:
*       12-11-87  JCR   Added "_loadds" functionality
*       12-18-87  JCR   Added _FAR_ to declarations
*       02-10-88  JCR   Cleaned up white space
*       05-03-89  JCR   Added _INTERNAL_IFSTRIP for relinc usage
*       08-15-89  GJF   Cleanup, now specific to OS/2 2.0 (i.e., 386 flat model)
*       10-30-89  GJF   Fixed copyright
*       11-02-89  JCR   Changed "DLL" to "_DLL"
*       03-01-90  GJF   Added #ifndef _INC_SETJMP and #include <cruntime.h>
*                       stuff. Also, removed some (now) useless preprocessor
*                       directives.
*       04-10-90  GJF   Replaced _cdecl with _CALLTYPE1.
*       05-18-90  GJF   Revised for SEH.
*       10-30-90  GJF   Moved definition of _JBLEN into cruntime.h.
*       02-25-91  SRW   Moved definition of _JBLEN back here [_WIN32_]
*       04-09-91  PNT   Added _MAC_ definitions
*       04-17-91  SRW   Fixed definition of _JBLEN for i386 and MIPS to not
*                       include the * sizeof(int) factor [_WIN32_]
*       05-09-91  GJF   Moved _JBLEN defs back to cruntime.h. Also, turn on
*                       intrinsic _setjmp for Dosx32.
*       08-27-91  GJF   #ifdef out everything for C++.
*       08-29-91  JCR   ANSI naming
*       11-01-91  GDP   MIPS compiler support -- Moved _JBLEN back here
*       01-16-92  GJF   Fixed _JBLEN and map to _setjmp intrinsic for i386
*                       target [_WIN32_].
*       05-08-92  GJF   Changed _JBLEN to support C8-32 (support for C6-386 has
*                       been dropped).
*       08-06-92  GJF   Function calling type and variable type macros. Revised
*                       use of compiler/target processor macros.
*       11-09-92  GJF   Fixed some preprocessing conditionals.
*       01-03-93  SRW   Fold in ALPHA changes
*       01-09-93  SRW   Remove usage of MIPS and ALPHA to conform to ANSI
*                       Use _MIPS_ and _ALPHA_ instead.
*       01-21-93  GJF   Removed support for C6-386's _cdecl.
*       02-20-93  GJF   Per ChuckG and MartinO, setjmp/longjmp to used in
*                       C++ programs.
*       03-23-93  SRW   Change _JBLEN for MIPS in preparation for SetJmpEx
*       04-06-93  SKS   Replace _CRTAPI1/2 with __cdecl, _CRTVAR1 with nothing
*       04-07-93  SKS   Add _CRTIMP keyword for CRT DLL model
*       04-13-93  SKS   Remove _CRTIMP from _setjmp() -- it's an intrinsic
*       04-23-93  SRW   Added _JBTYPE and finalized setjmpex support.
*       06-09-93  SRW   Missing one line in previous merge.
*       10-04-93  SRW   Fix ifdefs for MIPS and ALPHA to only check for
*                       _M_?????? defines
*       10-11-93  GJF   Merged NT and Cuda versions.
*       01-12-93  PML   Increased x86 _JBLEN from 8 to 16.  Added new fields
*                       to _JUMP_BUFFER for use with C9.0.
*       06-16-94  GJF   Fix for MIPS from Steve Hanson (Dolphin bug #13818)
*       10-02-94  BWT   Add PPC support.
*       11-03-94  GJF   Ensure 8 byte alignment.
*       12-29-94  JCF   Merged with mac header.
*       01-13-95  JWM   Added NLG prototypes.
*       02-11-95  CFW   Add _CRTBLD to avoid users getting wrong headers.
*       02-14-95  CFW   Clean up Mac merge.
*       06-23-95  JPM   Use _setjmp with PowerPC VC compiler
*       12-14-95  JWM   Add "#pragma once".
*       04-15-95  BWT   Add _setjmpVfp (setjmp with Virtual Frame Pointer) for MIPS
*       08-13-96  BWT   Redefine _setjmp to _setjmp on MIPS also
*       02-21-97  GJF   Cleaned out obsolete support for _NTSDK. Also, 
*                       detab-ed.
*       09-30-97  JWM   Restored not-so-obsolete _CRTAPI1 support.
*       05-13-99  PML   Remove _CRTAPI1
*       05-17-99  PML   Remove all Macintosh support.
*       10-25-99  PML   Temporarily issue error on _M_CEE (VS7#54572).
*       02-25-00  PML   Remove _M_CEE error (VS7#81945).
*       03-19-01  BWT   Add AMD64 definitions
*       06-13-01  PML   Compile clean -Za -W4 -Tc (vs7#267063)
*       07-15-01  PML   Remove all ALPHA, MIPS, and PPC code
*       08-05-01  GB    Included setjmpex.h when compiling /clr
*       10-10-04  ESC   Use _CRT_PACKING
*       10-18-04  MSL   C++ longjmp throws
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*
****/

#pragma once

#ifndef _INC_SETJMP
#define _INC_SETJMP

#include <crtdefs.h>

#if defined(_M_CEE)
/*
 * The reason why simple setjmp won't work here is that there may
 * be case when CLR stubs are on the stack e.g. function call just
 * after jitting, and not unwinding CLR will result in bad state of
 * CLR which then can AV or do something very bad.
 */
#include <setjmpex.h>
#endif  /* defined(_M_CEE) */

/*
 * Currently, all MS C compilers for Win32 platforms default to 8 byte
 * alignment.
 */
#pragma pack(push,_CRT_PACKING)

#ifdef  __cplusplus
extern "C" {
#endif

/*
 * Definitions specific to particular setjmp implementations.
 */

#if     defined(_M_IX86)

/*
 * MS compiler for x86
 */

#ifndef _INC_SETJMPEX
#define setjmp  _setjmp
#endif

#define _JBLEN  16
#define _JBTYPE int

/*
 * Define jump buffer layout for x86 setjmp/longjmp.
 */
typedef struct __JUMP_BUFFER {
    unsigned long Ebp;
    unsigned long Ebx;
    unsigned long Edi;
    unsigned long Esi;
    unsigned long Esp;
    unsigned long Eip;
    unsigned long Registration;
    unsigned long TryLevel;
    unsigned long Cookie;
    unsigned long UnwindFunc;
    unsigned long UnwindData[6];
} _JUMP_BUFFER;

#ifndef _INTERNAL_IFSTRIP_
#ifdef  __cplusplus
extern "C"
#endif
void __stdcall _NLG_Notify(unsigned long);

#ifdef  __cplusplus
extern "C"
#endif
void __stdcall _NLG_Return();
#endif

#elif defined(_M_X64)

typedef struct _CRT_ALIGN(16) _SETJMP_FLOAT128 {
    unsigned __int64 Part[2];
} SETJMP_FLOAT128;

#define _JBLEN  16
typedef SETJMP_FLOAT128 _JBTYPE;

#ifndef _INC_SETJMPEX
#define setjmp  _setjmp
#endif

typedef struct _JUMP_BUFFER {
    unsigned __int64 Frame;
    unsigned __int64 Rbx;
    unsigned __int64 Rsp;
    unsigned __int64 Rbp;
    unsigned __int64 Rsi;
    unsigned __int64 Rdi;
    unsigned __int64 R12;
    unsigned __int64 R13;
    unsigned __int64 R14;
    unsigned __int64 R15;
    unsigned __int64 Rip;
    unsigned long MxCsr;
    unsigned short FpCsr;
    unsigned short Spare;
    
    SETJMP_FLOAT128 Xmm6;
    SETJMP_FLOAT128 Xmm7;
    SETJMP_FLOAT128 Xmm8;
    SETJMP_FLOAT128 Xmm9;
    SETJMP_FLOAT128 Xmm10;
    SETJMP_FLOAT128 Xmm11;
    SETJMP_FLOAT128 Xmm12;
    SETJMP_FLOAT128 Xmm13;
    SETJMP_FLOAT128 Xmm14;
    SETJMP_FLOAT128 Xmm15;
} _JUMP_BUFFER;

#elif defined(_M_ARM)

#ifndef _INC_SETJMPEX
#define setjmp  _setjmp
#endif

/*
 * ARM setjmp definitions.
 */

#define _JBLEN  28
#define _JBTYPE int

typedef struct _JUMP_BUFFER {
    unsigned long Frame;

    unsigned long R4;
    unsigned long R5;
    unsigned long R6;
    unsigned long R7;
    unsigned long R8;
    unsigned long R9;
    unsigned long R10;
    unsigned long R11;

    unsigned long Sp;
    unsigned long Pc;
    unsigned long Fpscr;
    unsigned long long D[8]; // D8-D15 VFP/NEON regs
} _JUMP_BUFFER;

#endif


/* Define the buffer type for holding the state information */

#ifndef _JMP_BUF_DEFINED
typedef _JBTYPE jmp_buf[_JBLEN];
#define _JMP_BUF_DEFINED

#endif


/* Function prototypes */

int __cdecl setjmp(_Out_ jmp_buf _Buf);

#ifdef  __cplusplus
}
#endif

#ifdef  __cplusplus
#pragma warning(push)
#pragma warning(disable:4987)
extern "C"
{
_CRTIMP __declspec(noreturn) void __cdecl longjmp(_In_ jmp_buf _Buf, _In_ int _Value) throw(...);
}
#pragma warning(pop)
#else
_CRTIMP __declspec(noreturn) void __cdecl longjmp(_In_ jmp_buf _Buf, _In_ int _Value);
#endif

#pragma pack(pop)

#endif  /* _INC_SETJMP */
