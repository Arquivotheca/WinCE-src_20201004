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
 * tuxCaches.cpp
 */

/*
 * This file implements the tux specific part of the cache tests.
 * This includes the TESTPROCs (entry points) for the tests in the tux
 * function table.  There are some initialization functions for
 * setting stuff up.  The functions then call the code that runs the
 * test, which is in the cacheTest.* files.
 *
 * Each TESTPROC uses getArraySizeFromFT to figure out how large the
 * array should be.  It then checks that this size is within the
 * contraints for the given test that will be run.  It calls
 * runCacheTest with the cacheTestType of the cache test.
 *
 * runCacheTest allocates memory for the test and then figures out how
 * many test iterations need to be run to run the test for the time
 * interval given by the user.  runCacheTest then calls the test for
 * this given time period.  It frees the memory when the test is done
 * and returns to the TESTPROC, which propagates the return value to
 * tux.
 */

/***** Headers  **************************************************************/

/* gives TESTPROCs declarations for these functions */
#include "tuxCaches.h"

/* common utils */
#include "..\..\..\common\commonUtils.h"

/* actual implementation of the cache testing functions */
#include "cacheTest.h"

/* sets up the cache for the tests */
#include "cacheControl.h"

/***** Constants and Defines Local to This File ******************************/

/*
 * use this to change the method used to allocate the array.
 *
 * USE_VIRTUAL_ALLOC uses the vm system to allocate the memory.  The
 * memory region is not guaranteed to be contiguous.
 * USE_ALLOC_PHYS_MEM tells the kernel to allocate a physically
 * contiguous block of memory.  This memory has caching turned off by
 * default.  The test must turn it before proceeding.
 *
 * Defining both of these produces undefined results.
 */

#define ARG_STR_CALIB_THRESH (_T("-CalibThresh"))
/*
 * Allows the code to be more modular.  Define one of these for each
 * individual test.  The TESTPROCs then just call into a standard
 * function that sets everything up.  We could do this with function
 * pointers, except that each function takes a different set and
 * different number are arguments.  Switch statements are easier.
 */
enum cacheTestType_t {CACHE_TEST_WRITE_BOUNDARIES_READ_EVERYTHING,
              CACHE_TEST_WRITE_EVERYTHING_READ_EVERYTHING,
              CACHE_TEST_VARY_POWERS_OF_TWO,
              CACHE_TEST_WRITE_EVERYTHING_READ_EVERYTHING_MIXEDUP};

/*
 * The tests step through memory on this interval.  It can't be less
 * than four, or some of the tests will overlap values and fail.  8 is
 * a good choice because most boundaries will be above it.  It is
 * small enough, though, that we exercise the cache a good deal.
 *
 * If you change this be sure to change the limits on dwArraySizeBytes
 * in the functions below.  The minimum array size that the functions
 * can handle are checked in the TESTPROCs.
 */
#define CACHE_TEST_STEP_SIZE 8

/*
 * We calibrate the test by running it until this threshold is
 * reached.  We then extrapolate how long a real run (on the order
 * of hours) would take.  This is the calibration threshold in ms.
 *
 * If the clocks aren't working all bets are off.  This doesn't
 * influence cache correctness; it just provides useful output
 * concerning how long the test will take.
 */
#define TEST_CALIBRATION_THRESHOLD_MS 1000

/*
 * The length of the test, in seconds.  Currently, all of the tests
 * run for the same time period.
 *
 * This value can't be larger than 2^32 / 1000 seconds = 2.4 million
 * seconds.
 *
 * currently using 10 minutes.
 */
#define DEFAULT_CACHE_TEST_LENGTH_S (60 * 10)
//#define DEFAULT_CACHE_TEST_LENGTH_S 10

/*
 * The defaut array size.  This is used when the user doesn't specify
 * one from the command line (for the tests that require this option)
 * and for when the CeGetCacheInfo function returns zero for the cache
 * size.
 *
 * Currently using 4 MB
 */
#define DEFAULT_TEST_ARRAY_SIZE (4 * 1024 * 1024)

/*
 * Put the initialization code for a cache test in one place.
 *
 * See the definition for more info.
 */
INT
runCacheTest (cacheTestType_t cacheTest, DWORD dwArraySize,
          DWORD dwRunTimeS, DWORD dwParam, DWORD dwAllocMemMethod);

/*****  Command line arg parsing  **************************************/

/*
 * Figure out how large the array needs to be from the argument passed
 * in the tux function table.
 *
 * See the definition for more info.
 */
BOOL
getArraySizeFromFT (DWORD dwParam, DWORD * dwSize);

/* return TRUE if the array length command line arg is set */
BOOL
cmdArgALisSet();

/* return TRUE if the run time command line arg is set */
BOOL
cmdArgRTisSet();

/* return the test run time in seconds */
BOOL
getTestRunTimeS (DWORD * pdoRt);

/* set the cache mode */
BOOL
setCacheMode (DWORD * vals, DWORD dwSizeBytes, DWORD dwParam);

/***** Guessing the cache size **********************************************/

/*
 * Guess the cache size, because we can't trust the value from
 * ceGetCacheInfo and IOCTL_HAL_GET_CACHE_INFO.  Currently we do use
 * this value (contradictory, yes), but in the future it will be
 * verified through tests.
 */

/* The possible cache types */
enum cacheType_t {L1DATA, L1INSTR, L2DATA, L2INSTR, TLB};

/*
 * Guess the cache size.
 *
 * See the definition for more info.
 */
BOOL
guessCacheSize(cacheType_t cacheType, DWORD * dwSize);

