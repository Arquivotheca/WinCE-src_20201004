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
*mbscat_s.c - Concatenate one string to another (MBCS)
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Concatenate one string to another (MBCS)
*
*Revision History:
*       08-03-04  AC    Module created.
*       08-12-04  AC    Moved _l version to mbscat_s_l.c and added _SYSCRT
*
*******************************************************************************/

#ifdef  _MBCS

#include <mbstring.h>

#ifdef _SYSCRT

#include <internal_safecrt.h>

#define _FUNC_PROLOGUE
#define _FUNC_NAME _mbscat_s
#define _DEST _Dst
#define _SRC _Src

#include <mbscat_s.inl>

#else /* ndef _SYSCRT */

#include <internal.h>

_REDIRECT_TO_L_VERSION_3(errno_t, _mbscat_s, unsigned char *, size_t, const unsigned char *)

#endif /* _SYSCRT */

#endif  /* _MBCS */
