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

/* see header for more info */

#include "cacheRand.h"

/* common utils for oal tests */
#include "..\..\..\common\commonUtils.h"

DWORD g_dwRandSeed = 0;

void setCacheRandSeed (DWORD dwSeed)
{
    g_dwRandSeed = dwSeed;
}

/*
 * return random DWORD (all DWORDs possible)
 */
DWORD cacheRand ()
{
    DWORD dwFirst = g_dwRandSeed * 214013UL + 2531011UL;
    DWORD dwNext = dwFirst * 214013UL + 2531011UL;
    g_dwRandSeed = dwNext;

    return ((dwFirst >> 16) | (dwNext & 0xffff0000));
}


/*
 * random in the given range.  these are inclusive values.
 *
 * if dwLow == dwHigh, then you get back dwLow on each call.
 * Note the special casing for the total range of DWORDs.  We
 * don't want to overflow on the + 1 addition below.
 *
 * We need to break the range into buckets, and the number of
 * buckets is dwHigh - dwLow + 1 (since we are doing inclusive).
 */
DWORD cacheRandInRange (DWORD dwLow, DWORD dwHigh)
{
    /* not allowed */
    if (dwLow > dwHigh)
    {
        return 0;
    }

    /* special case.  don't want + 1 below to overflow. */
    if (dwHigh == DWORD_MAX && dwLow == 0)
    {
        return (cacheRand());
    }

    DWORD dwRand = cacheRand();

    DWORD result;

    /* there is only one value in this bucket.  we need each bucket
    * to be even (plus / minus 1).  so, we decrement in this case
    * to get it into the bucket below.
    */
    if (dwRand == DWORD_MAX)
    {
        result = dwHigh;
    }
    else
    {
        /* scale the random value number to the range that we need */
        result = (DWORD) (((double) dwRand / (double) DWORD_MAX) *
                       (double) (dwHigh - dwLow + 1));

        /* move this into the range that we are looking for */
        result += dwLow;
    }

    return (result);
}


/*
 * compute a hash for the index plus the salt.
 *
 * we take the salt and the index and xor them.  we
 * then run this through the random number generator.  we take
 * this result and run it through again.  We take the low 16
 * bits of each of these numbers and use them as the low and
 * high bits in the hash, respectively.
 */
DWORD hashIndex (DWORD dwIndex, DWORD dwSalt)
{
    /* set the low part first.  Then grab the high part. */
    DWORD dwVal = (dwIndex ^ dwSalt) * 214013UL + 2531011UL;

    return ((dwVal & 0xffff) |
      ((dwVal * 214013UL + 2531011UL) << 16));
}