/****** Kenel mode **********************************************************/
__inline BOOL inKernelMode( void )
{
    return (int)inKernelMode < 0;
}

/*****************************************************************************
 *
 *                                Print Usage / Help
 *
 *****************************************************************************/

/*
 * print a usage message for this set of tests
 *
 * See header for information.
 */
TESTPROCAPI
cacheTestPrintUsage(
               UINT uMsg,
               TPPARAM tpParam,
               LPFUNCTION_TABLE_ENTRY lpFTE)
{
    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return (TPR_NOT_HANDLED);
    }

    Info (_T("These tests aim to test the caches on the system."));
    Info (_T("They allocate a contiguous block of memory and read and write"));
    Info (_T("values in differing patterns, trying to find bugs in the cache"));
    Info (_T("routines and memory hierarchy."));
    Info (_T(""));
    Info (_T("The test requires no arguments, but acceptable arguments are:"));
    Info (_T("  -runTime <minutes> Number of minutes that each test runs."));
    Info (_T("                     This setting applies to all test cases "));
    Info (_T("                     except Print Cache Info.  The default value"));
    Info (_T("                     is 10 minutes."));
    Info (_T("  -arrayLen <array>  Array length, in bytes.  This setting affects"));
    Info (_T("                     test cases for which you can specify the size"));
    Info (_T("                     of the array.  The default value is %u bytes"),
            DEFAULT_TEST_ARRAY_SIZE);
    Info (_T("  -CalibThresh <milliseconds>  Minimum time for a test.  Set "));
    Info (_T("                     by default to %u."),TEST_CALIBRATION_THRESHOLD_MS);



    return (TPR_PASS);
}



/*****************************************************************************
 *
 *                                Print Cache Info
 *
 *****************************************************************************/

/*
 * See header for information.
 */
TESTPROCAPI printCacheInfo(
                         UINT uMsg,
                         TPPARAM tpParam,
                         LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_PASS;

    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    if (cmdArgALisSet () == TRUE)
    {
        Info (_T("Ignoring command line argument for array length."));
    }

    if (cmdArgRTisSet () == TRUE)
    {
        Info (_T("Ignoring command line argument for test run time."));
    }

    /* struct to store cache information in */
    CacheInfo cacheInfo = { 0 };

    BOOL rVal = CeGetCacheInfo (sizeof (cacheInfo), &cacheInfo);

    if (rVal == FALSE)
    {
        Error (_T("Got 0 as a return value from CeGetCacheInfo."));
        Error (_T("CeGetCacheInfo could not return information on the cache."));
        Error (_T("Most likely, IOCTL_HAL_GET_CACHE_INFO isn't implemented"));
        Error (_T("or is implemented incorrecty.  CeGetCacheInfo calls this"));
        Error (_T("ioctl."));
        returnVal = TPR_FAIL;
        goto cleanAndExit;
    }
    else if (rVal != TRUE)
    {
        Error (_T("CeGetCacheInfo is not returning the correct error value.  ")
         _T("It should return TRUE (%u) or FALSE (%u) but instead ")
         _T("returned %u."),
         TRUE, FALSE, rVal);
        returnVal = TPR_FAIL;
        goto cleanAndExit;
    }

    /* at this point cacheInfo has the cache info information */

    Info (_T("This information is reported from the OAL.  It is not verified "));
    Info (_T("by this test.  A pass for this test means only that the test "));
    Info (_T("was able to get the information, not that the information is "));
    Info (_T("correct."));
    Info (_T("These names are taken from the Windows CE documentation.  Please "));
    Info (_T("consult the CeGetCacheInfo documentation for more information. "));
    Info (_T(""));

    Info (_T("L1 Flags - dwL1Flags = 0x%x   %s %s %s"), cacheInfo.dwL1Flags,
    (cacheInfo.dwL1Flags & CF_UNIFIED) ? _T("CF_UNIFIED") : _T(""),
    (cacheInfo.dwL1Flags & CF_WRITETHROUGH) ? _T("CF_WRITETHROUGH") : _T(""),
    (cacheInfo.dwL1Flags & CF_COHERENT) ? _T("CF_COHERENT") : _T(""));
    Info (_T(""));

    Info (_T("L1 Instruction Cache Size - dwL1ICacheSize = 0x%x = %u"),
    cacheInfo.dwL1ICacheSize, cacheInfo.dwL1ICacheSize);
    Info (_T("L1 Instruction Cache Line Size - dwL1ICacheLineSize = 0x%x = %u"),
    cacheInfo.dwL1ICacheLineSize, cacheInfo.dwL1ICacheLineSize);
    Info (_T("L1 Instruction Cache Associativity - dwL1ICacheNumWays = %u"),
    cacheInfo.dwL1ICacheNumWays);
    Info (_T(""));

    Info (_T("L1 Data Cache Size - dwL1ICacheSize = 0x%x = %u"),
    cacheInfo.dwL1DCacheSize, cacheInfo.dwL1DCacheSize);
    Info (_T("L1 Data Cache Line Size - dwL1DCacheLineSize = 0x%x = %u"),
    cacheInfo.dwL1DCacheLineSize, cacheInfo.dwL1DCacheLineSize);
    Info (_T("L1 Data Cache Associativity - dwL1DCacheNumWays = %u"),
    cacheInfo.dwL1DCacheNumWays);

    Info (_T(""));

    Info (_T("L2 Flags - dwL2Flags = 0x%x   %s %s %s"), cacheInfo.dwL2Flags,
    (cacheInfo.dwL2Flags & CF_UNIFIED) ? _T("CF_UNIFIED") : _T(""),
    (cacheInfo.dwL2Flags & CF_WRITETHROUGH) ? _T("CF_WRITETHROUGH") : _T(""),
    (cacheInfo.dwL2Flags & CF_COHERENT) ? _T("CF_COHERENT") : _T(""));
    Info (_T(""));

    Info (_T("L2 Instruction Cache Size - dwL2ICacheSize = 0x%x = %u"),
    cacheInfo.dwL2ICacheSize, cacheInfo.dwL2ICacheSize);
    Info (_T("L2 Instruction Cache Line Size - dwL2ICacheLineSize = 0x%x = %u"),
    cacheInfo.dwL2ICacheLineSize, cacheInfo.dwL2ICacheLineSize);
    Info (_T("L2 Instruction Cache Associativity - dwL2ICacheNumWays = %u"),
    cacheInfo.dwL2ICacheNumWays);
    Info (_T(""));

    Info (_T("L2 Data Cache Size - dwL2ICacheSize = 0x%x = %u"),
    cacheInfo.dwL2DCacheSize, cacheInfo.dwL2DCacheSize);
    Info (_T("L2 Data Cache Line Size - dwL2DCacheLineSize = 0x%x = %u"),
    cacheInfo.dwL2DCacheLineSize, cacheInfo.dwL2DCacheLineSize);
    Info (_T("L2 Data Cache Associativity - dwL2DCacheNumWays = %u"),
    cacheInfo.dwL2DCacheNumWays);

    Info (_T(""));

#ifdef _SHX_
    Info (_T("Currrently don't support querying cache settings on default "));
    Info (_T("TLB entry."));
#else
    /* query setting on default tlb entry */
    DWORD entry;

   // if (!inKernelMode())
  //  {
  //      Info (_T(""));
  //      Info (_T("Warning!!!  AllocPhysMem will fail if not in kernel mode."));
  //      Info (_T("Are you sure you are running the test correctly?"));
  //      Info (_T("Consider using ktux)"));
  //      Info (_T(""));
  //  }

    if (queryDefaultCacheMode (&entry) != TRUE)
    {
        Error (_T("Couldn't query cache for default TLB entry."));
        returnVal = TPR_FAIL;
        goto cleanAndExit;
    }

    switch (entry)
    {
#ifdef _MIPS_
    case CT_WRITE_THROUGH:
    case CT_WRITE_BACK:
        Info (_T("MIPS doesn't support changing cache symantics in the TLB."));
        Info (_T("Cache is enabled for pages with default allocations."));
        break;
#else
    case CT_WRITE_THROUGH:
        Info (_T("Default cache behavior for TLB entries is write-through."));
        break;
    case CT_WRITE_BACK:
        Info (_T("Default cache behavior for TLB entries is write-back."));
        break;
#endif /* _MIPS_ */
    case CT_DISABLED:
        Info (_T("Default cache behavior for TLB entries is disabled."));
        break;
    default:
        IntErr (_T("queryDefaultCacheMode didn't return a valid value: %u"),
          entry);
        returnVal = TPR_FAIL;
        goto cleanAndExit;
        break;
    }
#endif /* _SHX_ */

cleanAndExit:

    return (returnVal);
}



