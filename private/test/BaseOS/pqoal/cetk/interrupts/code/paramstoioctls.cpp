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


#include <windows.h>
#include <tchar.h>
#include <tux.h>

/* common utils for oal tests */
#include "commonUtils.h"

/* test procs, etc */
#include "tuxInterrupts.h"

/* collateIRQs function lives here */
#include "prettyPrinting.h"

/* provides command line routines for the tests */
#include "interruptCmdLine.h"

/*
 * These test cases exercise the paramters to the IOCTLS.  As
 * opposed to the other files, which contain test cases that are
 * supposed to confirm that the IOCTLs are allocating and releasing
 * things correctly, these tests confirm that the IOCTL handles
 * odd parameter combinations correctly.
 *
 * The bulk of these tests expect the IOCTL to fail.  The IOCTLs
 * are quite strict in what they accept as input, except in the
 * output parameter case.  In this case there are tests that both
 * force failure and expect success.
 */

/* tells us whether we are in kernel mode */
__inline BOOL inKernelMode( void )
{
    return (int)inKernelMode < 0;
}

/***** for sanity checks *****/

/*
 * defined in requestReleaseSysintr.cpp.  Not in a header because
 * require use of functions specific to requestReleaseSysintr.cpp
 */

extern BOOL
getNumSysIntrsFree (DWORD dwIRQBegin, DWORD dwIRQEnd,
                    DWORD * pdwNumSysIntrsFree);

extern BOOL
checkNumSysIntrsFree (DWORD dwIRQBegin, DWORD dwIRQEnd,
                      DWORD dwNumSysIntrsFree);

/* maximum in bound buffer length tested for IOCTL_HAL_REQUEST/RELEASE_SYSINTR */
#define MAX_INBOUND_BUF_LENGTH_DW 8

/**** general test driver functions **************************************/

/*
 * The test has a driver that takes a structure denoting an IOCTL call.  This
 * driver makes the call and checks return values.
 *
 * The structure below provides that data needed to make the ioctl call.
 */

typedef struct
{
    DWORD dwIoctlCode;      // IOCTL code
    TCHAR * szDescription;  // test case description
    VOID * dwInputBuf;      // pointer to buffer, or null if param is null
    DWORD dwInputBufSize;       // input buf size (as told to ioctl)
    VOID * dwOutputBuf;     // pointer to buffer, or null if param is null
    DWORD dwOutputBufSize;  // output buf size (as told to ioctl)
    DWORD * dwLpBytesReturned;  // currently not used
} sAnIoctlCall;

/* function to run one test case, denoted by an instance of the struct above */
BOOL
runRow (const sAnIoctlCall *const sRow,
    BOOL bShouldFreeSysintr = FALSE,
    BOOL bTestingLpBytesReturned = FALSE,
    DWORD dwExpectedLpBytesReturned = 0);

/* print out data about a call that failed */
BOOL
dumpRowOnError (const sAnIoctlCall *const sRow, void * pInBuf, void * pOutBuf);


/**** IOCTL_HAL_REQUEST_SYSINTR *******************************************/

/* test procs and test functions */
BOOL
runBadInputNewModel_IncompleteArgSets();

BOOL
runBadInputNewModel_WrongInSizes ();

BOOL
runBadInputNewModel_WrongInSizes (const DWORD dwFlagForTest,
                  const TCHAR *const szFlagName);

BOOL
runBadInSizeOldModel ();

BOOL
runBadFlags ();
BOOL
runBadFlags (const sAnIoctlCall *const sRow);

BOOL
runOutSizeOldModel (BOOL bExpectedToPass, BOOL bTestLpBytesReturned);
BOOL
runOutSizeNewModel (BOOL bExpectedToPass, BOOL bTestLpBytesReturned);
BOOL
runOutSize (const sAnIoctlCall *const sRow, BOOL bExpectedToPass,
        BOOL bTestLpBytesReturned);

BOOL
fillInBoundArrayNewModel (__out_ecount (dwNumOfDwords) DWORD *const pdwBuf,
              DWORD dwNumOfDwords);
BOOL
fillInBoundArrayOldModel (__out_ecount (dwNumOfDwords) DWORD *const pdwBuf,
              DWORD dwNumOfDwords);

/****** IOCTL_HAL_REQUEST_IRQ **********************************************/

BOOL
runBadInSize_IOCTL_HAL_REQUEST_IRQ ();

BOOL
runBadOutSize_IOCTL_HAL_REQUEST_IRQ ();

/****** IOCTL_HAL_RELEASE_SYSINTR ******************************************/

/*
 * no outsize is tested, since this value is not used.  The code should
 * never touch it (we currently don't verify this in these sets of tests).
 */
BOOL
runBadInSize_IOCTL_HAL_RELEASE_SYSINTR ();

/**************************** general driver function **********************/

/*
 * maps the defines to functions.  The defines are in the function table.  This
 * allows us to easily add and update with new tests/changes without having
 * to add functions to the main headers.
 */
