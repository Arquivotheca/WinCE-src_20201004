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

#ifndef __CACHE_SIZE_CALCULATION_H
#define __CACHE_SIZE_CALCULATION_H

#include "..\..\..\..\common\disableWarnings.h"
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

#include "cacheDefs.h"

/*
 * This function is used to walk through different data cache sizes
 * to figure out the data cache size
 *
 * @vals              a chunk of DWORDs that is dwMaxArraySize; 
 * @dwMinArray        the minimum size of array to access
 * @dwMaxArray        the maximum size of array to access
 * @pAccessInfo       array of the cache access information 
 * @dwBlock           the total running cases
 * @ullIterations     the total interaion# needed to access the smallest 
 *                    array with smallest stride(1)
 * RETURN:
 *   TRUE if run successfully, FALSE otherwise
 */
BOOL
runCacheSizeCalculation (
            DWORD *vals, 
            DWORD dwMinArray, 
            DWORD dwMaxArray,
            DWORD dwMinStride,
            DWORD dwMaxStride,
            DWORD dwCacheSizeJumpRate,
            DWORD dwCacheLineJumpRate,
            CACHE_ACCESS_INFO *pAccessInfo,            
            ULONGLONG ullIterations);

/*
 * This function is used to read the values of array into cache to figure
 * out the access time in milliseconds 
 *
 * INPUT:
 *   @vals          a chunk of DWORDs that is dwArray; 
 *   @dwArray       the size of array to access
 *   @dwStride      the stride size
 *   @ullIterations the number of times to run through the values.
 *   @pRunTime      pointer to the access period 
 *
 * OUTPUT:
 *   @dwDummyValue  the dummy value used to avoid the code optimization
 *
 * RETURN:
 *   TRUE if run successfully, FALSE otherwise
 */
BOOL
runCacheAccess(
            DWORD *vals, 
            DWORD dwArray,
            DWORD dwStride,
            ULONGLONG ullIterations,
            ULONGLONG *pRunTime,
            DWORD *dwDummyValue);


/*
 * This function is used to hit each cache once and run with stride twice 
 *
 * INPUT:
 *   @vals      a chunk of DWORDs that is dwArray; 
 *   @dwSize    the size of array to access
 *   @dwStride  the stride size
 *
 * OUTPUT:
 *   @dwJunk    the dummy value used to avoid the code optimization
 *
 * RETURN:
 *   NONE
 */
VOID
cacheInit(DWORD *vals, DWORD dwSize, DWORD dwStride, DWORD *dwJunk);

/*
 * Figure out how many enumerations from minimum size to maximum size.
 *
 * e.g, dwMinSize= 1K, dwMaxSize= 32K, then there exist 6 enumerated sizes: 
 * 1K, 2K, 4K, 8K, 16K, 32K. The return value will be 6.
 *
 * INPUT:
 *   @dwMinSize  the minimum stride number
 *   @dwMaxSize  the maximum stride number
 *
 * OUTPUT:
 *   @dwEnum     the number of the enumberations
 *
 * RETURN:
 *   TRUE if run successfully, FALSE otherwise
 */
BOOL
numberOfEnum(DWORD dwMinSize, DWORD dwMaxSize, DWORD* dwEnum);

/*
 * This function is used to build/analyze the data based on the running info: 
 * compute the median data, based on which we can figure out the cache size 
 * and block/line size.
 *
 * INPUT:
 *   @pAccessInfo  array of the cache access information 
 *   @dwMinStride  the maximal stride number in DWORDs
 *   @dwMaxStride  the minimal stride number in DWORDs
 *   @dwSteps      the steps from start stride to end 
 *
 * RETURN:
 *   TRUE if run successfully, FALSE otherwise
 */
BOOL
analyzeData(
       CACHE_ACCESS_INFO **accessInfo, 
       DWORD dwMinStride, 
       DWORD dwMaxStride, 
       DWORD dwArrayEnum,
       DWORD dwCacheSizeJumpRate,
       DWORD dwCacheLineJumpRate);

/*
 * This function is used to calculate the size of L1/L2 data cache. we
 * figure out the cache size by analyzing the jump(s) based on cache
 * access for specific array size with THE SAME STRIDE NUMBER.
 *
 * If the jumps are not the same, we currently will report the error,
 * but anyway, we will still print out the middle results for
 * user to analyze the issues
 *
 * INPUT:
 *   @pAccessInfo     array of the cache access information 
 *   @dwMinStride     the maximal stride number in DWORDs
 *   @dwMaxStride     the minimal stride number in DWORDs
 *   @dwSteps    the steps from start stride to end 
 *
 * OUTPUT:
 *   @pdwJumpPos      reference to jump position
 *   @pdwCacheSize    reference to cache size (L1 Data Cache)
 *   @pwJumpStatus    status for jump searching
 *
 * RETURN:
 *   TRUE if run successfully, FALSE otherwise
 */
