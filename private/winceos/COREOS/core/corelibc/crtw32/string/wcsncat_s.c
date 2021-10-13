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
*wcsncat_s.c - append n chars of string to new string
*
*   Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*   defines wcsncat_s() - appends n characters of string onto
*   end of other string
*
*Revision History:
*   10-08-03  AC   Module created.
*   08-03-04  AC   Moved the implementation in tcsncat.inl.
*
*******************************************************************************/

#ifndef _POSIX_

#include <string.h>
#include <internal_securecrt.h>

#define _FUNC_PROLOGUE
#define _FUNC_NAME wcsncat_s
#define _CHAR wchar_t
#define _DEST _Dst
#define _SIZE _SizeInWords
#define _SRC _Src
#define _COUNT _Count

#include <tcsncat_s.inl>

#endif /* _POSIX_ */
