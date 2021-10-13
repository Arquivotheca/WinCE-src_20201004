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
 * cacheSizeCalculation.cpp
 */

/*
 * Main test code for caches.  The TESTPROCs call into here after
 * setting stuff up.
 */
#include "cacheSizeCalculation.h"
#include "..\..\..\..\common\commonUtils.h"
#include <windows.h>

/***************************************************************************
 *
 *                                Implementation
 *
 ***************************************************************************/

// see header for details
BOOL
runCacheSizeCalculation (
        DWORD *vals,
        DWORD dwMinArray,
        DWORD dwMaxArray,
        DWORD dwMinStride,
        DWORD dwMaxStride,
        DWORD dwCacheSizeJumpRate,
        DWORD dwCacheLineJumpRate,
        CACHE_ACCESS_INFO *pAccessRawData,
        ULONGLONG ullIterations)
  {
    BOOL returnVal = TRUE;
    ULONGLONG ullIter, ullRunningTime;
    DWORD dwDummy, index;
    DWORD dwArrayOffset  = 0;
    DWORD dwStrideOffset = 0;

    DWORD dwSteps, dwArrayEnum;
    numberOfEnum(dwMinStride, dwMaxStride, &dwSteps);
    numberOfEnum(dwMinArray, dwMaxArray, &dwArrayEnum);

    /*
     * Walk through different arrays with different strides
     */
    for (DWORD dwPt = 0; dwPt < MAX_RUNNING_LOOP; dwPt++)
      {
        Info (_T("Running loop %u of %u..."), (dwPt + 1), MAX_RUNNING_LOOP);
        ullIter = ullIterations; // reset

        DWORD dwArrayOffset = 0;

        for (DWORD dwArray = dwMinArray; dwArray <= dwMaxArray; dwArray *= 2)
          {
            DWORD dwStrideOffset = 0;
            for (DWORD dwStride = dwMinStride; dwStride <= dwMaxStride; dwStride *= 2)
              {
                cacheInit(vals, dwArray, dwStride, &dwDummy);

                returnVal = runCacheAccess (
                                      vals,
                                      dwArray,
                                      dwStride,
                                      ullIter,
                                      &ullRunningTime,
                                      &dwDummy);

                if (returnVal == TRUE)
                  {
                    /*
                     * for each test running, the results are organized
                     * consecutively, so this index is to locate the
                     * correct position
                     */
                    index = dwPt + dwArrayOffset + dwStrideOffset;

                    pAccessRawData[index].dwPt     = dwPt;
                    pAccessRawData[index].dwArray  = dwArray;
                    pAccessRawData[index].dwStride = dwStride;
                    pAccessRawData[index].ullTime  = ullRunningTime;
                    pAccessRawData[index].dwPnt    = ullIter * dwArray;
                    Sleep(500);
                  }
                else
                  {
                    Error (_T("Cache test failed"));
                    Info (_T(""));
                    goto cleanAndReturn;
                  }

                dwStrideOffset += MAX_RUNNING_LOOP;
              }


            /*
             * The calculated duration is used to normalize the total access point
             */
            ullIter /= 2;

            dwArrayOffset += dwSteps * MAX_RUNNING_LOOP;
          }
      }

    Info (_T(""));

    Info (_T("Cache access raw data by loop#, stride# and array size ..."));
    Info (_T("(*** The real cache accesses are NOT in such orders! ***)"));
    Info (_T("---------------------------"));
    printCacheCal(&pAccessRawData, dwSteps * dwArrayEnum * MAX_RUNNING_LOOP);
    Info (_T("---------------------------"));
    Info (_T(""));
    Sleep(1000);

    // to get the median data, and figure out the cache size/block size
    returnVal = analyzeData(
                      &pAccessRawData,
                      dwMinStride,
                      dwMaxStride,
                      dwArrayEnum,
                      dwCacheSizeJumpRate,
                      dwCacheLineJumpRate);

    Sleep(4000); // wait a while for the output

cleanAndReturn:
    return returnVal;
  }


// see header for the details
VOID
cacheInit(DWORD *vals, DWORD dwSize, DWORD dwStride, DWORD *dwDummy) {
    DWORD dwPos;
    DWORD dwJunk = 0;

    for (dwPos = 0; dwPos < dwSize; dwPos++)
      {
        dwJunk += vals[dwPos];
      }

    for (DWORD dwLoop = 0; dwLoop < 2; dwLoop++) {
      for (dwPos = 0; dwPos < dwSize; dwPos += dwStride)
        {
          dwJunk += vals[dwPos];
        }
      }

    *dwDummy = dwJunk; // cheat the optimizer
  }

/******************************************************************************
 * The core function to access the cache by reading the data from array       *
 *                                                                            *
 * See header for more information about this function.                       *
 ******************************************************************************
 */
BOOL
runCacheAccess(
            DWORD *vals,
            DWORD dwArray,
            DWORD dwStride,
            ULONGLONG ullIterations,
            ULONGLONG *pRunTime,
            DWORD *dwDummyValue)
{
  DWORD dwPos, dwSample, dwJunk = 0;
  ULONGLONG ullTimeBegin, ullTimeEnd, runningTime;

  ullTimeBegin = (ULONGLONG)GetTickCount();
  for (ULONGLONG ullIter = 0; ullIter < ullIterations; ullIter++)
    {
      for (dwSample = 0; dwSample < dwStride; dwSample++)
        {
          for (dwPos = 0; dwPos < dwArray; dwPos += dwStride)
          {
            dwJunk += vals[dwPos];  /* cache access*/
          }
        }
    }

  ullTimeEnd = (ULONGLONG)GetTickCount();

  /* subtract, accounting for overflow */
  if (AminusB(runningTime, ullTimeEnd, ullTimeBegin, MAXDWORD) == FALSE)
    {
      // This should never happen.
      IntErr (_T("AminusB failed in runCacheTest"));
      return FALSE;
    }

  *pRunTime = runningTime;
  *dwDummyValue = dwJunk;

  return (TRUE);
}

