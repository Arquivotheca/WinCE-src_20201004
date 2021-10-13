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
 * tuxCacheSizeCalculation.cpp
 */

/*
 * This file implements the tux specific part of the cache size calculation.
 *
 * Theoretically, accessing the elements from different size of arrays in 
 * different stride will cause different cache hit/miss rate, which will
 * affect the access time. 
 *
 * Based on the access time, we can figure out L1/L2 data cache size, 
 * if applicable
 */

/***** Headers  **************************************************************/

/* common utils */
#include "..\..\..\..\common\commonUtils.h"

/* safe int multiplication */
#include <intsafe.h>

/* gives TESTPROCs declarations for these functions */
#include "tuxCacheSizeCalculation.h"

/* actual implementation of the cache testing functions */
#include "cacheSizeCalculation.h"

/*****************************************************************************
 *                    Cache Size Calculation TESTPROCAPI
 *****************************************************************************/

#define DEBUG

/*
 * print a usage message
 *
 * See header for information.
 */
TESTPROCAPI 
entryCacheSizePrintUsage(
               UINT uMsg, 
               TPPARAM tpParam, 
               LPFUNCTION_TABLE_ENTRY lpFTE) 
{
  /* only supporting executing the test */ 
  if (uMsg != TPM_EXECUTE) 
    { 
      return (TPR_NOT_HANDLED);
    }

  Info (_T("The test aims to calculate L1 data cache size and line size."));
  Info (_T("By using different size of arrays with different length of"));
  Info (_T("strides to hit the cache, it compares all the access time"));
  Info (_T("trying to search the cache size jump and cache line jump."));
  Info (_T("It can then determine their sizes according to these jumps"));
  Info (_T(""));
  Info (_T("It requires no arguments, but acceptable arguments are:"));
  Info (_T(""));
  Info (_T("  -array_min <size>       minimum array length in Kbytes."));
  Info (_T("                          The default is %uKB"), MIN_ARRAY_SIZE);

  Info (_T("  -array_max <size>       maximum array length in Kbytes."));
  Info (_T("                          The default is %uKB"), MAX_ARRAY_SIZE);

  Info (_T("  -stride_min <stride>    minimum stride size in bytes."));
  Info (_T("                          The default is %ubytes"),
            MIN_STRIDE_SIZE);

  Info (_T("  -stride_max <stride>    maximum stride size in bytes."));
  Info (_T("                          The default is %ubytes"),
            MAX_STRIDE_SIZE);
  Info (_T("  -cache_size_jump <rate> the jump rate for cache size."));
  Info (_T("                          The default is %u percent"),
            CACHE_SIZE_JUMP_THRESHOLD);
  Info (_T("  -cache_line_jump <rate> the jump rate for line size."));
  Info (_T("                          The default is %u percent"),
            CACHE_LINE_JUMP_THRESHOLD);
  Info (_T("  -running_time  <period> the running time in milliseconds."));
  Info (_T("                          The default is %u percent"),
            MAX_RUNNING_TIME_MS);
  Info (_T(""));
  Info (_T(""));
  return (TPR_PASS);
}

/*
 * See header for information.
 */