TESTPROCAPI
runGeneralIoctlTest(
            UINT uMsg,
            TPPARAM tpParam,
            LPFUNCTION_TABLE_ENTRY lpFTE)
{
    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return (TPR_NOT_HANDLED);
    }

    BOOL rVal = FALSE;

    DWORD dwNumSysIntrsFree = 0;

    breakIfUserDesires();

    if (inKernelMode() &&
        !getNumSysIntrsFree (TEST_IRQ_BEGIN, TEST_IRQ_END, &dwNumSysIntrsFree))
    {
        goto cleanAndReturn;
    }

    switch ((tGeneralIoctlTestCase) lpFTE->dwUserData)
    {
        /* IOCTL_HAL_REQUEST_SYSINTR */
    case TESTCASE_BAD_IN_SIZE_NEW_MODEL:
        rVal = runBadInputNewModel_WrongInSizes ();
        break;
    case TESTCASE_BAD_INPUT_INCOMPLETE_ARGS_NEW_MODEL:
        rVal = runBadInputNewModel_IncompleteArgSets();
        break;
    case TESTCASE_BAD_IN_SIZE_OLD_MODEL:
        rVal = runBadInSizeOldModel ();
        break;
    case TESTCASE_BAD_FLAGS:
        rVal = runBadFlags ();
        break;
    case TESTCASE_BAD_OUT_SIZE_OLD_MODEL:
        /*
       * bad sizes should cause failure, hence FALSE
       * not testing lpBytesReturned, hence FALSE
       */
        rVal = runOutSizeOldModel (FALSE, FALSE);
        break;
    case TESTCASE_BAD_OUT_SIZE_NEW_MODEL:
        /* bad sizes should cause failure, hence FALSE */
        rVal = runOutSizeNewModel (FALSE, FALSE);
        break;
    case TESTCASE_GOOD_OUT_SIZE_OLD_MODEL:
        /* good sizes should cause success, hence TRUE */
        rVal = runOutSizeOldModel (TRUE, FALSE);
        break;
    case TESTCASE_GOOD_OUT_SIZE_NEW_MODEL:
        rVal = runOutSizeNewModel (TRUE, FALSE);
        break;

        /*
       * lpBytesReturned is linked to the output parameters, so use
       * these tests to confirm that it is working in the positive
       * case.
       */
    case TESTCASE_BYTES_RETURNED_OLD_MODEL:
        /* good sizes should cause success, hence TRUE */
        rVal = runOutSizeOldModel (TRUE, TRUE);
        break;
    case TESTCASE_BYTES_RETURNED_NEW_MODEL:
        rVal = runOutSizeNewModel (TRUE, TRUE);
        break;

        /* IOCTL_HAL_RELEASE_SYSINTR */
    case IOCTL_HAL_RELEASE_SYSINTR_TESTCASE_BAD_IN_SIZE:
        rVal = runBadInSize_IOCTL_HAL_RELEASE_SYSINTR ();
        break;

        /* IOCTL_HAL_REQUEST_IRQ */
    case IOCTL_HAL_REQUEST_IRQ_TESTCASE_BAD_IN_SIZE:
        rVal = runBadInSize_IOCTL_HAL_REQUEST_IRQ ();
        break;
    case IOCTL_HAL_REQUEST_IRQ_TESTCASE_BAD_OUT_SIZE:
        rVal = runBadOutSize_IOCTL_HAL_REQUEST_IRQ ();
        break;

    default:
        rVal = FALSE;
        Error (_T("Wrong test case id to runGeneralIoctlTest."));
        break;
    }

    Info (_T("")); // blank line

    if (inKernelMode() &&
        !checkNumSysIntrsFree (TEST_IRQ_BEGIN, TEST_IRQ_END, dwNumSysIntrsFree))
    {
        rVal = FALSE;
        goto cleanAndReturn;
    }

cleanAndReturn:

    if (rVal)
    {
        Info (_T("Test passed."));
        return (TPR_PASS);
    }
    else
    {
        Error (_T("Test failed!"));
        return (TPR_FAIL);
    }
}

/*
 * driver function.  sRow contains all of the info needed to make an
 * ioctl call.
 *
 * bShouldFreeSysintr.  If true, test will fail if sysintr can't be
 * allocated.  the allocated sysintr is freed before exit.
 *
 * bTestingLpBytesReturned.  If true, then IOCTL is called with
 * lpBytesReturned set.  Value must match dwExpectedLpBytesReturned.
 *
 * return true if the test worked and false if it failed.
 */