// see header for the details
BOOL
analyzeData(
       CACHE_ACCESS_INFO **pAccessRawData,
       DWORD dwMinStride,
       DWORD dwMaxStride,
       DWORD dwArrayEnum,
       DWORD dwCacheSizeJumpRate,
       DWORD dwCacheLineJumpRate)
  {
    BOOL returnVal = TRUE;
    DWORD dwJumpPos, dwCacheSize, dwBlockSize;
    DOUBLE doDeviation;
    DOUBLE *doDeviations = NULL;
    CACHE_ACCESS_INFO *pMedianValues = NULL;
    DWORD dwSteps;
    numberOfEnum(dwMinStride, dwMaxStride, &dwSteps);

    /*
     * for marking the jump position purpose
     *
     * Note:
     *  There exist four kinds of status which are used for marking the jump, i.e.
     *        - SUCCESSFUL    : jump found
     *        - FAILED        : jump having found is not correct
     *        - NOT_CALCULTED : jump calculation is skipped
     *        - NONE          : initialized
     *
     *  For all the jump positions relative to different stride#, only those
     *  whose jump status are one of the FIRST THREE will be marked
     */
    WORD *pwJumpStatus = new WORD[dwSteps];
    if (pwJumpStatus == NULL)
      {
        Info (_T("memory allocation error!"));
        return FALSE;
      }

    for (WORD i = 0; i < dwSteps; i++)
      {
        pwJumpStatus[i] = NONE;
      }

    DWORD dwSearchJumpLimit = JUMP_SEARCH_LIMIT / sizeof(DWORD);
    if (dwSearchJumpLimit > dwMaxStride)
      {
        dwSearchJumpLimit = dwMaxStride;
      }

    /*
     * To keep MAX_RUNNING_LOOP number of specific array/stride access info. Then,
     * retrieve the median values from it and store in pMedianValues as below
     *
     */
    CACHE_ACCESS_INFO *pInfo =
          (CACHE_ACCESS_INFO *)VirtualAlloc(
                      NULL,
                      sizeof(CACHE_ACCESS_INFO) * MAX_RUNNING_LOOP,
                      MEM_COMMIT,
                      PAGE_READWRITE);

    if (pInfo == NULL)
      {
        Info (_T("memory allocation error!"));
        returnVal = FALSE;
        goto cleanAndExit;
      }

    /*
     * save all the median point values of accessing arrays with different strides
     */
    DWORD dwArraySize = dwSteps * dwArrayEnum;
    DWORD dwArrayMemSize = sizeof(CACHE_ACCESS_INFO) * dwArraySize;
    pMedianValues = (CACHE_ACCESS_INFO *)VirtualAlloc(
                      NULL,
                      dwArrayMemSize,
                      MEM_COMMIT,
                      PAGE_READWRITE);

    if (pMedianValues == NULL || dwArrayMemSize <= dwArraySize)
      {
        Info (_T("memory allocation error!"));
        returnVal = FALSE;
        goto cleanAndExit;
      }

    DWORD newIndex = 0;
    DWORD dwCounter = dwSteps * dwArrayEnum * MAX_RUNNING_LOOP;
    for (DWORD index = 0; index < dwCounter; index+= MAX_RUNNING_LOOP)
      {
        for(DWORD inner = 0; inner < MAX_RUNNING_LOOP; inner++)
          {
            // we just need two params for sorting purposes
            pInfo[inner].ullTime = (*pAccessRawData)[index + inner].ullTime;
            pInfo[inner].dwPt = (*pAccessRawData)[index + inner].dwPt;
          }

        // bubble sorting
        sortData(&pInfo);

        // retrieve the mean values
        getMedianValue(pMedianValues, newIndex, pInfo);

        // we just simply retrieve one even though the sample is even#
        pMedianValues[newIndex].dwPt = pInfo[MAX_RUNNING_LOOP/2].dwPt;

        // we need other values, too
        pMedianValues[newIndex].dwArray = (*pAccessRawData)[index].dwArray;
        pMedianValues[newIndex].dwStride = (*pAccessRawData)[index].dwStride;
        pMedianValues[newIndex].dwPnt = (*pAccessRawData)[index].dwPnt;
        newIndex++;
      }

    Info (_T(""));
    Info (_T("Data processing..."));
    Info (_T("As for each size of array's specific stride cache accessess, pick up"));
    Info (_T("the median data point from %u samples"), MAX_RUNNING_LOOP);
    Info (_T(""));
    Info (_T("We use these median data as the source to calculate the cache sizes"));
    Info (_T("and cache line sizes"));

    Info (_T(""));
    Info (_T("Cache access median data output..."));
    Info (_T("----------------------------------------------------------------"));
    printCacheCal(&pMedianValues, dwSteps * dwArrayEnum);
    Info (_T("----------------------------------------------------------------"));
    Sleep(1000);

    Info (_T(""));
    Info (_T("Summary of the cache access time:(unit: ms)"));
    Info (_T("----------------------------------------------------------------"));
    /*
     * format print out the cache access data, array sizes as the rows, and
     * the stride sizes as the column
     */
    printCacheCalByRow(
          &pMedianValues,
          0,
          0,
          dwSteps * dwArrayEnum,
          dwSteps,
          NULL);
    Info (_T("----------------------------------------------------------------"));

    // analyze the data to figure out the size of the cache
    Info (_T(""));
    Info (_T("Calculating the cache size(s) ..."));
    Info (_T(""));
    returnVal = calculateCacheSize(
                          &pMedianValues,
                          dwMinStride,
                          dwMaxStride,
                          dwArrayEnum,
                          dwCacheSizeJumpRate,
                          &dwJumpPos,
                          &dwCacheSize,
                          pwJumpStatus);


    Sleep(1000);

    if (returnVal == FALSE)
      {
        Info (_T("**** Failed to calculate the cache size! ****"));
        goto cleanAndExit;
      }


    Info (_T(""));
    Info (_T("-----------------------------------------------------"));
    Info (_T("Summary of the cache access time:(unit: ms)"));
    Info (_T("==========================================="));
    Info (_T("(Note: S - Successful, F - Failed)"));
    Info (_T("-----------------------------------------------------"));

    /*
     * print out the cache calculation result with jump marked
     */
    printCacheCalByRow(
                &pMedianValues,
                0,
                dwJumpPos,
                dwSteps * dwArrayEnum,
                dwSteps,
                pwJumpStatus);

    doDeviation = getStandardDeviation(&pMedianValues, 0, dwJumpPos * dwSteps);

    doDeviations = (DOUBLE *)malloc(dwSteps * sizeof(DOUBLE));
    if (doDeviations == NULL)
      {
        Error (_T(""));
        Error (_T("memory allocation error"));
        Error (_T(""));

        returnVal = FALSE;
        goto cleanAndExit;
      }

    if (calculateDeviationsByStride(
                &pMedianValues,
                dwArrayEnum,
                dwSteps,
                dwJumpPos,
                doDeviations) == FALSE)
      {
        returnVal = FALSE;
        goto cleanAndExit;
      }

    Info (_T("----------------------------------------------------------------"));
    Info (_T("Standard deviations by strides: (unit: ms)"));
    Info (_T("Array ==> [%uKB ~ %uKB]"),
              pMedianValues[0].dwArray * sizeof (DWORD) / 1024,
              pMedianValues[(dwJumpPos - 1) * dwSteps].dwArray * sizeof(DWORD) / 1024);
    printDeviationByStride(doDeviations, dwSteps);
    Info (_T("----------------------------------------------------------------"));
    Info (_T(""));
    Info (_T("----------------------------------------------------------------"));

    Info (_T("Standard Deviation (unit: ms): *** %.2f ***"), doDeviation);
    Info (_T("Stride ==> [%ubytes ~ %ubytes]"),
              dwMinStride * sizeof(DWORD),
              dwSearchJumpLimit * sizeof(DWORD));

    Info (_T("Array ==> [%uKB ~ %uKB])"),
              pMedianValues[0].dwArray * sizeof (DWORD) / 1024,
              pMedianValues[(dwJumpPos - 1) * dwSteps].dwArray * sizeof(DWORD) / 1024);
    Info (_T("----------------------------------------------------------------"));
    Info (_T(""));
    Info (_T("NOTE:"));
    Info (_T("The standard deviation measures how spread out the cache access"));
    Info (_T("MISS times are. If MISS time points are all close to the mean,"));
    Info (_T("then the standard deviation is close to zero; otherwise, it is"));
    Info (_T("far from zero"));
    Info (_T(""));
    Info (_T("The standard deviation is measured in the same units as the MISS"));
    Info (_T("time values of the population"));
    Info (_T("----------------------------------------------------------------"));
    Info (_T(""));

    Info (_T("Calculating the line size(s) ..."));

    // analyze the data to figure out the size of the cache line
    returnVal = calculateBlockSize(
                      &pMedianValues,
                      dwSteps,
                      dwArrayEnum,
                      dwCacheSize,
                      dwMinStride,
                      dwMaxStride,
                      dwCacheLineJumpRate,
                      &dwBlockSize);

    Sleep(2000);

    Info (_T(""));
    Info (_T("Calculation Report:"));
    Info (_T("----------------------------------------------------------------"));
    Info (_T("**** Cache Size: %uKB ****"), dwCacheSize * 4 / 1024);
    if (returnVal == TRUE)
        Info (_T("**** Block Size: %ubytes ****"), dwBlockSize);
    else
        Info (_T("**** Failed to analyze the block size! ****"));

    Info (_T("----------------------------------------------------------------"));
    Info (_T(""));


cleanAndExit:
    if (pwJumpStatus != NULL)
    {
        delete[] pwJumpStatus;
    }

    if (doDeviations != NULL)
    {
        free(doDeviations);
    }

    if (VirtualFree (pInfo, 0, MEM_RELEASE) == 0)
      {
        Error (_T("Could not free memory. GetLastError: %u."), GetLastError);
        returnVal = FALSE;
      }

    if (VirtualFree (pMedianValues, 0, MEM_RELEASE) == 0)
      {
        Error (_T("Could not free memory. GetLastError: %u."), GetLastError);
        returnVal = FALSE;
      }

    return returnVal;
  }

