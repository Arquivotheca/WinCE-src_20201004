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
*eh.h - User include file for exception handling.
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       User include file for exception handling.
*
*       [Public]
*
*Revision History:
*       10-12-93  BSC   Module created.
*       11-04-93  CFW   Converted to CRT format.
*       11-03-94  GJF   Ensure 8 byte alignment. Also, changed nested include
*                       macro to match our naming convention.
*       12-15-94  XY    merged with mac header
*       02-11-95  CFW   Add _CRTBLD to avoid users getting wrong headers.
*       02-14-95  CFW   Clean up Mac merge.
*       12-14-95  JWM   Add "#pragma once".
*       02-20-97  GJF   Cleaned out obsolete support for _CRTAPI* and _NTSDK.
*                       Replaced !defined(_M_MPPC) && !defined(_M_M68K) with
*                       !defined(_MAC). Also, detab-ed.
*       09-30-97  JWM   Restored not-so-obsolete _CRTAPI1 support.
*       05-13-99  PML   Remove _CRTAPI1
*       05-17-99  PML   Remove all Macintosh support.
*       12-10-99  GB    Added __uncaught_exception().
*       09-22-04  AGH   Guard header with #ifndef RC_INVOKED, so RC ignores it
*       10-01-04  SJ    Added function _is_exception_typeof
*       10-10-04  ESC   Use _CRT_PACKING
*       10-20-04  MAL   Declare terminate() as __declspec(noreturn) for prefast
*       11-07-04  MSL   Allow get terminate and unexpected
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*                       Cleaned up mixed and pure support for set_terminate and set_unexpected
*                       Removed managfed se_translator
*       04-01-05  JMK   misc.
*       04-14-05  MSL   Standard-conformant managed termination types
*       05-27-05  MSL   Fixed NULL overloads for 64 bit definition of NULL
*       06-03-05  MSL   Revert 64 bit definition of NULL due to calling-convention fix
*       04-20-07  GM    Removed __Detect_msvcrt_and_setup().
*                       
*
****/

#pragma once

#include <crtdefs.h>

#ifndef _INC_EH
#define _INC_EH
#ifndef RC_INVOKED
#ifndef _CRTBLD
/* This version of the header files is NOT for user programs.
 * It is intended for use when building the C runtimes ONLY.
 * The version intended for public use will not have this message.
 */
#error ERROR: Use of C runtime library internal header file.
#endif  /* _CRTBLD */

/* Currently, all MS C compilers for Win32 platforms default to 8 byte 
 * alignment.
 */
#pragma pack(push,_CRT_PACKING)

#ifndef __cplusplus
#error "eh.h is only for C++!"
#endif

/* terminate_handler is the standard name; terminate_function is supported for historical reasons */
#ifndef _M_CEE_PURE
typedef void (__cdecl *terminate_function)();
typedef void (__cdecl *terminate_handler)();
typedef void (__cdecl *unexpected_function)();
typedef void (__cdecl *unexpected_handler)();
#else
typedef void (__clrcall *terminate_function)();
typedef void (__clrcall *terminate_handler)();
typedef void (__clrcall *unexpected_function)();
typedef void (__clrcall *unexpected_handler)();
#endif

#ifdef _M_CEE
typedef void (__clrcall *__terminate_function_m)();
typedef void (__clrcall *__terminate_handler_m)();
typedef void (__clrcall *__unexpected_function_m)();
typedef void (__clrcall *__unexpected_handler_m)();
#endif

struct _EXCEPTION_POINTERS;
#ifndef _M_CEE_PURE
typedef void (__cdecl *_se_translator_function)(unsigned int, struct _EXCEPTION_POINTERS*);
#endif

_CRTIMP __declspec(noreturn) void __cdecl terminate(void);
_CRTIMP __declspec(noreturn) void __cdecl unexpected(void);

_CRTIMP int __cdecl _is_exception_typeof(_In_ const type_info &_Type, _In_ struct _EXCEPTION_POINTERS * _ExceptionPtr);

#ifndef _M_CEE_PURE
/* only __clrcall versions provided by the MRT exist in pure */
_CRTIMP terminate_function __cdecl set_terminate(_In_opt_ terminate_function _NewPtFunc);
extern "C" _CRTIMP terminate_function __cdecl _get_terminate(void);
_CRTIMP unexpected_function __cdecl set_unexpected(_In_opt_ unexpected_function _NewPtFunc);
extern "C" _CRTIMP unexpected_function __cdecl _get_unexpected(void);
#endif

#ifndef _M_CEE_PURE
/* set_se_translator cannot be a managed implementation, and so cannot be called from _M_CEE_PURE code */
_CRTIMP _se_translator_function __cdecl _set_se_translator(_In_opt_ _se_translator_function _NewPtFunc);
#endif
_CRTIMP bool __cdecl __uncaught_exception();

/*
 * These overload helps in resolving NULL
 */
#ifdef _M_CEE
_CRTIMP terminate_function __cdecl set_terminate(_In_ int _Zero);
_CRTIMP unexpected_function __cdecl set_unexpected(_In_ int _Zero);
#endif

#pragma pack(pop)
#endif /* RC_INVOKED */
#endif  /* _INC_EH */