TESTPROCAPI 
entryCacheSizeCalculation( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) 
  {
    INT returnVal = TPR_PASS;
    DWORD * vals = NULL;
    CACHE_ACCESS_INFO *pAccessInfo = NULL;
 
    /* only support executing the test */ 
    if (uMsg != TPM_EXECUTE) 
      { 
        return TPR_NOT_HANDLED;
      }

    /*
     * get the min/max array sizes in bytes from command line; if no specified, 
     * then the default values will be used, instead
     */
    DWORD dwMinSize, dwMaxSize;
    if (getArraySizes (lpFTE->dwUserData, &dwMinSize, &dwMaxSize) == FALSE)
      {
        Error (_T(""));
        Error (_T("Could not determine the maximum array size!!!"));
        Error (_T(""));
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
      }

    if (!isPowerOfTwo(dwMinSize) || !isPowerOfTwo(dwMaxSize))
      {
        Error (_T(""));
        Error (_T("Cache size is invalid. Choose array size in KBytes as power of two!!!"));
        Error (_T(""));
        return TPR_FAIL;
      }

    Info (_T(""));
    Info (_T("Minimum array size: %uKB, maximum array size: %uKB"), 
              dwMinSize,
              dwMaxSize);
    /*
     * To convert the array size from KB into DWORDs, as as to create the 
     * corresponding number of elements' array for cache access
     */
    dwMinSize = dwMinSize * 1024 / sizeof(DWORD);
    dwMaxSize = dwMaxSize * 1024 / sizeof(DWORD); 

    DWORD dwMinStride;
    DWORD dwMaxStride;
    if (getStrideSizes(lpFTE->dwUserData, &dwMinStride, &dwMaxStride) == FALSE)
      {
        Error (_T(""));
        Error (_T("Cannot determine the size of the stride!!!"));
        Error (_T(""));
        return TPR_FAIL;
      }

    if (!isPowerOfTwo(dwMinStride) || !isPowerOfTwo(dwMaxStride))
      {
        Error (_T(""));
        Error (_T("Stride is invalid. Choose stride in bytes as power of two!!!"));
        Error (_T(""));
        return TPR_FAIL;
      }

    Info (_T(""));
    Info (_T("Min stride size is: %ubytes, Max stride size is: %ubytes"), 
              dwMinStride, dwMaxStride); 
    Info (_T(""));

    // convert to DWORD for internal array access
    dwMaxStride /= sizeof(DWORD);
    dwMinStride /= sizeof(DWORD);    

    if (dwMinSize == 0 || dwMaxSize == 0) 
    {
      Error (_T(""));
      Error (_T("Array size is ZERO."));
      Error (_T(""));
      return TPR_FAIL;
    }

    if (dwMinStride == 0 || dwMaxStride == 0) 
    {
      Error (_T(""));
      Error (_T("Stride size is smaller than 4 byte(s)."));
      Error (_T(""));
      return TPR_FAIL;
    }

    if (dwMinSize > dwMaxSize || 
        dwMinStride > dwMaxStride) 
      {
        Error (_T(""));
        Error (_T("Either array size or stride size is too small!!!"));
        Error (_T(""));
        return TPR_FAIL;
      }    

    DWORD dwCacheSizeJumpRate;
    if (getCacheSizeJumpRate(lpFTE->dwUserData, &dwCacheSizeJumpRate) == FALSE)
      {
        Error (_T(""));
        Error (_T("Cannot determine the cache size jump rate!!!"));
        Error (_T(""));
        return TPR_FAIL;
      }

    DWORD dwCacheLineJumpRate;
    if (getCacheLineJumpRate(lpFTE->dwUserData, &dwCacheLineJumpRate) == FALSE)
      {
        Error (_T(""));
        Error (_T("Cannot determine the cache line jump rate!!!"));
        Error (_T(""));
        return TPR_FAIL;
      }

    Info (_T(""));
    Info (_T("Using %u%% as the jump rate to search the cache size."), 
              dwCacheSizeJumpRate); 
    Info (_T("Using %u%% as the jump rate to search the cache line size."), 
              dwCacheLineJumpRate); 
    Info (_T(""));

    DWORD dwRunningTime;
    if (getRunningTime(lpFTE->dwUserData, &dwRunningTime) == FALSE)
      {
        Error (_T(""));
        Error (_T("Cannot determine the minimum running time!!!"));
        Error (_T(""));
        return TPR_FAIL;
      }

    Info (_T(""));
    Info (_T("Using %ums as the running time. The program will access the"),
              dwRunningTime);
    Info (_T("%uKB array with stride of %ubytes in this period of time to"),
              dwMinSize * sizeof(DWORD) / 1024,
              dwMinStride * sizeof(DWORD));
    Info (_T("determine the total access points for each array's access."));
    Info (_T(""));

    /*
     * calculate the total running cases so as to allocate the dynamic
     * space to store the access information
     */
    DWORD dwSteps;
    DWORD dwArrayEnum;    
    numberOfEnum(dwMinStride, dwMaxStride, &dwSteps);
    numberOfEnum(dwMinSize, dwMaxSize, &dwArrayEnum);

    DWORD dwSize = 0;
    /*
     * [sizeof(CACHE_ACCESS_INFO)*MAX_RUNNING_LOOP] * [dwSteps*dwArrayEnum] + 0
     */
    if (CeDWordMult2Add(
            sizeof(CACHE_ACCESS_INFO)*MAX_RUNNING_LOOP, 
            dwSteps*dwArrayEnum,
            0,             
            &dwSize) != S_OK) 
      {
        Error (_T(""));
        Error (_T("tuxCacheSizeCalculation: data overflow!"));
        Error (_T("Please limit the stride numbers or array numbers!!!"));
        Error (_T(""));
        return TPR_FAIL;
      }

    pAccessInfo = 
          (CACHE_ACCESS_INFO *)VirtualAlloc(
             NULL, 
             dwSize,
             MEM_COMMIT, 
             PAGE_READWRITE);

    if (pAccessInfo == NULL)
      {
        Error (_T(""));
        Error (_T("tuxCacheSizeCalculation: Memory allocation error!"));
        Error (_T(""));
        return TPR_FAIL;
      }
       
    /*
     * VirtualAlloc() used here to allocate the memory. The memory region isn't 
     * guaranteed to be contiguous; but the caching mode is still turned on, 
     * so we don't need to explicitly turn it on
     */
    vals = (DWORD *) VirtualAlloc(
                        NULL, 
                        dwMaxSize * sizeof(DWORD),
                        MEM_COMMIT, 
                        PAGE_READWRITE);

    if (vals == NULL)
      {
        Error (_T("The test tried to allocate %ubytes of memory using ")
               _T("VirtualAlloc.  The call failed.  GetLastError: %u."),
                    dwMaxSize * sizeof(DWORD), GetLastError ());
        Error (_T("Reboot the machine and try the test again"));
        Error (_T(""));

        returnVal = TPR_FAIL;
        goto cleanAndReturn;
      }

    Info (_T(""));
    Info (_T("%uKB of memory has been allocated using VirtualAlloc()"), 
              dwMaxSize * sizeof(DWORD)/1024);
    Info (_T(""));

    ULONGLONG ullIterations;
    DWORD dwDummy;

    /*
     * To compute the iteration number for accessing the smallest size of array 
     * with smallest stride number for the specific period.
     */
    if (runTest(
            vals, 
            dwMinSize, 
            1, /*Stride*/
            dwRunningTime, 
            &ullIterations, 
            &dwDummy) == FALSE)
      {
        Error (_T(""));
        Error (_T("runTest(): fails to figure out the initial iteration#")); 
        Error (_T(""));
        goto cleanAndReturn;
      }

    /*
     * adjust the iteration#, so as to keep the access points for all
     * size of array to be the same
     */
    adjustIterations(dwMinSize, dwMaxSize, &ullIterations);

    Info (_T("-------------------------------------------------------------"));
    Info (_T("Looping the cache aceess for totally %u TIMES, for each loop,"), 
              MAX_RUNNING_LOOP);
    Info (_T("%uKB ~ %uKB arrayes will be picked up; and for each size of"),
              dwMinSize * sizeof(DWORD) / 1024, 
              dwMaxSize * sizeof(DWORD) / 1024);
    Info (_T("the array's access, %ubytes ~ %ubytes strides will be chosen"),
              dwMinStride * sizeof(DWORD), 
              dwMaxStride * sizeof(DWORD)); 
    Info (_T(""));

    if (runCacheSizeCalculation(
                           vals, 
                           dwMinSize, 
                           dwMaxSize,
                           dwMinStride,
                           dwMaxStride,
                           dwCacheSizeJumpRate,
                           dwCacheLineJumpRate,
                           pAccessInfo,                               
                           ullIterations) == FALSE)
      {       
        Error (_T(""));
        Error (_T("CacheSizeCalculation fails"));
        Error (_T(""));
        returnVal = TPR_FAIL;
      }    

cleanAndReturn:  

    if (VirtualFree (pAccessInfo, 0, MEM_RELEASE) == 0)
      {
        Error (_T(""));
        Error (_T("Could not free memory.  GetLastError: %u."), GetLastError);
        Error (_T(""));
      }

    if (vals != NULL && VirtualFree (vals, 0, MEM_RELEASE) == 0)
      {
        Error (_T(""));
        Error (_T("Could not free memory.  GetLastError: %u."), GetLastError);
        Error (_T(""));
      }

    return (returnVal);
  }