// see the header for the details
BOOL
calculateCacheSize(
          CACHE_ACCESS_INFO **pMedianValues,
          DWORD dwMinStride,
          DWORD dwMaxStride,
          DWORD dwArrayEnum,
          DWORD dwCacheSizeJumpRate,
          DWORD *pdwJumpPos,
          DWORD *pdwCacheSize,
          WORD  *pwJumpStatus)
  {
    DWORD dwJumpPos, dwPrevSize = 0, dwSize = 0;
    BOOL bFound = FALSE;
    DOUBLE doJumpRate;

    DWORD dwSteps;
    numberOfEnum(dwMinStride, dwMaxStride, &dwSteps);

    DWORD dwBlock = dwSteps * dwArrayEnum;

    /*
     * Search each jump for every stride. And then compare all the jumps, if
     * they are different, then show the warning and print out the data
     */
    Info (_T("---------------------------------------------------------------"));
    Info (_T("\tSearching jump from stride: %u DWORDs, i.e. %ubytes"),
              dwMinStride,
              dwMinStride * sizeof(DWORD));
    if (isCacheJump(
              pMedianValues,
              0,
              dwSteps,
              dwBlock,
              dwCacheSizeJumpRate,
              &dwJumpPos,
              &dwPrevSize,
              &doJumpRate) == TRUE)
      {
        pwJumpStatus[0] = SUCCESSFUL;
        bFound = TRUE;
        Info (_T("\t\t==>Jump of %.0f%% found, this is above %u%% threshold!!!"),
                      doJumpRate,
                      dwCacheSizeJumpRate);
        Info (_T(""));
      }

    DWORD dwStart = 0;
    DWORD dwLimit = JUMP_SEARCH_LIMIT / sizeof (DWORD);
    dwLimit = (dwLimit < dwMaxStride) ? dwLimit : dwMaxStride;

    /*
     * walk through the remaining stride(s) up to the search limit to verify
     * the jump
     */
    for (DWORD dwStride = 2 * dwMinStride; dwStride <= dwLimit; dwStride *= 2)
      {
        Info (_T("\tSearching jump from stride: %u DWORDs, i.e. %ubytes"),
                  dwStride,
                  dwStride * sizeof(DWORD));

        dwStart++;
        if (isCacheJump(
                pMedianValues,
                dwStart,
                dwSteps,
                dwBlock,
                dwCacheSizeJumpRate,
                &dwJumpPos,
                &dwSize,
                &doJumpRate) == TRUE)
          {
            Info (_T("\t\t==>Jump of %.0f%% found, this is above %u%% threshold!!!"),
                      doJumpRate,
                      dwCacheSizeJumpRate);
            Info (_T(""));

            if (dwPrevSize != dwSize)
              {
                if (pwJumpStatus[dwStart - 1] == SUCCESSFUL)
                  {
                    pwJumpStatus[dwStart] = FAILED;
                    Info (_T(""));
                    Info (_T("\tWarning!!! The jumps are not the same"));
                    Info (_T(""));
                    bFound = FALSE;
                  }
                break;
              }
            else if (dwPrevSize != 0)
              {
                pwJumpStatus[dwStart] = SUCCESSFUL;
                bFound = TRUE;
              }

            dwPrevSize = dwSize;
          }

      }

      *pdwJumpPos = dwJumpPos;
      *pdwCacheSize = dwSize;

      // skip the remaining stride(s), mark them as "NOT_CALCULATED"
      for(DWORD dwExtra = dwLimit * 2;  dwExtra <= dwMaxStride; dwExtra *= 2)
        {
          dwStart++;
          pwJumpStatus[dwStart] = NOT_CALCULATED;
        }

      return bFound;
  }

