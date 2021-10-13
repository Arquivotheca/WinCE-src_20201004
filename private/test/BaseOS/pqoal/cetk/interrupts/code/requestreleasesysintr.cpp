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

#include "commonUtils.h"

#include "sysIntrOverloads.h"

#include "tuxInterrupts.h"

#include "interruptCmdLine.h"

#include "prettyPrinting.h"

/******************************************************************************/
/*                                                                            */
/*                                   Constants                                */
/*                                                                            */
/******************************************************************************/


/*
 * number of random values to test when we resort to random testing.
 * Lengthening probably won't increase test coverage that much but
 * will make the tests run longer.
 */
#define TEST_N_RANDOM_VALUES (10000)

/*
 * when doing allocations for multiple IRQs, this tells the test how
 * many interations to run.
 */
#define MULT_ALLOC_TEST_IT (100)

/*
 * the maximium possible IRQ per SYSINTR.  We are setting this
 * impossibly high
 */
#define MAX_POSSIBLE_IRQ_PER_SYSINTR (10)


/******************************************************************************/
/*                                                                            */
/*                                 Functions                                  */
/*                                                                            */
/******************************************************************************/

/******* helper ***************************************************************/



enum eReturnMaxSuccessfullyAllocatedIrq {FOUND_IT, MISSED_IT, FATAL_ERROR};

eReturnMaxSuccessfullyAllocatedIrq
findMaxSuccessfullyAllocatedIrq (DWORD dwIRQBegin, DWORD dwIRQEnd,
                 DWORD * dwMax);
BOOL
findHowManySysIntrsAreFree (DWORD dwIRQBegin, DWORD dwIRQEnd,
                DWORD * dwNumFree, BOOL bPrintDebug);

BOOL
handleIrqRange (DWORD dwUserData, DWORD * pdwArgIRQBegin, DWORD * pdwArgIRQEnd,
        BOOL * pbAllowedToPass);

BOOL
getNumSysIntrsFree (DWORD dwIRQBegin, DWORD dwIRQEnd,
            DWORD * pdwNumSysIntrsFree);

BOOL
checkNumSysIntrsFree (DWORD dwIRQBegin, DWORD dwIRQEnd,
              DWORD dwNumSysIntrsFree);

__inline BOOL inKernelMode( void )
{
    return (int)inKernelMode < 0;
}


/******* For running tests ****************************************************/

BOOL
findMaxIrqPerSysIntr (DWORD dwIRQBegin, DWORD dwIRQEnd,
              DWORD * pdwMaxIrqPerSysIntr, BOOL bPrint);

/***** Old model ******/

BOOL
OldModelStandardAllocate (DWORD dwIRQ, DWORD dwSysIntr);



enum enum_OldModelTestType {SIMPLE_ALLOCATE};

BOOL
setupAndRunUnallocatedSysintrsTest(enum_OldModelTestType type);

/***** new model ******/
BOOL
runAllocateAll_SingleIRQ (DWORD dwAllocationType, const TCHAR szTypeName[],
              DWORD dwIRQBegin, DWORD dwIRQEnd,
              BOOL bAllowedToFail, BOOL bAllowedToPass);

BOOL
runAllocateRand_MultIRQ (DWORD dwAllocationType, BOOL bAllowedToFail,
             BOOL bAllowedToPass,
             DWORD dwIRQBegin, DWORD dwIRQEnd,
             DWORD dwMaxIrqPerSysIntr);

BOOL
allocateRand_checkDuplicateIRQs (const DWORD *const dwIRQsOrig, DWORD dwLen,
                 DWORD dwAllocationType,
                 BOOL bAllowedToFail,
                 BOOL bAllowedToPass);

BOOL
runAllocateRandForceStatic_MultIRQ (DWORD dwIRQBegin, DWORD dwIRQEnd,
                    DWORD dwMaxIrqPerSysIntr,
                    BOOL bAllowedToPass);



/******************************************************************************/
/*                                                                            */
/*                                    Tests                                   */
/*                                                                            */
/******************************************************************************/

TESTPROCAPI
PrintUsage(
       UINT uMsg,
       TPPARAM tpParam,
       LPFUNCTION_TABLE_ENTRY lpFTE)
{
    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return (TPR_NOT_HANDLED);
    }

    printUsage ();

    return (TPR_PASS);
}

/******** Releasing SYSINTRs *********************************************/

/*
 * Only certain sysintrs are valid.  This function tries to free those
 * that aren't.
 *
 * ranges 0 - 7 are system intrs and should never be freed.
 * 8 - 71 are devices intrs and can be freed.
 * everything else is off limits
 *
 * We can't touch anything from 8 - 71 because we could crash the
 * system.
 *
 * Ideally, we would test everything that was not allowed in the range
 * of dwords, but this takes too long.  So, we get smart:
 *
 * 0 - 7 because these are obvious
 * 72 to 256 because we expect errors close to the allowable values.
 * -1 to -255 for the same reason.
 * SYSINTR_UNDEFINED for obvious reasons
 * Powers of two in case they are doing bit shifts and the like
 *
 * We need more test cases, so we turn to a random generator between
 * the invalid numbers.  This will hopefully catch out of bounds
 * memory accesses and other funky things that the targeted tests
 * wouldn't hit.
 *
 * Note that we could make the random range > 256 to < (DWORD) -255.
 * This is confusing to explain and would have to be changed if folks
 * changed these special cases later.  For this reason we just do the
 * entire range > 72.
 */
TESTPROCAPI  freeWhatShouldNotBeFreed(
                      UINT uMsg,
                      TPPARAM tpParam,
                      LPFUNCTION_TABLE_ENTRY lpFTE)
{

    INT returnVal = TPR_PASS;

    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        returnVal = TPR_NOT_HANDLED;
        goto cleanAndReturn;
    }

    breakIfUserDesires ();

    printDisclaimer ();
    Info (_T(""));

    Info (_T("This test confirms that the oal correctly handles SYSINTERS that"));
    Info (_T("can't be released.  This code calls IOCTL_HAL_RELEASE_SYSINTR with"));
    Info (_T("SYSINTR values that should not successfully be released.  The test"));
    Info (_T("passes if it receives false for each of these calls.  Any other"));
    Info (_T("return value constitutes an error."));
    Info (_T(""));
    Info (_T("The only SYSINTERS that can be released are those between 8 and 71,"));
    Info (_T("inclusive.  These are not touched, since releasing these could "));
    Info (_T("potentially crash the system."));
    Info (_T(""));
    Info (_T("If this test fails, it is possible that the system could hang or"));
    Info (_T("crash.  Furthermore, the system could hang before the failure is"));
    Info (_T("reported in the logs."));
    Info (_T(""));

    Info (_T("Checking SYSINTR_UNDEFINED (0x%x)"), SYSINTR_UNDEFINED);

    if (releaseSysIntr ((DWORD)SYSINTR_UNDEFINED))
    {
        /* something is wrong.  All of these calls should fail. */
        Error (_T("Error.  We were able to release sysintr %u."),
         SYSINTR_UNDEFINED);
        returnVal = TPR_FAIL;
    }

    Info (_T("Checking [0, 8)"));

    /* try to free the special interrupts from [0, 8) */
    for (DWORD dwSysIntr = 0; dwSysIntr < 8; dwSysIntr++)
    {
        if (releaseSysIntr (dwSysIntr))
        {
            /* something is wrong.  All of these calls should fail. */
            Error (_T("Error.  We were able to release sysintr %u."), dwSysIntr);
            returnVal = TPR_FAIL;
        }
    }

    Info (_T("Checking [72, 256]"));

    /* try to free the interrupts from [72, 256] */
    for (DWORD dwSysIntr = 72; dwSysIntr <= 256; dwSysIntr++)
    {
        if (releaseSysIntr (dwSysIntr))
        {
            /* something is wrong.  All of these calls should fail. */
            Error (_T("Error.  We were able to release sysintr %u."), dwSysIntr);
            returnVal = TPR_FAIL;
        }
    }

    Info (_T("Checking boundaries of powers of two (plus/minus 1)"));

    /* try to free all interrupts on boardaries of two. */
    for (DWORD dwSysIntrBase = 256; dwSysIntrBase != 0;
       dwSysIntrBase <<= 1)
    {
        /* try one less than the power of two */
        DWORD dwSysIntr = dwSysIntrBase - 1;
        if (releaseSysIntr (dwSysIntr))
        {
            /* something is wrong.  All of these calls should fail. */
            Error (_T("Error.  We were able to release sysintr %u."), dwSysIntr);
            returnVal = TPR_FAIL;
        }

        /* try power of two */
        dwSysIntr = dwSysIntrBase;
        if (releaseSysIntr (dwSysIntr))
        {
            /* something is wrong.  All of these calls should fail. */
            Error (_T("Error.  We were able to release sysintr %u."), dwSysIntr);
            returnVal = TPR_FAIL;
        }

        /* try one greater */
        dwSysIntr = dwSysIntrBase + 1;
        if (releaseSysIntr (dwSysIntr))
        {
            /* something is wrong.  All of these calls should fail. */
            Error (_T("Error.  We were able to release sysintr %u."), dwSysIntr);
            returnVal = TPR_FAIL;
        }
    }

    Info (_T("Checking (-256, -1]"));

    /*
   * check negative numbers from -1 to -255. There really correspond
   * to 0xffffffff, etc.
   */
    for (int iSysIntr = -1; iSysIntr > -256; iSysIntr--)
    {
        DWORD dwSysIntr = (DWORD) iSysIntr;

        if (releaseSysIntr (dwSysIntr))
        {
            /* something is wrong.  All of these calls should fail. */
            Error (_T("Error.  We were able to release sysintr %u."), dwSysIntr);
            returnVal = TPR_FAIL;
        }
    }

    /* check random values in the invalid range */
    Info (_T("Checking %u randomly generated sysintrs between [72, 0xffffffff]."),
    TEST_N_RANDOM_VALUES);

    for (DWORD dwCount = 0; dwCount < TEST_N_RANDOM_VALUES; dwCount++)
    {
        DWORD dwSysIntr = getRandomInRange (72, 0xffffffff);

        if (releaseSysIntr (dwSysIntr))
        {
            /* something is wrong.  All of these calls should fail. */
            Error (_T("Error.  We were able to release sysintr %u."), dwSysIntr);
            returnVal = TPR_FAIL;
        }
    }

    Info (_T("")); // blank line
    Info (_T("Test Complete"));
    Info (_T(""));

    if (returnVal == TPR_PASS)
    {
        Info (_T("Test passed"));
    }
    else
    {
        /* we don't use tpr_skip, so this is safe */
        Error (_T("Test failed"));
    }

 cleanAndReturn:

    return (returnVal);
}