// see header for the details
BOOL
runTest(
     DWORD *vals, 
     DWORD dwArray, 
     DWORD dwStride,
     DWORD dwRunningTime,
     ULONGLONG *ullIterations, 
     DWORD *dwDummy)
  {
    /* 
     * the initial iteration# as 1000, if the number is not big enough 
     * for running the specific period (=dwRunningTime), then
     * doubled it to 2000, 4000, and so on.
     */
    ULONGLONG iterations = 1000;
    ULONGLONG ullDiff;

    DWORD dwJunk = 0;

    while (1)
      {
        if (runCacheAccess( // figure out how many iterations needed
                      vals, 
                      dwArray, 
                      dwStride, 
                      iterations, 
                      &ullDiff, 
                      &dwJunk) == FALSE)
          {
            return TPR_FAIL; // error occurs, stop running ...
          }

        if (ullDiff < dwRunningTime)
          {
            /* if within threshold double number of runs and try again */
            ULONGLONG prev = iterations;          
            iterations *= 2; // doubled

              if (prev >= iterations)
              {
                /* 
                 * we overflowed.  We could try to recover but it is much easier
                 * to bail at this point.  If this happens the user either has
                 * lightening speed caches or something is wrong with either 
                 * the timers or something else.
                 *
                 * This should be a very rare error.
                 */
                  Error (_T("Could not calibrate.  We overflowed the variable"));
                  Error (_T("storing the number of calibration runs. Either the"));
                  Error (_T("cache routines are running very fast or the "));
                  Error (_T("calibration threshold is set too high."));
                  Error (_T("Reduce the calibration threshold in the code (see"));
                  Error (_T("the defines at the top of cacheDefs.cpp), recompile,"));
                  Error (_T("and try again."));
                return TPR_FAIL;
              }
          }
        else
          {
            break; // we got the solution, exit the loop
          }
      } /* while (1) */

    *ullIterations = iterations;
    *dwDummy = dwJunk; // cheat the optimizer

    return TPR_PASS;
  }