/*****************************************************************************
 *
 *                                Cache Test TESTPROCs
 *
 *****************************************************************************/


/*
 * See header for information.
 */
TESTPROCAPI
entryCacheTestWriteBoundariesReadEverything(
                        UINT uMsg,
                        TPPARAM tpParam,
                        LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_PASS;

    /* only support executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        returnVal =  TPR_NOT_HANDLED;
        goto cleanAndReturn;
    }

    Info (_T("This is the write-boundaries-read-everything cache test."));
    Info (_T("Please see the documentation for more information."));

    /* size of the array */
    DWORD dwArraySizeBytes;

    if (getArraySizeFromFT (lpFTE->dwUserData, &dwArraySizeBytes) == FALSE)
    {
        Error (_T("Could not determine how large the test array needs to be."));
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }

    /* This test requires at least 8 DWORDs in the array */
    if (dwArraySizeBytes < (8 * sizeof (DWORD)))
    {
        Error (_T("Array size of %u bytes is too small for this test. ")
         _T("It must be >= %u."),
         dwArraySizeBytes, 8 * sizeof (DWORD));
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }

    if ((dwArraySizeBytes % sizeof (DWORD)) != 0)
    {
        Error (_T("The array size in bytes, %u, is not a multiple of %u."),
            dwArraySizeBytes, sizeof (DWORD));
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }
    else
    {
        Info (_T("Using an array size of %u bytes, which is %u DWORDs."),
        dwArraySizeBytes, dwArraySizeBytes / sizeof (DWORD));
    }

    DWORD dwRunTimeS = 0;
    DWORD dwAllocMemMethod = (lpFTE->dwUserData & CT_ALLOC_PHYS_MEM) ? 
            CACHE_TEST_USE_ALLOC_PHYS_MEM : CACHE_TEST_USE_ALLOC_VIRTUAL;

    /* get test run time */
    if (getTestRunTimeS (&dwRunTimeS) == FALSE)
    {
        Error (_T("Could not get the test run time."));
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }

    returnVal = runCacheTest (CACHE_TEST_WRITE_BOUNDARIES_READ_EVERYTHING,
                dwArraySizeBytes / sizeof (DWORD),
                dwRunTimeS, lpFTE->dwUserData,dwAllocMemMethod);

cleanAndReturn:

    return (returnVal);

}

/*
 * See header for information.
 */
