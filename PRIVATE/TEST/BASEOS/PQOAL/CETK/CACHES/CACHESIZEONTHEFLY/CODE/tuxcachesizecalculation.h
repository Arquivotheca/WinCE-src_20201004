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
 * tuxCacheSizeCalculation.h
 */

#ifndef __TUX_CACHE_SIZE_CALCULATION_H
#define __TUX_CACHE_SIZE_CALCULATION_H

#include "..\..\..\..\common\disableWarnings.h"

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
 * These are defined in cacheDefs.h.  They are used for other
 * code and therefore are not defined in this header file.
 */
#include "cacheDefs.h"

/**************************************************************************
 * 
 *                           Cache Test Functions
 *
 **************************************************************************/
/*
 * If the array size is <SIZE>, access stride is <STRIDE>, and iterate the cache
 * memory for <ITERS> times, then the total access points are:
 *
 *    <Access Points> = ITERS * STRIDE * (SIZE / STRIDE)
 *                    = ITERS * SIZE
 *
 * In order to limit the running time for the test runs, we first compute the
 * iteration number for accessing the smallest size of array with smallest 
 * stride number for the specific period
 */
INT
runTest(
        DWORD *vals, 
        DWORD dwArray, 
        DWORD dwStride,
        DWORD dwRunningTime,
        ULONGLONG *ullIterations,
        DWORD *dwDummy);


/*****  Command line argument parsing  **************************************/
/*
 * Get the max/min array sizes from the command line. If no specified,
 * the default values will be used, instead.
 *
 */
BOOL
getArraySizes (DWORD dwParam, DWORD *dwMinSize, DWORD *dwMaxSize);

/*
 * Get the maximum stride size from the command line. If no specified,
 * the default values will be used, instead.
 */
BOOL
getStrideSizes (DWORD dwParam, DWORD *pdwMinStride, DWORD *pdwMaxStride);

/*
 * Get the cache size jump rate. This number is used to be a threshold
 * to determine the expected jump rate for the cache size calculation. 
 * If no specified, then the default values will be used, instead.
 *
 * INPUT:
 *    dwParam  the command line parameters
 * 
 * OUTPUT:
 *    *pdwCacheSizeJump the jump rate number.
 */
BOOL
getCacheSizeJumpRate (DWORD dwParam, DWORD *pdwCacheSizeJump);

/*
 * Get the cache line size jump rate. This number is used to be a threshold
 * to determine the expected jump rate for the cache line size calculation. 
 * If no specified, then the default values will be used, instead.
 *
 * INPUT:
 *    dwParam  the command line parameters
 * 
 * OUTPUT:
 *    *pdwCacheLineJump the jump rate number.
 */
BOOL
getCacheLineJumpRate (DWORD dwParam, DWORD *pdwCacheLineJump);

/*
 * Get the minimum running time (ms) to access the cache. This time will be 
 * used to access the minimum array with minimum stride size. 
 * If no specified, then the default values will be used, instead.
 *
 * INPUT:
 *    dwParam  the command line parameters
 * 
 * OUTPUT:
 *    *pdwRunningTime the minimum running time.
 */
BOOL
getRunningTime (DWORD dwParam, DWORD *pdwRunningTime);

/*
 * In order to easily evaluate all the access running time, we need to keep
 * the cache access points to the same for any size array of array. 
 *
 * Here, we choose the array sizes from smallest to biggest in consecutive
 * order, each is the power of TWO, i.g. 4Kbytes, 8Kbytes, 16Kbytes, ...,
 *
 * Since we have already figured out the iteration number# for accessing 
 * the smallest array, then we need to decrease in half, and so on, i.e. 
 * <ITERS>, <ITERS>/2^1, <ITERS>/2^2, <ITERS>/2^3, ..., <ITERS>/2^N
 *
 * Here, if the calculated iteration# for accessing the biggest array
 * is not an integer, we need to round up to its ceiling value. 
 */
VOID
adjustIterations(DWORD dwMin, DWORD dwMax, ULONGLONG *pullIter);

/*
 * entry point to print out the usage
 */
TESTPROCAPI 
entryCacheSizePrintUsage(
               UINT uMsg, 
               TPPARAM tpParam, 
               LPFUNCTION_TABLE_ENTRY lpFTE);

/*
 * entry point to calculate the size of L1 DATA cache
 */
TESTPROCAPI 
entryCacheSizeCalculation( 
                     UINT uMsg, 
                     TPPARAM tpParam,
                     LPFUNCTION_TABLE_ENTRY lpFTE);

#endif // __TUX_CACHES_H__