// see header for the details
BOOL
isCacheJump(
        CACHE_ACCESS_INFO **pMedianValues,
        DWORD  dwStart,
        DWORD  dwSteps,
        DWORD  dwBlock,
        DWORD  dwCacheSizeJumpRate,
        DWORD  *pdwJumpPos,
        DWORD  *pdwCacheSize,
        DOUBLE *pdoJumpRate)
  {
    DOUBLE doCurTime, doPrevTime;
    DWORD dwCacheSize, dwArray, dwJumpPos;

    DOUBLE doJumpFactor = 1.0 + dwCacheSizeJumpRate / 100.0;

    if ((*pMedianValues)[dwStart].ullTime == 0)
      {
        Error (_T("Cache access duration is ZERO"));
        return FALSE;
      }

    // get the start point info
    dwArray = (*pMedianValues)[dwStart].dwArray;
    dwCacheSize = (*pMedianValues)[dwStart].dwArray;
    doPrevTime = (DOUBLE)(*pMedianValues)[dwStart].ullTime;

    dwJumpPos = 0;

    Info (_T("\t\tArray Size: %u KB\t==> access time: %.0f"),
              dwArray * sizeof(DWORD) / 1024,
              doPrevTime);


    // search the next points and compare the access time for the possible jump
    for(DWORD dwIndex = dwStart + dwSteps; dwIndex < dwBlock; dwIndex += dwSteps)
      {
        if ((*pMedianValues)[dwIndex].ullTime == 0)
          {
            Error (_T("Cache access duration is ZERO"));
            return FALSE;
          }

        dwArray = (*pMedianValues)[dwIndex].dwArray;
        doCurTime = (DOUBLE)(*pMedianValues)[dwIndex].ullTime;
        dwJumpPos++;

        Info (_T("\t\tArray Size: %u KB\t==> access time: %.0f"),
                  dwArray * sizeof(DWORD) / 1024,
                  doCurTime);

        if (doCurTime > doJumpFactor * doPrevTime)
          {
            *pdwCacheSize = dwCacheSize;
            *pdwJumpPos = dwJumpPos;
            *pdoJumpRate = (doCurTime - doPrevTime) / doPrevTime * 100;
            Info (_T(""));
            return TRUE;
          }

        doPrevTime = doCurTime;
        dwCacheSize = (*pMedianValues)[dwIndex].dwArray;
      }

    Info (_T("\t\t*** No %u percent cache jump found ***"), dwCacheSizeJumpRate);
    Info (_T(""));
    return FALSE;
  }

