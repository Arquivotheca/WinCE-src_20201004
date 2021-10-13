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

#ifndef __CACHE_CHURN_HH__
#define __CACHE_CHURN_HH__



/*
 * we don't gather per block stats because it takes a good deal of
 * memory (especially on large caches).  For debugging, though, this
 * is useful (at least to confirm that the test code is working correctly;
 * as for debugging cache failures, I can see it less useful).  To
 * turn it on, set this define.
 *
 * we can't just use compiler opts to remove the relavant code paths
 * because this is a space issue, not a time issue.  The compiler won't
 * strip uneeded values from structs, so we need to phyically remove
 * them from the code to get the cost savings.
 */
#undef GATHER_BLOCK_STATS

/*
* a structure representing a block
*
* a block has a pointer to the memory for this block and
* a bunch of meta data.
*/
typedef struct _sBlock
{
    /* actual data.  if this is null we ignore the other non-stat values. */
    DWORD * pdwData;
    DWORD dwSizeDwords;     /* size of the block */

    DWORD bIsZeroed;  /* true if data is all zeros, false otherwise */
    DWORD dwSalt;    /* salt value. only valid if the data isn't all zeros */

#ifdef GATHER_BLOCK_STATS
    /* for stat gathering only */
    DWORD dwAllocations;
    DWORD dwFrees;
    DWORD dwReads;
    DWORD dwWrites;
#endif
} sBlock;


/*
 * defines functions exported by cacheChurn.cpp.  These include the meat
 * of the test code.
 */
typedef struct _sOverallStatistics
{
  ULONGLONG ullTotalIterations; /* for entire test */

  ULONGLONG ullTotalAllocations; /* for entire test */
  ULONGLONG ullTotalFrees;       /* for entire test */

  ULONGLONG ullTotalReadCheck_zero; /* total read / check on zero data */
  ULONGLONG ullTotalReadCheck_nonZero; /* total read / check on non-zero data */

  ULONGLONG ullTotalWrite_zero; /* total time we write on zero data */
  ULONGLONG ullTotalWrite_nonZero; /* total writes on non-zero data */

  ULONGLONG ullCurrMemoryUsed; /* mem snapshot at this given instance */
  /*
   * running total of the snap shot values.  these are taken after each operation.
   * this value allows us to compute the average memory usage, which tells
   * the pressure on the caches
   */
  ULONGLONG ullRunningTotalCurrMemoryUsed;
} sOverallStatistics;


/*
* these values define the percentage of times that we choose these
* operations.
*
* These three numbers must add up to 100.
*
* Note that allocation is handled differently.  It doesn't factor in
* here because these operations represent what can be accomplished on
* already allocated nodes.
*
* if you want to increase the allocation rate, you need to increase the
* free rate below, since only freed nodes can be allocated.
*
* currently, with these values set at 35, 35, 30 (respectively), when running
* we get this breakdown percentage wise:
*
* Total Allocations:            0.22653743
* Total Frees:                  0.226535936
* Total Read_Check (zero):      0.123785745
* Total Read_Check (non-zero):  0.149574686
* Total Write (zero):           0.123985458
* Total Write (non-zero):       0.149580745
* Sum of all of these:          1
*
* Note that we don't expect the zero and non-zero numbers to match, because you
* can only do the zero operation once per a given allocation while you can do
* the non-zero operation multiple times.  The allocation and free numbers aren't
* exactly the same because we don't include init and cleanup numbers in the statistics.
*/
#define DW_WRITE_THRESHOLD          (35)
#define DW_READ_CHECK_THRESHOLD     (35)
#define DW_FREE_THRESHOLD           (30)


/*
* How many reads we do when doing a read operation.
*
* The number of reads is keyed off of the block size.  setting this value
* to 100% (1.0) doesn't mean that we read the entire block.  It means that
* we do a number of reads that would be equivalent to reading the whole
* block, if we read each DWORD once.  we are reading randomly, though, and
* the larger this number means more chance of overlap.
*
* This number can be greater than 1.0 if you would like.
*
* a double value.
*/
#define DO_READ_BLOCK_PERCENTAGE    (0.20)


/*********************************************************************/
/*                                                                   */
/*                           Main function                           */
/*                                                                   */
/*********************************************************************/
BOOL iterateBlocks (
              DWORD dwNumOfBlocks,
              DWORD dwPageSize,
              ULONGLONG ullIterations,
              sOverallStatistics *pStats,
              ULONGLONG ullPrintAfterNIt,
              BOOL bPrintStats);

/*********************************************************************/
/*                                                                   */
/*                           Help functions                          */
/*                                                                   */
/*********************************************************************/

BOOL AllocateBlock (sBlock * pBlock, DWORD dwPageSize);

BOOL FreeBlock (sBlock * pBlock);

BOOL WriteBlock (sBlock * pBlock);

BOOL ReadBlock (const sBlock * pBlock);

BOOL DumpStats (const sOverallStatistics * pStats, DWORD dwNumOfBlocks, DWORD dwPageSize);

BOOL CheckAllBlocksForErrors (const sBlock * pBlocks, DWORD dwNumOfBlocks);

BOOL SetBlockEmpty (sBlock * pBlock);

void DumpPerBlockData (const sBlock * pBlock, DWORD dwBlockNum);

#endif
