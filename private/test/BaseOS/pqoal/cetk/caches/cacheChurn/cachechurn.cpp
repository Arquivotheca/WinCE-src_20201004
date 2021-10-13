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

/* common utils for oal tests */
#include "..\..\..\common\commonUtils.h"

/* functions exported by this file */
#include "cacheChurn.h"

/* random number generation functions for the cache tests */
#include "cacheRand.h"

/* functions to handle breaking into the debugger on failure */
#include "breakForDebug.h"


/*****************************************************************************/
/*                                                                           */
/*                                Implementation                             */
/*                                                                           */
/*****************************************************************************/

/*
 * main loop for the program.  this run the test.  it has the logic for
 * setting up the blocks, and then manipulating them.  the actual allocations,
 * read, writes, and frees happen in sub-functions.
 *
 * return true is if found no errors and false otherwise (found errors, bad
 * params, etc).  on false you get many error messages helping you to debug.
 *
 * dwNumOfBlocks is the number of blocks.  dwPageSize is the page size to
 * be used by the test.  This value doesn't have to be the page size on the
 * system.  this code does not set that requirement, and will still work
 * if these two values are different.  dwPageSize == 0 is not allowed, since
 * virtual alloc doesn't handle this, and it doesn't make sense in light of
 * the test.
 *
 * ullIterations is the number of iterations.  pStats is the stat structure,
 * which can't be null (this will cause fail as a return val) and is filled
 * with statistics on successful return.
 *
 * bPrintStats tells you whether you want the stats printed by this function
 * (amoung other messages).
 *
 * Info printing for this function works as follows:  any fatal error is
 * printed, no matter what the value of bPrintStats is currently set to.
 * This function also prints useful debugging stuff to the user.  We want
 * this data on the actual test run, but not on the calibration runs (it will
 * just confuse the use in this case).  We print this useful data only if
 * bPrintStats is true.
 *
 * Why does all of this matter?  Calibration runs the function for short
 * periods of time and extrapolates how long it will take on the actual run.
 * This involves two different code paths into this function.  Sometimes,
 * the compiler is smart enough to separate these into two different chunks
 * of assembly, and then remove the if statements that are always false in
 * the bPrintStats == FALSE case.
 *
 * This will skew the calibration only if these if statements are run many
 * many times.  This will occur if they are in the main loop.  So, if you
 * ever reference bPrintStats in the main for loop that drives the test
 * (init code is all right) then be careful that you don't skew the
 * calibration.  Realize, also, that future compiler changes might cause
 * skewed calibration as well.
 */
