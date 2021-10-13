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
#include <sdcommon.h>
#include <SDCardDDK.h>
#include <sdcard.h>
#include "mem_hlp.h"
#include <svsutil.hxx>
#include <sd_tst_cmn.h>
#include <sddrv.h>

void GetMemStat(UINT indent, MemData *md, SD_MEMORY_LIST_HANDLE /*hMem*/)
{
/* Note: this function does not check input parameters. pass in bad parameters at your own risk. */
    UINT tmp1 = 0, tmp2 = 0;
//1 Get memory stats
    LogDebug(indent, SDIO_ZONE_TST, TEXT("GetMemStat: Getting memory stats from svsutil..."));

       //we can not do this, SD_MEMORY_LIST_HANDLE can not cast to FixedMemDescr*
//  svsutil_GetFixedStats((FixedMemDescr *) hMem, &tmp1, &tmp2);
    md->uiFree = tmp1;
    md->uiTotal = tmp2;
}

SD_MEMORY_LIST_HANDLE CreateAndGetStat(UINT indent,  PTEST_PARAMS  pTestParams, MemData *md, DWORD dwNumBlks, DWORD dwBlkSize)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    SD_MEMORY_LIST_HANDLE hMem = NULL;

//1 Precheck
    if (md == NULL)
    {
        LogFail(indent1, TEXT("AllocWriteAndGetStat: Bad Parameter. md = NULL..."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT(" Bad Parameter. md = NULL. The memory stats can not be returned if this structure is NULL."));
        goto DONE;
    }
    if (dwNumBlks == 0)
    {
        LogFail(indent1, TEXT("AllocWriteAndGetStat: Bad Parameter. dwNumBlks = 0..."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT(" Bad Parameter. dwNumBlks = 0. One cannot allocate memory from a list with a depth of 0.."));
        goto DONE;
    }
    if (dwBlkSize == 0)
    {
        LogFail(indent1, TEXT("AllocWriteAndGetStat: Bad Parameter. dwBlkSize = 0..."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT(" Bad Parameter. dwBlkSize = 0. One cannot allocate memory blocks in 0-byte chunks from a memory list.."));
        goto DONE;
    }
//1 Create Memory List

    LogDebug(indent1, SDIO_ZONE_TST, TEXT("CreateAndGetStat: Creating Memory List Depth = %u, Entry Size = %u bytes..."), dwNumBlks, dwBlkSize);
    hMem = wrap_SDCreateMemoryList(0, dwNumBlks, dwBlkSize);
    if (hMem == NULL)
    {
        LogFail(indent1, TEXT("CreateAndGetStat: SDCreateMemoryList appears to have failed, because hMem is still NULL."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("SDCreateMemoryList appears to have failed, because hMem is still NULL."));
        goto DONE;
    }
//1 Get Stats
    LogDebug(indent, SDIO_ZONE_TST, TEXT("CreateAndGetStat: Getting Memory Info..."));
    GetMemStat(indent1, md, hMem);
DONE:
    return hMem;
}

BOOL AllocWriteAndGetStat(UINT indent,  PTEST_PARAMS  pTestParams, MemData *md, SD_MEMORY_LIST_HANDLE hMem, PVOID *ppvMem, DWORD dwBlkSize)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    BOOL bSuccess = TRUE;
    *ppvMem = NULL;
    PUCHAR buff = NULL;
//1 Precheck
    LogDebug(indent, SDIO_ZONE_TST, TEXT("AllocWriteAndGetStat: Checking params..."));
    if (md == NULL)
    {
        LogFail(indent1, TEXT("AllocWriteAndGetStat: Bad Parameter. md = NULL..."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT(" Bad Parameter. md = NULL. The memory stats can not be returned if this structure is NULL."));
        bSuccess = FALSE;
        goto DONE;
    }
    if (ppvMem == NULL)
    {
        LogFail(indent1, TEXT("AllocWriteAndGetStat: Bad Parameter. ppvMem = NULL..."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT(" Bad Parameter. ppvMem = NULL. This pointer to a PVOID cannot be NULL if a memory block is to be returned ."));
        bSuccess = FALSE;
        goto DONE;
    }
    if (hMem == NULL)
    {
        LogFail(indent1, TEXT("AllocWriteAndGetStat: Bad Parameter. hMem = NULL..."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT(" Bad Parameter. hMem = NULL. One can not allocate memory from a NULL handle to a memory list."));
        bSuccess = FALSE;
        goto DONE;
    }
    if (dwBlkSize == 0)
    {
        LogFail(indent1, TEXT("AllocWriteAndGetStat: Bad Parameter. dwBlkSize = 0..."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT(" Bad Parameter. dwBlkSize = 0. One cannot allocate memory blocks in 0-byte chunks from a memory list.."));
        bSuccess = FALSE;
        goto DONE;
    }
//1 Allocate fill buffer
    LogDebug(indent, SDIO_ZONE_TST, TEXT("AllocWriteAndGetStat: Allocating memory for a necessary buffer..."));
    if (AllocBuffer(indent1, dwBlkSize, BUFF_ALPHANUMERIC, &buff) == FALSE)
    {
        bSuccess = FALSE;
        LogFail(indent1, TEXT("AllocWriteAndGetStat: Failure allocating memory for a necessary buffer, this is probably due to a lack of memory, bailing..."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Failure allocating memory for a necessary buffer, this is probably due to a lack of memory."));
        goto DONE;
    }
//1 Allocate memory form Memory List
    LogDebug(indent, SDIO_ZONE_TST, TEXT("AllocWriteAndGetStat: Allocating block from Memory List..."));
    *ppvMem = wrap_SDAllocateFromMemList(hMem);
    if (*ppvMem == NULL)
    {
        bSuccess = FALSE;
        LogFail(indent1, TEXT("AllocWriteAndGetStat: Failure getting memory from memory list, bailing..."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Failure allocating memory from the memory list. If this is the first allocation from the list it could be that there was not enough memory when the memory list was created.\nIf it was not the first allocation from the list it could be that you tried to allocate memory from a list that already had all its memory allocated."));
        goto DONE;
    }
//1 Fill memory List block with data
    LogDebug(indent, SDIO_ZONE_TST, TEXT("AllocWriteAndGetStat: Filling Memory Block with Data..."));
    bSuccess = SDPerformSafeCopy(*ppvMem, buff, dwBlkSize);
    if (! bSuccess)
    {
        LogFail(indent1, TEXT("AllocWriteAndGetStat: Failure writing to memory allocated from memory list, bailing..."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT("Failure writing to memory allocated from memory list? Either the memory in the memory list was a fantasy, or dwBlkSize size did not the block size used in the memory list."));
        goto DONE;
    }
//1 Get current memory list statistics
    LogDebug(indent, SDIO_ZONE_TST, TEXT("AllocWriteAndGetStat: Getting Memory Info..."));
    GetMemStat(indent1, md, hMem);
//1 Cleanup
DONE:
    if (buff)
    {
        DeAllocBuffer(&buff);
    }
    return bSuccess;
}

BOOL FreeAndGetStat(UINT indent,  PTEST_PARAMS  pTestParams, MemData *md, SD_MEMORY_LIST_HANDLE hMem, PVOID *pvMem)
{
    UINT indent1 = indent + 1;
    PREFAST_ASSERT(indent1 > indent);
    BOOL bSuccess = TRUE;
//1 Precheck
    if (md == NULL)
    {
        LogFail(indent1, TEXT("FreeAndGetStat: Bad Parameter. md = NULL..."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT(" Bad Parameter. md = NULL. The memory stats can not be returned if this structure is NULL."));
        bSuccess = FALSE;
        goto DONE;
    }
    if (hMem == NULL)
    {
        LogFail(indent1, TEXT("FreeAndGetStat: Bad Parameter. hMem = NULL..."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT(" Bad Parameter. hMem = NULL. One can not free memory to a NULL handle to a memory list."));
        bSuccess = FALSE;
        goto DONE;
    }
    if (pvMem == NULL)
    {
        LogFail(indent1, TEXT("FreeAndGetStat: Bad Parameter. pvMem = NULL..."));
        AppendMessageBufferAndResult(pTestParams, TPR_FAIL, TEXT(" Bad Parameter. pvMem = NULL. One can not free memory pointed to by a NULL parameter."));
        bSuccess = FALSE;
        goto DONE;
    }
//1 Free Memory
    LogDebug(indent, SDIO_ZONE_TST, TEXT("FreeAndGetStat: Freeing block to Memory List..."));
    wrap_SDFreeToMemList(hMem, *pvMem);
    *pvMem = NULL;
    LogDebug(indent, SDIO_ZONE_TST, TEXT("FreeAndGetStat: Filling Memory Block with Data..."));
//1 Get stats
    LogDebug(indent, SDIO_ZONE_TST, TEXT("FreeAndGetStat: Getting Memory Info..."));
    GetMemStat(indent1, md, hMem);
DONE:
    return bSuccess;
}
