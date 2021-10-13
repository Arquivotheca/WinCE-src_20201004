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
 * tuxcaches.h
 */

#ifndef __TUX_CACHES_H__
#define __TUX_CACHES_H__

#include "..\..\..\common\disableWarnings.h"

#include <windows.h>
#include <tux.h>

/*
 * This file contains the TESTPROCs for the cache test functions.
 * Anything needed in the tux function table must be included here.
 * This file is include by the file with the tux function table.
 *
 * Please keep non-tux related stuff (stuff local to the caches) out
 * of this file to keep from polluting the name space.
 *
 * Each of the functions below that tests the cache takes an argument
 * from the tux function table.  This is passed in as part of the
 * LPFUNCTION_TABLE_ENTRY.  This argument is a DWORD that specifies
 * the array size to use for the test and the mode that the cache is
 * to be put into.
 * 
 * This argument is a bit mask telling which methods are being used.
 *
 * These are defined in cacheControlModes.h.  They are used for other
 * code and therefore are not defined in this header file.
 */
#include "cacheControlModes.h"

/*
 * The functions also take optional command line arguments.  The two
 * currently defined are 
 *
 * -runTime <minutes>  run time for the test in minutes
 * -arrayLen <array length>  array length in bytes
 *
 * -runTime is valid for all cache tests except for printCacheInfo.
 * -arrayLen is valid only for the user defined array sizes.
 */

/**************************************************************************
 * 
 *                           Cache Test Functions
 *
 **************************************************************************/

/*
 * Print cache information, as returned by CeGetCacheInfo.
 *
 * This test always passes, unless the function returns abnormally.
 */
TESTPROCAPI printCacheInfo(
                         UINT uMsg, 
                         TPPARAM tpParam, 
                         LPFUNCTION_TABLE_ENTRY lpFTE);

/*
 * This function tests the cache by writing values into array elements
 * with indexes near multiples of 8.  Specifically, if n is a multiple
 * of 8, this writes into the values n - 1, n, and n + 1.  Care is
 * taken to not write off the beginning or end of the array.  This
 * sequence is done in increasing order first.  Once the values have
 * been written the values in the array are checked for correctness.
 * At this point the function moves backward through the array, doing
 * much the same process.  The values are then read back to verify
 * correctness.  This process is repeated for the time given for the
 * test.
 *
 * The values that aren't going to be touched by this test are written
 * to known values before the test begins.  They can then be checked
 * after each write cycle for correctness.
 *
 * The length of the array is taken from the dwUserData field in lpFTE.  
 * This length follows the specifications set above.  This length must 
 * be at least 8 DWORDS, which is 32 bytes.
 */
TESTPROCAPI 
entryCacheTestWriteBoundariesReadEverything(
                        UINT uMsg, 
                        TPPARAM tpParam, 
                        LPFUNCTION_TABLE_ENTRY lpFTE);

/*
 * This function tests the cache by walking sequentially through an array,
 * writing values and then reading them back.
 *
 * The length of the array is taken from the dwUserData field in lpFTE.  
 * This length follows the specifications set above.  This length must 
 * be at least 4 bytes.
 */
TESTPROCAPI
entryCacheTestWriteEverythingReadEverything(
             UINT uMsg, 
             TPPARAM tpParam, 
             LPFUNCTION_TABLE_ENTRY lpFTE);

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
 * This function doesn't check the non-written values in the array for
 * correctness.  This type of testing is covered in other tests.
 *
 * The length of the array is taken from the dwUserData field in the
 * lpFTE.  This must be at least 8 DWORDs, or 32 bytes.
 */
TESTPROCAPI
entryCacheTestVaryPowersOfTwo(
                  UINT uMsg, 
                  TPPARAM tpParam, 
                  LPFUNCTION_TABLE_ENTRY lpFTE);

/*
 * This function writes and verifies all values in the array.  It
 * semi-randomly accesses these values, hopefully exercising bugs that
 * the sequential test would not find.  The access algorithm isn't
 * entirely random, and the locality should produce a mix of cache
 * hits and cache misses.
 *
 * The length of the array, as taken from the dwUserData field of the
 * lpFTE, must be at least a DWORD, and must a power of 2.
 */
TESTPROCAPI
entryCacheTestWriteEverythingReadEverythingMixedUp(
                           UINT uMsg, 
                           TPPARAM tpParam, 
                           LPFUNCTION_TABLE_ENTRY lpFTE);

/*
 * prints a usage message with command line args.  Always passes.
 */
TESTPROCAPI
cacheTestPrintUsage(
       UINT uMsg, 
       TPPARAM tpParam, 
       LPFUNCTION_TABLE_ENTRY lpFTE);


#define CACHE_TEST_USE_ALLOC_VIRTUAL 1
#define CACHE_TEST_USE_ALLOC_PHYS_MEM 2

#endif // __TUX_CACHES_H__
