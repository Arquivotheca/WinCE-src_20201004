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
*stdexcpt.h - User include file for standard exception classes
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       This file is the previous location of the standard exception class
*       definitions, now found in the standard header <exception>.
*
*       [Public]
*
*Revision History:
*       11-15-94  JWM   Made logic & exception classes _CRTIMP
*       11-21-94  JWM   xmsg typedef now #ifdef __RTTI_OLDNAMES
*       02-11-95  CFW   Add _CRTBLD to avoid users getting wrong headers, 
*                       protect with _INC_STDEXCPT.
*       02-14-95  CFW   Clean up Mac merge.
*       02-15-95  JWM   Minor cleanups related to Olympus bug 3716.
*       07-02-95  JWM   Now generally ANSI-compliant; excess baggage removed.
*       12-14-95  JWM   Add "#pragma once".
*       03-04-96  JWM   Replaced by C++ header "exception".
*       02-21-97  GJF   Cleaned out obsolete support for _NTSDK. Also,
*                       detab-ed.
*       01-06-99  GJF   Changes for 64-bit size_t.
*       05-17-99  PML   Remove all Macintosh support.
*       10-06-99  PML   Add _W64 modifier to types which are 32 bits in Win32,
*                       64 bits in Win64.
*       03-17-01  PML   Remove everything under #elif 0, leaving just wrapper.
*
****/

#pragma once

#include <crtdefs.h>

#ifndef _INC_STDEXCPT
#define _INC_STDEXCPT

#ifndef _CRTBLD
/* This version of the header files is NOT for user programs.
 * It is intended for use when building the C runtimes ONLY.
 * The version intended for public use will not have this message.
 */
#error ERROR: Use of C runtime library internal header file.
#endif  /* _CRTBLD */
#ifdef  __cplusplus

#include <exception>

#endif  /* __cplusplus */
#endif  /* _INC_STDEXCPT */