TESTPROCAPI
entryCacheTestWriteEverythingReadEverything(
                        UINT uMsg,
                        TPPARAM tpParam,
                        LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_PASS;

    /* only support executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        returnVal =  TPR_NOT_HANDLED;
        goto cleanAndReturn;
    }

    Info (_T("This cache test sequentially writes values and then verifies them."));
    Info (_T("Please see the documentation for more information."));

    /* size of the array */
    DWORD dwArraySizeBytes;

    if (getArraySizeFromFT (lpFTE->dwUserData, &dwArraySizeBytes) == FALSE)
    {
        Error (_T("Could not determine how large the test array needs to be."));
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }

    /* The test array must be at least a dword */
    if (dwArraySizeBytes < sizeof (DWORD))
    {
        Error (_T("Array size must be at least one DWORD (%u bytes)"),
         sizeof (DWORD));
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }
    
    if ((dwArraySizeBytes % sizeof (DWORD)) != 0)
    {
        /* truncate down */

        Error (_T("The array size in bytes, %u, is not a multiple of %u.  "),
            dwArraySizeBytes, sizeof (DWORD));
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }
    else
    {
        Info (_T("Using an array size of %u bytes, which is %u DWORDs."),
        dwArraySizeBytes, dwArraySizeBytes / sizeof (DWORD));
    }

    DWORD dwAllocMemMethod = (lpFTE->dwUserData & CT_ALLOC_PHYS_MEM) ? 
            CACHE_TEST_USE_ALLOC_PHYS_MEM : CACHE_TEST_USE_ALLOC_VIRTUAL;
    DWORD dwRunTimeS = 0;

    /* get test run time */
    if (getTestRunTimeS (&dwRunTimeS) == FALSE)
    {
        Error (_T("Could not get the test run time."));
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }

    returnVal = runCacheTest (CACHE_TEST_WRITE_EVERYTHING_READ_EVERYTHING,
                dwArraySizeBytes / sizeof (DWORD),
                dwRunTimeS, lpFTE->dwUserData,dwAllocMemMethod);

cleanAndReturn:

    return (returnVal);

}

/*
 * See header for information.
 */
TESTPROCAPI
entryCacheTestVaryPowersOfTwo(
                  UINT uMsg,
                  TPPARAM tpParam,
                  LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_PASS;

    /* only support executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        returnVal =  TPR_NOT_HANDLED;
        goto cleanAndReturn;
    }

    Info (_T("This cache test works boundaries near powers of two."));
    Info (_T("Please see the documentation for more information."));

    /* size of the array */
    DWORD dwArraySizeBytes = 0;

    if (getArraySizeFromFT (lpFTE->dwUserData, &dwArraySizeBytes) == FALSE)
    {
        Error (_T("Could not determine how large the test array needs to be."));
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }

    /* This test requires at least 8 DWORDs in the array */
    if (dwArraySizeBytes < (8 * sizeof (DWORD)))
    {
        Error (_T("Array size of %u bytes is too small for this test. ")
         _T("It must be >= %u."),
         dwArraySizeBytes, 8 * sizeof (DWORD));
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }

    DWORD dwAllocMemMethod = (lpFTE->dwUserData & CT_ALLOC_PHYS_MEM) ? 
            CACHE_TEST_USE_ALLOC_PHYS_MEM : CACHE_TEST_USE_ALLOC_VIRTUAL;
    
    if (isPowerOfTwo (dwArraySizeBytes / sizeof (DWORD)) == FALSE)
    {
        Error (_T("The array size in bytes, %u, is not a power of 2.  "),
         dwArraySizeBytes);
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }

    Info (_T("Using an array size of %u bytes, which is %u DWORDs."),
    dwArraySizeBytes, dwArraySizeBytes / sizeof (DWORD));

    DWORD dwRunTimeS = 0;

    /* get test run time */
    if (getTestRunTimeS (&dwRunTimeS) == FALSE)
    {
        Error (_T("Could not get the test run time."));
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }

    returnVal = runCacheTest (CACHE_TEST_WRITE_EVERYTHING_READ_EVERYTHING,
                dwArraySizeBytes / sizeof (DWORD),
                dwRunTimeS, lpFTE->dwUserData,dwAllocMemMethod);

 cleanAndReturn:

    return (returnVal);

}


/*
 * See header for information.
 */
TESTPROCAPI
entryCacheTestWriteEverythingReadEverythingMixedUp(
                           UINT uMsg,
                           TPPARAM tpParam,
                           LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_PASS;

    /* only support executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        returnVal =  TPR_NOT_HANDLED;
        goto cleanAndReturn;
    }

    Info (_T("This cache test sequentially writes values and then verifies them."));
    Info (_T("Please see the documentation for more information."));

    /* size of the array */
    DWORD dwArraySizeBytes = 0;
    DWORD dwArraySizeDwords = 0;

    if (getArraySizeFromFT (lpFTE->dwUserData, &dwArraySizeBytes) == FALSE)
    {
        Error (_T("Could not determine how large the test array needs to be."));
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }

    /* This test requires at least 8 DWORDs in the array */
    if (dwArraySizeBytes < sizeof (DWORD))
    {
        Error (_T("Array size must be at least one DWORD (%u bytes)"),
         sizeof (DWORD));
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }

    DWORD dwAllocMemMethod = (lpFTE->dwUserData & CT_ALLOC_PHYS_MEM) ? 
            CACHE_TEST_USE_ALLOC_PHYS_MEM : CACHE_TEST_USE_ALLOC_VIRTUAL;

    dwArraySizeDwords = dwArraySizeBytes / sizeof (DWORD);

    if ((dwArraySizeDwords == 0) ||
        (dwArraySizeBytes % sizeof (DWORD) != 0) ||
        (isPowerOfTwo (dwArraySizeDwords) == FALSE))
    {
        /* wrong array size */
        Error (_T("The array size in bytes, %u, is incorrect.  It must be a power ")
         _T("of two >= 8."), dwArraySizeBytes);
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }
    else
    {
        Info (_T("Using an array size of %u bytes, which is %u DWORDs."),
        dwArraySizeBytes, dwArraySizeDwords);
    }

    DWORD dwRunTimeS = 0;

    /* get test run time */
    if (getTestRunTimeS (&dwRunTimeS) == FALSE)
    {
        Error (_T("Could not get the test run time."));
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }

    returnVal = runCacheTest (CACHE_TEST_WRITE_EVERYTHING_READ_EVERYTHING_MIXEDUP,
                dwArraySizeDwords, dwRunTimeS, lpFTE->dwUserData,dwAllocMemMethod);

cleanAndReturn:

    return (returnVal);

}