BOOL
runRow (const sAnIoctlCall *const sRow, BOOL bShouldFreeSysintr,
    BOOL bTestingLpBytesReturned, DWORD dwExpectedLpBytesReturned)
{
    if (!sRow)
    {
        return (FALSE);
    }

    BOOL returnVal = FALSE;

    Info (_T("description: %s"), sRow->szDescription);

    /* these are set depending on the table */
    VOID * pInBuf = NULL;
    VOID * pOutBuf = NULL;

    if (sRow->dwInputBuf)
    {
        /* alloc and copy it */

        pInBuf = guardedMalloc (sRow->dwInputBufSize);

        if (!pInBuf)
    {
      Error (_T("Couldn't allocate memory."));
      goto cleanAndReturn;
    }

        memcpy (pInBuf, sRow->dwInputBuf, sRow->dwInputBufSize);
    }

    if (sRow->dwOutputBuf)
    {
        /* alloc and copy it */

        pOutBuf = guardedMalloc (sRow->dwOutputBufSize);

        if (!pOutBuf)
    {
      Error (_T("Couldn't allocate memory."));
      goto cleanAndReturn;
    }

        memcpy (pOutBuf, sRow->dwOutputBuf, sRow->dwOutputBufSize);
    }

    /* at this point all values are set, ready for the call */

    BOOL rc;

    if (bTestingLpBytesReturned)
    {
        DWORD dwBytesReturned;

        rc = KernelIoControl (sRow->dwIoctlCode, pInBuf, sRow->dwInputBufSize,
                pOutBuf, sRow->dwOutputBufSize, &dwBytesReturned);

        if (dwBytesReturned != dwExpectedLpBytesReturned)
    {
      Error (_T("Bytes returned is not correct.  Expected %u and got %u"),
         dwExpectedLpBytesReturned, dwBytesReturned);
      dumpRowOnError (sRow, pInBuf, pOutBuf);
      goto cleanAndReturn;
    }
    }
    else
    {
        rc = KernelIoControl (sRow->dwIoctlCode, pInBuf, sRow->dwInputBufSize,
                pOutBuf, sRow->dwOutputBufSize, NULL);
    }

    if (rc && !bShouldFreeSysintr)
    {
        Error (_T("Expected false when calling IOCTL_HAL_REQUEST_SYSINTR ")
         _T("and got true."));

        dumpRowOnError (sRow, pInBuf, pOutBuf);

        goto cleanAndReturn;
    }
    else if (!rc && bShouldFreeSysintr)
    {
        Error (_T("Expected true when calling IOCTL_HAL_REQUEST_SYSINTR ")
         _T("and got false."));

        dumpRowOnError (sRow, pInBuf, pOutBuf);

        goto cleanAndReturn;
    }

    if (bShouldFreeSysintr)
    {
        /* release interrupt */

        /* make sure that user gave us enough memory */
        if (sRow->dwOutputBufSize < sizeof (DWORD))
    {
      IntErr (_T("Test Error: User wants test to succeed but didn't give")
          _T("enough outbound memory.  %u < %u"),
          sRow->dwOutputBufSize, sizeof (DWORD));
      goto cleanAndReturn;
    }

        /* get sysintr; make a copy so that we don't affect checks below */
        DWORD dwSysIntr = *(DWORD *) pOutBuf;

        if (!KernelIoControl (IOCTL_HAL_RELEASE_SYSINTR,
               &dwSysIntr, sizeof (DWORD), NULL, 0, NULL))
    {
      Error (_T("Couldn't free SYSINTR %u."), dwSysIntr);
      goto cleanAndReturn;
    }
    }

    /* compare bufs to original to make sure unchanged */
    if ((sRow->dwInputBuf) &&
        (memcmp (sRow->dwInputBuf, pInBuf, sRow->dwInputBufSize) != 0))
    {
        /* buffers don't match */
        Error (_T("IOCTL changed input buffer."));

        /* dump output */
        dumpRowOnError (sRow, pInBuf, pOutBuf);
        goto cleanAndReturn;
    }

    /* use guarded memory to check for overwriting edge of buffer */
    if ((sRow->dwInputBuf) && (!guardedCheck (pInBuf, sRow->dwInputBufSize)))
    {
        Error (_T("IOCTL wrote outside of array boundaries for input buffer."));

        goto cleanAndReturn;
    }

    if ((sRow->dwOutputBuf) && (!guardedCheck (pOutBuf, sRow->dwOutputBufSize)))
    {
        Error (_T("IOCTL wrote outside of array boundaries for output buffer."));

        goto cleanAndReturn;
    }

    returnVal = TRUE;

 cleanAndReturn:

    if ((pInBuf) && !guardedFree (pInBuf, sRow->dwInputBufSize))
    {
        Error (_T("Couldn't free input buffer."));
        returnVal = FALSE;
    }

    if ((pOutBuf) && !guardedFree (pOutBuf, sRow->dwOutputBufSize))
    {
        Error (_T("Couldn't free output buffer."));
        returnVal = FALSE;
    }

    return (returnVal);
}

/* print out data about a call that failed */
BOOL
dumpRowOnError (const sAnIoctlCall *const sRow, void * pInBuf, void * pOutBuf)
{

    BOOL returnVal = TRUE;

    Error (_T("The values passed into the IOCTL are:"));

    if (!sRow->dwInputBuf)
    {
        /* null */
        Error (_T("Input buffer is NULL."));
    }
    /* if not divisible by 4 */
    else if (sRow->dwInputBufSize & 0x3)
    {
        /* collateIRQs takes number of values, not bytes, so have to convert */
        Error (_T("Input buffer (truncated): %s"),
         collateIRQs ((DWORD *)pInBuf,
              sRow->dwInputBufSize / sizeof (DWORD)));
    }
    else
    {
        Error (_T("Input buffer: %s"),
         collateIRQs ((DWORD *)pInBuf,
              sRow->dwInputBufSize / sizeof (DWORD)));
    }

    Error (_T("Input size (bytes): %u"), sRow->dwInputBufSize);

    if (!sRow->dwOutputBuf)
    {
        /* null */
        Error (_T("Output buffer is NULL."));
    }
    else if (sRow->dwOutputBufSize & 0x3)
    {
        Error (_T("Output buffer (truncated): %s"),
         collateIRQs ((DWORD *)pOutBuf,
              sRow->dwOutputBufSize / sizeof (DWORD)));
    }
    else
    {
        Error (_T("Output buffer: %s"),
         collateIRQs ((DWORD *)pOutBuf,
              sRow->dwOutputBufSize / sizeof (DWORD)));
    }

    Error (_T("Output size (bytes): %u"), sRow->dwOutputBufSize);
    Error (_T(""));

    return (returnVal);
}

/******************* TEST CASES for IOCTL_HAL_REQUEST_SYSINTR **************/

/****** inBound Size Check *****/

/*
 * a structure denoting what a call to IOCTL_HAL_REQUEST_SYSINTR in the new
 * model will look like.  Used to set up a table of test cases below.
 */
typedef struct
{
    DWORD dwFlag;
    DWORD dwMode;
    DWORD dwIRQ1;
    DWORD dwIRQ2;
    DWORD dwIRQ3;
    DWORD dwIRQ4;
} sLargeCall;

/*
 * instances of the largeCell structure, one for each request mode supported
 * by the IOCTL.
 */
sLargeCall LargeCallDynamic =
    {(DWORD)SYSINTR_UNDEFINED, OAL_INTR_DYNAMIC};
sLargeCall LargeCallStatic =
    {(DWORD)SYSINTR_UNDEFINED, OAL_INTR_STATIC};
sLargeCall LargeCallForceStatic =
    {(DWORD)SYSINTR_UNDEFINED, OAL_INTR_FORCE_STATIC};

