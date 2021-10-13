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
*wcsnset_s.c - contains wcsnset_s()
*
*Purpose:
*   wcsnset_s() sets at most the first n characters of a string
*   to a given character; ensure null termination.
*
*******************************************************************************/

#ifndef _POSIX_

#include <string.h>
#include <internal_securecrt.h>

#define _FUNC_PROLOGUE
#define _FUNC_NAME _wcsnset_s
#define _CHAR wchar_t
#define _CHAR_INT wchar_t
#define _DEST _Dst
#define _SIZE _SizeInWords
#define _COUNT _Count

#include <tcsnset_s.inl>

#endif /* _POSIX_ */
