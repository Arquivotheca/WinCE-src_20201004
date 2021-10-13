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
 * cacheTest.h
 */

#ifndef __CACHE_TEST_H
#define __CACHE_TEST_H

#include "..\..\..\common\disableWarnings.h"
#include <windows.h>

/*
 * We want the actual test code to always be optimized, even if we are
 * in debug mode.  With no optimizations enabled the compiler produces
 * code that writes each register, as it changes, out to memory.  This
 * puts undue pressure on the cache, which is problematic since we are
 * trying to access it only as part of the test.  Enabling
 * optimizations on the fly fixes this.
 *
 * We enable space optimizations because we don't want the compiler to
 * optimize for time, which generally means modifying memory access
 * patterns.
 *
 * If you are debugging and _know_ that you want to disable the
 * optimizations for these tests then undefine this variable.  This
 * will cause optimization to be dictated by the command line
 * arguments passed to the compiler.  Note that disabling this define
 * won't give you unoptimized code for retail builds.
 */
#define ALWAYS_OPTIMIZE
//#undef ALWAYS_OPTIMIZE

/*
 * Test the cache.
 *
 * This function tests the cache by walking sequentially through an
 * array, writing values and then reading them back.
 *
 * vals is a chunk of DWORDs that is dwArraySize.  dwIterations is the
 * number of times to run through the values.
 *
 * This function returns true on success and false with printed error
 * messages on failure.
 */
BOOL
cacheTestWriteEverythingReadEverything(__inout_ecount(dwArraySize) volatile DWORD * vals, DWORD dwArraySize, DWORD dwIterations);

/*
 * This function tests the cache by writing values into array elements
 * with indexes near multiples of dwStep.  Specifically, if n is a
 * multiple of dwStep, this writes the values n - 1, n, and n + 1.
 * Care is taken to not write off the beginning or end of the array.
 * This sequence is done in increasing order first.  Once the values
 * have been written, the values in the array are checked for
 * correctness.  At this point the function moves backward through the
 * array, doing much the same process.  This process is repeated for
 * dwIterations.
 *
 * The values that aren't going to be touched by this test are written
 * to known values before the test begins.  They can then be checked
 * after each write cycle for correctness.
 *
 * vals is the array to be written and read from.  dwArraySize is the
 * length of this array (in DWORDS).  dwMaxIterations is the number of
 * times to do the read-write cycle.  dwStep is the step size to use.
 *
 * The length of the array must be at least 8 DWORDS.
 */
BOOL
cacheTestWriteBoundariesReadEverything(__inout_ecount(dwArraySize) volatile DWORD * vals, DWORD dwArraySize, DWORD dwMaxIterations, DWORD dwStep);

/*
 * This function tests the cache, focusing on memory accesses around
 * indexes close to powers of 2.  In general, most bugs occur on these
 * boundaries and therefore these areas should get more stress than
 * the other areas.
 *
 * This test loops through the array with an ever increasing step
 * size.  The step size doubles each time.  A unique value is written
 * into each value on each iteration.  These values are checked after
 * the given set of values has been filled in the array.  If any of
 * them differ from what they should have said then we return an
 * error, with debug output to help the user figure out where the
 * error is.
 *
 * vals is a pointer to an array of DWORD of length dwArraySize.
 * dwArraySize is the size of the array, which must be at least 8 and
 * a power of two.  dwNumIterations is the number of iteratations to
 * do the entire cycle.
 *
 * For the checking algorithm to work, getTracking must not repeat
 * values for a given iteration of the for loop.  In practice, this
 * means that its period must be larger than the array size.
 *
 * This function returns true on success and false, with error messages,
 * if errors are found.
 */
BOOL
cacheTestVaryPowersOfTwo(__inout_ecount(dwArraySize) volatile DWORD * vals, DWORD dwArraySize,
             DWORD dwNumIterations);

/*
 * This function reads and writes all values in the array in a
 * semi-random order.
 *
 * Vals is the array to written and read.  dwArraySize is the size of
 * this array and must be a power of 2.  dwIterations is the number of
 * write-read cycles to perform.
 *
 * The walking order is determined by the getNextMixed function in
 * randomNumbers.h.
 *
 * Return TRUE on success and FALSE otherwise.
 */
BOOL
cacheTestWriteEverythingReadEverythingMixedUp(__inout_ecount(dwArraySize) volatile DWORD * vals,
                          DWORD dwArraySize,
                          DWORD dwIterations);


#endif