/* we need memory locations to point to in the table.  These provide them. */

/* 10 is arbitrary - this can't be -1 */
DWORD dwOldModelInBuff = 10;

DWORD dwCorrectOutputBuff = 0;

DWORD lpBytesReturnedCorrect = sizeof (DWORD);

/* thse are really runBadInSizeNewModel */
sAnIoctlCall newModelIncompleteArgSets[] =
    {
    /* standard checks */
    IOCTL_HAL_REQUEST_SYSINTR,
    _T("In buf is null"),

    NULL,
    sizeof (DWORD),
    &dwCorrectOutputBuff,
    sizeof (DWORD),
    &lpBytesReturnedCorrect,


    IOCTL_HAL_REQUEST_SYSINTR,
    _T("Out buf is null - old model"),

    &dwOldModelInBuff,
    sizeof (DWORD),
    NULL,
    sizeof (DWORD),
    &lpBytesReturnedCorrect,

    /* new model */
    IOCTL_HAL_REQUEST_SYSINTR,
    _T("inbound size of 2 - new mode - dynamic"),

    &LargeCallDynamic,
    2 * sizeof (DWORD),
    &dwCorrectOutputBuff,
    sizeof (DWORD),
    &lpBytesReturnedCorrect,


    IOCTL_HAL_REQUEST_SYSINTR,
    _T("inbound size of 2 - new mode - static"),

    &LargeCallStatic,
    2 * sizeof (DWORD),
    &dwCorrectOutputBuff,
    sizeof (DWORD),
    &lpBytesReturnedCorrect,


    IOCTL_HAL_REQUEST_SYSINTR,
    _T("inbound size of 2 - new mode - force static"),

    &LargeCallForceStatic,
    2 * sizeof (DWORD),
    &dwCorrectOutputBuff,
    sizeof (DWORD),
    &lpBytesReturnedCorrect

    };

BOOL
runBadInputNewModel_IncompleteArgSets()
{
    BOOL returnVal = TRUE;

    /* special cases. these consist mostly of incomplete argument sets */
    for (DWORD dwRow = 0; dwRow < _countof (newModelIncompleteArgSets); dwRow++)
    {
        if (!runRow (newModelIncompleteArgSets + dwRow))
        {
            returnVal = FALSE;
        }
    }

    return (returnVal);

}

/*
 * confirm that inbound sizes are correct
 */
BOOL
runBadInputNewModel_WrongInSizes ()
{
    BOOL returnVal = TRUE;

    if (!runBadInputNewModel_WrongInSizes (OAL_INTR_STATIC,
                      _T("OAL_INTR_STATIC")))
    {
        Error (_T("Failed on checks for OAL_INTR_STATIC"));
        returnVal = FALSE;
    }
    else if (!runBadInputNewModel_WrongInSizes (OAL_INTR_FORCE_STATIC,
                      _T("OAL_INTR_FORCE_STATIC")))
    {
        Error (_T("Failed on checks for OAL_INTR_FORCE_STATIC"));
        returnVal = FALSE;
    }
    else if (!runBadInputNewModel_WrongInSizes (OAL_INTR_DYNAMIC,
                      _T("OAL_INTR_DYNAMIC")))
    {
        Error (_T("Failed on checks for OAL_INTR_DYNAMIC"));
        returnVal = FALSE;
    }

    return (returnVal);
}


/*
 */
BOOL
runBadInputNewModel_WrongInSizes (const DWORD dwFlagForTest,
                  const TCHAR *const szFlagName)
{
    BOOL returnVal = TRUE;

    /* set up for old model */
    DWORD dwInBuf [MAX_INBOUND_BUF_LENGTH_DW] = { 0 };
    DWORD dwOutBuf = 0;

    sAnIoctlCall sStart =
    {
        IOCTL_HAL_REQUEST_SYSINTR,
        _T("New Model - Wrong Input Sizes"),

        dwInBuf,
        0,          // will be set later by the test
        &dwOutBuf,
        sizeof (DWORD),
        NULL
    };

    /* fill irq part */
    if (!fillInBoundArrayNewModel (dwInBuf, MAX_INBOUND_BUF_LENGTH_DW))
    {
        Error (_T("Couldn't fill in bound array for new model."));
        returnVal = FALSE;
        goto cleanAndReturn;
    }

    /* second dword in struct tells gives the allocation type. */
    dwInBuf[1] = dwFlagForTest;

    /* start from zero, even though we get overlap with other test cases */
    for (DWORD dwSize = 0; dwSize < MAX_INBOUND_BUF_LENGTH_DW * sizeof (DWORD);
       dwSize++)
    {
        /* skip all values that are multiples of four and provide at least one IRQ */
        if ((dwSize % sizeof (DWORD) == 0) && (dwSize > sizeof (DWORD) * 2))
        {
            continue;
        }

        Info (_T("Setting InputBufSize to %u.  Flag is %s"), dwSize, szFlagName);

        sStart.dwInputBufSize = dwSize;

     /* make call */
        if (!runRow (&sStart))
        {
            returnVal = FALSE;
        }
    }

cleanAndReturn:

    return (returnVal);
}


/*
 * for the old model, input must be 4 bytes.  No other values are allowed.
 */