BOOL
calculateCacheSize(
                CACHE_ACCESS_INFO **pAccessInfo, 
                DWORD dwMinStride,
                DWORD dwMaxStride,
                DWORD dwArrayEnum,
                DWORD dwCacheSizeJumpRate,
                DWORD *pdwJumpPos,                
                DWORD *pdwCacheSize,
                WORD  *pwJumpStatus);

/*
 * This function is used to calculate the size of cache line size
 *
 * INPUT:
 *   @pAccessInfo    array of the cache access information 
 *   @dwSteps        the total number of the strides
 *   @dwArrays       the total number of the arrays
 *   @dwCacheSize    the cache size  
 *   @dwMinStride    the maximal stride number in DWORDs
 *   @dwMaxStride    the minimal stride number in DWORDs
 *
 * OUTPUT:
 *   @dwBlockSize     reference to the cache line size
 *
 * RETURN:
 *   TRUE if run successfully, FALSE otherwise
 */
BOOL
calculateBlockSize(
            CACHE_ACCESS_INFO **pAccessInfo, 
            DWORD dwSteps, 
            DWORD dwArrays,
            DWORD dwCacheSize,
            DWORD dwMinStride,
            DWORD dwMaxStride,
            DWORD dwCacheLineJumpRate,
            DWORD *dwBlockSize);

/*
 * calculate the standard deviation for the sample data 
 *
 *   Standard Deviation = SQRT{(Sum(X^2) - [(Sum(X))^2 / N]) / (N-1)}
 *     where 
 *       X - the individual data sample value
 *       N - number of the samples
 *
 * INPUT:
 *   @pAccessInfo  array of the cache access information 
 *   @dwStart      start index
 *   @dwEnd        end index
 *
 * OUTPUT:
 *   the standard deviation
 *
 */
DOUBLE
getStandardDeviation(
            CACHE_ACCESS_INFO **pAccessInfo, 
            DWORD dwStart, 
            DWORD dwEnd); 

/*
 * print the cache accessing data in friendly format
 *
 * INPUT:
 *   @pAccessInfo  array of the cache access information 
 *   @dwCounter    the total running cases to access all cache sizes
 *
 * OUTPUT:
 *   NONE
 */
VOID
printCacheCal(CACHE_ACCESS_INFO **pAccessInfos, DWORD dwCounter);

/*
 * sort the array in ascending order. Currently, bubble sort will be
 * used
 *
 * INPUT:
 *   @pInfo  the array to be sorted
 *
 * OUTPUT:
 *   the sorted array
 */
VOID
sortData(CACHE_ACCESS_INFO **pInfo);

/*
 * Sort the array in ascending order. If the array has been in order,
 * its complexity is O(N), otherwise, it is O(N^2).
 * Since the elements to be sorted are very small, so bubble sort 
 * here should be enough.
 *
 * INPUT:
 *   @pInfo  the array to be sorted
 *   @dwNum  the size of the array
 *
 * OUTPUT:
 *   the sorted array
 */
VOID 
bubbleSort(CACHE_ACCESS_INFO **pInfo, DWORD dwNum);

/*
 * swap the two elements from an array
 *
 * @pInfo     the array to be swapped
 * @dwIndex1  the index of the 1st element
 * @dwIndex2  the index of the 2ed element
 */
VOID
swapData(CACHE_ACCESS_INFO **pInfo, DWORD dwIndex1, DWORD dwIndex2);

/*
 * search the jump point for cache accessing to figure out the cache size. 
 *
 * INPUT:
 *   @pAccessInfo  the array to be sorted
 *   @dwStart      the start point to search the jump from
 *   @dwStep       the stride nymbers
 *   @dwBlock      the total numbers of access data
 *
 * OUTPUT:
 *   @pdwJumpPos   the array size position where jump occurs
 *   @pdwCacheSize the cache size found
 *   @pdoJumpRate  the jump rate
 *
 * RETURN:
 *   TRUE if run successfully, FALSE otherwise
 */
BOOL
isCacheJump(
        CACHE_ACCESS_INFO **pAccessInfo,
        DWORD  dwStart,
        DWORD  dwSteps,
        DWORD  dwBlock,
        DWORD  dwCacheSizeJumpRate,
        DWORD  *pdwJumpPos,
        DWORD  *pdwCacheSize,
        DOUBLE *pdoJumpRate);

/*
 * print out the data for calculating the cache line size, the first column is 
 * expected stride size, the remaining columns will be the cache MISS time(s) 
 * corresponding to different stride sizes that we use to access the cache
 *
 * INPUT:
 *   @dwStride     the expected stride size  
 *   @doMissTime   the cache access miss time
 *   @dwCounter    the total running case to run cache access 
 *   @needMark     the flag to decide whether to mark the data for big jump
 *
 * OUTPUT:
 *   NONE
 *
 * RETURN:
 *   NONE
 */
