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
*wtox.c - _wtoi and _wtol conversion
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       Converts a wide character string into an int or long.
*
*Revision History:
*       09-10-93  CFW   Module created, based on ASCII version.
*       10-07-93  CFW   Optimize WideCharToMultiByte, use NULL default char.
*       02-07-94  CFW   POSIXify.
*       03-13-95  CFW   Use -1 for length since NT compares past NULLS.
*       01-19-96  BWT   Add __int64 versions.
*       05-13-96  BWT   Fix _NTSUBSET_ version
*       11-03-76  JWM   Fix buffer-size bug in __int64 version.
*       05-23-00  GB    Using ascii version with tchar macros for UNICODE
*                       version.
*
*******************************************************************************/

#ifndef _POSIX_

#ifndef _UNICODE
#define _UNICODE
#endif

#include <wchar.h>
#include "atox.c"
#endif  /* _POSIX_ */