// see the header for the details
BOOL
calculateBlockSize(
            CACHE_ACCESS_INFO **pMedianValues,
            DWORD dwSteps,
            DWORD dwArrays,
            DWORD dwCacheSize,
            DWORD dwMinStride,
            DWORD dwMaxStride,
            DWORD dwCacheLineJumpRate,
            DWORD *pdwBlockSize)
  {
    BOOL bFound = FALSE;
    BOOL bReturnVal = TRUE;
    DOUBLE doHitTime;
    DWORD dwCacheLine = 0, dwArrayIndex = 0, dwStartPoint = 0;

    DWORD dwCounter = dwSteps * dwArrays;
    DWORD dwArrayMemSize = dwCounter * sizeof(DOUBLE);

    // allocate the maximum memory space to store the calculations
    DOUBLE *doMissTime = (DOUBLE *)malloc(dwArrayMemSize);
    if (doMissTime == NULL || dwArrayMemSize <= dwCounter)
      {
        Error (_T(""));
        Error (_T("Cannot allocate the memory for storing cache MISS times"));
        Error (_T(""));

        bReturnVal = FALSE;
        goto checkAndExit;
      }

    /*
     * search the array that jump occurs. we calculate the cache
     * access MISS/HIT based on this array's access data
     */
    for (; dwStartPoint < dwCounter; dwStartPoint++)
      {
        if ((*pMedianValues)[dwStartPoint].dwArray > dwCacheSize)
          {
            break;
          }
      }

    if (dwStartPoint == 0 || dwStartPoint == dwCounter)
      {
        Error (_T(""));
        Error (_T("Cannot figure out the block size"));
        Error (_T(""));

        bReturnVal = FALSE;
        goto checkAndExit;
      }

    if (dwStartPoint < dwSteps)
      {
        Error (_T(""));
        Error (_T("Invalid cache size!!!"));
        Error (_T(""));

        bReturnVal = FALSE;
        goto checkAndExit;
      }

    doHitTime = (DOUBLE)(*pMedianValues)[dwStartPoint - dwSteps].ullTime;

    Info (_T("\tHitTime ==> %.0f ms"), doHitTime);

    DOUBLE doJumpRate;

    Info (_T(""));

    DWORD dwCacheLineJumpPos = 0;
    DWORD dwMissTimeCounter = 0;
    DWORD dwStart, dwEnd;
    DWORD dwSize;

    /*
     * start the size of the cache line from 2 DWORDs, i.e. 8 bytes
     */
    for (dwCacheLine = dwMinStride; dwCacheLine <= dwMaxStride; dwCacheLine *= 2)
      {
        Info (_T("\t--------------------------------------------------"));
        Info (_T("\tSupposed that the cache line size is: %ubytes, ..."),
                  dwCacheLine * sizeof(DWORD));

        DWORD dwMaxStrideEnum;
        numberOfEnum(1, dwCacheLine, &dwMaxStrideEnum);

        //dwArrayIndex = dwStartPoint + dwMaxStrideEnum - 1;
        /*
         * calculate the index, just case the calculation overflow. This is supposed
         * to happen!!!
         */
        if (CeDWordAdd3(dwStartPoint, dwMaxStrideEnum - 1, 0, &dwArrayIndex) != S_OK)
          {
            Error (_T("Data overflow during index calculation"));
            bFound = FALSE;
            goto checkAndExit;
          }

        if (dwArrayIndex >= dwCounter)
          {
            Error (_T("Index out of boundary"));
            bFound = FALSE;
            goto checkAndExit;
          }

        dwArrayIndex = dwStartPoint;

        // counter the element for the following loop
        numberOfEnum(dwMinStride, dwCacheLine, &dwSize);

        dwStart = dwMissTimeCounter;
        dwEnd = dwStart + dwSize - 1;

        /*
         * based on this pretended cache line size, we then calculate the
         * cache MISS time for different stride#
         */
        for (DWORD dwStride = dwMinStride; dwStride <= dwCacheLine; dwStride *= 2)
          {
            Info (_T("\t\tCalculate the cache miss time for stride: %ubytes ..."),
                      dwStride * sizeof(DWORD));

            Info (_T("\t\t\t%u/%u * MISS + (1 - %u/%u) * HIT = %I64u ms"),
                      dwStride,
                      dwCacheLine,
                      dwStride,
                      dwCacheLine,
                      (*pMedianValues)[dwArrayIndex].ullTime);

            /*
             * The formula to calcuate the MISS time:
             *
             * ACCESS_TIME[dwStride]
             *   = (dwStride / dwCacheLine) * MISS + (1 - dwStride / dwCacheLine) * HIT
             *
             * ==> MISS = [dwCacheLine*ACCESS - (dwCacheLine/dwStride - 1)*HITT]/dwStride
             */
            DOUBLE doCurMissTime  = (DOUBLE)dwCacheLine;
            doCurMissTime *= (DOUBLE)(*pMedianValues)[dwArrayIndex].ullTime;
            doCurMissTime -= (dwCacheLine - dwStride) * doHitTime;
            doMissTime[dwMissTimeCounter] = doCurMissTime / dwStride;

            Info (_T("\t\t\t*** HIT time: %.0f ms"),doHitTime);
            Info (_T("\t\t\t==> MISS time: %.0f ms"), doMissTime[dwMissTimeCounter]);
            Info (_T(""));

            dwMissTimeCounter++;
            dwArrayIndex++;
          }

        if (isBlockJump(
                   doMissTime,
                   dwStart,
                   dwEnd,
                   dwCacheLineJumpRate,
                   &doJumpRate) == TRUE)
          {
            // need to mark the jump point
            dwCacheLineJumpPos = dwCacheLine;

            Info (_T("*** Jump of %.0f%% found, this is above %u%% threshold! ***"),
                      doJumpRate,
                      dwCacheLineJumpRate);

            // the previous stride# is the cache line size
            dwCacheLine = dwCacheLine * sizeof(DWORD) / 2;
            bFound = TRUE;
            goto output;
          }
      }

output:
    dwStart = 0;
    Info (_T(""));
    Info (_T("Summary of cache access MISS time(ms) per stride"));
    Info (_T("----------------------------------------------------------------"));

    for (DWORD dwOut = dwMinStride; dwOut <= dwMaxStride; dwOut *= 2)
      {
        DWORD dwSize;
        numberOfEnum(dwMinStride, dwOut, &dwSize);

        if (dwCacheLineJumpPos > 0 && dwCacheLineJumpPos == dwOut)
          {
            printBlockCalByRow(dwOut, doMissTime, dwStart, dwStart + dwSize - 1, TRUE);
            break;
          }
        else
          {
            printBlockCalByRow(dwOut, doMissTime, dwStart, dwStart + dwSize - 1, FALSE);
          }
        dwStart += dwSize;
      }
    Info (_T("----------------------------------------------------------------"));
    Info (_T(""));

checkAndExit:
    if (bFound == FALSE)
      {
        Info (_T("\t\t*** No %u percent block jump found ***"), dwCacheLineJumpRate);
        Info (_T(""));
        bReturnVal = FALSE;
      }
    else
      {
        *pdwBlockSize = dwCacheLine;
      }

    if (doMissTime != NULL)
      {
        free (doMissTime);
      }

    return bReturnVal;
  }

