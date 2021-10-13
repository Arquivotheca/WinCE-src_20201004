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
*mbsncat_s.c - concatenate string2 onto string1, max length n
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       defines mbsncat_s() - concatenate maximum of n characters
*
*Revision History:
*       10-14-03  AC    Module created.
*       03-10-04  AC    Return ERANGE when buffer is too small
*       03-29-04  AC    Allow and short-circuit the case dst == NULL && sizeInBytes == 0 && cnt == 0
*                       VSW#239396
*       04-07-04  MSL   Fixed bad call to mbstype with empty strings
*                       VSW#274124
*       08-03-04  AC    Moved the implementation in tcsncat.inl.
*       08-12-04  AC    Moved _l version to mbsncat_s_l.c and added _SYSCRT
*
*******************************************************************************/

#ifdef  _MBCS

#include <mbstring.h>

#ifdef _SYSCRT

#include <internal_safecrt.h>

#define _FUNC_PROLOGUE
#define _FUNC_NAME _mbsncat_s
#define _DEST _Dst
#define _SRC _Src
#define _COUNT _CountInChars
#define _COUNT_IN_BYTES 0

#include <mbsncat_s.inl>

#else /* ndef _SYSCRT */

#include <internal.h>

_REDIRECT_TO_L_VERSION_4(errno_t, _mbsncat_s, unsigned char *, size_t, const unsigned char *, size_t)

#endif /* _SYSCRT */

#endif  /* _MBCS */