BOOL iterateBlocks (
            DWORD dwNumOfBlocks,
            DWORD dwPageSize,
            ULONGLONG ullIterations,
            sOverallStatistics * pStats,
            ULONGLONG ullPrintAfterNIt,
            BOOL bPrintStats)
{
    if (!dwNumOfBlocks || !dwPageSize || !pStats)
    {
        return FALSE;
    }

    BOOL returnVal = FALSE;

    /* figure out how much memory we need for the block array */
    DWORD dwTotalMemBytes = 0;
    sBlock * pBlocks = NULL;

    if (DWordMult (dwNumOfBlocks, sizeof (sBlock), &dwTotalMemBytes) != S_OK)
    {
        Error (_T("Rollover when calculating memory needs."));
        goto cleanAndReturn;
    }

    /* allocate mem for block array */
    pBlocks = (sBlock *) malloc (dwTotalMemBytes);

    if (!pBlocks)
    {
        Error (_T("Couldn't allocate %u bytes.  GetLastError = %u"),
            dwTotalMemBytes, GetLastError());
        goto cleanAndReturn;
    }

    /*
    * initialize the pdwData pointer to null, to handle easy error
    * tracking.
    *
    * on error during the true initialization (see below) we need to jump to the
    * cleanAndReturn label.  we need to make sure that the block
    * array is in a known state, so that we can free mem without leaking
    * it.  we must set all of the pdwData pointers to null here, and
    * then init.
    */
    for (DWORD dwPos = 0; dwPos < dwNumOfBlocks; dwPos++)
    {
        pBlocks[dwPos].pdwData = NULL;
    }

    /* we don't want to keep stats for init.  We do need to set the
    * curr mem used, though, because its initial value is the curr
    * mem used, not zero.  we will set it as we set the values below.
    */
    pStats->ullCurrMemoryUsed = 0;

    BOOL bFoundErrorDuringInit = FALSE;

    /*
    * initialize blocks
    *
    * note that we don't start keeping stats at this point.
    */
    for (DWORD dwPos = 0; dwPos < dwNumOfBlocks; dwPos++)
    {
        /* saves typing */
        sBlock * pThisBlock = pBlocks + dwPos;

        /*
        * if even then initialize with an actual block.  if odd
        * then set to empty
        */
        if (dwPos % 2 == 0)
        {
            /* set block to empty first. */
            if (!SetBlockEmpty (pThisBlock))
            {
                Error (_T("Something went wrong when emptying block %u."),
                    dwPos);
                Error (_T("This is not expected on init, so failing the test."));
                bFoundErrorDuringInit = TRUE;
                break;
            }

            /*
            * use the allocate function.  if this fails then something
            * is wrong.  This will set all of the values in the block
            * as needed.
            */
            if (!AllocateBlock (pThisBlock, dwPageSize))
            {
                Error (_T("Something went wrong while allocating block %u."),
                    dwPos);
                Error (_T("This is not expected on init, so failing the test."));
                bFoundErrorDuringInit = TRUE;
                break;
            }

            pStats->ullCurrMemoryUsed++;
        }
        else
        {
            /* set block as empty */
            if (!SetBlockEmpty (pThisBlock))
            {
                Error (_T("Something went wrong when emptying block %u."),
                    dwPos);
                Error (_T("This is not expected on init, so failing the test."));
                bFoundErrorDuringInit = TRUE;
                break;
            }
        }

    }

    if (bFoundErrorDuringInit)
    {
        /*
        * init will leave the blocks array is a "good" state on failure.
        * this is why we set all of the data pointers to null before init.
        */
        if (!CheckAllBlocksForErrors (pBlocks, dwNumOfBlocks))
        {
            Error (_T("Error scanning other blocks for bad values."));

            /* we are already in an error condition, so don't set error code */
        }

        goto cleanAndReturn;
    }

    /* blocks are ready at this point */

    /*
    * set up statistics gathering.  note that we start gathering stats
    * after the array is initialized.
    */

    pStats->ullTotalIterations = 0;
    pStats->ullTotalAllocations = 0;
    pStats->ullTotalFrees = 0;
    pStats->ullTotalReadCheck_zero = 0;
    pStats->ullTotalReadCheck_nonZero = 0;
    pStats->ullTotalWrite_zero = 0;
    pStats->ullTotalWrite_nonZero = 0;
    pStats->ullRunningTotalCurrMemoryUsed = 0;

    BOOL bWeFoundAnError = FALSE;

    for (ULONGLONG ullCurrIt = 0;
        ullCurrIt < ullIterations && !bWeFoundAnError; ullCurrIt++)
    {

        /* choose block */
        /* these are inclusive ranges */
        DWORD dwCurrBlock = cacheRandInRange (0, dwNumOfBlocks - 1);

        /* saves typing */
        sBlock * pThisBlock = pBlocks + dwCurrBlock;

        if (!pThisBlock->pdwData)
        {
            /* it is empty */
            if (!AllocateBlock (pThisBlock, dwPageSize))
            {
                bWeFoundAnError = TRUE;
                break;
            }

            /* update statistics */
            pStats->ullTotalAllocations++;
            pStats->ullCurrMemoryUsed++;
        }
        else
        {
            /* it has data */

            /* choose which operation */
            DWORD dwOp = cacheRandInRange (0, 99);

            if (dwOp < DW_WRITE_THRESHOLD)
            {
                /* write operation */

                BOOL bWasZeroed = pThisBlock->bIsZeroed;

                if (!WriteBlock (pThisBlock))
                {
                    bWeFoundAnError = TRUE;
                    break;
                }

                /* update statistics */
                if (bWasZeroed)
                {
                    pStats->ullTotalWrite_zero++;
                }
                else
                {
                    pStats->ullTotalWrite_nonZero++;
                }
            }
            else if (dwOp < (DW_WRITE_THRESHOLD + DW_READ_CHECK_THRESHOLD))
            {
                /* read_check operation */

                BOOL bWasZeroed = pThisBlock->bIsZeroed;

                if (!ReadBlock(pThisBlock))
                {
                    bWeFoundAnError = TRUE;
                    break;
                }

                /* update statistics */
                if (bWasZeroed)
                {
                    pStats->ullTotalReadCheck_zero++;
                }
                else
                {
                    pStats->ullTotalReadCheck_nonZero++;
                }

            }
            else if (dwOp < (DW_WRITE_THRESHOLD + DW_READ_CHECK_THRESHOLD + DW_FREE_THRESHOLD))
            {
                /* free it */

                if (!FreeBlock (pThisBlock))
                {
                    bWeFoundAnError = TRUE;
                    break;
                }

                pStats->ullTotalFrees++;

                if (pStats->ullCurrMemoryUsed == 0)
                {
                    IntErr (_T("Memory used cannot be negative."));
                    goto cleanAndReturn;
                }

                pStats->ullCurrMemoryUsed--;

            }
            else
            {
                /*
                * we should never get here, assuming the thresholds are set
                * correctly.
                */
                IntErr (_T("Thresholds are set incorrectly.  Should never")
                    _T(" get here."));
                goto cleanAndReturn;
            }

        }

        /*
        * grab overall stats on memory usage and iterations.
        * we record them here since we know that we succeeded on this
        * iteration.
        */
        pStats->ullRunningTotalCurrMemoryUsed += pStats->ullCurrMemoryUsed;
        pStats->ullTotalIterations++;

        /*
        * if we are printing stats and we want to print check points and
        * not the first iteration and we are at a check point
        */
        if (bPrintStats && ullPrintAfterNIt && pStats->ullTotalIterations &&
            (pStats->ullTotalIterations % ullPrintAfterNIt == 0))
        {
            Info (_T("Check point:  At iteration %I64u."), pStats->ullTotalIterations);
        }

    } // for loop


    if (bWeFoundAnError)
    {
        /* look for other values that might be incorrect */
        /* this call could possible generate a huge amount of output */

        Error (_T(""));
        Error (_T("We found an error."));
        Error (_T("Looking for all errors in the allocated memory."));
        Error (_T("This could potentially generate a huge amount of output."));
        Error (_T(""));

        /* this will return false only on a fatal error.  if it finds bad
        * values it still returns true.  at this point we already know that
        * something is amiss.  we just want to walk through, looking for
        * other oddities.
        */
        if (CheckAllBlocksForErrors (pBlocks, dwNumOfBlocks))
        {
            Error (_T("Error scanning other blocks for bad values."));

            /* we are already in an error condition, so don't set error code */
        }

        Info (_T(""));
        Info (_T("Dumping the stats that we have collected up to this point."));
        Info (_T("These don't include the operation that encountered the error."));
        Info (_T(""));

        if (!DumpStats (pStats, dwNumOfBlocks, dwPageSize))
        {
            Error (_T("Error dumping stats."));

            /* we are already in an error condition, so don't set error code */
        }

        /*
        * returnVal is already set to false (per standard error handling semantics),
        * so don't set it as such here
        */

        goto cleanAndReturn;
    }
    else
    {
        /* we succeeded */
        if (bPrintStats)
        {
            Info (_T(""));
            Info (_T("Test run was successful!"));
            Info (_T(""));

            Info (_T("Notes on statistics:"));
            Info (_T("1) \"Total Allocations\" might not equal \"Total Frees\"."));
            Info (_T("Init and cleanup operations aren't included in these numbers."));
            Info (_T("2) \"Sum of all of these\" should equal \"Total Iterations\""));
            Info (_T("3) The percentage reported for \"Average memory allocated\" is"));
            Info (_T("   relative to the memory available for the test, not the"));
            Info (_T("   maximum system memory available."));
            Info (_T(""));

            Info (_T("Statistics gathered for this test run:"));

            if (!DumpStats (pStats, dwNumOfBlocks, dwPageSize))
            {
                Error (_T("Error dumping stats."));

                /* we had succeeded, but something went wrong.  bail out. */
                goto cleanAndReturn;
            }
        }
    }

    /* at this point we succeeded.  If we failed, we won't get here */

    returnVal = TRUE;


cleanAndReturn:

    /* free memory if need to */
    if (pBlocks)
    {
        for (DWORD dwPos = 0; dwPos < dwNumOfBlocks; dwPos++)
        {
            /* for debugging.  make sure that we are hitting all of the blocks. */
            if (bPrintStats)
            {
                DumpPerBlockData (pBlocks + dwPos, dwPos);
            }

            if (pBlocks[dwPos].pdwData)
            {
                VirtualFree (pBlocks[dwPos].pdwData, 0, MEM_RELEASE);

                pBlocks[dwPos].pdwData = NULL;
            }
        }

        free (pBlocks);
    }

    return (returnVal);

}


