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
*strcpy_s.c - contains wcscpy_s()
*
*Purpose:
*   wcscpy_s() copies one string onto another.
*
*******************************************************************************/

#ifndef _POSIX_

#include <string.h>
#include <internal_securecrt.h>

#define _FUNC_PROLOGUE
#define _FUNC_NAME wcscpy_s
#define _CHAR wchar_t
#define _DEST _Dst
#define _SIZE _SizeInWords
#define _SRC _Src

#include <tcscpy_s.inl>

#endif /* _POSIX_ */