// see the header for details
BOOL
getArraySizes (DWORD dwParam, DWORD *pdwMinSize, DWORD *pdwMaxSize) 
  {
    if (pdwMinSize == NULL || pdwMaxSize == NULL)
      {
        IntErr (_T("getArraySizes --> ArraySize(s) is NULL"));
        return FALSE;
      }

    CClParse cmdLine(g_szDllCmdLine);

    /* find the size from the command line*/
    if (cmdLine.GetOptVal (_T("array_min"), pdwMinSize) == FALSE)
      {
        Info (_T(""));
        Info (_T("Looking for an option in the form \"array_min <size>\". Could"));
        Info (_T("not get the minimum cache test array length. Using the"));
        Info (_T("default minimum value of %uKB"), MIN_ARRAY_SIZE);
        *pdwMinSize = MIN_ARRAY_SIZE;          
      }

    if (cmdLine.GetOptVal (_T("array_max"), pdwMaxSize) == FALSE)
      {
        Info (_T(""));
        Info (_T("Looking for an option in the form \"array_max <size>\". Could"));
        Info (_T("not get the maximum cache test array length. Using the"));
        Info (_T("default maximum value of %uKB"), MAX_ARRAY_SIZE);
        *pdwMaxSize = MAX_ARRAY_SIZE;          
      }

    return (TRUE);
  }

BOOL
getStrideSizes (DWORD dwParam, DWORD *pdwMinStride, DWORD *pdwMaxStride) 
  {
    if (pdwMinStride == NULL || pdwMaxStride == NULL)
      {
        Info (_T("getStrideSize --> stride size is NULL"));
        return FALSE;
      }

    CClParse cmdLine(g_szDllCmdLine);

    DWORD dwStride;

    /* find the size from the command line*/
    if (cmdLine.GetOptVal (_T("stride_min"), &dwStride) == FALSE)
      {
        Info (_T(""));
        Info (_T("Looking for an option in the form \"stride_min <stride>\"."));
        Info (_T("Could not get the minimum stride size for cache access."));        
        Info (_T("Use the default size: %ubytes"), MIN_STRIDE_SIZE);
        Info (_T(""));
        dwStride = MIN_STRIDE_SIZE;
      }
    else
      {
        if (dwStride <= 0)
          {
            Error(_T("minimum stride is zero!!!"));
            return FALSE;
          }
      }

    *pdwMinStride = dwStride;

        /* find the size from the command line*/
    if (cmdLine.GetOptVal (_T("stride_max"), &dwStride) == FALSE)
      {
        Info (_T(""));
        Info (_T("Looking for an option in the form \"stride_max <stride>\"."));
        Info (_T("Could not get the maximum stride size for cache access."));        
        Info (_T("Use the default size: %ubytes"), MAX_STRIDE_SIZE);
        dwStride = MAX_STRIDE_SIZE;
      }
    else
      {
        if (dwStride <= 0)
          {
            Error(_T("maximum stride is zero!!!"));
            return FALSE;
          }
      }

    *pdwMaxStride = dwStride;

    return (TRUE);
  }