/*
 * return how many free sysintrs are currently available.  Calls a helper
 * function that does the work.
 */
TESTPROCAPI  testFindNumFreeSysIntrs(
                      UINT uMsg,
                      TPPARAM tpParam,
                      LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_FAIL;

    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        returnVal =  TPR_NOT_HANDLED;
        goto cleanAndReturn;
    }

    breakIfUserDesires ();

    printDisclaimer ();

    Info (_T("This test tries to find the number of free SYSINTRs on the"));
    Info (_T("the system."));

    /* the ranges supported by the test, inclusive */
    DWORD dwIRQ = TEST_IRQ_BEGIN;
    DWORD dwUpperBoundInclusive = TEST_IRQ_END;

    /* adjust the ranges if need be */
    if (!handleCmdLineIRQRange (&dwIRQ, &dwUpperBoundInclusive))
    {
        Error (_T("Couldn't parse command line."));
        goto cleanAndReturn;
    }

    DWORD dwNumSysIntrsFree1, dwNumSysIntrsFree2;

    if (!inKernelMode())
    {
        Error (_T("Must be in kernel mode to run this test."));
        goto cleanAndReturn;
    }

    if (!findHowManySysIntrsAreFree (dwIRQ, dwUpperBoundInclusive,
                   &dwNumSysIntrsFree1, TRUE))
    {
        goto cleanAndReturn;
    }

    if (!findHowManySysIntrsAreFree (dwIRQ, dwUpperBoundInclusive,
                   &dwNumSysIntrsFree2, TRUE))
    {
        goto cleanAndReturn;
    }

    if (dwNumSysIntrsFree2 != dwNumSysIntrsFree2)
    {
        Error (_T("Two successive calls yielded different numbers of SYSINTRs")
         _T("free."));
        Error (_T("First got %u and then got %u."),
         dwNumSysIntrsFree1, dwNumSysIntrsFree2);
        goto cleanAndReturn;
    }

    Info (_T("There are %u SYSINTRs free."), dwNumSysIntrsFree1);

    returnVal = TPR_PASS;

 cleanAndReturn:

    return (returnVal);
}


/******** Old model (allocate ) ****************************/

/*
 * This test tries to force errors on unallocated sysintrs.
 *
 * IRQs are allocated until they can't be allocated anymore.  This
 * gives us a list of sysintrs that are unallocated.  We can then play
 * with these.
 *
 * This routines assumes (and confirms) that the interrupt routines
 * don't double allocate SYSINTRs.
 */
TESTPROCAPI  stressUnallocatedSysInters(
                       UINT uMsg,
                       TPPARAM tpParam,
                       LPFUNCTION_TABLE_ENTRY lpFTE)
{
    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return (TPR_NOT_HANDLED);
    }

    printDisclaimer ();

    breakIfUserDesires ();

    Info (_T(""));
    Info (_T("This test works with the unallocated SYSINTRs on the system."));
    Info (_T("Multiple simultaneous allocations allow the test to develop"));
    Info (_T("a list of unallocated SYSINTRs.  These are then stressed with"));
    Info (_T("different types of calls."));
    Info (_T("If no SYSINTRs are currently available on the system this test"));
    Info (_T("will pass."));
    Info (_T(""));

    if (setupAndRunUnallocatedSysintrsTest (SIMPLE_ALLOCATE))
    {
        return (TPR_PASS);
    }
    else
    {
        return (TPR_FAIL);
    }

}

/* kern mode only */
BOOL
setupAndRunUnallocatedSysintrsTest(enum_OldModelTestType type)
{
    /*
   * Allows us to easily track the return values from the sysintr
   * calls.  We track the return value and the sysintr that was returned.
   *
   * Don't change the array length without changing the comparison code below
   */
    typedef struct
    {
    DWORD dwIRQ;        // irq
    DWORD dwRC;         // return code
    } sTrackSysIntrs;


    BOOL bRet = TRUE;

    DWORD dwIRQBegin = TEST_IRQ_BEGIN;
    DWORD dwIRQEnd = TEST_IRQ_END;

    if (!handleCmdLineIRQRange (&dwIRQBegin, &dwIRQEnd))
    {
        bRet = FALSE;
        goto cleanAndReturn;
    }

    sTrackSysIntrs pWhichSysIntrsToRelease[SYSINTR_MAXIMUM];

    /* initialize array */
    for (DWORD dwPos = 0; dwPos < SYSINTR_MAXIMUM; dwPos++)
    {
        pWhichSysIntrsToRelease[dwPos].dwRC = FALSE;
    }

    Info (_T("Finding which SYSINTRs are unallocated."));

    DWORD dwIRQ = dwIRQBegin;

    for (BOOL bJustRolled = FALSE;
       dwIRQ <= dwIRQEnd && !bJustRolled; dwIRQ++)
    {
        DWORD dwSysIntr;

        BOOL rc;

        rc = getSysIntr (dwIRQ, &dwSysIntr);
        if(!rc)
        {
            continue;
        }

        /* if out of range go to next irq */
        /* throw in second check (essentially a nop) to make it prefast clean */
        if (!checkSysIntrRange (dwSysIntr) || dwSysIntr >= SYSINTR_MAXIMUM)
        {
            bRet = FALSE;
            continue;
        }

        /*
       * we are now sure that an array access wouldn't cause a buffer
       * overflow
       */
        if (pWhichSysIntrsToRelease[dwSysIntr].dwRC)
        {
            Error (_T("SYSINTR %u has already been allocated to IRQ %u."),
                 dwSysIntr, pWhichSysIntrsToRelease[dwSysIntr].dwIRQ);
            Error (_T("Routines just allocated it to IRQ %u as well."),
                dwIRQ);
            bRet = FALSE;
            continue;
        }

        /* sysIntr is clear.  Record that we got it back. */
        pWhichSysIntrsToRelease[dwSysIntr].dwRC = TRUE;
        pWhichSysIntrsToRelease[dwSysIntr].dwIRQ = dwIRQ;

        /* if we just worked on DWORD_MAX then want to bail */
        bJustRolled = (dwIRQ == DWORD_MAX);

    } /* for */

    if (!bRet)
    {
        Error (_T("Error during allocation step.  Cannot continue."));
        goto cleanAndReturn;
    }

    /*
   * now, we have a list of sysinters that we can play with that won't
   * crash the system.
   */

    Info (_T("The SYSINTRs that were just allocated by this test."));
    Info (_T(""));

    BOOL bFoundSysIntr = FALSE;

    /* print sysintr array for debugging */
    for (DWORD dwSysIntr = 0; dwSysIntr < SYSINTR_MAXIMUM; dwSysIntr++)
    {
        if (pWhichSysIntrsToRelease[dwSysIntr].dwRC)
        {
            Info (_T("SYSINTR %u  IRQ %u"),
            dwSysIntr, pWhichSysIntrsToRelease[dwSysIntr].dwIRQ);
            bFoundSysIntr = TRUE;
        }
    }

    if (!bFoundSysIntr)
    {
        Info (_T("No free SYSINTRs found.  This is odd, but not unheard of."));
        Info (_T("Passing the test because this isn't a definitive failure."));
        goto cleanAndReturn;
    }

    Info (_T(""));

    for (DWORD dwSysIntr = 0; dwPos < SYSINTR_MAXIMUM; dwSysIntr++)
    {
        /* only work on SYSINTERs that were successfully allocated */
        if (!pWhichSysIntrsToRelease[dwSysIntr].dwRC)
        {
            continue;
        }

        switch (type)
        {
        case SIMPLE_ALLOCATE:
            bRet = OldModelStandardAllocate (pWhichSysIntrsToRelease[dwSysIntr].dwIRQ, dwSysIntr);
            break;
        default:
            IntErr (_T("test type is not defined."));
            bRet = FALSE;
            break;
        }
    }

    if (!bRet)
    {
        Error (_T("Above errors probably put the system into an unstable state."));
        Error (_T("Going to try to release SYSINTRs that we allocated anyway."));
    }

    Info (_T(""));
    Info (_T("Releasing the SYSINTRs allocated by the test."));
    Info (_T(""));

    /* release them - these should succeed */
    for (DWORD dwSysIntr = 0; dwSysIntr < SYSINTR_MAXIMUM; dwSysIntr++)
    {
        /* only work on SYSINTERs that were successfully allocated */
        if (!pWhichSysIntrsToRelease[dwSysIntr].dwRC)
        {
            continue;
        }

        if (!releaseSysIntr (dwSysIntr))
        {
            Error (_T("Error!  Weren't able to release SYSINTR %u."), dwSysIntr);
            bRet = FALSE;
        }
    }

 cleanAndReturn:

    if (bRet)
    {
        Info (_T("Test passed."));
        return (TPR_PASS);
    }
    else
    {
        Error (_T("Test failed."));
        return (TPR_FAIL);
    }
}


