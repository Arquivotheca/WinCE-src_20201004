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
 * tuxKitl.h
 */

#ifndef __TUX_CACHE_CHURN_H__
#define __TUX_CACHE_CHURN_H__

#include <windows.h>
#include <tux.h>

/*
* default run time in seconds
*/
#define DEFAULT_RUN_TIME_S              (10 * 60)

/*
* We calibrate the test by running it until this threshold is
* reached.  We then extrapolate how long a real run would take.  This
* is the calibration threshold in ms.  If the clocks won't work this
* won't work.
*
* This is assumed to be a dword.  Make sure that the multiplication
* won't overflow.
*/
#define TEST_CALIBRATION_THRESHOLD_S    (10)
#define TEST_CALIBRATION_THRESHOLD_MS   (TEST_CALIBRATION_THRESHOLD_S * 1000)

/* check point message every 10 minutes */
#define DW_PRINT_MESSAGE_EVERY_N_MS     (10 * 60 * 1000)

#define ARG_STR_WORKING_SET             _T("-maxWorkingSet")
#define ARG_STR_PAGE_SIZE               _T("-pageSize")
#define ARG_STR_RUN_TIME                _T("-runTime")
#define ARG_STR_CACHE_SIZE              _T("-cacheSize")
#define ARG_STR_BREAK_ON_FAILURE        _T("-breakOnFailure")
#define ARG_STR_CALI_THRESHOLD          _T("-caliThreshold")

/*
* default run time for the test cases.
*
* currently set to 10 min
*/
#define ULL_DEFAULT_RUN_TIME_S          (10 * 60)

/**************************************************************************
*
*                           Cache Test Functions
*
**************************************************************************/

TESTPROCAPI
PrintUsage(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE);

TESTPROCAPI testCacheChurn(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE);

/***************************** helper functions *****************************/

BOOL
runTestGivenWorkingSize (
                DWORD dwArraySize,
                DWORD dwPageSize,
                ULONGLONG pullRunTimeS,
                DWORD dwCalibrationInMs);

BOOL retrieveOptions(
                LPFUNCTION_TABLE_ENTRY lpFTE,
                DWORD* pdwCacheSize,
                DWORD* pdwPageSize,
                ULONGLONG* pullRunTimeS,
                DWORD* pdwCalibrationInMs);

BOOL
getCacheSize (DWORD * pdwCacheSize);

BOOL
getUserWorkingSet (DWORD * pdwWorkingSet);

BOOL
getPageSize (DWORD * pdwPageSize);

BOOL
getRunTime (ULONGLONG * pullRunTimeS);

BOOL
getCalibrationThreshold(DWORD *pdwCalibrationInMs);

BOOL
handleBreakOnFailureFlag ();

#endif // __TUX_KITL_LOOP_H__
