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
*mbstok_s.c - Break string into tokens (MBCS)
*
*       Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Break string into tokens (MBCS)
*
*Revision History:
*       08-04-04  AC    Created (from mbstok.c, mbstok_s.inl).
*
*******************************************************************************/

#ifdef  _MBCS

#include <mbstring.h>
#include <internal_securecrt.h>
#include <mtdll.h>
#include <setlocal.h>

#define _FUNC_PROLOGUE
#define _FUNC_NAME _mbstok_s_l
#define _SB_FUNC_NAME strtok_s

#include <mbstok_s.inl>

_REDIRECT_TO_L_VERSION_3(unsigned char *, _mbstok_s, unsigned char *, const unsigned char *, unsigned char **)

#endif  /* _MBCS */
