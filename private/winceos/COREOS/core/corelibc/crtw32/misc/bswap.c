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
*rotl.c - rotate an unsigned integer left
*
*	Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*	defines _byteswap() - performs a byteswap on an unsigned integer.
*
*Revision History:
*	09-06-00  GB	Module created
*
*******************************************************************************/

#include <cruntime.h>
#include <stdlib.h>

#pragma function(_byteswap_ulong, _byteswap_uint64, _byteswap_ushort)

/***
*unsigned long _byteswap_ulong(i) - long byteswap
*
*Purpose:
*	Performs a byte swap on an unsigned integer.
*
*Entry:
*	unsigned long i:	value to swap
*
*Exit:
*	returns swaped
*
*Exceptions:
*	None.
*
*******************************************************************************/


unsigned long __cdecl _byteswap_ulong(unsigned long i)
{
    unsigned int j;
    j =  (i << 24);
    j += (i <<  8) & 0x00FF0000;
    j += (i >>  8) & 0x0000FF00;
    j += (i >> 24);
    return j;
}

unsigned short __cdecl _byteswap_ushort(unsigned short i)
{
    unsigned short j;
    j =  (i << 8) ;
    j += (i >> 8) ;
    return j;
}

unsigned __int64 __cdecl _byteswap_uint64(unsigned __int64 i)
{
    unsigned __int64 j;
    j =  (i << 56);
    j += (i << 40)&0x00FF000000000000;
    j += (i << 24)&0x0000FF0000000000;
    j += (i <<  8)&0x000000FF00000000;
    j += (i >>  8)&0x00000000FF000000;
    j += (i >> 24)&0x0000000000FF0000;
    j += (i >> 40)&0x000000000000FF00;
    j += (i >> 56);
    return j;
    
}
