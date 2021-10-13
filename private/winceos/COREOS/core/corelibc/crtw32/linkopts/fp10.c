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
* fp10.c - Set default FP precision to 64 bits (10-byte 'long double')
*
*	Copyright (c) Microsoft Corporation.  All rights reserved.
*
*Purpose:
*
*Revision History:
*   03-23-93  JWM	created file
*
*******************************************************************************/
#include <float.h>
#include <internal.h>

void  _setdefaultprecision(void);

/*
 * Routine to set default FP precision to 64 bits, used to override
 * standard 53-bit precision version in fpinit.c.
 */
 
void _setdefaultprecision()
{
#ifndef _M_X64
    _ERRCHECK(_controlfp_s(NULL, _PC_64, _MCW_PC));
#endif
}

