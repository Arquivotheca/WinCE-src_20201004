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
*assert.h - define the assert macro
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Defines the assert(exp) macro.
*       [ANSI/System V]
*
*       [Public]
*
*Revision History:
*       12-18-87  JCR   Added _FAR_ to declarations
*       01-18-88  JCR   Added fflush(stderr) to go with new stderr buffering scheme
*       02-10-88  JCR   Cleaned up white space
*       05-19-88  JCR   Use routine _assert() to save space
*       07-14-88  JCR   Allow user's to enable/disable assert multiple times in
*                       a single module [ANSI]
*       10-19-88  JCR   Revised to also work for the 386 (small model only)
*       12-22-88  JCR   Assert() must be an expression (no 'if' statements)
*       05-03-89  JCR   Added _INTERNAL_IFSTRIP for relinc usage
*       07-27-89  GJF   Cleanup, now specific to the 386
*       10-30-89  GJF   Fixed copyright
*       11-02-89  JCR   Changed "DLL" to "_DLL"
*       02-27-90  GJF   Added #include <cruntime.h> stuff. Also, removed some
*                       (now) useless preprocessor directives.
*       03-21-90  GJF   Replaced _cdecl with _CALLTYPE1 in prototype.
*       07-31-90  SBM   added ((void)0) to NDEBUG definition, now ANSI
*       08-20-91  JCR   C++ and ANSI naming
*       08-26-92  GJF   Function calling type and variable type macros.
*       09-25-92  SRW   Don't use ? in assert macro to keep CFRONT happy.
*       01-21-93  GJF   Removed support for C6-386's _cdecl.
*       02-01-93  GJF   Replaced SteveWo's assert macro with an ANSI-conformant
*                       one. Also got rid of double slash comment characters.
*       04-06-93  SKS   Replace _CRTAPI1/2 with __cdecl, _CRTVAR1 with nothing
*       04-07-93  SKS   Add _CRTIMP keyword for CRT DLL model
*       09-01-93  GJF   Merged Cuda and NT SDK versions.
*       02-11-95  CFW   Add _CRTBLD to avoid users getting wrong headers.
*       02-14-95  CFW   Clean up Mac merge.
*       03-02-95  CFW   Removed _INC_ASSERT. According to ANSI, must be able
*                       to include this file more than once.
*       12-14-95  JWM   Add "#pragma once".
*       12-19-95  JWM   Removed "#pragma once" - ANSI restriction.
*       02-20-97  GJF   Cleaned out obsolete support for _CRTAPI* and _NTSDK.
*                       Also, detab-ed.
*       09-30-97  JWM   Restored not-so-obsolete _CRTAPI1 support.
*       05-13-99  PML   Remove _CRTAPI1
*       05-17-99  PML   Remove all Macintosh support.
*       05-24-02  MSL   Fixed to be type correct, not void *, void *
*       08-28-03  SJ    Added _wassert, CrtSetReportHookW2,CrtDbgReportW, 
*                       & other helper functions. VSWhidbey#55308
*       12-12-03  SJ    Removed the temporary switch provided to revert to 
*                       the old ANSI Assert Behaviour VSW#172434
*       03-08-04  MSL   Added parameter comments
*       04-07-04  MSL   Double slash removal
*       07-20-04  AC    Moved _CRT_WIDE to crtdefs.h
*       09-24-04  MSL   Param names
*       10-18-04  MSL   Fix to allow reinclude with different NDEBUG setting
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       01-14-05  AC    Fix SAL annotations (using prefast espx plug-in)
*       03-23-05  MSL   _P removal - not needed
*       04-14-05  MSL   Prefer op! over op ||
*
****/

#ifndef _CRTBLD
/* This version of the header files is NOT for user programs.
 * It is intended for use when building the C runtimes ONLY.
 * The version intended for public use will not have this message.
 */
#error ERROR: Use of C runtime library internal header file.
#endif  /* _CRTBLD */
#ifndef _INTERNAL_IFSTRIP_
#ifndef _ASSERT_OK
#error assert.h not for CRT internal use, use dbgint.h
#endif  /* _ASSERT_OK */
#include <cruntime.h>
#endif  /* _INTERNAL_IFSTRIP_ */
#include <crtdefs.h>

#undef  assert

#ifdef  NDEBUG

#define assert(_Expression)     ((void)0)

#else

#ifdef  __cplusplus
extern "C" {
#endif

_CRTIMP void __cdecl _wassert(_In_z_ const wchar_t * _Message, _In_z_ const wchar_t *_File, _In_ unsigned _Line);

#ifdef  __cplusplus
}
#endif

#define assert(_Expression) (void)( (!!(_Expression)) || (_wassert(_CRT_WIDE(#_Expression), _CRT_WIDE(__FILE__), __LINE__), 0) )

#endif  /* NDEBUG */
