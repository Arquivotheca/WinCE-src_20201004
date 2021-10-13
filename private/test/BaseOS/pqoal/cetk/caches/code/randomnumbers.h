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
/*
 * randomNumbers.h
 */

#ifndef __RANDOM_NUMBERS_H
#define __RANDOM_NUMBERS_H

#include "..\..\..\common\disableWarnings.h"
#include <windows.h>


/*
 * get the next value in a simi random way.  This randomizes values by
 * counting differently.  We use the digits 0 through 3, and order
 * them like follows:
 *
 *     0 -> 3 -> 1 -> 2
 *
 * This serves to mix the values a little bit but still preserve some
 * locality.
 *
 * Addition occurs much like standard arithmetic.  The result is a
 * function that will loop through every value less than a given power
 * of two once and only once.
 *
 * dwCurrent is the current value from which the next one is supposed
 * to be calculated.  dwNumBits is the number of bits in this value
 * that is valid.
 *
 * This returns the next number in the sequence.  These values will
 * overflow, and overflow is allowed to happen, much like standard alu
 * operations.
 */
DWORD
getNextMixed (DWORD dwCurrent, DWORD dwNumBits);

#endif