BOOL
handleCmdLineCalibThresh(DWORD * pdwCalibThresh)
{
    BOOL returnVal = FALSE;

    if (!pdwCalibThresh)
    {
        return (FALSE);
    }

    *pdwCalibThresh = TEST_CALIBRATION_THRESHOLD_MS;

    if (g_szDllCmdLine != NULL)
    {
        cTokenControl tokens;

        /* break up command line into tokens */
        if (!tokenize ((TCHAR *)g_szDllCmdLine, &tokens))
        {
            Error (_T("Couldn't parse command line."));
            goto cleanAndReturn;
        }

        if (getOptionDWord (&tokens, ARG_STR_CALIB_THRESH, pdwCalibThresh))
        {
            Info (_T("User specified beginning Calibration Threshold, which is %u."), *pdwCalibThresh);
        }
        else
        {
            Info (_T("Using default beginning Calibration Threshold, which is %u."), *pdwCalibThresh);
        }
    }
    else
    {
        Info (_T("Using default beginning Calibration Threshold, which is %u."), *pdwCalibThresh);
    }
    returnVal = TRUE;
cleanAndReturn:

    return (returnVal);
}

/*****************************************************************************
 *
 *                      Test Initialization / Run the Test
 *
 *****************************************************************************/

/*
 * Run the test.  This function serves as a common entry point for the
 * testprocs.  It does some initialization and then calls the actual
 * test code.
 *
 * Before a test can be run, memory must be allocated, and the test
 * must determine how long it needs to be run to satisfy the time
 * requirement given by the user.  This is all common code, so it is
 * put in this routine.
 *
 * Each test function takes a different set of arguments, though, so
 * they are special cased at two points in this function.  The first
 * is for initialization and the second is when the test is run.
 *
 * cacheTest is the cache test type (see the enum above).  dwArraySize
 * is the arraySize and must be > 0 (But each test has its own
 * constraints that must be observed.  These are checked in the
 * functions that call this function.)
 *
 * This function return tux return values.
 */
