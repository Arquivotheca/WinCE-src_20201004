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
///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2002 BSQUARE Corporation. All rights reserved.
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
// --------------------------------------------------------------------
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// --------------------------------------------------------------------
//
#include <SDCardDDK.h>
#include <sdcard.h>
#include "mem_hlp.h"
#include <sdmem.h>
#include <sd_tst_cmn.h>
#include <sddrv.h>

#define DEPTH_OF_LIST 6

void MemListTest(UINT indent, PTEST_PARAMS  pTestParams,  PSDCLNT_CARD_INFO /*pClientInfo*/)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    UINT indent2 = indent + 2;
    PREFAST_ASSERT(indent2 > indent);
    MemData md[(DEPTH_OF_LIST* 2) + 3];
    PVOID pvMem[DEPTH_OF_LIST];
    BOOL bVal;
    UINT c;
    DWORD dwBlockSize = 1024;
    DWORD dwNumBlocks = DEPTH_OF_LIST;
    SD_MEMORY_LIST_HANDLE hMem = NULL;

    LogDebug(indent, SDIO_ZONE_TST, TEXT("MemLstTst: Entering Driver Test Function..."));
//1 Initilize
    ZeroMemory(md, sizeof(MemData) * ((DEPTH_OF_LIST* 2) + 3));
    for (c= 0; c < DEPTH_OF_LIST; c++)
    {
        pvMem[c] = NULL;
    }
//1 Create Memory List + Get Stats
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("MemLstTst: Creating Memory List Depth = %u, Entry Size = %u bytes..."), dwNumBlocks, dwBlockSize);
    hMem = CreateAndGetStat(indent2, pTestParams, &(md[0]), dwNumBlocks, dwBlockSize);
    if (hMem == NULL)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("MemLstTst: Bailing..."));
        goto DONE;
    }
//1 1st Allocation Memory from List
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("MemLstTst: Allocating block from Memory List..."));
    bVal = AllocWriteAndGetStat(indent2, pTestParams, &(md[1]), hMem, &(pvMem[0]), dwBlockSize);
    if (bVal == FALSE)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("MemLstTst: Bailing..."));
        goto DONE;
    }

//1 Free Memory from List
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("MemLstTst: Freeing block to Memory List..."));
    bVal = FreeAndGetStat(indent2, pTestParams, &(md[2]), hMem, &(pvMem[0]));
    if (bVal == FALSE)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("MemLstTst: Bailing..."));
        goto DONE;
    }
//1Allocate All Memory from List
    for (c = 0; ((c < DEPTH_OF_LIST) && (bVal == TRUE)); c++)
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("MemLstTst: Allocating %u%s block from Memory List..."), c+1, GetNumberSuffix(c+1));
        bVal = AllocWriteAndGetStat(indent2, pTestParams, &(md[3+c]), hMem, &(pvMem[c]), dwBlockSize);
        if (bVal == FALSE)
        {
            LogDebug(indent2, SDIO_ZONE_TST, TEXT("MemLstTst: Bailing..."));
            goto DONE;
        }
    }
//1 Free All Memory from List
    for (c = 0; ((c < DEPTH_OF_LIST) && (bVal == TRUE)); c++)
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("MemLstTst: Freeing %u%s block to Memory List..."), c+1, GetNumberSuffix(c+1));
        bVal = FreeAndGetStat(indent2, pTestParams, &(md[c+ 3 + DEPTH_OF_LIST]), hMem, &(pvMem[c]));
        if (bVal == FALSE)
        {
            LogDebug(indent2, SDIO_ZONE_TST, TEXT("MemLstTst: Bailing..."));
            goto DONE;
        }
    }