/*
* set a block as empty.
*
* this should be used on init only.  This
* set everything in the block as empty.  It clears out all
* of the stats.
*
* we ignore the size, etc., when pdwData is null, but we still
* set these here for completeness.
*
* true on success and false otherwise.
*/
BOOL SetBlockEmpty (sBlock * pBlock)
{
    if (!pBlock)
    {
        Error (_T("Wrong args to SetBlockEmpty."));
        return (FALSE);
    }

    pBlock->pdwData = NULL;
    pBlock->dwSizeDwords = 0;
    pBlock->bIsZeroed = 0;
    pBlock->dwSalt = 0;

#ifdef GATHER_BLOCK_STATS
    pBlock->dwAllocations = 0;
    pBlock->dwFrees = 0;
    pBlock->dwReads = 0;
    pBlock->dwWrites = 0;
#endif
    return (TRUE);
}


/*
* Allocate a block of memory.
*
* pBlock is a non-null ptr to the block that we are allocating.  We
* assume that this block is free and we change this block.
* pBlock->pdwData must be null on input.  if not, we return false.
*
* The pageSize is the size of the mem to be allocated.  it needs to
* be a multiple of the size of a dword.
*
* Allocation is done with virtual alloc.  It must be freed with
* virtualfree or the mem will leak.  we will have to prefast suppress
* the warning generated for this function by prefast.
*
* on success, pBlock->pdwData points to the memory allocated.  this
* memory is guanteed zero filled (we check it).  bIsZeroed is true.
* dwSalt is zero.  dwAllocations is one greater (we don't check for
* overflow and we don't increase this on failure).  The other values
* are untouched.
*
* Failure modes:
* bad input params
* couldn't allocate memory
* memory allocated wasn't zero filled
*
* IMPORTANT::::
*
* If the pBlock->pdwData is not-null then there is allocated memory
* at this address that MUST be freed.  If you don't free this you leak.
* Note that this can be true even if this function returns FALSE.  In
* this case the function failed on the zero filled check and is
* returning the mem to you for further analysis.
*
* if this function fails in the zero check then bIsZeroed is still
* set to true.  this tells you that it is supposed to zero filled and
* isn't.
*/

