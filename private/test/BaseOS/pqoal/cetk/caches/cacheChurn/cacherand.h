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
//

#ifndef __CACHE_RAND_H__
#define __CACHE_RAND_H__

#include <windows.h>

/*
 * rand functions needed by this code.
 *
 * we would just use the what the kernel provides, but these require
 * stepping across the PSL boundary when calling.  this will put pressure
 * on the caches and might cause TLB flush on some procs, which we don't
 * want to do.  we need a set of routines that we can statically link
 * against.
 *
 * Furthermore, we need slightly different semantics.
 */

/*
 * set seed for cacheRand and cacheRandInRange
 */
void setCacheRandSeed (DWORD dwSeed);


/*
 * return random DWORD (all DWORDs possible)
 */
DWORD cacheRand ();

/*
 * random in the given range.  these are inclusive values.
 */
DWORD cacheRandInRange (DWORD dwLow, DWORD dwHigh);

/*
 * hash an index
 */
DWORD hashIndex (DWORD dwIndex, DWORD dwSalt);

#endif




