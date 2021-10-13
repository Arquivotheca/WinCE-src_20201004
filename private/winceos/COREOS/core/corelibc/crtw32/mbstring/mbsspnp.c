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
*mbsspnp.c - Find first string char in charset, pointer return (MBCS)
*
*	Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*       Returns maximum leading segment of string consisting solely
*	of characters from charset.  Handles MBCS characters correctly.
*
*Revision History:
*	11-19-92  KRS	Ported from 16-bit sources.
*
*******************************************************************************/

#ifdef _MBCS
#define _RETURN_PTR
#include "mbsspn.c"
#endif	/* _MBCS */