INT
runCacheTest (cacheTestType_t cacheTest, DWORD dwArraySize,
          DWORD dwCacheTestLengthS, DWORD dwTestParam, DWORD dwAllocMemMethod)
{
    DWORD dwCalibThresh = TEST_CALIBRATION_THRESHOLD_MS;

    if (dwArraySize == 0)
    {
        IntErr (_T("runCacheTest: dwArraySize == 0"));
        return TPR_FAIL;
    }

    if (dwCacheTestLengthS < 0)
    {
        IntErr (_T("(dwCacheTestLengthS < 0)"));
        return (TPR_FAIL);
    }

    if(!handleCmdLineCalibThresh(&dwCalibThresh))
    {
        Error (_T("Could not handle command line "));
        return (TPR_FAIL);
    }

    /* main return value (TESTPROC format) */
    INT returnVal = TPR_PASS;

    /* return value for cache test helper functions (boolean format) */
    BOOL bRetVal = FALSE;

    /* make nice times when printing */
    double doRealisticTime = 0;
    _TCHAR * pszRealisticUnits = NULL;

    /*
   * they array that we do testing over.  Must be page aligned in mem.
   */
    DWORD * vals = NULL;

    /* check for overflow on array size */
    DWORD dwArraySizeBytes = dwArraySize * sizeof (DWORD);

    if (dwArraySizeBytes < MAX(dwArraySize, sizeof (DWORD)))
    {
        Error (_T("Array size is too large.  Memory allocation will overflow ")
         _T("DWORD"));
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }

    const TCHAR * szMemoryAllocationType = NULL;

    if(dwAllocMemMethod == CACHE_TEST_USE_ALLOC_VIRTUAL)
    {
        /*
       * Force the os to get us this memory.  Memory will be on a page boundary
       * so the boundary conditions will actually hit cache boundaries.
       */
        Info (_T("Using VirtualAlloc"));
        vals = (DWORD *) VirtualAlloc (NULL, dwArraySizeBytes,
                     MEM_COMMIT, PAGE_READWRITE);

        szMemoryAllocationType = _T("VirtualAlloc");
    } 
    else if( dwAllocMemMethod == CACHE_TEST_USE_ALLOC_PHYS_MEM)
    {
        Info (_T("Using AllocPhysMem"));
        if (!inKernelMode())
        {
            Error (_T(""));
            Error (_T("Error!!!  AllocPhysMem must be tested in kernel mode."));
            Error (_T("Are you sure you are running the test correctly?"));
            Error (_T("Consider using -n option on the tux command line"));
            Error (_T(""));
            returnVal = TPR_FAIL;
            goto cleanAndReturn;
        }

        /* physical address of the allocation.  currently unused */
        DWORD dwPhysicalAddress = 0;

        /*
        /* Force the os to get a contiguious chunk of memory. */
        vals = (DWORD *) AllocPhysMem (dwArraySizeBytes, PAGE_READWRITE, 0, 0,
                     &dwPhysicalAddress);

        szMemoryAllocationType = _T("AllocPhysMem");
    }
    else
    {
        Error (_T("No memory allocation scheme specified"));
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }

    if (vals == NULL)
    {
        Error (_T("The test tried to allocate %u bytes of memory using ")
         _T("AllocPhysMem.  The call failed.  GetLastError returned %u."),
         dwArraySizeBytes, GetLastError ());
        Error (_T("This probably means that a physically contiguous block of"));
        Error (_T("memory was not available.  Reboot the machine and try the"));
        Error (_T("test again.  You should also be running only on TinyKernel."));

        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }

    Info (_T("Allocated %u bytes of memory using %s."),
    dwArraySizeBytes, szMemoryAllocationType);

#ifdef _SHX_
    Info (_T("Currently don't support setting cache into specific modes for SHX."));
    Info (_T("The cache is running in the default mode."));
#else
    /* this will print out what cache is set to */
    if (setCacheMode (vals, dwArraySizeBytes, dwTestParam) == FALSE)
    {
        Error (_T("Could not set cache mode for test."));
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }
#endif

    /**** Figure out how long one test iteration takes ************************/

    Info (_T("Calibrating..."));

    DWORD dwTestCalibrationRuns = 1;

    /*
   * these are ulonglong because we are using functions from the timing code,
   * which take ulonglong parameters
   */
    ULONGLONG ullDiff = 0;
    ULONGLONG ullTimeBegin = 0, ullTimeEnd = 0;

    for(;;)
    {
        ullTimeBegin = (ULONGLONG) GetTickCount ();

        /* run the correct test */
        switch (cacheTest)
        {
        case CACHE_TEST_WRITE_BOUNDARIES_READ_EVERYTHING:
            bRetVal = cacheTestWriteBoundariesReadEverything(vals, dwArraySize,
                               dwTestCalibrationRuns,
                               CACHE_TEST_STEP_SIZE);
            break;
        case CACHE_TEST_WRITE_EVERYTHING_READ_EVERYTHING:
            bRetVal = cacheTestWriteEverythingReadEverything(vals, dwArraySize,
                               dwTestCalibrationRuns);
            break;
        case CACHE_TEST_VARY_POWERS_OF_TWO:
            bRetVal = cacheTestVaryPowersOfTwo(vals, dwArraySize,
                         dwTestCalibrationRuns);
            break;
        case CACHE_TEST_WRITE_EVERYTHING_READ_EVERYTHING_MIXEDUP:
            bRetVal = cacheTestWriteEverythingReadEverythingMixedUp(vals,
                                  dwArraySize,
                                  dwTestCalibrationRuns);
            break;
        default:
            /* an internal error, so bail out */
            Error (_T("No test specified for function runCacheTest."));
            returnVal = TPR_FAIL;
            goto cleanAndReturn;
            break;
        }

        /* check for error */
        if (bRetVal == FALSE)
        {
            Error (_T("Test failed during calibration."));
            Error (_T("Calibration runs the given cache test for a short length"));
            Error (_T("of time to determine how long it takes.  Failure commonly"));
            Error (_T("means that the cache test failed.  Most likely, something"));
            Error (_T("is wrong with the cache routines or memory subsystem "));
            Error (_T("(either hardware or software) on the machine"));

            returnVal = TPR_FAIL;
            goto cleanAndReturn;
        }

        ullTimeEnd = (ULONGLONG) GetTickCount ();

        /* subtract, accounting for overflow */
        if (AminusB (ullDiff, ullTimeEnd, ullTimeBegin, DWORD_MAX) == FALSE)            {
            /*
             * This should never happen.  If something goes wrong, then there is
             * a bug in the code.
             */
            IntErr (_T("AminusB failed in runCacheTest"));

            returnVal = TPR_FAIL;
            goto cleanAndReturn;
        }

        if (ullDiff < dwCalibThresh)
        {
            /* if within threshold double number of runs and try again */
            DWORD prev = dwTestCalibrationRuns;

            dwTestCalibrationRuns *= 2;

            if (prev >= dwTestCalibrationRuns)
            {
                /*
                 * we overflowed.  We could try to recover but it is much easier
                 * to bail at this point.  If this happens the user either has
                 * lightening speed caches or something is wrong with either
                 * the timers or something else.
                 *
                 * This should be a very rare error.
                 */
                Error (_T("Could not calibrate.  We overflowed the variable "));
                Error (_T("storing the number of calibration runs.  Either the "));
                Error (_T("cache routines are running very fast or the "));
                Error (_T("calibration threshold is set too high."));
                Error (_T("Reduce the calibration threshold in the code using "));
                Error (_T("-CalibThresh command line option."));
                Error (_T("try again."));

                returnVal = TPR_FAIL;
                goto cleanAndReturn;
            }
        }
        else
        {
            break;
        }
    } /* fpr(;;) */

    /*
     * we now know how long one test will take.  We calculate how many
     * tests we need to do for the given test length.
     */

    /*
     * convert test length into ms.  Cast ullDiff to DWORD.  This won't cause
     * us to loose data because there will be nothing in the upper 32 bits.
     * AminusB prevents this.
     */
    DWORD dwIterations =
        (dwCacheTestLengthS * 1000) / (DWORD) ullDiff;

    DWORD dwIterationsPrev = dwIterations;

    /* factor in that it took dwTestCalibrationRun loops for each iteration */
    dwIterations *= dwTestCalibrationRuns;

    if (dwIterationsPrev > dwIterations)
    {
        Error (_T("Overflow in dwIterations"));
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }

    if (dwIterations == 0)
    {
        Error (_T("Either calibration is incorrect (maybe the timers haven't been"));
        Error (_T("tested?) or the test length constants for the cache tests "));
        Error (_T("are not set correctly.  Exiting..."));

        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }

    Info (_T("We are going to do %u iterations for this cache test."),
    dwIterations);
    realisticTime ((double) dwCacheTestLengthS,
         doRealisticTime, &pszRealisticUnits);
    Info (_T("It should take about %g %s."),
    doRealisticTime, pszRealisticUnits);
    Info (_T("Step size is %u."), CACHE_TEST_STEP_SIZE);

    /* run the correct test */
    switch (cacheTest)
    {
    case CACHE_TEST_WRITE_BOUNDARIES_READ_EVERYTHING:
        bRetVal = cacheTestWriteBoundariesReadEverything(vals, dwArraySize,
                               dwIterations,
                               CACHE_TEST_STEP_SIZE);
        break;
    case CACHE_TEST_WRITE_EVERYTHING_READ_EVERYTHING:
        bRetVal = cacheTestWriteEverythingReadEverything(vals, dwArraySize,
                               dwIterations);
        break;
    case CACHE_TEST_VARY_POWERS_OF_TWO:
        bRetVal = cacheTestVaryPowersOfTwo(vals, dwArraySize, dwIterations);
        break;
    case CACHE_TEST_WRITE_EVERYTHING_READ_EVERYTHING_MIXEDUP:
        bRetVal = cacheTestWriteEverythingReadEverythingMixedUp(vals,
                                  dwArraySize,
                                  dwIterations);
        break;
    default:
        /* an internal error, so bail out */
        Error (_T("No test specified for function runCacheTest."));
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
        break;
    }

    if (bRetVal == FALSE)
    {
        /* we print out more info in the test function */
        Error (_T("Cache test failed"));
        returnVal = TPR_FAIL;
        goto cleanAndReturn;
    }
    else
    {
        Info (_T("Cache test succeeded"));
    }

cleanAndReturn:

    if (vals != NULL)
    {
        /* clean up if we successfully allocated the memory */

        if( dwAllocMemMethod == CACHE_TEST_USE_ALLOC_PHYS_MEM)
        {
            if (FreePhysMem (vals) != TRUE)
            {
              Error (_T("Could not free memory.  GetLastError returned %u."),
                 GetLastError);
              returnVal = TPR_FAIL;
            }
        } 
        else if(dwAllocMemMethod == CACHE_TEST_USE_ALLOC_VIRTUAL)
        {
            if (VirtualFree (vals, 0, MEM_RELEASE) == 0)
            {
              Error (_T("Could not free memory.  GetLastError returned %u."),
                 GetLastError);
              returnVal = TPR_FAIL;
            }
        }
        else
        {
            Error (_T("No memory allocation scheme specified"));
            returnVal = TPR_FAIL;
        }
    }

    return (returnVal);
}

