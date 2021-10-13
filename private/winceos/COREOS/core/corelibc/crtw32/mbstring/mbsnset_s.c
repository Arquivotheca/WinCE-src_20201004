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
*mbsnset_s.c - Sets first n characters of string to given character (MBCS)
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Sets first n characters of string to given character (MBCS)
*
*Revision History:
*       08-03-04  AC    Module created.
*       08-12-04  AC    Moved _l version to mbsnset_s_l.c and added _SYSCRT
*
*******************************************************************************/

#ifdef  _MBCS

#include <mbstring.h>

#ifdef _SYSCRT

#include <internal_safecrt.h>

#define _FUNC_PROLOGUE
#define _FUNC_NAME _mbsnset_s
#define _DEST _Dst
#define _COUNT _CountInChars
#define _COUNT_IN_BYTES 0

#include <mbsnset_s.inl>

#else /* ndef _SYSCRT */

#include <internal.h>

_REDIRECT_TO_L_VERSION_4(errno_t, _mbsnset_s, unsigned char *, size_t, unsigned int, size_t)

#endif /* _SYSCRT */

#endif  /* _MBCS */