/*
 * for each sysinter:
 * - release it (should work)
 * - release it (double release should fail)
 * - reallocate that IRQ.  We should get the same sysinter (the
 * tables are full, we released one, there should be the only space
 * left).
 * - reallocate it again.  We should get failure, since there is
 * no room.
 */
BOOL
OldModelStandardAllocate (DWORD dwIRQ, DWORD dwSysIntr)
{
    BOOL returnVal = FALSE;

    Info (_T("  --- Working on SYSINTR %u ---"), dwSysIntr);

    Info (_T("Trying to release previously allocated sysinter %u."),
    dwSysIntr);

    if (releaseSysIntr (dwSysIntr))
    {
        Info (_T("Successful!  SYSINTR %u is released."), dwSysIntr);
    }
    else
    {
        Error (_T("Error!  SYSINTR %u not released."), dwSysIntr);
        goto cleanAndReturn;
    }

    Info (_T("Trying to release sysinter %u again.  This should fail."),
    dwSysIntr);

    if (releaseSysIntr (dwSysIntr))
    {
        Error (_T("Error!  SYSINTR %u released a second time."), dwSysIntr);
        goto cleanAndReturn;
    }
    else
    {
        Info (_T("Successful!  SYSINTR %u not released."), dwSysIntr);
    }

    /* try to reallocate it */
    DWORD dwNewSysIntr;

    Info (_T("Trying to reallocate SYSINTR %u for irq %u"),
    dwSysIntr, dwIRQ);

    if (getSysIntr (dwIRQ, &dwNewSysIntr))
    {
        if (dwSysIntr == dwNewSysIntr)
        {
            Info (_T("Successful!  Got the same SYSINTR back"));
        }
        else
        {
            Error (_T("Error!  Expected SYSINTR %u and recieved SYSINTER %u"),
                dwSysIntr, dwNewSysIntr);
            goto cleanAndReturn;
        }

    }
    else
    {
        Error (_T("Error!  We should be able to get a SYSINTR for IRQ %u"),
         dwIRQ);
        Error (_T("(we know that SYSINTR %u is free) but we couldn't ")
         _T("allocate it."), dwSysIntr);
        goto cleanAndReturn;
    }

    Info (_T("Trying to reallocate IRQ %u again."), dwIRQ);

    /* reallocate.  Expect failure */
    if (getSysIntr (dwIRQ, &dwNewSysIntr))
    {
        Error (_T("Error!  There should be no room in the tables and yet"));
        Error (_T("we were able to allocate another IRQ."));
        goto cleanAndReturn;
    }
    else
    {
        Info (_T("Successful!  Couldn't allocate SYSINTR."));
    }

    returnVal = TRUE;

 cleanAndReturn:

    return (returnVal);
}


/********* Max IRQ per SYSINTR *********************************************/

/*
 * This test allocates a given set of irqs.  Successfully returned
 * sysintrs and immediately freed.
 *
 * irq range is support through command line options.
 */
TESTPROCAPI  testFindMaxIRQPerSysIntr(
                      UINT uMsg,
                      TPPARAM tpParam,
                      LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_FAIL;

    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        returnVal =  TPR_NOT_HANDLED;
        goto cleanAndReturn;
    }

    breakIfUserDesires ();

    printDisclaimer ();

    Info (_T("This test tries to find the maximum allowed IRQ per Sysintr"));
    Info (_T("on this system.  Note that this assumes that this value is"));
    Info (_T("constant across all sysintrs.  If this isn't the case (say"));
    Info (_T("that the code uses a linked list implementation) this value"));
    Info (_T("might be incorrect."));

    /* the ranges supported by the test, inclusive */
    DWORD dwIRQ = TEST_IRQ_BEGIN;
    DWORD dwUpperBoundInclusive = TEST_IRQ_END;

    /* adjust the ranges if need be */
    if (!handleCmdLineIRQRange (&dwIRQ, &dwUpperBoundInclusive))
    {
        Error (_T("Couldn't parse command line."));
        goto cleanAndReturn;
    }

    DWORD dwNumSysIntrsFree = 0;

    if (inKernelMode() &&
        !getNumSysIntrsFree (dwIRQ, dwUpperBoundInclusive, &dwNumSysIntrsFree))
    {
        goto cleanAndReturn;
    }

    DWORD dwMaxIrqPerSysIntr = 0;

    if (!findMaxIrqPerSysIntr (dwIRQ, dwUpperBoundInclusive, &dwMaxIrqPerSysIntr,
                 TRUE))
    {
        goto cleanAndReturn;
    }

    if (inKernelMode() &&
        !checkNumSysIntrsFree (dwIRQ, dwUpperBoundInclusive, dwNumSysIntrsFree))
    {
        goto cleanAndReturn;
    }

    returnVal = TPR_PASS;

cleanAndReturn:

    return (returnVal);
}

/*
 * start from the largest value and work down.  Return the first IRQ
 * that we can successfully allocate.
 */
eReturnMaxSuccessfullyAllocatedIrq
findMaxSuccessfullyAllocatedIrq (DWORD dwIRQBegin, DWORD dwIRQEnd,
                 DWORD * dwMax)
{
    if (dwIRQBegin > dwIRQEnd || !dwMax)
    {
        return (FATAL_ERROR);
    }

    /* assume couldn't find it */
    eReturnMaxSuccessfullyAllocatedIrq returnVal = MISSED_IT;

    DWORD dwIRQ = dwIRQEnd;
    DWORD dwLowerBoundInclusive = dwIRQBegin;

    /*
   * use just rolled to catch the case in which
   * dwUpperBoundInclusive == 0
   */
    for (BOOL bJustRolled = FALSE;
       dwIRQ >= dwLowerBoundInclusive && !bJustRolled; dwIRQ--)
    {
        DWORD dwSysIntr;

        /* do the allocation.  If fail then we need to go to the next value */
        if (getSysIntr (SPECIAL_OAL_INTR_STANDARD, &dwIRQ, 1, &dwSysIntr))
        {
            if (!checkSysIntrRange (dwSysIntr))
            {
                returnVal = FATAL_ERROR;
                goto cleanAndReturn;
            }

            if (!releaseSysIntr (dwSysIntr))
            {
                Error (_T("Couldn't release sysintr %u"), dwSysIntr);
                returnVal = FATAL_ERROR;
                goto cleanAndReturn;
            }

            /* found first one that successfully allocates */
            *dwMax = dwIRQ;
            returnVal = FOUND_IT;
            break;
        }

        /* if we just worked on 0 then want to bail */
        bJustRolled = (dwIRQ == 0);
    }

cleanAndReturn:

    return (returnVal);

}

/*
 * find the max IRQ per sysintr allowed on the system.
 *
 * The first two values provide the irq range, inclusive.
 * pdwMaxIrqPerSysIntr returns the max IRQ per sysintr on the system.
 *
 * Function returns true if the max is found.  False if the parameters
 * are bad or it can't find the value.
 */