BOOL AllocateBlock (sBlock * pBlock, DWORD dwPageSize)
{
    BOOL returnVal = FALSE;
    DWORD * pdwMem = NULL;

    /* pBlock has to exist and it must be empty and dwPageSize can't be zero */
    if (!pBlock || pBlock->pdwData || !dwPageSize)
    {
        Error (_T("Wrong params to AllocateBlock."));
        return (FALSE);
    }

    if (dwPageSize % sizeof (DWORD) != 0)
    {
        /* programs assumes DWORDs.  If we are asked to allocate
        * some fractional size the bail out.
        */
        Error (_T("dwPageSize is not a multiple of a DWORD (%u bytes)."),
            sizeof (DWORD));
        goto cleanAndReturn;
    }

    DWORD dwNumOfElements = dwPageSize / sizeof (DWORD);

    /* allocate mem */
    pdwMem = (DWORD *) VirtualAlloc(NULL, dwPageSize, MEM_COMMIT, PAGE_READWRITE);

    if (!pdwMem)
    {
        Error (_T("Couldn't allocate %u bytes.  GetLastError is %u."),
            dwPageSize, GetLastError());
        goto cleanAndReturn;
    }

    /* set this here, so that even if we fail on the zero fill check
    * we return this to the user to free.
    */
    pBlock->pdwData = pdwMem;
    pBlock->dwSizeDwords = dwNumOfElements;
    pBlock->bIsZeroed = TRUE;
    pBlock->dwSalt = 0;  /* not valid in this case, but set anyhow */

    /* check for zero fill.  bail on first one. */
    for (DWORD dwPos = 0; dwPos < dwNumOfElements; dwPos++)
    {
        /* read it here in case it changes between reads.  read it once
        * so that we can actually tell the user on error what happened.
        */
        DWORD dwTemp = pdwMem[dwPos];

        if (dwTemp != 0)
        {
            /* we break before printing out because the printing op,
            * if done through kitl or kato, will probaly cause a context
            * switch.  If the user is using this feature, then they
            * want to be notified immediately.
            */
            breakOnFailureIfRequested ();

            /* we found one that isn't zero filled */
            /* print out the ptr as a hex val, always 8 chars long */
            Error (_T("Incorrect value on zero fill check:  ")
                _T("expected: %u  got: %u   VM ptr: 0x%x"),
                0, dwTemp, (DWORD) (pdwMem + dwPos));

            goto cleanAndReturn;
        }
    }

#ifdef GATHER_BLOCK_STATS
    /* we allocated, so increase count */
    pBlock->dwAllocations++;
#endif

    returnVal = TRUE;

cleanAndReturn:

    /* if we fail, we don't want to free the mem, because the caller might want
    * to look for other failures.  in this case we just return.
    */

    return (returnVal);

}