// see the header for the details
BOOL
isBlockJump(
       DOUBLE *doMissTime,
       DWORD dwStart,
       DWORD dwEnd,
       DWORD dwCacheLineJumpRate,
       DOUBLE *pdoJumpRate)
  {
    if ((dwEnd - dwStart) > 1)
      {
        DOUBLE doMin = getMin(doMissTime, dwStart, dwEnd);
        DOUBLE doMax = getMax(doMissTime, dwStart, dwEnd);

        if (doMin < 1.0e-3)
          {
            Error (_T("Maximum miss time is too small"));
            return FALSE;
          }

        *pdoJumpRate = (doMax - doMin)/doMin * 100.0;

        if (*pdoJumpRate > (DOUBLE)dwCacheLineJumpRate)
          {
            return TRUE;
          }
      }

    return FALSE;
  }

// see the header for the details
DOUBLE
getStandardDeviation(CACHE_ACCESS_INFO **pMedianValues, DWORD dwStart, DWORD dwEnd)
  {
    DWORD dwCounter, dwSize;
    DWORD dwArrayMemSize;
    DOUBLE doResult;

    doResult = 0.0;

    dwCounter = 0;
    dwSize = dwEnd - dwStart;
    dwArrayMemSize = dwSize * sizeof (DOUBLE);

    DOUBLE *doSamples = (DOUBLE *)malloc (dwArrayMemSize);

    if (NULL != doSamples && dwArrayMemSize > dwSize) {
      for(DWORD dwIndex = dwStart; dwIndex < dwEnd; dwIndex++)
        {
          doSamples[dwCounter++] = (DOUBLE)(*pMedianValues)[dwIndex].ullTime;
        }

      doResult = getStandardDeviation(doSamples, dwSize);
    }

    if (NULL != doSamples)
    {
        free (doSamples);
    }

    return doResult;
  }

// see the header for the details
DOUBLE
getStandardDeviation(DOUBLE *doSamples, DWORD dwSize)
  {
    if (dwSize <= 1)
      {
        return 0.0;
      }

    DOUBLE doSquare = 0;
    DOUBLE doSum = 0;

    for(DWORD dwIndex = 0; dwIndex < dwSize; dwIndex++)
      {
        DOUBLE doTime = doSamples[dwIndex];
        doSquare += doTime * doTime;
        doSum += doTime;
      }

    return sqrt((doSquare - doSum * doSum / (DOUBLE)dwSize) / (DOUBLE)(dwSize - 1));
  }


VOID
printCacheCal(CACHE_ACCESS_INFO **pAccessRawData, DWORD dwCounter)
  {
    Info (_T("Stride unit:   DWORD(s)"));
    Info (_T("Duration unit: millisecond(s)"));
    Info (_T("----------------------------------------------------------------"));
    for(DWORD k = 0; k < dwCounter; k++)
      {
        Info (_T("pt = %u\tsize = %uKB\t\t\t\tstride = %u\t\t\tduration = %I64u"),
                (*pAccessRawData)[k].dwPt,
                (*pAccessRawData)[k].dwArray * 4/1024,
                (*pAccessRawData)[k].dwStride,
                (*pAccessRawData)[k].ullTime);
      }
  }