BOOL
findMaxIrqPerSysIntr (DWORD dwIRQBegin, DWORD dwIRQEnd,
              DWORD * pdwMaxIrqPerSysIntr, BOOL bPrint)
{
    BOOL returnVal = FALSE;

    /* MAX_POSSIBLE_IRQ_PER_SYSINTR check comes from for loop */
    if ((dwIRQBegin > dwIRQEnd) || (!pdwMaxIrqPerSysIntr))
    {
        return (FALSE);
    }

    DWORD * pdwIRQBuffer = NULL;

    /*
   * reduce the dwIRQEnd value if we can.  This will help the random
   * guessing routine get better irq values.
   */
    {
        DWORD dwReducedIRQEnd;

        switch (findMaxSuccessfullyAllocatedIrq (dwIRQBegin, dwIRQEnd,
                         &dwReducedIRQEnd))
        {
        case FOUND_IT:
            dwIRQEnd = dwReducedIRQEnd;
            if (bPrint)
            {
                Info (_T("Appears that the max IRQ value is %u.  Using this."),
                    dwIRQEnd);
            }
            break;
        case MISSED_IT:
            /* do nothing */
            break;
        case FATAL_ERROR:
            /* fatal error - bailing out */
            goto cleanAndReturn;
        default:
            IntErr (_T("Wrong return from findMaxSuccessfullyAllocatedIrq"));
            goto cleanAndReturn;
        }
    }

    DWORD dwMaxPossIRQPerSysIntr = MAX_POSSIBLE_IRQ_PER_SYSINTR;

    /* structure has two leading DWORDs and then the IRQ values */
    DWORD dwBufferSizeBytes = (2 + dwMaxPossIRQPerSysIntr) * sizeof (DWORD);

    if (dwBufferSizeBytes < dwMaxPossIRQPerSysIntr)
    {
        /* we overflowed */
        Error (_T("Overflowed when calculating needed memory."));
        goto cleanAndReturn;
    }

    pdwIRQBuffer = (DWORD *) malloc (dwBufferSizeBytes);

    if (!pdwIRQBuffer)
    {
        Error (_T("Couldn't get memory."));
        goto cleanAndReturn;
    }

    DWORD dwSupportedIRQs = 1;

    /* for each number of possible supported IRQs */
    for (; dwSupportedIRQs <= dwMaxPossIRQPerSysIntr; dwSupportedIRQs++)
    {
        DWORD dwNumTries = 100;

        if (bPrint)
        {
            Info (_T("Testing %u IRQs."), dwSupportedIRQs);
        }

        BOOL bStillLooking = TRUE;
        /* try this many times to get a result */
        while ((dwNumTries != 0) && bStillLooking)
        {
            /* load the irq array */
            for (DWORD dwPos = 0; dwPos < dwSupportedIRQs; dwPos++)
            {
                pdwIRQBuffer[dwPos] = getRandomInRange (dwIRQBegin, dwIRQEnd);
            }

            if (bPrint)
            {
                Info (_T("IRQ list is %s"),
                collateIRQs (pdwIRQBuffer, dwSupportedIRQs));
            }

            DWORD dwSysIntr = 0;

            if (getSysIntr (OAL_INTR_DYNAMIC, pdwIRQBuffer,
                  dwSupportedIRQs, &dwSysIntr))
            {
                if (!checkSysIntrRange (dwSysIntr))
                {
                    goto cleanAndReturn;
                }

                /*
                 * successfully allocated, so we can support at least
                 * this many irqs
                 */
                bStillLooking = FALSE;

                if (!releaseSysIntr (dwSysIntr))
                {
                    Error (_T("Couldn't release sysintr %u."), dwSysIntr);
                    goto cleanAndReturn;
                }
            }
            else
            {
                /*
                 * it could be that we picked the wrong IRQs.  decrement
                 * vars and try again.
                 */
                dwNumTries--;
                if ((dwNumTries) && (bPrint))
                {
                    Info (_T("%u tries left for max value of %u."),
                    dwNumTries, dwSupportedIRQs);
                }
            }
        } /* while */

        if (bStillLooking)
        {
            /*
             * we ran out of tries and still didn't find it.  Since we
             * are counting up this is the first value that didn't allow
             * us to allocate this many IRQs.  It is the one that we are
             * looking for.
             */
            Info (_T(""));
            Info (_T("The max number of IRQs per SYSINTR is %u."),
            dwSupportedIRQs - 1);

            /*
             * if dwSupportedIRQs is 1, then something is funky, since
             * we should always be able to allocate at least one IRQ.
             * this will never underflow.
             */
            *pdwMaxIrqPerSysIntr = dwSupportedIRQs - 1;

            break;
        }

    } /* for */

    if (dwSupportedIRQs > dwMaxPossIRQPerSysIntr)
    {
        Error (_T("Couldn't find the max possible IRQ per Sysintr.  Tried ")
         _T("[1, %u]."), dwMaxPossIRQPerSysIntr);
        goto cleanAndReturn;
    }


    returnVal = TRUE;
cleanAndReturn:

    if (pdwIRQBuffer)
    {
        free (pdwIRQBuffer);
    }

    return (returnVal);
}



/********  Allocation Tests **********************************************/

/*
 * find the max successfully allocated IRQ.  Calls helper function above.
 */
TESTPROCAPI  testFindMaxSuccessfullyAllocatedIrq(
                         UINT uMsg,
                         TPPARAM tpParam,
                         LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_FAIL;

    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        returnVal =  TPR_NOT_HANDLED;
        goto cleanAndReturn;
    }

    breakIfUserDesires ();

    printDisclaimer ();

    Info (_T("This test tries to find the maximum allowed IRQ on the"));
    Info (_T("system.  This assumes that this value exists and that it"));
    Info (_T("is less than the starting point."));

    /* the ranges supported by the test, inclusive */
    DWORD dwIRQBegin = TEST_IRQ_BEGIN;
    DWORD dwIRQEnd = TEST_IRQ_END;

    if (lpFTE->dwUserData & IN_RANGE)
    {
        /* adjust the ranges if need be */
        if (!handleCmdLineIRQRange (&dwIRQBegin, &dwIRQEnd))
        {
            Error (_T("Couldn't parse command line."));
            goto cleanAndReturn;
        }
    }
    else
    {
        Info (_T("Ignoring user supplied IRQ params (if given).  Use the"));
        Info (_T("in range tests to confirm that the IOCTLs are returning"));
        Info (_T("the correct return values for IRQs within the OALs"));
        Info (_T("supported range."));
        Info (_T(""));
    }

    DWORD dwNumSysIntrsFree = 0;

    if (inKernelMode() &&
        !getNumSysIntrsFree (dwIRQBegin, dwIRQEnd, &dwNumSysIntrsFree))
    {
        goto cleanAndReturn;
    }

    DWORD dwMaxIrq = 0;

    switch (findMaxSuccessfullyAllocatedIrq (dwIRQBegin, dwIRQEnd,
                       &dwMaxIrq))
    {
    case FOUND_IT:
        Info (_T("Largest possible IRQ in [%u, %u] is %u."),
        dwIRQBegin, dwIRQEnd, dwMaxIrq);
        break;
    case MISSED_IT:
        Info (_T("Largest possible IRQ in [%u, %u] is %u."),
        dwIRQBegin, dwIRQEnd, dwIRQEnd);
        break;
    case FATAL_ERROR:
        Error (_T("Fatal error when finding largest IRQ."));
        goto cleanAndReturn;
        break;
    default:
        IntErr (_T("Wrong return from findMaxSuccessfullyAllocateIrq."));
        goto cleanAndReturn;
    }

    if (inKernelMode() &&
        !checkNumSysIntrsFree (dwIRQBegin, dwIRQEnd, dwNumSysIntrsFree))
    {
        goto cleanAndReturn;
    }

    returnVal = TPR_PASS;

cleanAndReturn:

    return (returnVal);
}

/*
 * This test allocates a given set of irqs.  Successfully returned
 * sysintrs and immediately freed.
 *
 * irq range is support through command line options.
 */