/*****************************************************************************
 *
 *                            Argument Processing
 *
 *****************************************************************************/

/*
 * Figure out what array size to use for the test from given in the
 * function table.
 *
 * See the notes in the header for information on how dwParam is laid
 * out.
 *
 * Returns the size of the array size in the bytes.
 */
BOOL
getArraySizeFromFT (DWORD dwParam, DWORD * pdwCacheSize)
{
    if (pdwCacheSize == NULL)
    {
        IntErr (_T("getArraySizeFromFT: pdwCacheSize == NULL"));
        return FALSE;
    }

    BOOL returnVal = TRUE;

    CClParse cmdLine(g_szDllCmdLine);

    /* used for overflow checking in some calculations */
    DWORD prev = BAD_VAL;

    /* grab only the cache size bits */
    dwParam &= CT_ARRAY_SIZE_MASK;

    switch (dwParam)
    {
    case CT_CACHE_SIZE:
        if (cmdArgALisSet () == TRUE)
        {
            Info (_T("Ignoring command line argument for array length."));
            Info (_T("Cache size data is taken from CeGetCacheInfo."));
        }

        /* get the cache size from the system */
        if (guessCacheSize (L1DATA, pdwCacheSize) == FALSE)
        {
            Error (_T("Could not get the cache size."));
            returnVal = FALSE;
            goto cleanAndExit;
        }

        Info (_T("The L1 data cache size is %u bytes."), *pdwCacheSize);

        break;
    case CT_TWICE_CACHE_SIZE:
        if (cmdArgALisSet () == TRUE)
        {
            Info (_T("Ignoring command line argument for array length."));
            Info (_T("Cache size data is taken from CeGetCacheInfo."));
        }

        /* get the cache size from the system */
        if (guessCacheSize (L1DATA, pdwCacheSize) == FALSE)
        {
            Error (_T("Could not get the cache size."));
            returnVal = FALSE;
            goto cleanAndExit;
        }

        Info (_T("The L1 data cache size is %u bytes."), *pdwCacheSize);

        prev = *pdwCacheSize;

        *pdwCacheSize *= 2;

        if (*pdwCacheSize <= prev)
        {
            Error (_T("Overflow when doubling cache size"));
            returnVal = FALSE;
            goto cleanAndExit;
        }
        break;
    case CT_USER_DEFINED:
        /* in this case parse the command line to find the size */
        if (cmdLine.GetOptVal (_T("arrayLen"), pdwCacheSize) == FALSE)
        {
            Info (_T("Could not get the cache test array length."));
            Info (_T("Looking for an option in the form ")
            _T("arrayLen <number>."));

            *pdwCacheSize = DEFAULT_TEST_ARRAY_SIZE;

            Info (_T("Using default value of %u"), *pdwCacheSize);
        }
        break;
    default:
        Error (_T("Bad paramter."));
        returnVal = FALSE;
        goto cleanAndExit;
    }

cleanAndExit:
    return (returnVal);
}