// see the header for details
BOOL
numberOfEnum(DWORD dwMinSize, DWORD dwMaxSize, DWORD* dwEnum)
  {
    DWORD dwNum = 0;

    if (dwMinSize > 0 && dwMaxSize > 0)
      {
        while (dwMinSize <= dwMaxSize)
          {
            dwNum++;
            dwMinSize *= 2;
          }

        *dwEnum = dwNum;

        if (dwNum > 0)
          {
            return TRUE;
          }
      }

    return FALSE;
  }

// see the header for the details
VOID
printCacheCalByRow(
         CACHE_ACCESS_INFO **pInfo,
         DWORD dwStart,
         DWORD dwJump,
         DWORD dwEnd,
         DWORD dwSteps,
         WORD *pwJumpStatus)
  {
    TCHAR tcTitle[BUFFER_SIZE];
    TCHAR tcInfo[BUFFER_SIZE];
    TCHAR tcTmp[BUFFER_SIZE];

    DWORD dwRows = (dwEnd - dwStart) / dwSteps;

    _tcscpy_s(tcTitle, BUFFER_SIZE, _T("Array Size"));

    for (DWORD i = 0; i < dwSteps; i++)
      {
        concatString(tcTitle, _T("Stride"), 10);
      }

    Info (tcTitle); // print the title 1

    copyString(tcTitle, _T("------"), 10);

    for (DWORD i = 0; i < dwSteps; i++)
        concatString(tcTitle, _T("-bytes-"), 10);

    Info (tcTitle); // print the title 2

    copyString(tcTitle, _T("###"), 10);
    for (DWORD i = 0; i < dwSteps; i++)
      {

        swprintf_s(tcInfo,
                   BUFFER_SIZE,
                   _T("%u"),
                  (*pInfo)[dwStart + i].dwStride * sizeof(DWORD));
         concatString(tcTitle, tcInfo, 10);
      }
    Info (tcTitle); // print the stride numbers
    Info (_T(""));

    /*
     * print array size, and the access time corresponding to each stride
     */
    for (DWORD i = 0; i < dwRows; i++)
      {
        swprintf_s(tcTmp,
                   BUFFER_SIZE,
                   _T("%uKB"),
                  (*pInfo)[dwStart + i * dwSteps].dwArray * 4 / 1024);
        copyString(tcInfo, tcTmp, 10);

        for (DWORD j = 0; j < dwSteps; j++)
          {
            ULONGLONG ullTime = (*pInfo)[dwStart + i * dwSteps + j].ullTime;

            if (pwJumpStatus != NULL && i == dwJump) // mark the jump point(s)
              {
                switch(pwJumpStatus[j])
                  {
                    case SUCCESSFUL:
                      swprintf_s(tcTmp, BUFFER_SIZE, _T("(S)%I64u"), ullTime);
                      break;

                    case FAILED:
                      swprintf_s(tcTmp, BUFFER_SIZE, _T("(F)%I64u"), ullTime);
                      break;

                    default:
                      swprintf_s(tcTmp, BUFFER_SIZE, _T("%I64u"),    ullTime);
                      break;
                  }
              }
            else // normal output
              {
                swprintf_s(tcTmp, BUFFER_SIZE, _T("%I64u"), ullTime);
              }
            concatString(tcInfo, tcTmp, 10);
          }

        Info (tcInfo);
      }
  }

// see the header for the details
BOOL
calculateDeviationsByStride(
         CACHE_ACCESS_INFO **pInfo,
         DWORD dwNumberOfArray,
         __in DWORD dwNumberOfStride,
         DWORD dwJumpPosOfArray,
         __out_ecount(dwNumberOfStride) DOUBLE *doDeviations)
  {
    DWORD dwDevs = 0;

    DOUBLE *doAccessTime = (DOUBLE *)malloc(dwJumpPosOfArray * sizeof(DOUBLE));
    if (doAccessTime == NULL)
      {
        Error (_T(""));
        Error (_T("memory allocation error for cache info print"));
        Error (_T(""));
        return FALSE;
      }

    /*
     * For each stride#, search the access time for different array size up to the
     * cache size, then its calculate the standard deviation and saved for
     * output
     */
    for (DWORD dwCol = 0; dwCol < dwNumberOfStride; dwCol++)
      {
        DWORD dwCounter = 0;
        for (DWORD dwRow = 0; dwRow < dwJumpPosOfArray; dwRow++)
          {
            ULONGLONG ullTime = (*pInfo)[dwCol + dwRow * dwNumberOfStride].ullTime;
            doAccessTime[dwCounter++] = (DOUBLE)ullTime;
          }
          doDeviations[dwDevs++] = getStandardDeviation(doAccessTime, dwCounter);
      }

    free(doAccessTime);

    return TRUE;
  }

// see the header for the details
VOID
printDeviationByStride(DOUBLE *doDeviations, DWORD dwSteps)
  {
    DWORD dwCounter = 0;
    TCHAR tcDevs[BUFFER_SIZE];
    TCHAR tcTmp[BUFFER_SIZE];

    if (dwSteps == 0)
      {
        return;
      }

    // calculate the size of the buffer needed to hold the
    // output string

    copyString(tcDevs,_T("=========="), 10);

    swprintf_s(tcTmp, BUFFER_SIZE, _T("%.2f"), doDeviations[0]);
    concatString(tcDevs, tcTmp, 10);

    for (DWORD dwIndex = 1; dwIndex < dwSteps; dwIndex++)
      {
        swprintf_s(tcTmp, BUFFER_SIZE, _T("%.2f"), doDeviations[dwIndex]);
        concatString(tcDevs, tcTmp, 10);
      }
    Info (tcDevs);
  }