TESTPROCAPI
allocateAll_SingleIRQ(
              UINT uMsg,
              TPPARAM tpParam,
              LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_FAIL;

    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return (TPR_NOT_HANDLED);
    }

    breakIfUserDesires ();

    DWORD dwAllocationType = 0;
    const TCHAR * szName = NULL;
    BOOL bAllowedToFail = TRUE;
    BOOL bAllowedToPass = TRUE;

    printDisclaimer ();

    /* the ranges supported by the test, inclusive */
    DWORD dwIRQBegin = TEST_IRQ_BEGIN;
    DWORD dwIRQEnd = TEST_IRQ_END;

    if( (lpFTE->dwUserData & MUST_FAIL) && inKernelMode() ) {
        Info (_T("Test is expecting to fail while running in KernelMode!") );
        Info (_T("This test should not be running with the -n option") );
        Info (_T("Skipping this test. Please see Usage.") );
        return TPR_SKIP;
    }
    else if( !(lpFTE->dwUserData & MUST_FAIL) && !inKernelMode() ) {
        Info (_T("You are running this test in usermode.") );
        Info (_T("Many interrupt operations will fail from usermode.") );
        Info (_T("Please read the documentation for this test.") );
        // fall through and try anyways
    }

    if (!handleIrqRange (lpFTE->dwUserData, &dwIRQBegin, &dwIRQEnd,
               &bAllowedToPass))
    {
        goto cleanAndReturn;
    }

    /*
   * this allows for us to both setup the szName and error check the
   * inbound params
   */
    switch (lpFTE->dwUserData)
    {
    case (SPECIAL_OAL_INTR_STANDARD | IN_RANGE):
        dwAllocationType = SPECIAL_OAL_INTR_STANDARD;
        szName = _T("standard");
        bAllowedToFail = FALSE;
        break;
    case (OAL_INTR_STATIC | IN_RANGE):
        Info (_T("The static IRQs currently enabled on the system can be"));
        Info (_T("determined from the data below by looking for IRQs that"));
        Info (_T("the test can't allocate.  These IRQs are currently statically"));
        Info (_T("allocated."));
        dwAllocationType = OAL_INTR_STATIC;
        szName = _T("OAL_INTR_STATIC");
        bAllowedToFail = TRUE;
        break;
    case (OAL_INTR_DYNAMIC | IN_RANGE):
        dwAllocationType = OAL_INTR_DYNAMIC;
        szName = _T("OAL_INTR_DYNAMIC");
        bAllowedToFail = FALSE;
        break;
    case (SPECIAL_OAL_INTR_STANDARD | MUST_FAIL):
        dwAllocationType = SPECIAL_OAL_INTR_STANDARD;
        szName = _T("standard");
        bAllowedToFail = TRUE;
        break;
    case OAL_INTR_STATIC | MUST_FAIL:
        Info (_T("The static IRQs currently enabled on the system can be"));
        Info (_T("determined from the data below by looking for IRQs that"));
        Info (_T("the test can't allocate.  These IRQs are currently statically"));
        Info (_T("allocated."));
        dwAllocationType = OAL_INTR_STATIC;
        szName = _T("OAL_INTR_STATIC");
        bAllowedToFail = TRUE;
        break;
    case OAL_INTR_DYNAMIC | MUST_FAIL:
        dwAllocationType = OAL_INTR_DYNAMIC;
        szName = _T("OAL_INTR_DYNAMIC");
        bAllowedToFail = TRUE;
        break;
    case (SPECIAL_OAL_INTR_STANDARD):
        dwAllocationType = SPECIAL_OAL_INTR_STANDARD;
        szName = _T("standard");
        bAllowedToFail = TRUE;
        break;
    case OAL_INTR_STATIC:
        Info (_T("The static IRQs currently enabled on the system can be"));
        Info (_T("determined from the data below by looking for IRQs that"));
        Info (_T("the test can't allocate.  These IRQs are currently statically"));
        Info (_T("allocated."));
        dwAllocationType = OAL_INTR_STATIC;
        szName = _T("OAL_INTR_STATIC");
        bAllowedToFail = TRUE;
        break;
    case OAL_INTR_DYNAMIC:
        dwAllocationType = OAL_INTR_DYNAMIC;
        szName = _T("OAL_INTR_DYNAMIC");
        bAllowedToFail = TRUE;
        break;
    default:
        Error (_T("Unsupported interrupt allocation type."));
        return (TPR_FAIL);
    }

    DWORD dwNumSysIntrsFree = 0;

    if (inKernelMode() &&
        !getNumSysIntrsFree (dwIRQBegin, dwIRQEnd, &dwNumSysIntrsFree))
    {
        goto cleanAndReturn;
    }

    if (!runAllocateAll_SingleIRQ (dwAllocationType, szName,
                 dwIRQBegin, dwIRQEnd,
                 bAllowedToFail, bAllowedToPass))
    {
        goto cleanAndReturn;
    }

    if (inKernelMode() &&
        !checkNumSysIntrsFree (dwIRQBegin, dwIRQEnd, dwNumSysIntrsFree))
    {
        goto cleanAndReturn;
    }

    returnVal = TPR_PASS;

cleanAndReturn:

    return (returnVal);
}


/*
 * This test allocates a given set of irqs.  Successfully returned
 * sysintrs are immediately freed.
 *
 * allocation type is one of the interrupt allocation schemes
 * specified in the documentation.  See #defines for more info.
 * szTypeName is the friendly human readable name for this allocation
 * scheme.  bAllowedToFail specifies whether the allocation is allowed
 * to fail for this test.  If this value is false then the allocation
 * must succeed or the test will fail.
 *
 * return true on success and false otherwise.
 */
BOOL
runAllocateAll_SingleIRQ (DWORD dwAllocationType,
              const TCHAR szTypeName[],
              DWORD dwIRQBegin, DWORD dwIRQEnd,
              BOOL bAllowedToFail, BOOL bAllowedToPass)
{
    BOOL returnVal = FALSE;

    Info (_T("This test tries to allocate SYSINTRs for the given set of IRQs."));
    Info (_T("These SYSINTRs are immediately freed."));
    Info (_T(""));
    Info (_T("These IRQs will be allocated as %s."), szTypeName);
    Info (_T(""));

    DWORD dwIRQ = dwIRQBegin;
    DWORD dwUpperBoundInclusive = dwIRQEnd;

    Info (_T(""));
    Info (_T("Checking [%u, %u]."), dwIRQ, dwUpperBoundInclusive);

    /* let the test run for as long as possible */
    BOOL bRet = TRUE;

    /*
   * use just rolled to catch the case in which
   * dwUpperBoundInclusive == DWORD_MAX
   */
    for (BOOL bJustRolled = FALSE;
       dwIRQ <= dwUpperBoundInclusive && !bJustRolled; dwIRQ++)
    {
        DWORD dwSysIntr;

        /* do the allocation.  If succeed then everything is fine */
        if (getSysIntr (dwAllocationType, &dwIRQ, 1, &dwSysIntr))
        {
            Info (_T("Allocated SYSINTR %u to IRQ %u"), dwSysIntr, dwIRQ);

            if (!bAllowedToPass)
            {
                Error (_T("IOCTL call succeeded and it shouldn't have."));
                bRet = FALSE;
            }

            if (!checkSysIntrRange (dwSysIntr))
            {
                bRet = FALSE;
            }

            if (!releaseSysIntr (dwSysIntr))
            {
                Error (_T("Couldn't release sysintr %u"), dwSysIntr);
                bRet = FALSE;
            }
        }
        else if (!bAllowedToFail)
        {
            /*
             * this type of allocation should always succeed (for
             * instance dynamic)
             */
            Error (_T("Allocation failed for IRQ %u."), dwIRQ);
            bRet = FALSE;
        }

        /* if we just worked on DWORD_MAX then want to bail */
        bJustRolled = (dwIRQ == DWORD_MAX);
    }

    Info (_T(""));

    if (bRet)
    {
        Info (_T("Test passed."));
    }
    else
    {
        Info (_T("Test failed."));
        goto cleanAndReturn;
    }

    returnVal = TRUE;

cleanAndReturn:

    return (returnVal);
}

/*
 * Test random allocations on multiple IRQs.
 */