/*
* free a block.
*
* inbound block must have pdwData value pointing to memory allocated with
* vitualAlloc.  if it is null, you get an error.
*
* on sucessful exist, value has been freed.  pdwData is set to null.  size,
* bIsZeroed, and salt are all zero.  dwFrees is updated.
*/
BOOL FreeBlock (sBlock * pBlock)
{
    if (!pBlock)
    {
        Error (_T("Wrong params to FreeBlock."));
        return (FALSE);
    }

    /* track this differently, since it might mean that something went wrong
    * with the caches.
    */
    if (!pBlock->pdwData)
    {
        Error (_T("Trying to free empty block."));
        return (FALSE);
    }

    BOOL returnVal = FALSE;

    if (!VirtualFree(pBlock->pdwData, 0, MEM_RELEASE))
    {
        Error (_T("Error freeing block.  ptr: 0x%x  GetLastError = %u"),
            (DWORD) (pBlock->pdwData), GetLastError ());
        goto cleanAndReturn;
    }

    pBlock->pdwData = NULL;

    /* by convention, when pdwData are null, others are ignored.
    * Set to zero for easier debugging. */
    pBlock->dwSizeDwords = 0;
    pBlock->bIsZeroed = FALSE;
    pBlock->dwSalt = 0;

#ifdef GATHER_BLOCK_STATS
    /* record stats */
    pBlock->dwFrees++;
#endif

    returnVal = TRUE;

cleanAndReturn:

    return (returnVal);


}


