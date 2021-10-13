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
*wcscat_s.c - contains wcscat_s()
*
*Purpose:
*   wcscat_s() appends one wchar_t string onto another.
*
*   wcscat() concatenates (appends) a copy of the source string to the
*   end of the destination string.
*   Strings are wide-character strings.
*
*******************************************************************************/

#ifndef _POSIX_

#include <string.h>
#include <internal_securecrt.h>

#define _FUNC_PROLOGUE
#define _FUNC_NAME wcscat_s
#define _CHAR wchar_t
#define _DEST _Dst
#define _SIZE _SizeInWords
#define _SRC _Src

#include <tcscat_s.inl>

#endif /* _POSIX_ */