TESTPROCAPI
allocateRand_MultIRQ(
             UINT uMsg,
             TPPARAM tpParam,
             LPFUNCTION_TABLE_ENTRY lpFTE)
{
    INT returnVal = TPR_FAIL;

    /* only supporting executing the test */
    if (uMsg != TPM_EXECUTE)
    {
        return (TPR_NOT_HANDLED);
    }

    breakIfUserDesires ();

    DWORD dwAllocationType = 0;
    /* underlying object can't change, but pointer can */
    const TCHAR * szName = NULL;
    BOOL bAllowedToFail = TRUE;
    BOOL bAllowedToPass = TRUE;

    /* the ranges supported by the test, inclusive */
    DWORD dwIRQBegin = TEST_IRQ_BEGIN;
    DWORD dwIRQEnd = TEST_IRQ_END;

    if (!handleIrqRange (lpFTE->dwUserData, &dwIRQBegin, &dwIRQEnd,
               &bAllowedToPass))
    {
        goto cleanAndReturn;
    }

    /*
   * this allows for us to both setup the szName and error check the
   * inbound params
   */
    switch (lpFTE->dwUserData)
    {
    case (OAL_INTR_STATIC | IN_RANGE):
        dwAllocationType = OAL_INTR_STATIC;
        szName = _T("OAL_INTR_STATIC");
        bAllowedToFail = TRUE;
        break;
    case (OAL_INTR_DYNAMIC | IN_RANGE):
        dwAllocationType = OAL_INTR_DYNAMIC;
        szName = _T("OAL_INTR_DYNAMIC");
        bAllowedToFail = FALSE;
        break;
    case OAL_INTR_FORCE_STATIC | IN_RANGE:
        dwAllocationType = OAL_INTR_FORCE_STATIC;
        szName = _T("OAL_INTR_FORCE_STATIC");
        bAllowedToFail = FALSE;   /* this value is not used */
        break;
    case OAL_INTR_STATIC | MUST_FAIL:
        dwAllocationType = OAL_INTR_STATIC;
        szName = _T("OAL_INTR_STATIC");
        bAllowedToFail = TRUE;
        break;
    case OAL_INTR_DYNAMIC | MUST_FAIL:
        dwAllocationType = OAL_INTR_DYNAMIC;
        szName = _T("OAL_INTR_DYNAMIC");
        /* allowed to fail because could be out of irq range */
        bAllowedToFail = TRUE;
        break;
    case OAL_INTR_FORCE_STATIC | MUST_FAIL:
        dwAllocationType = OAL_INTR_FORCE_STATIC;
        szName = _T("OAL_INTR_FORCE_STATIC");
        bAllowedToFail = FALSE;   /* this value is not used */
        break;
    case OAL_INTR_STATIC:
        dwAllocationType = OAL_INTR_STATIC;
        szName = _T("OAL_INTR_STATIC");
        bAllowedToFail = TRUE;
        break;
    case OAL_INTR_DYNAMIC:
        dwAllocationType = OAL_INTR_DYNAMIC;
        szName = _T("OAL_INTR_DYNAMIC");
        /* allowed to fail because could be out of irq range */
        bAllowedToFail = TRUE;
        break;
    case OAL_INTR_FORCE_STATIC:
        dwAllocationType = OAL_INTR_FORCE_STATIC;
        szName = _T("OAL_INTR_FORCE_STATIC");
        bAllowedToFail = FALSE;   /* this value is not used */
        break;
    default:
        Error (_T("Unsupported interrupt allocation type."));
        return (TPR_FAIL);
    }

    DWORD dwNumSysIntrsFree = 0;

    if (inKernelMode() &&
        !getNumSysIntrsFree (dwIRQBegin, dwIRQEnd, &dwNumSysIntrsFree))
    {
        goto cleanAndReturn;
    }


    Info (_T("Allocation type for this test is %s."), szName);

    printDisclaimer ();

    DWORD dwMaxIrqPerSysIntr = 0;

    BOOL bMaxIrqOnCmdLine = FALSE;

    if (g_szDllCmdLine != NULL)
    {
        cTokenControl tokens;

        /* break up command line into tokens */
        if (!tokenize ((TCHAR *)g_szDllCmdLine, &tokens))
        {
            Error (_T("Couldn't parse command line."));
            goto cleanAndReturn;
        }

        if (getOptionDWord (&tokens, ARG_STR_MAX_IRQ_PER_SYSINTRS,
              &dwMaxIrqPerSysIntr))
        {
            Info (_T("User specified max irq per sysintrs, which is %u."),
            dwMaxIrqPerSysIntr);
            bMaxIrqOnCmdLine = TRUE;
        }
    }

    if (!bMaxIrqOnCmdLine)
    {
        if (bAllowedToPass)
        {
            /* guess if user doesn't tell us this */
            Info (_T("Guessing how many IRQs can be allocated to each sysIntr."));

            if (!findMaxIrqPerSysIntr (dwIRQBegin, dwIRQEnd, &dwMaxIrqPerSysIntr,
                     FALSE))
            {
                goto cleanAndReturn;
            }
        }
        else
        {
            dwMaxIrqPerSysIntr = MAX_POSSIBLE_IRQ_PER_SYSINTR;
            Info (_T("We are not allowed to pass, so determining the max number"));
            Info (_T("of IRQs per SYSINTR will fail.  Using a default value of %u."),
            dwMaxIrqPerSysIntr);
        }
    }

    /* at this point dwMaxIrqPerSysIntr has been initialized */

    if (dwMaxIrqPerSysIntr <= 1)
    {
        Error (_T("This test is applicable only if the platform supports two or"));
       Error (_T("more irqs per sysintr. Skipping the test."));
        returnVal = TPR_SKIP;
        goto cleanAndReturn;
    }

 if (dwAllocationType == OAL_INTR_FORCE_STATIC)
    {
        /*
         * this is special cased because it has special contraints on which IRQs
         * it can play with.
         */
        if (!runAllocateRandForceStatic_MultIRQ (dwIRQBegin, dwIRQEnd,
                           dwMaxIrqPerSysIntr,
                           bAllowedToPass))
        {
            goto cleanAndReturn;
        }
    }
    else
    {
        if (!runAllocateRand_MultIRQ (dwAllocationType, bAllowedToFail,
                    bAllowedToPass,
                   dwIRQBegin, dwIRQEnd, dwMaxIrqPerSysIntr))
        {
            goto cleanAndReturn;
        }
    }


    if (inKernelMode() &&
        !checkNumSysIntrsFree (dwIRQBegin, dwIRQEnd, dwNumSysIntrsFree))
    {
        goto cleanAndReturn;
    }

    returnVal = TPR_PASS;

cleanAndReturn:

    return (returnVal);

}


BOOL
runAllocateRand_MultIRQ (DWORD dwAllocationType, BOOL bAllowedToFail,
             BOOL bAllowedToPass,
             DWORD dwIRQBegin, DWORD dwIRQEnd,
             DWORD dwMaxIrqPerSysIntr)
{
    BOOL returnVal = FALSE;
    DWORD * dwIRQs = NULL;

    /* dwMaxIrqPerSysIntr checks for loop overflow */
    if (dwIRQBegin > dwIRQEnd || dwMaxIrqPerSysIntr == 0 ||
        dwMaxIrqPerSysIntr == DWORD_MAX)
    {
        return (FALSE);
    }

    DWORD dwMaxIrqPerSysIntrArraySizeBytes;

    if (DWordMult (dwMaxIrqPerSysIntr, sizeof (DWORD),
         &dwMaxIrqPerSysIntrArraySizeBytes) != S_OK)
    {
        Error (_T("Overflow."));
        goto cleanAndReturn;
    }

    /* allocate array */
    dwIRQs =  (DWORD *) malloc (dwMaxIrqPerSysIntrArraySizeBytes);

    if (!dwIRQs)
    {
        Error (_T("Couldn't allocate memory for IRQs."));
        goto cleanAndReturn;
    }

    Info (_T(""));
    Info (_T("Randomly selecting values between [%u, %u]."), dwIRQBegin, dwIRQEnd);

    /* let the test run for as long as possible */
    BOOL bRet = TRUE;

    for (DWORD dwIrqsPerSysIntrs = 1; dwIrqsPerSysIntrs <= dwMaxIrqPerSysIntr;
       dwIrqsPerSysIntrs++)
    {
        Info (_T(""));
        Info (_T("Working on Irqs per SYSINTRs = %u"), dwIrqsPerSysIntrs);

        /* run each for a given number of times */
        for (DWORD dwIt = 0; dwIt < MULT_ALLOC_TEST_IT; dwIt++)
        {
            DWORD dwSysIntr;

            /* load array with IRQs */
            for (DWORD dwPos = 0; dwPos < dwIrqsPerSysIntrs; dwPos++)
            {
                /* duplicate IRQs are allowed */
                dwIRQs[dwPos] = getRandomInRange (dwIRQBegin, dwIRQEnd);
            }

            /* do the allocation.  If succeed then everything is fine */
            if (getSysIntr (dwAllocationType, dwIRQs, dwIrqsPerSysIntrs,
              &dwSysIntr))
            {
                if (!bAllowedToPass)
                {
                    Error (_T("IOCTL call succeeded and it shouldn't have."));
                    Error (_T("IRQ(s) are: %s"),
                        collateIRQs (dwIRQs, dwIrqsPerSysIntrs));
                    bRet = FALSE;
                }

                if (!checkSysIntrRange (dwSysIntr))
                {
                    bRet = FALSE;
                }

                if (!releaseSysIntr (dwSysIntr))
                {
                    Error (_T("Couldn't release sysintr %u"), dwSysIntr);
                    Error (_T("IRQ(s) are: %s"),
                        collateIRQs (dwIRQs, dwIrqsPerSysIntrs));
                    bRet = FALSE;
                }
                else
                {
                    Info (_T("Sucessfully allocated and freed SYSINTR %u. ")
                        _T("IRQs %s"),
                        dwSysIntr,
                        collateIRQs (dwIRQs, dwIrqsPerSysIntrs));
                }

                if (!allocateRand_checkDuplicateIRQs (dwIRQs, dwIrqsPerSysIntrs,
                            dwAllocationType,
                            bAllowedToFail,
                            bAllowedToPass))
                {
                    bRet = FALSE;
                }

            }
            else if (!bAllowedToFail)
            {
                /*
                 * this type of allocation should always succeed (for
                 * instance dynamic)
                 */
                Error (_T("Allocation failed."));
                Error (_T("IRQ(s) are: %s"),
                    collateIRQs (dwIRQs, dwIrqsPerSysIntrs));
                bRet = FALSE;
            }
        }
    }

    returnVal = TRUE;

cleanAndReturn:

    if (dwIRQs)
    {
        free (dwIRQs);
    }

    return (returnVal);
}

/*
 * call this with an array of IRQs.  Set up allowed to fail and allowed to pass
 * based from what you know about these IRQs (will their allocate always work?).
 *
 * It will take the first two irqs in the array and duplicate them alternately in
 * a copy of the array of length dwLen.  The code then makes the call, and figures
 * out if the result is what the user wanted.
 *
 * If the result is correct, you get true.  Else, you get false.
 */