/*
 * Guess the cache size.  If the value from the ceGetCacheInfo function
 * doesn't make sense, then use the default value.
 *
 * Currently only L1 data cache queries are supported.  If the L1 data
 * cache has a size of zero, we use the default cache size.  It will be
 * very rare that we won't have a L1 data cache, and even if we didn't
 * and wanted to test memory we would still want a default sized array.
 *
 * return TRUE on success and FALSE otherwise.  Print out messages in
 * both cases denoting what we are doing.
 */
BOOL
guessCacheSize(cacheType_t cacheType, DWORD * pdwSize)
{
    if (pdwSize == NULL)
    {
        IntErr (_T("guessCacheSize: pdwSize == NULL"));
        return (FALSE);
    }

    BOOL returnVal = TRUE;

    /* struct to store cache information in */
    CacheInfo cacheInfo;

    if (cacheType != L1DATA)
    {
        Error (_T("Error:  Currently only support guessing the cache size")
         _T("for the L1 data cache"));
        returnVal = FALSE;
        goto cleanAndReturn;
    }

    returnVal = CeGetCacheInfo (sizeof (cacheInfo), &cacheInfo);

    if (returnVal == FALSE)
    {
        Error (_T("Got 0 as a return value from CeGetCacheInfo."));
        Error (_T("CeGetCacheInfo could not return information on the cache."));
        Error (_T("Most likely, IOCTL_HAL_GET_CACHE_INFO isn't implemented"));
        Error (_T("or is implemented incorrecty.  CeGetCacheInfo calls this"));
        Error (_T("ioctl."));
        returnVal = FALSE;
        goto cleanAndReturn;
    }
    else if (returnVal != TRUE)
    {
        Error (_T("CeGetCacheInfo is not returning the correct error value.  ")
         _T("It should return TRUE (%u) or FALSE (%u) but instead ")
         _T("returned %u."),
         TRUE, FALSE, returnVal);
        returnVal = FALSE;
        goto cleanAndReturn;
    }

    /* at this point cacheInfo has the cache info information */

    Info (_T("The cache size is reported from the OAL.  It currently cannot"));
    Info (_T("be verified.  If it isn't correct then the test might generate"));
    Info (_T("false negatives.  An incorrect cache size should not generate "));
    Info (_T("false positives."));

    *pdwSize = cacheInfo.dwL1DCacheSize;

    if (*pdwSize == 0)
    {
        /*
       * most likely this means that it isn't set.  We currently don't set
       * this information on the stock x86, since the x86 varies so much and
       * it can't be determined on the fly.
       *
       * For now, we just allocate a standard size in this case and hope for
       * the best.  In the future we will have better checks and will probably
       * use some timing measurements to figure out how large the cache really
       * is.
       */
        *pdwSize = DEFAULT_TEST_ARRAY_SIZE;

        Info (_T("The L1 data cache size is zero.  This probably isn't correct."));
        Info (_T("For example, this value often isn't set on the x86 platform"));
        Info (_T("since it varies so much from platform to platform."));
        Info (_T("If you modified the OAL to return a value other than zero "));
        Info (_T("then something is wrong."));
        Info (_T("This test requires a value greater than zero, so we are going"));
        Info (_T("to use a default value of %u."), *pdwSize);
    }

cleanAndReturn:

    return (returnVal);
}


/*
 * Get the test run time.
 *
 * If it wasn't passed in as an argument use the default value.
 *
 * doRt has the test run time in seconds.
 *
 * return TRUE on success and FALSE otherwise.
 */
BOOL
getTestRunTimeS (DWORD * pdwRt)
{
    BOOL returnVal = FALSE;

    CClParse cmdLine(g_szDllCmdLine);

    /* returned run time from the argument processing function */
    DWORD dwArgRt;

    if (cmdLine.GetOptVal (_T("runTime"), &dwArgRt) == TRUE)
    {
        /* convert from minutes into seconds */
        *pdwRt = dwArgRt * 60;

        if (*pdwRt < MAX (dwArgRt, 60))
        {
            Error (_T("Overflow converting run time to seconds."));
            goto cleanAndReturn;
        }
    }
    else
    {
        /* no option, use default */
        *pdwRt = DEFAULT_CACHE_TEST_LENGTH_S;
    }

    /* only succeeded if we got to here */
    returnVal = TRUE;

cleanAndReturn:
    return (returnVal);
}


/*
 * return TRUE if the array size option is on the command line and
 * false if it isn't.
 *
 * This looks for correct options (in this case flag and associated
 * parameter.  If the command line arg parsing function fails then
 * this function returns false, because this option won't be able to
 * be parsed by this program.
 */
BOOL
cmdArgALisSet()
{
    CClParse cmdLine(g_szDllCmdLine);

    return (cmdLine.GetOpt (_T("arrayLen")));
}

/*
 * return TRUE if the test run time option is on the command line
 * and false if it isn't.
 *
 * This looks for correct options (in this case flag and associated
 * parameter.  If the command line arg parsing function fails then
 * this function returns false, because this option won't be able to
 * be parsed by this program.
 */
BOOL
cmdArgRTisSet()
{
    CClParse cmdLine(g_szDllCmdLine);

    return (cmdLine.GetOpt (_T("runTime")));
}