VOID
printBlockCalByRow(
        DWORD dwStride, 
        DOUBLE *doMissTime, 
        DWORD dwStart, 
        DWORD dwEnd,
        BOOL needMark);

/*
 * check if the big jump occurs for the specific expected stride size
 *
 * INPUT:
 *   @doMissTime   array of the miss time
 *   @dwSize       array size
 *
 * OUTPUT:
 *   @pdoJumpRate  the jump rate to return 
 *
 * RETURN:
 *   TRUE if there exists jump; FALSE otherwise
 */
BOOL
isBlockJump(
        DOUBLE * doMissTime, 
        DWORD dwStart, 
        DWORD dwEnd,
        DWORD dwCacheLineJumpRate, 
        DOUBLE *pdoJumpRate);

/*
 * print out the data for calculating the cache size, the first column is 
 * array size, the remaining columns will be the cache access time(s) 
 * corresponding to different stride sizes that we use to access the cache
 *
 * INPUT:
 *   @pInfo         cache access data  
 *   @dwStart       start index
 *   @dwJump        the row that big jump occurs
 *   @dwEnd         end index
 *   @steps         the number of stride to walk through
 *
 * OUTPUT:
 *   @pwJumpStatus  the jump status
 */
VOID
printCacheCalByRow(
       CACHE_ACCESS_INFO **pInfo, 
       DWORD dwStart,
       DWORD dwJump,
       DWORD dwEnd, 
       DWORD dwSteps,
       WORD *pwJumpStatus);

/*
 * To align the string. If the length of the string is shorter
 * than "len", then add the space ahead of it
 *
 * @data  the string
 * @len   the length needed to be aligned
 */
VOID
formatString(TCHAR *data, DWORD len); 

/*
 * To copy the string from src to dest, and add the space if its length is
 * than the specified length
 * 
 * @dest           the destination string
 * @src            the source string
 * @lengthInTotal  the total length after copying
 */
VOID
copyString(TCHAR *dest, const TCHAR *src, int lengthInTotal); 

/*
 * To concat the string from src to dest, and add the space if its length is
 * than the specified length
 * 
 * @dest           the destination string
 * @src            the source string
 * @lengthInTotal  the total length after concating
 */
VOID
concatString(TCHAR *dest, const TCHAR *src, int lengthInTotal);

/*
 * To retrieve the median value(s) from total MAX_RUNNING_LOOP
 *
 * If the sample# is odd, we retrieve the real median value; otherwise,
 * we need to average the two middle values
 */
VOID
getMedianValue(
       CACHE_ACCESS_INFO *pDest, 
       DWORD dwIndex, 
       CACHE_ACCESS_INFO *pSrc);
/*
 * To calculate the access periods' standard deviations per stride for all 
 * array sizes that are smaller than or equal to cache size
 *
 * INPUT:
 *   @pInfo             cache access data 
 *   @dwNumberOfArray   the number of all the arrays
 *   @dwNumberOfStride  the number of all the strides
 *   @dwJumpPosOfArray  the jump position
 *
 * OUTPUT:
 *   @doDeviations      the array of the standard deviation per stride
 *
 * RETURN:
 *   TRUE if run successfully, FALSE otherwise
 */
BOOL
calculateDeviationsByStride(
         CACHE_ACCESS_INFO **pInfo, 
         DWORD dwNumberOfArray, 
         __in DWORD dwNumberOfStride, 
         DWORD dwJumpPosOfArray,
         __out_ecount(dwNumberOfStride) DOUBLE *doDeviations);

/*
 * To return the standard deviation for the sample data 
 *
 *   Standard Deviation = SQRT{(Sum(X^2) - [(Sum(X))^2 / N]) / (N-1)}
 *     where 
 *       X - the individual data sample value
 *       N - number of the samples
 *
 * @doSamples  array of the samples 
 * @dwSize     array size
 *
 */
DOUBLE
getStandardDeviation(DOUBLE *doSamples, DWORD dwSize);

/*
 * To return the standard deviation for the sample data 
 *
 * @pAccessInfo  array of the cache access information 
 * @dwStart      start index
 * @dwEnd        end index
 */
DOUBLE
getStandardDeviation(
            CACHE_ACCESS_INFO **pAccessInfo, 
            DWORD dwStart, 
            DWORD dwEnd); 

/*
 * To print out standard deviations per stride for all array sizes that are 
 * smaller than or equal to cache size
 *
 * @doDeviations  array of the standard deviation per stride
 * @dwSteps       the number of all the strides
 */
VOID
printDeviationByStride(DOUBLE *doDeviations, DWORD dwSteps);

// To return the maximum value from array
DOUBLE
getMax(DOUBLE *doSamples, DWORD dwStart, DWORD dwEnd);

// To return the minimum value from array
DOUBLE
getMin(DOUBLE *doSamples, DWORD dwStart, DWORD dwEnd);

#endif