/*
* write a block
*
* pBlock can't be null and must not be empty.  we overwrite what is
* already in the block and write the whole block.  pBlock is updated
* with the new salt on return.
*/
BOOL WriteBlock (sBlock * pBlock)
{
    if (!pBlock || !pBlock->pdwData)
    {
        Error (_T("Wrong args or block was empty in function WriteBlock."));
        return (FALSE);
    }

    /* choose salt */
    DWORD dwSalt = cacheRand();

    /* write hashes of the index into the block */
    for (DWORD dwPos = 0; dwPos < pBlock->dwSizeDwords; dwPos++)
    {
        pBlock->pdwData[dwPos] = hashIndex (dwPos, dwSalt);
    }

    pBlock->dwSalt = dwSalt;
    pBlock->bIsZeroed = FALSE;

#ifdef GATHER_BLOCK_STATS
    pBlock->dwWrites++;
#endif

    return (TRUE);
}


/*
* read a block
*
* pBlock must not be null and must have data (can't be empty).
*
* we read a given number of times, as defined by DO_READ_BLOCK_PERCENTAGE.
* See the define for more info.
*
* if you read a data value that isn't correct, the test prints out
* information about what it expected and what it didn't receive.  At this
* point is exist, without reading any further values.
*
* false means we encountered a bad value or bad params.  true means that
* everything worked.
*/
BOOL ReadBlock (const sBlock * pBlock)
{
    BOOL returnVal = FALSE;

    if (!pBlock || !pBlock->pdwData)
    {
        Error (_T("Wrong args or block was empty in function WriteBlock."));
        return (FALSE);
    }

    /* read only a percentage of the blocks (and, if you hit duplicates because of
    * random reading, this will be smaller even still)
    */
    DWORD dwReadIts = (DWORD) (((double) pBlock->dwSizeDwords) * DO_READ_BLOCK_PERCENTAGE);

    if (pBlock->bIsZeroed)
    {
        /* we expect all zeros.  read randomly and confirm */

        for (DWORD dwIt = 0; dwIt < dwReadIts; dwIt++)
        {
            /* this is inclusive */
            DWORD dwPos = cacheRandInRange (0, pBlock->dwSizeDwords - 1);

            /* only read once */
            DWORD dwActual = pBlock->pdwData[dwPos];
            if (0 != dwActual)
            {
                /* we found a bad value */

                /* we break before printing out because the printing op,
                * if done through kitl or kato, will probaly cause a context
                * switch.  If the user is using this feature, then they
                * want to be notified immediately.
                */
                breakOnFailureIfRequested ();

                /* print out the ptr as a hex val, always 8 chars long */
                Error (_T("Incorrect value on read check (zero):  ")
                    _T("expected: %u  got: %u  ptr: 0x%x"),
                    0, dwActual, (DWORD) (pBlock->pdwData + dwPos));

                goto cleanAndReturn;
            }
        }


    }
    else
    {
        /* values should match hashed values.  read randomly and confirm. */

        for (DWORD dwIt = 0; dwIt < dwReadIts; dwIt++)
        {
            /* this is inclusive */
            DWORD dwPos = cacheRandInRange (0, pBlock->dwSizeDwords - 1);

            DWORD dwHash = hashIndex (dwPos, pBlock->dwSalt);

            /* only read once */
            DWORD dwActual = pBlock->pdwData[dwPos];
            if (dwHash != dwActual)
            {
                /* we found a bad value */

                /* we break before printing out because the printing op,
                * if done through kitl or kato, will probaly cause a context
                * switch.  If the user is using this feature, then they
                * want to be notified immediately.
                */
                breakOnFailureIfRequested ();

                /* print out the ptr as a hex val, always 8 chars long */
                Error (_T("Incorrect value on read check (non-zero):  ")
                    _T("expected: %u  got: %u  salt: %u  ptr: 0x%x"),
                    dwHash, dwActual, pBlock->dwSalt,
                    (DWORD) (pBlock->pdwData + dwPos));

                goto cleanAndReturn;
            }
        }

    }

#ifdef GATHER_BLOCK_STATS
    pBlock->dwReads++;
#endif

    returnVal = TRUE;

cleanAndReturn:

    return (returnVal);
}


