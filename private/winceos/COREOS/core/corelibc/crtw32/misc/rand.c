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
*rand.c - random number generator
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       defines rand(), srand() - random number generator
*
*******************************************************************************/
#include <cruntime.h>

/***
*void srand(seed) - seed the random number generator
*
*Purpose:
*       Seeds the random number generator with the int given.  Adapted from the
*       BASIC random number generator.
*
*Entry:
*       unsigned seed - seed to seed rand # generator with
*
*Exit:
*       None.
*
*Exceptions:
*
*******************************************************************************/
void
__cdecl
srand(
    unsigned int seed
    )
    {
    long * prand = __crt_get_storage_rand();
    if (prand != NULL)
        {
        *prand = (long)seed;
        }
    }

/***
*int rand() - returns a random number
*
*Purpose:
*       returns a pseudo-random number 0 through 32767.
*
*Entry:
*       None.
*
*Exit:
*       Returns a pseudo-random number 0 through 32767.
*
*Exceptions:
*
*******************************************************************************/
int
__cdecl
rand(void)
    {
    long * prand = __crt_get_storage_rand();
    if (prand != NULL)
        {
        return (int)(((*prand = *prand * 214013L + 2531011L) >> 16) & 0x7fff);
        }
    return 0;
    }