BOOL
allocateRand_checkDuplicateIRQs (const DWORD *const dwIRQsOrig, DWORD dwLen,
                 DWORD dwAllocationType,
                 BOOL bAllowedToFail,
                 BOOL bAllowedToPass)
{
    if (!dwIRQsOrig || dwLen <= 1 || (DWORD_MAX / sizeof (DWORD)) > dwLen)
    {
        return (FALSE);
    }

    BOOL returnVal = FALSE;

    /**** make local copy */
    DWORD * dwIRQs = (DWORD *) malloc (dwLen * sizeof (DWORD));

    if (!dwIRQs)
    {
        Error (_T("Couldn't allocate memory."));
        goto cleanAndReturn;
    }

    /*
   * put duplicate IRQs in array.  Duplicate first two for how many
   * spots we have.  Note that in the two IRQ case we special case
   * so as to still duplicate IRQs.
   */
    if (dwLen == 2)
    {
        dwIRQs[0] = dwIRQsOrig[0];
        dwIRQs[1] = dwIRQsOrig[0];
    }
    else
    {
        for (DWORD dwPos = 0; dwPos < dwLen; dwPos++)
        {
            if (dwPos % 2)
            {
                dwIRQs[dwPos] = dwIRQsOrig[0];
            }
            else
            {
                dwIRQs[dwPos] = dwIRQsOrig[1];
            }
        }
    }

    DWORD dwSysIntr = 0;

    /* do the allocation.  If succeed then everything is fine */
    if (getSysIntr (dwAllocationType, dwIRQs, dwLen, &dwSysIntr))
    {
        if (!bAllowedToPass)
        {
            Error (_T("IOCTL call succeeded and it shouldn't have."));
            Error (_T("IRQ(s) are: %s"),
                collateIRQs (dwIRQs, dwLen));
            goto cleanAndReturn;
        }

        if (!checkSysIntrRange (dwSysIntr))
        {
            goto cleanAndReturn;
        }

        if (!releaseSysIntr (dwSysIntr))
        {
            Error (_T("Couldn't release sysintr %u"), dwSysIntr);
            Error (_T("IRQ(s) are: %s"),
             collateIRQs (dwIRQs, dwLen));
            goto cleanAndReturn;
        }
        else
        {
            Info (_T("Sucessfully allocated and freed SYSINTR %u. ")
            _T("IRQs %s"),
            dwSysIntr,
            collateIRQs (dwIRQs, dwLen));
        }
    }
    else if (!bAllowedToFail)
    {
        /*
         * this type of allocation should always succeed (for
         * instance dynamic)
         */
        Error (_T("Allocation failed."));
        Error (_T("IRQ(s) are: %s"),
         collateIRQs (dwIRQs, dwLen));
        goto cleanAndReturn;
    }

    returnVal = TRUE;

cleanAndReturn:

    if (dwIRQs)
    {
        free (dwIRQs);
    }

    return (returnVal);
}

/*
 * This is special cased since FORCE_STATIC is quite powerful
 */
BOOL
runAllocateRandForceStatic_MultIRQ (DWORD dwIRQBegin, DWORD dwIRQEnd,
                   DWORD dwMaxIrqPerSysIntr,
                    BOOL bAllowedToPass)
{
    BOOL returnVal = FALSE;
    DWORD * dwIRQs = NULL;

    if (dwIRQBegin > dwIRQEnd || dwMaxIrqPerSysIntr == 0 ||
        dwMaxIrqPerSysIntr == DWORD_MAX)
    {
        return (FALSE);
    }

    DWORD dwMaxIrqPerSysIntrArraySizeBytes;

    if (DWordMult (dwMaxIrqPerSysIntr, sizeof (DWORD),
         &dwMaxIrqPerSysIntrArraySizeBytes) != S_OK)
    {
        Error (_T("Overflow."));
        goto cleanAndReturn;
    }

    /* allocate array */
    dwIRQs =  (DWORD *) malloc (dwMaxIrqPerSysIntrArraySizeBytes);

    if (!dwIRQs)
    {
        Error (_T("Couldn't allocate memory for IRQs."));
        goto cleanAndReturn;
    }

    Info (_T(""));
    Info (_T("Randomly selecting values between [%u, %u]."), dwIRQBegin, dwIRQEnd);

    /* let the test run for as long as possible */
    BOOL bRet = TRUE;

    for (DWORD dwIrqsPerSysIntrs = 1; dwIrqsPerSysIntrs <= dwMaxIrqPerSysIntr;
       dwIrqsPerSysIntrs++)
    {
        Info (_T(""));
        Info (_T("Working on Irqs per SYSINTRs = %u"), dwIrqsPerSysIntrs);

        /* how many irq sets we tested */
        DWORD dwTestedNIRQSets = 0;

        /* run each for a given number of times */
        for (DWORD dwIt = 0; dwIt < MULT_ALLOC_TEST_IT; dwIt++)
        {
            DWORD dwSysIntrOrig, dwSysIntrForceStatic;

            /* load array with IRQs */
            for (DWORD dwPos = 0; dwPos < dwIrqsPerSysIntrs; dwPos++)
            {
                dwIRQs[dwPos] = getRandomInRange (dwIRQBegin, dwIRQEnd);
            }

            /* do the allocation.  If succeed then everything is fine */
            if (!getSysIntr (OAL_INTR_STATIC, dwIRQs, dwIrqsPerSysIntrs,
                  &dwSysIntrOrig))
            {
                /*
                 * one of these IRQs is already marked as static.
                 * Forcing it as static again could crash the system, so
                 * skipping it.
                 */
                Info (_T("Can't force static on this irq set: %s"),
                    collateIRQs (dwIRQs, dwIrqsPerSysIntrs));
                continue;
            }

            if (!bAllowedToPass)
            {
                Error (_T("We shouldn't have been able to force static and we")
                    _T("could."));
                Error (_T("IRQs: %s"), collateIRQs (dwIRQs, dwIrqsPerSysIntrs));
                goto cleanAndReturn;
            }

            if (!checkSysIntrRange (dwSysIntrOrig))
            {
                goto cleanAndReturn;
            }

            dwTestedNIRQSets++;

            /* at this point we have a static map to this IRQ set */

            /*
             * force static should kick off this static map and give us another.
             * this means that the sysintr will have changed.
             * old sysintr now becomes dynamic.
             */
            if (!getSysIntr (OAL_INTR_FORCE_STATIC, dwIRQs, dwIrqsPerSysIntrs,
              &dwSysIntrForceStatic))
            {
                /* we can't force static.  something is wrong. */
                Error (_T("We can't force static.  IRQs: %s"),
                    collateIRQs (dwIRQs, dwIrqsPerSysIntrs));

                goto cleanAndReturn;
            }

            if (dwSysIntrForceStatic == dwSysIntrOrig)
            {
                Error (_T("The force static call should have allocated a ")
                    _T("new sysintr."));
                Error (_T("It did not.  SYSINTR is %u."), dwSysIntrOrig);
                goto cleanAndReturn;
            }

            /* we need to free all sysintrs allocated */

            BOOL bPrintDebug = FALSE;

            /* release the sysintrs */
            if (!releaseSysIntr (dwSysIntrOrig))
            {
                Error (_T("Couldn't release sysintr %u"), dwSysIntrOrig);
                bPrintDebug = TRUE;
            }

            if (!releaseSysIntr (dwSysIntrForceStatic))
            {
                Error (_T("Couldn't release sysintr %u"), dwSysIntrForceStatic);
                bPrintDebug = TRUE;
            }

            if (bPrintDebug)
            {
                Error (_T("Orig sysintr = %u"), dwSysIntrOrig);
                Error (_T("Force static sysintr = %u"), dwSysIntrForceStatic);
                Error (_T(""));
                Error (_T("Couldn't release, so exiting..."));
                goto cleanAndReturn;
            }
        } /* for dwIt */

        /*
       * if we are setup to fail for every case then we don't want to
       * trigger this error condition.
       */
        if ((dwTestedNIRQSets < 1) && (bAllowedToPass))
        {
            Info (_T(""));
            Info (_T("Warning: Didn't find any IRQ sets to test for %u IRQs."),
                dwIrqsPerSysIntrs);
            Info (_T("This can result when the set of IRQs being used for the"));
            Info (_T("test is a good deal larger than valid set of IRQs on the"));
            Info (_T("device.  Each randomly allocated set of IRQs will contain"));
            Info (_T("one that is out of range, and the routines will not allow"));
            Info (_T("the allocation to proceed."));
            Info (_T("The test will pass in this case, since other test cases"));
            Info (_T("cover this functionality.  This message just alerts you"));
            Info (_T("that this condition was reached in the code."));
        }
        else if (bAllowedToPass)
        {
            Info (_T(""));
            Info (_T("Found %u IRQ sets to test for %u IRQs."),
            dwTestedNIRQSets, dwIrqsPerSysIntrs);
        }

    } /* for number of sysinters in the array */

    if (!bRet)
    {
        goto cleanAndReturn;
    }

    returnVal = TRUE;

cleanAndReturn:

    if (dwIRQs)
    {
        free (dwIRQs);
    }

    return (returnVal);
}