BOOL
runBadInSizeOldModel ()
{
    BOOL returnVal = TRUE;

    /* set up for old model */
    DWORD dwInBuf [MAX_INBOUND_BUF_LENGTH_DW] = { 0 };
    DWORD dwOutBuf = 0;

    sAnIoctlCall sStart =
    {
        IOCTL_HAL_REQUEST_SYSINTR,
        _T("Unsupported OAL_INTR mode"),

        dwInBuf,
        0,          // will be set later by the test
        &dwOutBuf,
        sizeof (DWORD),
        NULL
    };

    if (!fillInBoundArrayOldModel (dwInBuf, MAX_INBOUND_BUF_LENGTH_DW))
    {
        Error (_T("Couldn't fill in bound array for old model."));
        returnVal = FALSE;
        goto cleanAndReturn;
    }

    for (DWORD dwSize = 0; dwSize < MAX_INBOUND_BUF_LENGTH_DW * sizeof (DWORD);
       dwSize++)
    {
        if (dwSize == sizeof (DWORD))
        {
            /* this is the valid insize, so skip it */
            continue;
        }

        Info (_T("Setting InputBufSize to %u."), dwSize);

        sStart.dwInputBufSize = dwSize;

        /* make call */
        if (!runRow (&sStart))
        {
            returnVal = FALSE;
        }
    }

cleanAndReturn:

    return (returnVal);
}

/****** Bad OAL_INTR_* flag Check *****/

/* only needed for new model */

BOOL
runBadFlags ()
{
    BOOL returnVal = TRUE;

    DWORD dwInBuf [MAX_INBOUND_BUF_LENGTH_DW] = { 0 };
    DWORD dwOutBuf = 0;     // this is just used for something to
                // point to.  value doesn't matter

    sAnIoctlCall sStart =
    {
        IOCTL_HAL_REQUEST_SYSINTR,
        _T("Unsupported OAL_INTR mode"),

        &dwInBuf,
        0,          // will be set later by the test
        &dwOutBuf,
        sizeof (DWORD),
        NULL
    };

    if (!fillInBoundArrayNewModel (dwInBuf, MAX_INBOUND_BUF_LENGTH_DW))
    {
        Error (_T("Couldn't fill in bound array for new model."));
        returnVal = FALSE;
        goto cleanAndReturn;
    }

    for (DWORD dwNumIRQs = 2; dwNumIRQs < MAX_INBOUND_BUF_LENGTH_DW; dwNumIRQs++)
    {
        sStart.dwInputBufSize = sizeof (DWORD) * dwNumIRQs;

        if (!runBadFlags (&sStart))
        {
            returnVal = FALSE;
        }
    }

cleanAndReturn:

    return (returnVal);
}


/* inputs bad flags where OAL_INTR_BLAH is supposed to go */
BOOL
runBadFlags (const sAnIoctlCall *const sRow)
{
    sAnIoctlCall sLocal = { 0 };

    BOOL returnVal = TRUE;

    /* make local copy so that we can modify it */
    memcpy (&sLocal, sRow, sizeof (sAnIoctlCall));

    for (DWORD dwFlag = 0; dwFlag <= 256; dwFlag++)
    {
        if (dwFlag == OAL_INTR_TRANSLATE || dwFlag == OAL_INTR_STATIC ||
        dwFlag == OAL_INTR_DYNAMIC || dwFlag == OAL_INTR_FORCE_STATIC ||
        dwFlag == 0)
        {
            /* this is a valid flag, so skip it */
            /*zero (no flags) behaves identical to OAL_INTR_DYNAMIC. It is kept for BC */
            continue;
        }

        ((DWORD *)sLocal.dwInputBuf)[1] = dwFlag;

        /* make call */
        if (!runRow (&sLocal))
        {
            returnVal = FALSE;
        }
    }

    for (DWORD dwFlag = 0x3; dwFlag != (0x3 << 30); dwFlag <<= 1)
    {
        ((DWORD *)sLocal.dwInputBuf)[1] = dwFlag;

        /* make call */
        if (!runRow (&sLocal))
        {
            returnVal = FALSE;
        }
    }

    return (returnVal);
}

/****** OutSize Check *****/

/*
 * confirm that the output size is being handled correctly.
 *
 * bTestLpBytesReturned set to true if testing with lpBytesReturned
 * passed to the IOCTL, false if not.
 */
BOOL
runOutSizeOldModel (BOOL bExpectToPass, BOOL bTestLpBytesReturned)
{
    BOOL returnVal = TRUE;

    /* setup for old model */
    DWORD dwInBuf = 0;
    /* only used for output, so the value doesn't matter */
    DWORD dwOutBuf = 0;

    sAnIoctlCall sStart =
    {
        IOCTL_HAL_REQUEST_SYSINTR,
        _T("Output size"),

        &dwInBuf,
        0,          // will be set later by the test
        &dwOutBuf,
        0,          // will be set later by the test
        NULL
    };

    Info (_T("Working on Old Model..."));

    /* set up for old model */
    if (!fillInBoundArrayOldModel (&dwInBuf, 1))
    {
        Error (_T("Couldn't fill inbound array for old model."));
        returnVal = FALSE;
        goto cleanAndReturn;
    }

    /* only one possible good value for the old model */
    sStart.dwInputBufSize = sizeof (DWORD);

    if (!runOutSize (&sStart, bExpectToPass, bTestLpBytesReturned))
    {
        returnVal = FALSE;
    }

cleanAndReturn:

    return (returnVal);

}

/*
 * confirm that the output size is being handled correctly.  This test
 * pushes values through for both the new model and the old *model.
 *
 * bTestLpBytesReturned set to true if testing with lpBytesReturned
 * passed to the IOCTL, false if not.
 */