BOOL
getCacheSizeJumpRate (DWORD dwParam, DWORD *pdwCacheSizeJump) 
  {
    if (pdwCacheSizeJump == NULL)
      {
        Info (_T("getCacheJumpRate --> pdwCacheSizeJump is NULL"));
        return FALSE;
      }

    CClParse cmdLine(g_szDllCmdLine);

    DWORD dwCacheSizeJump;

    /* find the jump rate from the command line*/
    if (cmdLine.GetOptVal (_T("cache_size_jump"), &dwCacheSizeJump) == FALSE)
      {
        Info (_T(""));
        Info (_T("Looking for an option \"cache_size_jump <rate>\"."));
        Info (_T("Could not get the cache size jump rate."));
      Info (_T("Use the default jump: %u%%"), CACHE_SIZE_JUMP_THRESHOLD);

        dwCacheSizeJump = CACHE_SIZE_JUMP_THRESHOLD;
      }
    else
      {
        if (dwCacheSizeJump <= 0)
          {
            Error(_T("cache size jump rate is zero!!!"));
            return FALSE;
          }
      }

    *pdwCacheSizeJump = dwCacheSizeJump;

    return (TRUE);
  }

BOOL
getCacheLineJumpRate (DWORD dwParam, DWORD *pdwCacheLineJump) 
  {
    if (pdwCacheLineJump == NULL)
      {
        Info (_T("getCacheJumpRate --> pdwCacheLineJump is NULL"));
        return FALSE;
      }

    CClParse cmdLine(g_szDllCmdLine);

    DWORD dwCacheLineJump;

    /* find the jump rate from the command line*/
    if (cmdLine.GetOptVal (_T("cache_line_jump"), &dwCacheLineJump) == FALSE)
      {
        Info (_T(""));
        Info (_T("Looking for an option \"cache_line_jump <rate>\"."));
        Info (_T("Could not get the cache line jump rate."));
    Info (_T("Use the default jump: %u%%"), CACHE_LINE_JUMP_THRESHOLD);

        dwCacheLineJump = CACHE_LINE_JUMP_THRESHOLD;
      }
    else
      {
        if (dwCacheLineJump <= 0)
          {
            Error(_T("cache line jump rate is zero!!!"));
            return FALSE;
          }
      }

    *pdwCacheLineJump = dwCacheLineJump;

    return (TRUE);
  }

BOOL
getRunningTime (DWORD dwParam, DWORD *pdwRunningTime) 
  {
    if (pdwRunningTime == NULL)
      {
        Info (_T("getRunningTime --> running time is NULL"));
        return FALSE;
      }

    CClParse cmdLine(g_szDllCmdLine);

    DWORD dwRunningTime;

    /* find the size from the command line*/
    if (cmdLine.GetOptVal (_T("running_time"), &dwRunningTime) == FALSE)
      {
        Info (_T(""));
        Info (_T("Looking for an option in the form \"running_time <period>\"."));
        Info (_T("Could not get the minimum running time for cache access."));        
        Info (_T("Use the default running time: %u milliseconds"), MAX_RUNNING_TIME_MS);
        Info (_T(""));
        dwRunningTime = MAX_RUNNING_TIME_MS;
      }
    else
      {
        if (dwRunningTime <= 0)
          {
            Error(_T("minimum running time is zero!!!"));
            return FALSE;
          }
      }

    *pdwRunningTime = dwRunningTime;
    return (TRUE);
  }


VOID
adjustIterations(DWORD dwMin, DWORD dwMax, ULONGLONG *pullIter)
  {
    ULONGLONG ullIter = *pullIter;
    DWORD dwFactor = dwMax / dwMin;

    if (ullIter % dwFactor) // Opps, round up this number
      {
        
        ullIter = ((ullIter/dwFactor) + 1) * dwFactor;
        *pullIter = ullIter;
      }

    // otherwise, do nothing    
  }