/******************************************************************************/
/*                                                                            */
/*                            Helper  Functions                               */
/*                                                                            */
/******************************************************************************/

/*
 * algorithm: Allocate every SYSINTR on the system.  When we are done,
 * count how many we allocated.  This was the number free to start with
 */
BOOL
findHowManySysIntrsAreFree (DWORD dwIRQBegin, DWORD dwIRQEnd,
                DWORD * pdwNumFree, BOOL bPrintDebug)
{
    if (!pdwNumFree || dwIRQBegin > dwIRQEnd)
    {
        return (FALSE);
    }

    typedef struct
    {
        DWORD dwRC;         // return code
    } sTrackSysIntrs;

    sTrackSysIntrs pWhichSysIntrsToRelease[SYSINTR_MAXIMUM];

    /* initialize array */
    for (DWORD dwPos = 0; dwPos < SYSINTR_MAXIMUM; dwPos++)
    {
        pWhichSysIntrsToRelease[dwPos].dwRC = FALSE;
    }

    DWORD dwIRQ = dwIRQBegin;

    BOOL bRet = TRUE;

    for (BOOL bJustRolled = FALSE;
       dwIRQ <= SYSINTR_MAXIMUM && !bJustRolled; dwIRQ++)
    {
        DWORD dwSysIntr;

        BOOL rc;

        rc = getSysIntr (dwIRQ, &dwSysIntr);
        if(!rc)
        {
            continue;
        }

        /* if out of range go to next irq */
        if (!checkSysIntrRange (dwSysIntr) || dwSysIntr >= SYSINTR_MAXIMUM)
        {
            bRet = FALSE;
            continue;
        }

        /*
       * we are now sure that an array access wouldn't cause a buffer
       * overflow
       */

        if (pWhichSysIntrsToRelease[dwSysIntr].dwRC)
        {
            Error (_T("SYSINTR %u has already been allocated to IRQ %u."),
                dwSysIntr, dwIRQ);
            Error (_T("Routines just allocated it to IRQ %u as well."),
                dwIRQ);
            bRet = FALSE;
            continue;
        }

        /* sysIntr is clear.  Record that we got it back. */
        pWhichSysIntrsToRelease[dwSysIntr].dwRC = TRUE;

        if (bPrintDebug)
        {
            Info (_T("Test allocated SYSINTR %u to IRQ %u"), dwSysIntr, dwIRQ);
        }

        /* if we just worked on DWORD_MAX then want to bail */
        bJustRolled = (dwIRQ == DWORD_MAX);

    } /* for */

        /* count how many sysintrs we allocated.*/
        DWORD dwCount = 0;

        for (DWORD dwSysIntr = 0; dwSysIntr < SYSINTR_MAXIMUM; dwSysIntr++)
        {
            if (pWhichSysIntrsToRelease[dwSysIntr].dwRC)
            {
                dwCount++;
            }
        }

    /* release them - these should succeed */
    for (DWORD dwSysIntr = 0; dwSysIntr < SYSINTR_MAXIMUM; dwSysIntr++)
    {
        /* only work on SYSINTERs that were successfully allocated */
        if (!pWhichSysIntrsToRelease[dwSysIntr].dwRC)
        {
            continue;
        }

        if (!releaseSysIntr (dwSysIntr))
        {
            Error (_T("Error!  Weren't able to release SYSINTR %u."), dwSysIntr);
            bRet = FALSE;
        }
    }

    if (bRet)
    {
        *pdwNumFree = dwCount;
    }

    return (bRet);
}


/*
 * handle the different modes for the irq range.
 *
 * The irq range varies, depending on whether we are working IN_RANGE, and
 * whether we are given command line args.  If not, we want to guess which
 * values to use.
 *
 * This function is needed because everything is platform dependent.  The test
 * has to guess at every value on the system before it can run.  some testers
 * will be able to tell it the values.  Other won't, and we have to support
 * this case as well.
 *
 * dwUserData is the parameter from the tux function able.  Everything else is
 * pointers to parameters used for the test.
 */
BOOL
handleIrqRange (DWORD dwUserData, DWORD * pdwArgIRQBegin, DWORD * pdwArgIRQEnd,
        BOOL * pbAllowedToPass)
{
    if (!pdwArgIRQBegin || !pdwArgIRQEnd || !pbAllowedToPass)
    {
        return (FALSE);
    }

    BOOL returnVal = FALSE;

    /* use these locally and copy over outbound args only if everything works */
    DWORD dwIRQBegin = *pdwArgIRQBegin;
    DWORD dwIRQEnd = *pdwArgIRQEnd;

    if (dwUserData & IN_RANGE)
    {
        DWORD dwIRQBeginOrig = dwIRQBegin;
        DWORD dwIRQEndOrig = dwIRQEnd;

        /* adjust the ranges if need be */
        if (!handleCmdLineIRQRange (&dwIRQBegin, &dwIRQEnd))
        {
            Error (_T("Couldn't parse command line."));
            goto cleanAndReturn;
        }

        if ((dwIRQBeginOrig == dwIRQBegin) && (dwIRQEndOrig == dwIRQEnd))
        {
            Info (_T("Command line params were either not given or the same"));
            Info (_T("as the defaults.  Guessing at the largest IRQ on the"));
            Info (_T("system."));
            Info (_T(""));

            DWORD dwMaxIRQ = 0;


            switch (findMaxSuccessfullyAllocatedIrq (dwIRQBegin, dwIRQEnd,
                           &dwMaxIRQ))
            {
            case FATAL_ERROR:
                Error (_T("Fatal error calculating largest possible IRQ."));
                goto cleanAndReturn;
            case MISSED_IT:
                Info (_T("Couldn't find largest possible IRQ.  Using defaults."));
                dwIRQBegin = dwIRQBeginOrig;
                dwIRQEnd = dwIRQEndOrig;
                break;
            case FOUND_IT:
                dwIRQBegin = dwIRQBeginOrig;
                dwIRQEnd = dwMaxIRQ;
                Info (_T("Successfully found largest possible IRQ.  New bounds ")
                _T("are [%u, %u]."), dwIRQBegin, dwMaxIRQ);
                break;
            default:
                IntErr (_T("Wrong return from findMaxSuccessfullyAllocatedIrq"));
                goto cleanAndReturn;
            }
        }
    }
    else
    {
        Info (_T("Ignoring user supplied IRQ params (if given).  Use the"));
        Info (_T("in range tests to confirm that the IOCTLs are returning"));
        Info (_T("the correct return values for IRQs within the OALs"));
        Info (_T("supported range."));
        Info (_T(""));
    }

    if (dwUserData & MUST_FAIL)
    {
        Info (_T("No IOCTL calls are allowed to pass for this test."));
        Info (_T(""));
        *pbAllowedToPass = FALSE;
    }

    /* return correctly munged values to the user */
    *pdwArgIRQBegin = dwIRQBegin;
    *pdwArgIRQEnd = dwIRQEnd;

    returnVal = TRUE;

cleanAndReturn:

    return (returnVal);
}


BOOL
getNumSysIntrsFree (DWORD dwIRQBegin, DWORD dwIRQEnd,
            DWORD * pdwNumSysIntrsFree)
{
    if (dwIRQBegin > dwIRQEnd || !pdwNumSysIntrsFree || !inKernelMode())
    {
        Error (_T("bad args or not in kernel mode."));
        return (FALSE);
    }

    BOOL rc = findHowManySysIntrsAreFree (dwIRQBegin, dwIRQEnd,
                    pdwNumSysIntrsFree, FALSE);

    if (rc)
    {
        Info (_T("  ** Sanity check:  %u SYSINTRs free"), *pdwNumSysIntrsFree);
    }

    return (rc);
}

BOOL
checkNumSysIntrsFree (DWORD dwIRQBegin, DWORD dwIRQEnd,
              DWORD dwNumSysIntrsFree)
{
    if (dwIRQBegin > dwIRQEnd || !inKernelMode())
    {
        Error (_T("bad args or not in kernel mode."));
        return (FALSE);
    }

    DWORD dwNewNumSysIntrsFree = (DWORD)-1;

    BOOL bRet = findHowManySysIntrsAreFree (dwIRQBegin, dwIRQEnd,
                      &dwNewNumSysIntrsFree, FALSE);

    if (dwNewNumSysIntrsFree != dwNumSysIntrsFree)
    {
        Error (_T("We started with %u SYSINTRs free and ended up with %u ")
         _T("free."), dwNumSysIntrsFree, dwNewNumSysIntrsFree);
        Error (_T("This sanity check failed.  Failing the test."));
        bRet = FALSE;
    }

    return (bRet);
}