/*
* dump stat structure for the user.
*/
BOOL DumpStats (const sOverallStatistics * pStats, DWORD dwNumOfBlocks, DWORD dwPageSize)
{
    if (dwNumOfBlocks == 0)
    {
        Error (_T("Zero number of blocks passed to DumpStats."));
        return (FALSE);
    }

    ULONGLONG ullTotalSum = pStats->ullTotalAllocations + pStats->ullTotalFrees + pStats->ullTotalReadCheck_zero +
        pStats->ullTotalReadCheck_nonZero + pStats->ullTotalWrite_zero + pStats->ullTotalWrite_nonZero;

    /* make it easier to change the field width.  This concated to the end
    * of the string by the CPP.  Valid for this function only.
    */
#define SZ_PERCENT_FIELD _T("%.1f%%")

    if (ullTotalSum > 0)
    {
        Info (_T("Total Iterations:   %I64u"), pStats->ullTotalIterations);
        Info (_T(""));
        Info (_T("Total Allocations:  %I64u ops   ") SZ_PERCENT_FIELD,
            pStats->ullTotalAllocations, (double) pStats->ullTotalAllocations / (double) ullTotalSum * 100.0);
        Info (_T("Total Frees:  %I64u ops   ") SZ_PERCENT_FIELD,
            pStats->ullTotalFrees, (double) pStats->ullTotalFrees / (double) ullTotalSum * 100.0);
        Info (_T("Total Read_Check (zero):  %I64u   ") SZ_PERCENT_FIELD,
            pStats->ullTotalReadCheck_zero, (double) pStats->ullTotalReadCheck_zero / (double) ullTotalSum * 100.0);
        Info (_T("Total Read_Check (non-zero):  %I64u   ") SZ_PERCENT_FIELD,
            pStats->ullTotalReadCheck_nonZero, (double) pStats->ullTotalReadCheck_nonZero / (double) ullTotalSum * 100.0);
        Info (_T("Total Write (zero):  %I64u   ") SZ_PERCENT_FIELD,
            pStats->ullTotalWrite_zero, (double) pStats->ullTotalWrite_zero / (double) ullTotalSum * 100.0);
        Info (_T("Total Write (non-zero):  %I64u   ") SZ_PERCENT_FIELD,
            pStats->ullTotalWrite_nonZero, (double) pStats->ullTotalWrite_nonZero / (double) ullTotalSum * 100.0);
    }
    else
    {
        /* slight change that we could have rolled over, so still print values.
        * more likely we just didn't run anything.
        */
        Info (_T("Total Iterations:   %I64u"), pStats->ullTotalIterations);
        Info (_T(""));
        Info (_T("Total Allocations:  %I64u ops   "), pStats->ullTotalAllocations);
        Info (_T("Total Frees:  %I64u ops   "), pStats->ullTotalFrees);
        Info (_T("Total Read_Check (zero):  %I64u   "), pStats->ullTotalReadCheck_zero);
        Info (_T("Total Read_Check (non-zero):  %I64u   "), pStats->ullTotalReadCheck_nonZero);
        Info (_T("Total Write (zero):  %I64u   "), pStats->ullTotalWrite_zero);
        Info (_T("Total Write (non-zero):  %I64u   "), pStats->ullTotalWrite_nonZero);
    }

    Info (_T("Sum of all of these:  %I64u"), ullTotalSum);

    Info (_T(""));

    double doAveragePagesUsed = (double) pStats->ullRunningTotalCurrMemoryUsed / (double) pStats->ullTotalIterations;

    Info (_T("Average memory allocated:  %g pages = %g bytes = ") SZ_PERCENT_FIELD _T(" of available."),
        doAveragePagesUsed, doAveragePagesUsed * (double) dwPageSize,
        doAveragePagesUsed / (double) dwNumOfBlocks * 100.0);
#undef SZ_PERCENT_FIELD

    return (TRUE);
}