BOOL
runOutSizeNewModel (BOOL bExpectToPass, BOOL bTestLpBytesReturned)
{
    BOOL returnVal = TRUE;

    /* setup for new model */
    DWORD dwInBuf [MAX_INBOUND_BUF_LENGTH_DW] = { 0 };
    /* only used for output, so the value doesn't matter */
    DWORD dwOutBuf = 0;

    sAnIoctlCall sStart =
    {
        IOCTL_HAL_REQUEST_SYSINTR,
        _T("Output size"),

        dwInBuf,
        0,          // will be set later by the test
        &dwOutBuf,
        0,          // will be set later by the test
        NULL
    };

    Info (_T("Working on New Model..."));

    if (!fillInBoundArrayNewModel (dwInBuf, MAX_INBOUND_BUF_LENGTH_DW))
    {
        Error (_T("Couldn't fill in bound array for new model."));
        returnVal = FALSE;
        goto cleanAndReturn;
    }

    /*
   * for, now assume that it must support at least one IRQ.
   */
    sStart.dwInputBufSize = 3 * sizeof (DWORD);

    if (!runOutSize (&sStart, bExpectToPass, bTestLpBytesReturned))
    {
        returnVal = FALSE;
    }

cleanAndReturn:

    return (returnVal);

}

/*
 * for the given structure, vary the output size.  Only sizeof (DWORD)
 * is supported.
 *
 * This supports both the old model and the model.  The inbound params
 * on the structure are not touched by this test.
 *
 * bTestLpBytesReturned set to true if testing with lpBytesReturned
 * passed to the IOCTL, false if not.
 */
BOOL
runOutSize (const sAnIoctlCall *const sRow, BOOL bExpectToPass,
        BOOL bTestLpBytesReturned)
{

    if (!sRow)
    {
        return (FALSE);
    }

    if (bTestLpBytesReturned && !bExpectToPass)
    {
        IntErr (_T("We must expect to pass if using lpBytesReturned."));
        return (FALSE);
    }

    BOOL returnVal = TRUE;

    /* local copy of the structure */
    sAnIoctlCall sLocal;

    memcpy (&sLocal, sRow, sizeof (sAnIoctlCall));

    /* array so that the call is "correct" */
    DWORD dwOutBuf [MAX_INBOUND_BUF_LENGTH_DW] = { 0 };

    sLocal.dwOutputBuf = dwOutBuf;

    if (bExpectToPass)
    {
        /* IOCTL should work for these test cases */

        /* for each possible size in the array */
        for (DWORD dwSize = sizeof (DWORD);
             dwSize < MAX_INBOUND_BUF_LENGTH_DW * sizeof (DWORD); 
             dwSize++)
        {
            Info (_T("Working on output buffer size of %u"), dwSize);

            sLocal.dwOutputBufSize = dwSize;

            /* make call */
            /*
             * if we are testing lpBytesReturned, output size must always
             * be sizeof (DWORD).  If not testing this, runRow will
             * ignore these args.
             */
            if (!runRow (&sLocal, bExpectToPass, bTestLpBytesReturned,
               sizeof (DWORD)))
            {
                returnVal = FALSE;
            }
        }
    }
    else
    {
        /* IOCTL should fail for these test cases */

        /* for each possible size in the array */
        for (DWORD dwSize = 0; dwSize < sizeof (DWORD); dwSize++)
        {
            if (dwSize == sizeof (DWORD))
            {
                /* this is the valid outsize, so skip it */
                continue;
            }

            Info (_T("Working on output buffer size of %u"), dwSize);

            sLocal.dwOutputBufSize = dwSize;

            /* make call */
            if (!runRow (&sLocal))
            {
                returnVal = FALSE;
            }
        }
    }

    return (returnVal);
}

/*************************  helpers ********************************/

BOOL
fillInBoundArrayNewModel (__out_ecount (dwNumOfDwords) DWORD *const pdwBuf,
              DWORD dwNumOfDwords)
{
    BOOL returnVal = FALSE;

    if (!pdwBuf)
    {
        goto cleanAndReturn;
    }

    /* new model uses first two DWORDS as flags */
    if (dwNumOfDwords == 0) 
    {
        returnVal = TRUE;
        goto cleanAndReturn;
    }

    pdwBuf[0] = (DWORD)SYSINTR_UNDEFINED;

    if (dwNumOfDwords == 1)
    {
        returnVal = TRUE;
        goto cleanAndReturn;
    }

    /* need to assume something.  use dynamic.  arbitrary. */
    pdwBuf[1] = OAL_INTR_DYNAMIC;

    if (!inKernelMode())
    {
        Info (_T("Not in K-mode, so can't determine which IRQs are valid."));
        Info (_T("Choosing ones that will probably work."));

        for (DWORD dwPos = 2; dwPos < dwNumOfDwords; dwPos++)
        {
            /* arbitrary */
            pdwBuf[dwPos] = dwPos + 8;
        }
        returnVal = TRUE;
        goto cleanAndReturn;
    }

    /* the ranges supported by the test, inclusive */
    DWORD dwIRQBegin = TEST_IRQ_BEGIN;
    DWORD dwIRQEnd = TEST_IRQ_END;

    /* adjust the ranges if need be */
    if (!handleCmdLineIRQRange (&dwIRQBegin, &dwIRQEnd))
    {
        Error (_T("Couldn't parse command line."));
        goto cleanAndReturn;
    }

    DWORD dwArrayPosToFill = 2;

    DWORD dwIRQ = dwIRQBegin;

    for (BOOL bJustRolled = FALSE;
       dwIRQ <= dwIRQEnd && !bJustRolled; dwIRQ++)
    {
        DWORD dwSysIntr;
        DWORD dwIRQCall;

        dwIRQCall = dwIRQ;

        if (!KernelIoControl (IOCTL_HAL_REQUEST_SYSINTR,
               &dwIRQCall, sizeof (DWORD),
               &dwSysIntr, sizeof (DWORD), NULL))
        {
            /* already in use */
            Info (_T("IRQ %u is already in use."), dwIRQ);
            continue;
        }

        /* this one is good */

        /* free it */
        if (!KernelIoControl (IOCTL_HAL_RELEASE_SYSINTR,
                &dwSysIntr, sizeof (dwSysIntr),
                NULL, NULL, NULL))
        {
            Error (_T("Couldn't free %u, which was just allocated."),
                dwSysIntr);
            goto cleanAndReturn;
        }


        dwArrayPosToFill++;

        if(dwArrayPosToFill < dwNumOfDwords) {
            pdwBuf[dwArrayPosToFill-1] = dwIRQ;
        }
        else if(dwArrayPosToFill == dwNumOfDwords) {
            pdwBuf[dwArrayPosToFill-1] = dwIRQ;
            dwArrayPosToFill--;
            break;
        }
        else /* dwArrayPosToFill > dwNumOfDword */ {
            /* should not get here */
            break;
        }

        /* if we just worked on DWORD_MAX then want to bail */
        bJustRolled = (dwIRQ == DWORD_MAX);
    }

    if (dwArrayPosToFill == dwNumOfDwords - 1)
    {
        /* we successfully filled the array */
        returnVal = TRUE;
    }
    else
    {
        Error (_T("Couldn't fill array with valid IRQs."));
    }

 cleanAndReturn:

    return (returnVal);
}

