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
*mbspbrk.c - Find first string char in charset, pointer return (MBCS)
*
*	Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*	Find first string char in charset, pointer return (MBCS)
*	Shares common source file with mbscspn.c.
*
*Revision History:
*	11-19-92  KRS	Ported from 16-bit sources.
*
*******************************************************************************/

#ifdef _MBCS
#define _RETURN_PTR
#include "mbscspn.c"
#endif	/* _MBCS */