/*
* work through all allocated blocks, checking for errors.  print these
* error to the screen for further debugging.
*
* This function has the potential to generate huge amounts of output.
*
* return true on success.  Success is defined as working through all of
* the blocks without crashing.  Even if the function encounters errors
* in the block, it will still consider this success.
*
* if not errors are found, the function will print a message telling
* the user of this.  If errors are found, each is printed on its own
* line, with debugging data.
*/
BOOL CheckAllBlocksForErrors (const sBlock * pBlocks, DWORD dwNumOfBlocks)
{
    BOOL returnVal = TRUE;

    if (!pBlocks)
    {
        Error (_T("Wrong args to function CheckAllBlocksForErrors."));
        return (FALSE);
    }

    BOOL bFoundError = FALSE;

    for (DWORD dwBlock = 0; dwBlock < dwNumOfBlocks; dwBlock++)
    {
        const sBlock * pThisBlock = pBlocks + dwBlock;

        if (pThisBlock->pdwData)
        {
            /* block has some data */

            if (pThisBlock->bIsZeroed)
            {
                /* looking for all zeros */
                for (DWORD dwPos = 0; dwPos < pThisBlock->dwSizeDwords; dwPos++)
                {

                    /* only read once */
                    DWORD dwActual = pThisBlock->pdwData[dwPos];

                    if (0 != dwActual)
                    {
                        /* we found a bad value */
                        Error (_T("Incorrect value: block: %4u  pos: %4u  ")
                            _T("expected: %10u  got: %10u  ptr: 0x%x"), dwBlock, dwPos,
                            0, dwActual, (DWORD) (pThisBlock->pdwData + dwPos));

                        bFoundError = TRUE;
                    }
                }
            }
            else
            {
                /* looking for values matching the hash */
                for (DWORD dwPos = 0; dwPos < pThisBlock->dwSizeDwords; dwPos++)
                {
                    DWORD dwHash = hashIndex (dwPos, pThisBlock->dwSalt);

                    /* only read once */
                    DWORD dwActual = pThisBlock->pdwData[dwPos];

                    if (dwHash != dwActual)
                    {
                        /* we found a bad value */

                        Error (_T("Incorrect value: block: %4u  pos: %4u  ")
                            _T("expected: %10u  got: %10u  ptr: 0x%x"), dwBlock, dwPos,
                            dwHash, dwActual, (DWORD) (pThisBlock->pdwData + dwPos));

                        bFoundError = TRUE;
                    }
                }

            }
        }
    }

    if (!bFoundError)
    {
        Info (_T("Didn't find any errors in any of the blocks."));
    }

    return (returnVal);
}

/*
* dump stats per block.
*/
void DumpPerBlockData (const sBlock * pBlock, DWORD dwBlockNum)
{
    if (!pBlock)
    {
        return;
    }
#ifdef GATHER_BLOCK_STATS
    Info (_T("Block num: %4u  Alloc: %8u  Frees: %8u  Reads:  %8u  Writes: %8u"),
        dwBlockNum, pBlock->dwAllocations, pBlock->dwFrees, pBlock->dwReads, pBlock->dwWrites);
#endif
}