/*
 * command line args or defines tell us which IRQ range can be used.
 * for these tests we want an IRQ range that is valid.  We therefore
 * take what the user gives us and start searching for valid IRQs.
 * If we find them we fill in the array and return it.  If not we
 * error out.
 *
 * const says that pointers don't change, but objects can
 */
BOOL
fillInBoundArrayOldModel (__out_ecount (dwNumOfDwords) DWORD *const pdwBuf,
              DWORD dwNumOfDwords)
{
    if ((!pdwBuf) || (dwNumOfDwords == 0))
    {
        return (FALSE);
    }

    BOOL returnVal = FALSE;

    if (!inKernelMode())
    {
        Info (_T("Not in K-mode, so can't determine which IRQs are valid."));
        Info (_T("Choosing ones that will probably work."));

        for (DWORD dwPos = 0; dwPos < dwNumOfDwords; dwPos++)
        {
            /* arbitrary */
            pdwBuf[dwPos] = dwPos + 10;
        }
        returnVal = TRUE;
        goto cleanAndReturn;
    }

    /* the ranges supported by the test, inclusive */
    DWORD dwIRQBegin = TEST_IRQ_BEGIN;
    DWORD dwIRQEnd = TEST_IRQ_END;

    /* adjust the ranges if need be */
    if (!handleCmdLineIRQRange (&dwIRQBegin, &dwIRQEnd))
    {
        Error (_T("Couldn't parse command line."));
        goto cleanAndReturn;
    }

    DWORD dwArrayPosToFill = 0;

    DWORD dwIRQ = dwIRQBegin;

    for (BOOL bJustRolled = FALSE;
       dwIRQ <= dwIRQEnd && !bJustRolled; dwIRQ++)
    {
        DWORD dwSysIntr;

        DWORD dwInBuf[3] = {(DWORD)-1, OAL_INTR_STATIC};
        dwInBuf[2] = dwIRQ;

        if (!KernelIoControl (IOCTL_HAL_REQUEST_SYSINTR,
               dwInBuf, sizeof (dwInBuf),
               &dwSysIntr, sizeof (dwSysIntr), NULL))
        {
            /* already in use */
            Info (_T("IRQ %u is already in use."), dwIRQ);
            continue;
        }

        /* this one is good */

        /* free it */
        if (!KernelIoControl (IOCTL_HAL_RELEASE_SYSINTR,
                &dwSysIntr, sizeof (dwSysIntr),
                NULL, NULL, NULL))
        {
            Error (_T("Couldn't free %u, which was just allocated."),
                dwSysIntr);
            goto cleanAndReturn;
        }


        dwArrayPosToFill++;

        if(dwArrayPosToFill < dwNumOfDwords) {
            pdwBuf[dwArrayPosToFill-1] = dwIRQ;
        }
        else if(dwArrayPosToFill == dwNumOfDwords) {
            pdwBuf[dwArrayPosToFill-1] = dwIRQ;
            dwArrayPosToFill--;
            break;
        }
        else /* dwArrayPosToFill > dwNumOfDword */ {
            /* should not get here */
            break;
        }

        /* if we just worked on DWORD_MAX then want to bail */
        bJustRolled = (dwIRQ == DWORD_MAX);
    }

    if (dwArrayPosToFill == dwNumOfDwords - 1)
    {
        /* we successfully filled the array */
        returnVal = TRUE;
    }
    else
    {
        Error (_T("Couldn't fill array with valid IRQs."));
    }

 cleanAndReturn:

    return (returnVal);
}

/***************** TEST CASES for IOCTL_HAL_RELEASE_SYSINTR ******************/

/*
 * input must be 4 bytes.  No other values are allowed.
 */