/*
//1 Verify and Dump Stats
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("MemLstTst: Memory Stats:"));
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("MemLstTst:\tMemory Before Initial Allocation of 1st Block from List:\tMemory Total in List = %u blocks\t\tMemory Free in List = %u blocks"), md[0].uiTotal, md[0].uiFree);
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("MemLstTst:\tMemory After Initial Allocation of 1st Block from List:\t\tMemory Total in List = %u blocks\t\tMemory Free in List = %u blocks"), md[1].uiTotal, md[1].uiFree);
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("MemLstTst:\tMemory After Initial  Freeing of 1st Block to List:\t\t\tMemory Total in List = %u blocks\t\tMemory Free in List = %u blocks"), md[2].uiTotal, md[2].uiFree);
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("MemLstTst:\tMemory After Second Allocation of 1st Block from List:\t\tMemory Total in List = %u blocks\t\tMemory Free in List = %u blocks"), md[3].uiTotal, md[3].uiFree);
    for (c = 1; c < DEPTH_OF_LIST; c++)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("MemLstTst:\tMemory After Allocation of %u%s Block from List:\t\t\t\tMemory Total in List = %u blocks\t\tMemory Free in List = %u blocks"), c + 1, GetNumberSuffix(c + 1), md[3 + c].uiTotal, md[3+ c].uiFree);
    }
    LogDebug(indent2, SDIO_ZONE_TST, TEXT("MemLstTst:\tMemory After Second Freeing of 1st Block to List:\t\t\tMemory Total in List = %u blocks\t\tMemory Free in List = %u blocks"), md[3 + DEPTH_OF_LIST].uiTotal, md[3 + DEPTH_OF_LIST].uiFree);
    for (c = 1; c < DEPTH_OF_LIST; c++)
    {
        LogDebug(indent2, SDIO_ZONE_TST, TEXT("MemLstTst:\tMemory After Freeing of %u%s Block from List:\t\t\t\tMemory Total in List = %u blocks\t\tMemory Free in List = %u blocks"), c + 1, GetNumberSuffix(c + 1), md[3 + DEPTH_OF_LIST + c].uiTotal, md[3 + DEPTH_OF_LIST + c].uiFree);
    }

    LogDebug(indent1, SDIO_ZONE_TST, TEXT("MemLstTst: Verifying Success..."));
    if ((md[0].uiFree != DEPTH_OF_LIST) || (md[0].uiTotal != DEPTH_OF_LIST))
    {
        LogFail(indent1, TEXT("MemLstTst: The total memory and free  memory available should both be %u blocks after the MemoryList is created, as the memory allocation is deferred the first allocation from the list is attempted. This has not happened, however."), DEPTH_OF_LIST);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The total memory and free  memory available should both be %u after the MemoryList is created, as the memory allocation is deferred the first allocation from the list is attempted.\nFree Memory = %u blocks, Total Memory = %u blocks"), DEPTH_OF_LIST, md[0].uiFree, md[0].uiTotal);
    }
    if  (md[1].uiTotal != dwNumBlocks)
    {
        LogFail(indent1, TEXT("MemLstTst: The total memory allocated in the Memory List does not appear to match the amount requested."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The total memory allocated in the Memory List does not appear to match the amount requested.\nTotal Memory = %u blocks, Number of blocks requested = %u blocks"), md[1].uiTotal, dwNumBlocks);
    }
    if (!SameVal(md[1].uiTotal, md[2].uiTotal, md[3].uiTotal, md[4].uiTotal, md[5].uiTotal, md[6].uiTotal, md[7].uiTotal, md[8].uiTotal, md[9].uiTotal, md[10].uiTotal, md[11].uiTotal, md[12].uiTotal, md[13].uiTotal, md[14].uiTotal, STOP_SERIES))
    {
        LogFail(indent1, TEXT("MemLstTst: The total memory allocated in the Memory List (once the memory is really allocated) is not reported as a consistent value as expected."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("The total memory allocated in the Memory List (once the memory is really allocated) is not reported as a consistent value as expected.\nAt all points in time (except immediately after SDCreateMemoryList) total memory should = %u blocks"), dwNumBlocks);
    }
    if (md[1].uiFree != (dwNumBlocks - 1))
    {
        LogFail(indent1, TEXT("MemLstTst: After the very first allocation from the memory list the amount of free memory should be one less than the total number of blocks requested."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("After the very first allocation from the memory list the amount of free memory should be one less than the total number of blocks requested. Instead  of %u blocks, it is = %u blocks"), dwNumBlocks, md[1].uiFree);
    }
    if (md[2].uiTotal!= md[2].uiFree)
    {
        LogFail(indent1, TEXT("MemLstTst: Immediately after freeing that first allocation form the memory list, the total memory does not equal the free memory as expected."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT(" Immediately after freeing that first allocation form the memory list, the total memory does not equal the free memory as expected. Total  = %u blocks, Free = %u blocks"), md[2].uiTotal, md[2].uiFree);
    }
    if (0 != md[DEPTH_OF_LIST + 2].uiFree)
    {
        LogFail(indent1, TEXT("MemLstTst: After %u consecutive allocations from the memory list, the number of memory blocks free should be zero. It is not ."), DEPTH_OF_LIST);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("After %u consecutive allocations from the memory list, the number of memory blocks free should be zero. It is not. Free = %u blocks"), DEPTH_OF_LIST, md[DEPTH_OF_LIST + 2].uiFree);
    }
    if(!InOrderVal(FALSE, md[2].uiFree, md[3].uiFree, md[4].uiFree, md[5].uiFree, md[6].uiFree, md[7].uiFree, md[8].uiFree, STOP_SERIES))
    {
         LogFail(indent1, TEXT("MemLstTst: During the  %u consecutive allocations from the memory list, the number of memory blocks free should decrease in 1 block increments. They do not."), DEPTH_OF_LIST);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("During the  %u consecutive allocations from the memory list, the number of memory blocks free should decrease in 1 block increments. They do not."), DEPTH_OF_LIST);
    }
    if(!InOrderVal(TRUE, md[8].uiFree, md[9].uiFree, md[10].uiFree, md[11].uiFree, md[12].uiFree, md[13].uiFree, md[14].uiFree, STOP_SERIES))
    {
         LogFail(indent1, TEXT("MemLstTst: During the  %u consecutive freeing of memory to the memory list, the number of memory blocks free should increase in 1 block increments. They do not."), DEPTH_OF_LIST);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("During the  %u consecutive freeing of memory to the memory list, the number of memory blocks free should increase in 1 block increments. They do not."), DEPTH_OF_LIST);
    }
    if (md[(DEPTH_OF_LIST * 2) + 2].uiTotal != md[(DEPTH_OF_LIST * 2) + 2].uiFree)
    {
        LogFail(indent1, TEXT("MemLstTst: After All blocks of memory have been freed to the list, the number of blocks free should equal the number of blocks total. It is not ."), DEPTH_OF_LIST);
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("After All blocks of memory have been freed to the list, the number of blocks free should equal the number of blocks total. It is not . Free = %u blocks, Total = %u blocks"), md[DEPTH_OF_LIST + 2].uiFree, md[DEPTH_OF_LIST + 2].uiTotal);
    }
*/
DONE:
    LogDebug(indent1, SDIO_ZONE_TST, TEXT("MemLstTst: Cleaning Up..."));
    for (c = 0; c < DEPTH_OF_LIST; c++)
    {
        if (pvMem[c])
        {
            wrap_SDFreeToMemList(hMem, pvMem[c]);
            pvMem[c] = NULL;
        }
    }
    if (hMem)
    {
        LogDebug(indent1, SDIO_ZONE_TST, TEXT("MemLstTst: Deleting Memory List..."));
        wrap_SDDeleteMemList(hMem);
        hMem = NULL;
    }
    GenerateSucessMessage(pTestParams);
    LogDebug(indent, SDIO_ZONE_TST, TEXT("MemLstTst: Exiting Driver Test Function..."));
}