// see the header for the details
VOID
formatString(TCHAR *data, DWORD length)
  {
    TCHAR tchar[BUFFER_SIZE];
    length -= _tcslen(data);

    if (length > 0)
      {
        _tcscpy_s(tchar, BUFFER_SIZE, _T(" "));

        for (DWORD i = 0; i < length - 1; i++)
          {
            _tcscat_s(tchar, BUFFER_SIZE, _T(" "));
          }
        _tcscat_s(tchar, BUFFER_SIZE, data);
        _tcscpy_s(data, BUFFER_SIZE, tchar);
      }
  }

// see the header for the details
VOID
copyString(TCHAR *dest, const TCHAR *src, int lengthInTotal)
  {
    _tcscpy_s(dest, BUFFER_SIZE, src);
    formatString(dest, lengthInTotal);
  }

// see the header for the details
VOID
concatString(TCHAR *dest, const TCHAR *src, int lengthInTotal)
  {
    TCHAR tTmp[BUFFER_SIZE];
    _tcscpy_s(tTmp, BUFFER_SIZE, src);
    formatString(tTmp, lengthInTotal);
    _tcscat_s(dest, BUFFER_SIZE, tTmp);
  }

// see the header for the details
VOID
getMedianValue(CACHE_ACCESS_INFO *pDest, DWORD dwIndex, CACHE_ACCESS_INFO *pSrc)
  {
    DWORD median = MAX_RUNNING_LOOP / 2;
    ULONGLONG ullTime = pSrc[median].ullTime;

    // even samples. so, we need to average the two middle ones
    BOOL evenSamples = (MAX_RUNNING_LOOP & 1) ? FALSE : TRUE;
    if (evenSamples)
      {
        ullTime = (ullTime + pSrc[median - 1].ullTime) / 2;
      }

    pDest[dwIndex].ullTime = ullTime;
  }

//  see the header for the details
VOID
sortData(CACHE_ACCESS_INFO **pInfo)
  {
    bubbleSort(pInfo, MAX_RUNNING_LOOP);
  }

//  see the header for the details
VOID
bubbleSort(CACHE_ACCESS_INFO **pInfo, DWORD dwNum)
  {
    BOOL bSorted = FALSE;

    for (DWORD dwOut = 0; dwOut < dwNum && !bSorted; dwOut++)
      {
        bSorted = TRUE;
        for (DWORD dwInner = 0; dwInner < dwNum - dwOut - 1; dwInner++)
          {
            if ((*pInfo)[dwInner].ullTime > (*pInfo)[dwInner+1].ullTime)
              {
                swapData(pInfo, dwInner, dwInner+1);
                bSorted = FALSE;
              }
          }
      }
  }

// see the header for the details
VOID swapData(CACHE_ACCESS_INFO **pInfo, DWORD dwIndex1, DWORD dwIndex2)
  {
    ULONGLONG ullTime = (*pInfo)[dwIndex1].ullTime;
    DWORD dwPt = (*pInfo)[dwIndex1].dwPt;

    (*pInfo)[dwIndex1].ullTime = (*pInfo)[dwIndex2].ullTime;
    (*pInfo)[dwIndex1].dwPt = (*pInfo)[dwIndex2].dwPt;

    (*pInfo)[dwIndex2].ullTime = ullTime;
    (*pInfo)[dwIndex2].dwPt = dwPt;
  }

// see the header for the details
VOID
printBlockCalByRow(
              DWORD dwStride,
              DOUBLE *doMissTime,
              DWORD dwStart,
              DWORD dwEnd,
              BOOL needMark)
  {
    TCHAR tMissTime [BUFFER_SIZE];
    TCHAR tTmp[BUFFER_SIZE];

    // stride size
    swprintf_s(tTmp, BUFFER_SIZE,_T("%u"), dwStride * sizeof(DWORD));
    copyString(tMissTime, tTmp, 10);
    concatString(tMissTime, _T("bytes"), 6);

    for (DWORD dwIndex = dwStart; dwIndex <= dwEnd; dwIndex++)
      {
        // miss time
        swprintf_s(tTmp, BUFFER_SIZE, _T("%.0f"), doMissTime[dwIndex]);
        concatString(tMissTime, tTmp, 8);
      }

    // cache line size found, mark the jump position
    if (needMark == TRUE)
      {
        concatString(tMissTime, _T("<==Jump"), 8);
      }

    Info (tMissTime);
  }

// see the header for the details
DOUBLE
getMax(DOUBLE *doSamples, DWORD dwStart, DWORD dwEnd)
  {
    DOUBLE doMax = doSamples[dwStart];
    for (DWORD dwIndex = dwStart; dwIndex <= dwEnd; dwIndex++)
      {
        if (doMax < doSamples[dwIndex])
          {
            doMax = doSamples[dwIndex];
          }
      }

    return doMax;
  }

// see the header for the details
DOUBLE
getMin(DOUBLE *doSamples, DWORD dwStart, DWORD dwEnd)
  {
    DOUBLE doMin = doSamples[dwStart];
    for (DWORD dwIndex = dwStart; dwIndex <= dwEnd; dwIndex++)
      {
        if (doMin > doSamples[dwIndex])
          {
            doMin = doSamples[dwIndex];
          }
      }

    return doMin;
  }