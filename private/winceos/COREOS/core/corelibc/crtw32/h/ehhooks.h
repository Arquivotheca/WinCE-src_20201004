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
*ehhook.h - declaration of exception handling hook variables
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Declaration of E.H. hook variables
*
*       [Internal]
*
*Revision History:
*       05-26-93  BS    Module created.
*       09-02-94  SKS   This header file added.
*       12-15-94  XY    merged with mac header
*       02-14-95  CFW   Clean up Mac merge.
*       03-29-95  CFW   Add error message to internal headers.
*       12-14-95  JWM   Add "#pragma once".
*       02-24-97  GJF   Detab-ed.
*       05-17-99  PML   Remove all Macintosh support.
*       11-18-04  ATC   SAL Annotations for CRT Headers
*       03-23-05  MSL   _P removal - not needed
*                       New deprecation warning with function name
*                       Packing fix
*
****/

#pragma once

#ifndef _INC_EHHOOKS
#define _INC_EHHOOKS

#ifndef _CRTBLD
/*
 * This is an internal C runtime header file. It is used when building
 * the C runtimes only. It is not to be used as a public header file.
 */
#error ERROR: Use of C runtime library internal header file.
#endif  /* _CRTBLD */

#include <crtdefs.h>

/* Private Revectoring control for _inconsistency() faults */

typedef void (__cdecl *_inconsistency_function)();
_CRTIMP void __cdecl _inconsistency(void);
_inconsistency_function __cdecl __set_inconsistency(_In_opt_ _inconsistency_function _FuncPtr);


/* Thread specific pointers to handler functions */

#define __pTerminate     (*((terminate_function*) &(_getptd()->_terminate)))
#define __pUnexpected    (*((unexpected_function*) &(_getptd()->_unexpected)))
#define __pSETranslator  (*((_se_translator_function*) &(_getptd()->_translator)))

extern _inconsistency_function __pInconsistency;

//
// This filter needs to be dragged in for any exe which has any eh.
//
LONG WINAPI __CxxUnhandledExceptionFilter( LPEXCEPTION_POINTERS );

#endif  /* _INC_EHHOOKS */