BOOL
runBadInSize_IOCTL_HAL_RELEASE_SYSINTR ()
{
    BOOL returnVal = FALSE;

    /* set up */
    DWORD dwInBuf [MAX_INBOUND_BUF_LENGTH_DW] = { 0 };

    sAnIoctlCall sStart =
    {
        IOCTL_HAL_RELEASE_SYSINTR,
        _T("Bad inBoundSize"),

        dwInBuf,
        0,          // will be set later by the test
        NULL,           // ignored by code
        0,
        NULL
    };

    /*
   * sysintr for the k-mode case.  This is the sysintr that is
   * actually allocated to make the code work.
   */
    DWORD dwSysIntr;

    if (inKernelMode())
    {
        /*
       * we need to find a valid IRQ that we can then allocate a sysIntr
       * to.  This makes sure that we have something valid to free.
       */

        DWORD dwGoodIRQ;

        if (!fillInBoundArrayOldModel (&dwGoodIRQ, 1))
        {
            Error (_T("Couldn't find an IRQ to allocate."));
            goto cleanAndReturn;
        }

        /* now, allocate this IRQ */

        DWORD dwBuf[3] = {(DWORD)-1, OAL_INTR_STATIC};
        dwBuf[2] = dwGoodIRQ;
        if (!KernelIoControl (IOCTL_HAL_REQUEST_SYSINTR,
                dwBuf, sizeof (dwBuf),
                &dwSysIntr, sizeof (dwSysIntr), NULL))
        {
            Error (_T("IRQ %u should be free for static and now isn't."),
                dwGoodIRQ);
            goto cleanAndReturn;
        }

        /*
       * IRQ is allocated, and we therefore have a sysintr that can now
       * be freed.  Use this as the test case.
       */
        dwInBuf[0] = dwSysIntr;
    }
    else /* no in k-mode */
    {
        /* arbitrary */
        dwInBuf[0] = 18;
    }


    /* fill in the rest with random stuff */
    for (DWORD dwPos = 1; dwPos < MAX_INBOUND_BUF_LENGTH_DW; dwPos++)
    {
        /* fill with random values */
        dwInBuf[dwPos] = getRandomInRange(0, 71);
    }

    for (DWORD dwSize = 0; dwSize < MAX_INBOUND_BUF_LENGTH_DW * sizeof (DWORD);
       dwSize++)
    {
        if (dwSize == sizeof (DWORD))
        {
            /* this is the valid insize, so skip it */
            continue;
        }

        Info (_T("Setting InputBufSize to %u."), dwSize);

        sStart.dwInputBufSize = dwSize;

        /* make call */
        if (!runRow (&sStart))
        {
        /*
         * something when wrong.  We freed the sysintr.  At this point
         * we can't continue since the preconditions are wrong.  Bail instead
         * of trying to fix it.
         */
            goto cleanAndReturn;
        }
    }

    if (inKernelMode())
    {
        /* release sysintr */
        if (!KernelIoControl (IOCTL_HAL_RELEASE_SYSINTR,
                &dwSysIntr, sizeof (dwSysIntr),
                NULL, NULL, NULL))
        {
            Error (_T("Couldn't free sysintr %u."), dwSysIntr);
            goto cleanAndReturn;
        }
    }

    returnVal = TRUE;

cleanAndReturn:

    return (returnVal);
}

/***************** TEST CASES for IOCTL_HAL_REQUEST_IRQ *********************/

/*
 */
BOOL
runBadInSize_IOCTL_HAL_REQUEST_IRQ ()
{
    BOOL returnVal = TRUE;

    /* set up - size if arbitrary */
    DWORD dwInBuf [sizeof (DEVICE_LOCATION) * 4] = { 0 };
    /* ignored in this test */
    DWORD dwOutBuf = 0;

    sAnIoctlCall sStart =
    {
        IOCTL_HAL_REQUEST_IRQ,
        _T("Bad inBoundSize"),

        dwInBuf,
        0,          // will be set later by the test
        &dwOutBuf,
        sizeof (dwOutBuf),
        NULL
    };

    for (DWORD dwPos = 0; dwPos < sizeof (DEVICE_LOCATION) * 4; dwPos++)
    {
        /* fill with random values */
        /* 1000000 is arbitrary */
        dwInBuf[dwPos] = getRandomInRange(0, 1000000);
    }

    for (DWORD dwSize = 0; dwSize < sizeof (DEVICE_LOCATION) * 4; dwSize++)
    {
        if (dwSize == sizeof (DEVICE_LOCATION))
        {
            /* this is the valid insize, so skip it */
            continue;
        }

        Info (_T("Setting InputBufSize to %u."), dwSize);

        sStart.dwInputBufSize = dwSize;

        /* make call */
        if (!runRow (&sStart))
        {
            returnVal = FALSE;
        }
    }

    return (returnVal);
}


/*
 * note that we have no good way to determine what a valid REQUEST_IRQ
 * contains.  The inputs are too spread out.  Without this, we push
 * some values through it and hope that we see errors.  We really
 * can't do much else.
 */
BOOL
runBadOutSize_IOCTL_HAL_REQUEST_IRQ ()
{
    BOOL returnVal = TRUE;

    /* set up - size is arbitrary */
    DWORD dwInBuf [sizeof (DEVICE_LOCATION)] = { 0 };

    /* outbound buffer used for test */
    DWORD dwOutBuf [MAX_INBOUND_BUF_LENGTH_DW] = { 0 };

    sAnIoctlCall sStart =
    {
        IOCTL_HAL_RELEASE_SYSINTR,
        _T("Bad inBoundSize"),

        dwInBuf,            // input that will be constant
        sizeof (DEVICE_LOCATION),
        dwOutBuf,           // input under test
        0,          // will be set by test
        NULL
    };

    /*
     * fill inbound with random.  Not nearly ideal, but we can do
     * nothing else.
     */
    for (DWORD dwPos = 0; dwPos < sizeof (DEVICE_LOCATION); dwPos++)
    {
        /* fill with random values */
        dwInBuf[dwPos] = getRandomInRange(0, 71);
    }

    /* fill outbound with random for test. */
    for (DWORD dwPos = 0; dwPos < MAX_INBOUND_BUF_LENGTH_DW; dwPos++)
    {
        /* fill with random values */
        dwOutBuf[dwPos] = getRandomInRange(0, 71);
    }

    for (DWORD dwSize = 0; dwSize < sizeof (DWORD); dwSize++)
    {
        Info (_T("Setting OutputBufSize to %u."), dwSize);

        sStart.dwOutputBufSize = dwSize;

     /* make call */
        if (!runRow (&sStart))
        {
            returnVal = FALSE;
        }
    }

    return (returnVal);
}

