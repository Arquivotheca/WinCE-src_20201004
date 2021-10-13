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
#include "main.h"
#include "globals.h"
#include "volume.h"
#include <bcrypt.h>

// ----------------------------------------------------------------------------
// This test case verifies the IOCTL_FLASH_PDD_GET_REGION_COUNT IOCTL.
//
// The test succeeds if the following conditions are met:
//
// 1) The IOCTL succeeds
//
// 2) The number of reported regions is >= 1
// ----------------------------------------------------------------------------
TESTPROCAPI Tst_GetRegionCount(UINT uMsg,TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    UNREFERENCED_PARAMETER(tpParam);
    DWORD dwRetVal = TPR_FAIL;
    DWORD dwFlashRegionCount = 0;

    if( TPM_EXECUTE != uMsg )
    {
        return TPR_NOT_HANDLED;
    }

    // Check for the user test consent flag
    if (!TestConsent(lpFTE->dwUniqueID))
    {
        dwRetVal = TPR_SKIP;
        goto done;
    }

    // Check if the test volume has been successfully initialized.
    if (!InitVolume())
    {
        SKIP("The volume has not been successfully initialized, skipping test case");
        dwRetVal = TPR_SKIP;
        goto done;
    }

    // Call the GetFlashRegionCount() helper function to query number of regions
    if (!GetFlashRegionCount(g_hDevice, &dwFlashRegionCount))
    {
        FAIL("The flash region count query failed");
        goto done;
    }

    // Check if the number of regions was at least 1
    if (0 == dwFlashRegionCount)
    {
        FAIL("The flash region count reported by IOCTL_FLASH_PDD_GET_REGION_COUNT was 0");
        goto done;
    }
    else
    {
        g_pKato->Log(LOG_DETAIL,
            _T("The IOCTL_FLASH_PDD_GET_REGION_COUNT query returned %u regions on this flash device"),
            dwFlashRegionCount);
    }

    // success
    dwRetVal = TPR_PASS;
done:
    return dwRetVal;
}

// ----------------------------------------------------------------------------
// This test case verifies the IOCTL_FLASH_PDD_GET_REGION_INFO IOCTL.
//
// The test succeeds if the following conditions are met:
//
// 1) The IOCTL succeeds
//
// 2) For every region reported by IOCTL_FLASH_PDD_GET_REGION_COUNT
// there is a corresponding entry in the FLASH_REGION_INFO table
// ----------------------------------------------------------------------------
TESTPROCAPI Tst_GetRegionInfo(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    UNREFERENCED_PARAMETER(tpParam);
    DWORD dwRetVal = TPR_FAIL;
    FLASH_REGION_INFO * pFlashRegionInfoTable = NULL;
    DWORD dwFlashRegionInfoTableSize = 0;
    BOOL fSanityCheckFail = FALSE;

    if( TPM_EXECUTE != uMsg )
    {
        return TPR_NOT_HANDLED;
    }

    // Check for the user test consent flag
    if (!TestConsent(lpFTE->dwUniqueID))
    {
        dwRetVal = TPR_SKIP;
        goto done;
    }

    // Check if the test volume has been successfully initialized.
    if (!InitVolume())
    {
        SKIP("The volume has not been successfully initialized, skipping test case");
        dwRetVal = TPR_SKIP;
        goto done;
    }

    // Allocate a FLASH_REGION_INFO table large enough to hold all
    // the region info structures for the number of regions reported by
    // the IOCTL_FLASH_PDD_GET_REGION_COUNT query. The number of regions
    // is stored in the global variable g_dwRegionCount, which is intialized
    // during the call to InitVolume()
    dwFlashRegionInfoTableSize = g_dwFlashRegionCount * sizeof(FLASH_REGION_INFO);
    pFlashRegionInfoTable = (FLASH_REGION_INFO*) malloc(dwFlashRegionInfoTableSize);
    if (NULL == pFlashRegionInfoTable)
    {
        FAIL("Out of memory");
        goto done;
    }
    memset(pFlashRegionInfoTable, 0, dwFlashRegionInfoTableSize);

    // Call the GetFlashRegionInfo() helper function to fill in the region table
    if (!GetFlashRegionInfo(g_hDevice,
        pFlashRegionInfoTable,
        dwFlashRegionInfoTableSize))
    {
        FAIL("The flash region info query failed");
        goto done;
    }

    // Perform a sanity check on all FLASH_REGION_INFO structures
    for (DWORD dwFlashRegionIndex = 0;
        dwFlashRegionIndex < g_dwFlashRegionCount;
        dwFlashRegionIndex++)
    {
        FLASH_REGION_INFO flashRegionInfo = pFlashRegionInfoTable[dwFlashRegionIndex];
        if (!flashRegionInfo.BlockCount)
        {
            g_pKato->Log(LOG_FAIL,
                _T("The FLASH_REGION_INFO entry for region %u has BlockCount = 0"),
                dwFlashRegionIndex);
            fSanityCheckFail = TRUE;
        }
        if (!PowerOfTwo(flashRegionInfo.DataBytesPerSector))
        {
            g_pKato->Log(LOG_DETAIL,
                _T("Warning: the FLASH_REGION_INFO entry for region %u has DataBytesPerSector = %u, is not a power of 2"),
                dwFlashRegionIndex,
                flashRegionInfo.DataBytesPerSector);
            g_pKato->Log(LOG_DETAIL, _T("Sector sizes that are not a power of 2 may cause problems for certain filesystems"));
        }
        if (flashRegionInfo.DataBytesPerSector < 512)
        {
            g_pKato->Log(LOG_DETAIL,
                _T("Warning: the FLASH_REGION_INFO entry for region %u has DataBytesPerSector = %u, which is less than 512 bytes"),
                dwFlashRegionIndex,
                flashRegionInfo.DataBytesPerSector);
            g_pKato->Log(LOG_DETAIL, _T("Sectors smaller than 512 bytes may cause problems for certain filesystems"));
        }
        if (!flashRegionInfo.DataBytesPerSector)
        {
            g_pKato->Log(LOG_FAIL,
                _T("The FLASH_REGION_INFO entry for region %u has DataBytesPerSector = 0"),
                dwFlashRegionIndex);
            fSanityCheckFail = TRUE;
        }
        if (!flashRegionInfo.SectorsPerBlock)
        {
            g_pKato->Log(LOG_FAIL,
                _T("The FLASH_REGION_INFO entry for region %u has SectorsPerBlock = 0"),
                dwFlashRegionIndex);
            fSanityCheckFail = TRUE;
        }
        if (!flashRegionInfo.SpareBytesPerSector)
        {
            g_pKato->Log(LOG_FAIL,
                _T("The FLASH_REGION_INFO entry for region %u has SpareBytesPerSector = 0"),
                dwFlashRegionIndex);
            fSanityCheckFail = TRUE;
        }
        else if (flashRegionInfo.SpareBytesPerSector < 6)
        {
            g_pKato->Log(LOG_FAIL,
                _T("The FLASH_REGION_INFO entry for region %u has SpareBytesPerSector = %u. The spare area should be at least 6 bytes."),
                dwFlashRegionIndex,
                flashRegionInfo.SpareBytesPerSector);
        }
    }

    // If any of the FLASH_REGION_INFO entries failed the sanity check, fail.
    if (fSanityCheckFail)
    {
        FAIL("One or more FLASH_REGION_INFO entries has failed the sanity check");
        goto done;
    }
    else
    {
        g_pKato->Log(LOG_DETAIL, _T("The IOCTL_FLASH_PDD_GET_REGION_INFO query was successful"));
    }

    // success
    dwRetVal = TPR_PASS;
done:
    CHECK_FREE(pFlashRegionInfoTable);
    return dwRetVal;
}

// ----------------------------------------------------------------------------
// This test case verifies the IOCTL_FLASH_PDD_GET_BLOCK_STATUS IOCTL.
// The blocks status is checked either one or multiple blocks at a time, based
// on the test parameters.
//
// The test succeeds if the following conditions are met:
//
// 1) Every IOCTL succeeds
//
// 2) The queries successfully return a block status for every block in every
// region for both single bock and multi-block queries
// ----------------------------------------------------------------------------
TESTPROCAPI Tst_GetBlockStatus(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    UNREFERENCED_PARAMETER(tpParam);
    DWORD dwRetVal = TPR_FAIL;
    BOOL fMultiBlock = FALSE;
    PULONG pStatusBuffer = NULL;
    FLASH_GET_BLOCK_STATUS_REQUEST blockGetStatusRequest = {0};

    if( TPM_EXECUTE != uMsg )
    {
        return TPR_NOT_HANDLED;
    }

    // Check for the user test consent flag
    if (!TestConsent(lpFTE->dwUniqueID))
    {
        dwRetVal = TPR_SKIP;
        goto done;
    }

    // Check if the test volume has been successfully initialized.
    if (!InitVolume())
    {
        SKIP("The volume has not been successfully initialized, skipping test case");
        dwRetVal = TPR_SKIP;
        goto done;
    }

    // Read the tux parameter that specifies whether this test should
    // query multiple blocks on each call
    fMultiBlock = (BOOL)lpFTE->dwUserData;

    // Create status array for retrieving block status data
    pStatusBuffer = (PULONG) malloc(MAX_BLOCK_RUN_SIZE * sizeof(ULONG));
    if (NULL == pStatusBuffer)
    {
        FAIL("Out of memory");
        goto done;
    }

    // Iterate over all the regions on the flash device. The information about
    // the region geometries is stored in global variables set in InitVolume()
    for (DWORD dwFlashRegionIndex = 0;
        dwFlashRegionIndex < g_dwFlashRegionCount;
        dwFlashRegionIndex++)
    {
        // Get geometry for this region
        ULONGLONG ullBlockCount = g_pFlashRegionInfoTable[dwFlashRegionIndex].BlockCount;
        ULONGLONG ullCurrentBlock = 0;
        blockGetStatusRequest.IsInitialFlash = FALSE;
        blockGetStatusRequest.BlockRun.RegionIndex = dwFlashRegionIndex;
        
        // Keep track of percentage for current region
        ULONGLONG ullPercent = 0;
        g_pKato->Log(LOG_DETAIL,
            "Checking block status for all blocks in region %u",
            dwFlashRegionIndex);

        // Iterate over all the blocks in this region
        while(ullCurrentBlock < ullBlockCount)
        {
            // Display progress
            ShowProgress(&ullPercent, ullCurrentBlock, ullBlockCount);

            // The block run size will be 1 for the single-block test, or some number
            // between 1 and MAX_BLOCK_RUN_SIZE for the multi-block test.
            ULONG ulNextBlockRunSize = 1;
            if (fMultiBlock)
            {
                ulNextBlockRunSize = (ULONG)min(MAX_BLOCK_RUN_SIZE,
                    ullBlockCount - ullCurrentBlock);
            }
            blockGetStatusRequest.BlockRun.StartBlock = ullCurrentBlock;
            blockGetStatusRequest.BlockRun.BlockCount = ulNextBlockRunSize;
            
            // Clear the status array
            memset(pStatusBuffer, 0xFF, MAX_BLOCK_RUN_SIZE);

            // Call the GetBlockStatus() helper function
            if (!GetBlockStatus(g_hDevice,
                &blockGetStatusRequest,
                pStatusBuffer,
                ulNextBlockRunSize * sizeof(ULONG)))
            {
                g_pKato->Log(LOG_FAIL,
                    _T("The get block status query failed for region %u, block range %u - %u"),
                    dwFlashRegionIndex,
                    (DWORD)blockGetStatusRequest.BlockRun.StartBlock,
                    (DWORD)blockGetStatusRequest.BlockRun.StartBlock + ulNextBlockRunSize);
                goto done;
            }

            // Do a sanity check on the block status array
            for (ULONG ulIndex = 0; ulIndex < ulNextBlockRunSize; ulIndex++)
            {
                // Check if the status is GOOD, BAD, or RESERVED
                ULONG ulBlockStatus = pStatusBuffer[ulIndex];
                if (!((FLASH_BLOCK_STATUS_BAD & ulBlockStatus) ||
                    (FLASH_BLOCK_STATUS_RESERVED & ulBlockStatus) ||
                    (0 == ulBlockStatus)))
                {
                    g_pKato->Log(LOG_FAIL,
                        _T("The block status query returned invalid value for region %u, block %u"),
                        dwFlashRegionIndex,
                        (DWORD)blockGetStatusRequest.BlockRun.StartBlock + ulIndex);
                    goto done;
                }
            }

            ullCurrentBlock += ulNextBlockRunSize;
        }
    }

    // Print success message
    g_pKato->Log(LOG_DETAIL, _T("The IOCTL_FLASH_PDD_GET_BLOCK_STATUS queries were successful"));

    // success
    dwRetVal = TPR_PASS;
done:
    CHECK_FREE(pStatusBuffer);
    return dwRetVal;
}

// ----------------------------------------------------------------------------
// This test case verifies the IOCTL_FLASH_PDD_SET_BLOCK_STATUS IOCTL.
// The blocks status is set for either one or multiple blocks at a time, based
// on the test parameters.
//
// The test succeeds if the following conditions are met:
//
// 1) Every IOCTL succeeds
//
// 2) The test is able to set each "good" block to "reserved", verify
// the new status, then reset the status back to "good" by erasing the block.
// ----------------------------------------------------------------------------
TESTPROCAPI Tst_SetBlockStatus(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    UNREFERENCED_PARAMETER(tpParam);
    DWORD dwRetVal = TPR_FAIL;
    BOOL fMultiBlock = FALSE;
    PULONG pStatusBuffer = NULL;
    PULONG pAllBlocksStatusBuffer = 0;
    FLASH_GET_BLOCK_STATUS_REQUEST blockGetStatusRequest = {0};
    FLASH_SET_BLOCK_STATUS_REQUEST blockSetStatusRequest = {0};
    BOOL fBlockStatusModified = FALSE;

    if( TPM_EXECUTE != uMsg )
    {
        return TPR_NOT_HANDLED;
    }

    // Check for the user test consent flag
    if (!TestConsent(lpFTE->dwUniqueID))
    {
        dwRetVal = TPR_SKIP;
        goto done;
    }

    // Check if the test volume has been successfully initialized.
    if (!InitVolume())
    {
        SKIP("The volume has not been successfully initialized, skipping test case");
        dwRetVal = TPR_SKIP;
        goto done;
    }

    // Read the tux parameter that specifies whether this test should
    // query multiple blocks on each call
    fMultiBlock = (BOOL)lpFTE->dwUserData;

    // Create status array for retrieving block status data
    pStatusBuffer = (PULONG) malloc(MAX_BLOCK_RUN_SIZE * sizeof(ULONG));
    if (NULL == pStatusBuffer)
    {
        FAIL("Out of memory");
        goto done;
    }

    // Iterate over all the regions on the flash device. The information about
    // the region geometries is stored in global variables set in InitVolume()
    for (DWORD dwFlashRegionIndex = 0;
        dwFlashRegionIndex < g_dwFlashRegionCount;
        dwFlashRegionIndex++)
    {
        // Get geometry for this region
        ULONGLONG ullBlockCount = g_pFlashRegionInfoTable[dwFlashRegionIndex].BlockCount;
        ULONGLONG ullCurrentBlock = 0;

        // Initialize portions of the requests that will be reused for each region
        blockGetStatusRequest.IsInitialFlash = FALSE;
        blockGetStatusRequest.BlockRun.RegionIndex = dwFlashRegionIndex;
        blockSetStatusRequest.BlockRun.RegionIndex = dwFlashRegionIndex;

        // Get a block status array for this region
        if (!GetStatusForAllBlocks(g_hDevice,
            g_pFlashRegionInfoTable[dwFlashRegionIndex],
            dwFlashRegionIndex,
            &pAllBlocksStatusBuffer))
        {
            g_pKato->Log(LOG_FAIL,
                _T("Failed to get block status for blocks in region %u"),
                dwFlashRegionIndex);
            goto done;
        }

        // Keep track of percentage for current region
        ULONGLONG ullPercent = 0;
        g_pKato->Log(LOG_DETAIL,
            "Setting block status for all known good blocks in region %u",
            dwFlashRegionIndex);

        // Iterate over all the blocks
        while(ullCurrentBlock < ullBlockCount)
        {
            // Display progress
            ShowProgress(&ullPercent, ullCurrentBlock, ullBlockCount);

            ULONG ulNextBlockRunSize =
                GetGoodBlockRun(((fMultiBlock)? MAX_BLOCK_RUN_SIZE : 1),
                ullBlockCount,
                &ullCurrentBlock,
                pAllBlocksStatusBuffer,
                &blockSetStatusRequest.BlockRun,
                FALSE);

            // If ulNextBlockRunSize is 0 that means we reached the end of the flash
            // and no more eligible block runs have been found. Break out of the loop
            if (!ulNextBlockRunSize)
            {
                break;
            }

            // Initialize the get and set requests
            blockGetStatusRequest.BlockRun.BlockCount = blockSetStatusRequest.BlockRun.BlockCount;
            blockGetStatusRequest.BlockRun.StartBlock = blockSetStatusRequest.BlockRun.StartBlock;
            blockSetStatusRequest.BlockRun = blockGetStatusRequest.BlockRun;
            blockSetStatusRequest.BlockStatus = FLASH_BLOCK_STATUS_RESERVED;

            // Call the SetBlockStatus() helper function
            if (!SetBlockStatus(g_hDevice, &blockSetStatusRequest))
            {
                g_pKato->Log(LOG_FAIL,
                    _T("The set block status request failed for region %u, block range %u - %u"),
                    dwFlashRegionIndex,
                    (DWORD)blockSetStatusRequest.BlockRun.StartBlock,
                    (DWORD)blockSetStatusRequest.BlockRun.StartBlock + (ulNextBlockRunSize - 1));
                goto done;
            }
            fBlockStatusModified = TRUE;

            // Clear the status array
            memset(pStatusBuffer, 0xFF, MAX_BLOCK_RUN_SIZE);

            // Call the GetBlockStatus() helper function
            if (!GetBlockStatus(g_hDevice,
                &blockGetStatusRequest,
                pStatusBuffer,
                ulNextBlockRunSize * sizeof(ULONG)))
            {
                g_pKato->Log(LOG_FAIL,
                    _T("The get block status query failed for region %u, block range %u - %u"),
                    dwFlashRegionIndex,
                    (DWORD)blockGetStatusRequest.BlockRun.StartBlock,
                    (DWORD)blockGetStatusRequest.BlockRun.StartBlock + (ulNextBlockRunSize - 1));
                goto done;
            }

            // Do a check on the block status array
            for (ULONG ulIndex = 0; ulIndex < ulNextBlockRunSize; ulIndex++)
            {
                ULONG ulBlockStatus = pStatusBuffer[ulIndex];
                if (!(FLASH_BLOCK_STATUS_RESERVED & ulBlockStatus))
                {
                    g_pKato->Log(LOG_FAIL,
                        _T("The block status query returned wrong status for region %u, block %u"),
                        dwFlashRegionIndex,
                        (DWORD)blockGetStatusRequest.BlockRun.StartBlock + ulIndex);
                    goto done;
                }
            }

            // Erase the blocks to reset their status to GOOD
            if (!EraseBlocks(g_hDevice,
                &blockSetStatusRequest.BlockRun,
                sizeof(BLOCK_RUN)))
            {
                g_pKato->Log(LOG_FAIL,
                    _T("Failed to erase region %u, block range %u - %u"),
                    dwFlashRegionIndex,
                    (DWORD)blockGetStatusRequest.BlockRun.StartBlock,
                    (DWORD)blockGetStatusRequest.BlockRun.StartBlock + (ulNextBlockRunSize - 1));
                goto done;
            }

            // Call the GetBlockStatus() helper function
            if (!GetBlockStatus(g_hDevice,
                &blockGetStatusRequest,
                pStatusBuffer,
                ulNextBlockRunSize * sizeof(ULONG)))
            {
                g_pKato->Log(LOG_FAIL,
                    _T("The get block status query failed for region %u, block range %u - %u"),
                    dwFlashRegionIndex,
                    (DWORD)blockGetStatusRequest.BlockRun.StartBlock,
                    (DWORD)blockGetStatusRequest.BlockRun.StartBlock + (ulNextBlockRunSize - 1));
                goto done;
            }

            // Do a check on the block status array (this time check that the status is good)
            for (ULONG ulIndex = 0; ulIndex < ulNextBlockRunSize; ulIndex++)
            {
                ULONG ulBlockStatus = pStatusBuffer[ulIndex];
                if (0 != ulBlockStatus)
                {
                    g_pKato->Log(LOG_FAIL,
                        _T("The block status query returned wrong status for region %u, block %u"),
                        dwFlashRegionIndex,
                        (DWORD)blockGetStatusRequest.BlockRun.StartBlock + ulIndex);
                    goto done;
                }
            }
        }

        // Check if any eligible blocks were found in the region
        if (!fBlockStatusModified)
        {
            g_pKato->Log(LOG_FAIL,
                _T("No blocks were found in region %u with 'good' block status"),
                dwFlashRegionIndex);
            goto done;
        }

        // Clear the block status table
        CHECK_FREE(pAllBlocksStatusBuffer);
    }

    // Print success message
    g_pKato->Log(LOG_DETAIL, _T("The IOCTL_FLASH_PDD_SET_BLOCK_STATUS requests were successful"));

    // success
    dwRetVal = TPR_PASS;
done:
    CHECK_FREE(pStatusBuffer);
    CHECK_FREE(pAllBlocksStatusBuffer);
    return dwRetVal;
}

// ----------------------------------------------------------------------------
// This test case verifies the IOCTL_FLASH_PDD_READ_PHYSICAL_SECTORS IOCTL.
// The reads are configured to be single-sector, multi-sector, or multiple
// discontiguous sectors, depending on the test parameters.
//
// The test succeeds if the following conditions are met:
//
// 1) Every IOCTL succeeds
//
// 2) All groupings of sectors from "good" blocks can be read succesfully
// ----------------------------------------------------------------------------
TESTPROCAPI Tst_ReadSectors(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    UNREFERENCED_PARAMETER(tpParam);
    DWORD dwRetVal = TPR_FAIL;
    BOOL fMultiSectorReads = FALSE;
    BOOL fDiscontiguousReads = FALSE;
    PULONG pAllBlocksStatusBuffer = 0;
    FLASH_PDD_TRANSFER * pFlashTransferRequests = NULL;
    PBYTE pSectorData = NULL;
    PBYTE pSectorSpare = NULL;
    BLOCK_RUN blockRun = {0};
    BOOL fSectorsRead = FALSE;

    if( TPM_EXECUTE != uMsg )
    {
        return TPR_NOT_HANDLED;
    }

    // Check for the user test consent flag
    if (!TestConsent(lpFTE->dwUniqueID))
    {
        dwRetVal = TPR_SKIP;
        goto done;
    }

    // Check if the test volume has been successfully initialized.
    if (!InitVolume())
    {
        SKIP("The volume has not been successfully initialized, skipping test case");
        dwRetVal = TPR_SKIP;
        goto done;
    }

    // Read the tux parameter that specifies whether this test should
    // read single, multiple, or multiple discontiguous sectors
    if (SINGLE_UNIT_REQUEST == (DWORD)lpFTE->dwUserData)
    {
        fMultiSectorReads = FALSE;
        fDiscontiguousReads = FALSE;
    }
    else if (MULTI_UNIT_REQUEST == (DWORD)lpFTE->dwUserData)
    {
        fMultiSectorReads = TRUE;
        fDiscontiguousReads = FALSE;
    }
    else if (MULTI_UNIT_SPARSE_REQUEST == (DWORD)lpFTE->dwUserData)
    {
        fMultiSectorReads = TRUE;
        fDiscontiguousReads = TRUE;
    }

    // Allocate a flash transfer array for up to MAX_TRANSFER_REQUESTS i/o requests
    pFlashTransferRequests = (FLASH_PDD_TRANSFER*) malloc(MAX_TRANSFER_REQUESTS * sizeof(FLASH_PDD_TRANSFER));
    if (NULL == pFlashTransferRequests)
    {
        FAIL("Out of memory");
        goto done;
    }

    // Iterate over all the regions on the flash device. The information about
    // the region geometries is stored in global variables set in InitVolume()
    for (DWORD dwFlashRegionIndex = 0;
        dwFlashRegionIndex < g_dwFlashRegionCount;
        dwFlashRegionIndex++)
    {  
        // Get geometry for this region
        ULONGLONG ullBlockCount = g_pFlashRegionInfoTable[dwFlashRegionIndex].BlockCount;
        ULONGLONG ullCurrentBlock = 0;
        // Get sector geometry and allocate buffers for spare and data areas
        ULONG ulSectorsPerBlock = g_pFlashRegionInfoTable[dwFlashRegionIndex].SectorsPerBlock;
        ULONG ulDataBytesPerSector = g_pFlashRegionInfoTable[dwFlashRegionIndex].DataBytesPerSector;
        ULONG ulSpareBytesPerSector = g_pFlashRegionInfoTable[dwFlashRegionIndex].SpareBytesPerSector;
        // Allocate enough buffer to read at least 2 block's worth of data
        // for up to MAX_TRANSFER_REQUESTS requests
        DWORD dwSectorDataPerRequest = ulDataBytesPerSector * ulSectorsPerBlock * 2;
        DWORD dwSpareDataPerRequest = ulSpareBytesPerSector * ulSectorsPerBlock * 2;
        DWORD dwDataBufferSize = 0;
        DWORD dwSpareBufferSize = 0;
        if ((S_OK != DWordMult(dwSectorDataPerRequest, MAX_TRANSFER_REQUESTS, &dwDataBufferSize)) ||
            (S_OK != DWordMult(dwSpareDataPerRequest, MAX_TRANSFER_REQUESTS, &dwSpareBufferSize)) )
        {
            FAIL("Integer overflow while calculating buffer size");
            goto done;
        }
        pSectorData = (PBYTE) malloc(dwDataBufferSize);
        if (NULL == pSectorData)
        {
            FAIL("Out of memory");
            goto done;
        }
        pSectorSpare = (PBYTE) malloc(dwSpareBufferSize);
        if (NULL == pSectorSpare)
        {
            FAIL("Out of memory");
            goto done;
        }

        // Get a block status array for this region
        if (!GetStatusForAllBlocks(g_hDevice,
            g_pFlashRegionInfoTable[dwFlashRegionIndex],
            dwFlashRegionIndex,
            &pAllBlocksStatusBuffer))
        {
            g_pKato->Log(LOG_FAIL,
                _T("Failed to get block status for blocks in region %u"),
                dwFlashRegionIndex);
            goto done;
        }

        // Initialize common portions of the pFlashTransferRequests structure
        for (DWORD dwIndex = 0; dwIndex < MAX_TRANSFER_REQUESTS; dwIndex++)
        {
            pFlashTransferRequests[dwIndex].pData = pSectorData +
                (dwIndex) * dwSectorDataPerRequest;
            pFlashTransferRequests[dwIndex].pSpare = pSectorSpare + 
                (dwIndex) * dwSpareDataPerRequest;
            pFlashTransferRequests[dwIndex].RegionIndex = dwFlashRegionIndex;
        }

        // Keep track of percentage for current region
        ULONGLONG ullPercent = 0;
        g_pKato->Log(LOG_DETAIL,
            "Reading data from all reserved/good blocks in region %u",
            dwFlashRegionIndex);

        // Iterate over all the blocks
        while(ullCurrentBlock < ullBlockCount)
        {
            // Display progress
            ShowProgress(&ullPercent, ullCurrentBlock, ullBlockCount);

            ULONG ulNextBlockRunSize =
                GetGoodBlockRun(((fMultiSectorReads)? MAX_TRANSFER_REQUESTS : 1),
                ullBlockCount,
                &ullCurrentBlock,
                pAllBlocksStatusBuffer,
                &blockRun,
                TRUE);

            // If ulNextBlockRunSize is 0 that means we reached the end of the flash
            // and no more eligible block runs have been found. Break out of the loop.
            if (!ulNextBlockRunSize)
            {
                break;
            }

            // Pick sectors to read and initialize the transfer structure
            if (!fMultiSectorReads && !fDiscontiguousReads)
            {
                // Read all the sectors in the next block run individually
                pFlashTransferRequests[0].SectorRun.SectorCount = 1;
                ULONGLONG ullStartSector = blockRun.StartBlock * ulSectorsPerBlock;
                ULONGLONG ullEndSector = ullStartSector + (ulSectorsPerBlock * ulNextBlockRunSize);
                for (ULONGLONG ullCurrentSector = ullStartSector;
                    ullCurrentSector < ullEndSector;
                    ullCurrentSector++)
                {
                    pFlashTransferRequests[0].SectorRun.StartSector = ullCurrentSector;
                    // Use the ReadSectors() helper to read the sectors
                    if (!ReadSectors(g_hDevice,
                        pFlashTransferRequests,
                        sizeof(FLASH_PDD_TRANSFER)))
                    {
                        g_pKato->Log(LOG_FAIL,
                            _T("Read failed for region %u, sector %u"),
                            dwFlashRegionIndex,
                            (DWORD)ullCurrentSector);
                        goto done;
                    }
                }
                // We found at least one set of blocks eligible for read
                fSectorsRead = TRUE;
            }
            else if (fMultiSectorReads && !fDiscontiguousReads)
            {
                // Issue up to MAX_TRANSFER_REQUESTS to read all sectors in the block run
                ULONGLONG ullStartSector = blockRun.StartBlock * ulSectorsPerBlock;
                DWORD dwSectorCount = (ulSectorsPerBlock * ulNextBlockRunSize);
                // If there are fewer than 2 sectors, this iteration does not apply
                if (dwSectorCount < 2)
                {
                    continue;
                }
                // In each request read 1/MAX_TRANSFER_REQUESTS of the total sector count
                DWORD dwSectorsPerRequest = max(1,dwSectorCount / MAX_TRANSFER_REQUESTS);
                // Use the lower number if there are fewer sectors per block than MAX_TRANSFER_REQUESTS
                DWORD dwRequestCount = min(MAX_TRANSFER_REQUESTS, ulSectorsPerBlock);
                for (DWORD dwIndex = 0; dwIndex < dwRequestCount; dwIndex++)
                {
                    pFlashTransferRequests[dwIndex].SectorRun.StartSector =
                        ullStartSector + (dwIndex * dwSectorsPerRequest);
                    pFlashTransferRequests[dwIndex].SectorRun.SectorCount = dwSectorsPerRequest;
                }
                // Use the ReadSectors() helper to read the sectors
                if (!ReadSectors(g_hDevice,
                    pFlashTransferRequests,
                    sizeof(FLASH_PDD_TRANSFER) * dwRequestCount))
                {
                    g_pKato->Log(LOG_FAIL,
                        _T("Read failed for region %u, sectors %u - %u"),
                        dwFlashRegionIndex,
                        (DWORD)pFlashTransferRequests[0].SectorRun.StartSector,
                        (DWORD)pFlashTransferRequests[dwRequestCount-1].SectorRun.StartSector + 
                        pFlashTransferRequests[dwRequestCount-1].SectorRun.SectorCount);
                    goto done;
                }
                // We found at least 2 sectors eligible for read
                fSectorsRead = TRUE;
            }
            else if (fMultiSectorReads && fDiscontiguousReads)
            {
                // Issue reads for discontiguous groups of sectors
                ULONGLONG ullStartSector = blockRun.StartBlock * ulSectorsPerBlock;
                DWORD dwSectorCount = (ulSectorsPerBlock * ulNextBlockRunSize);
                // If there are fewer than 3 sectors, this iteration does not apply
                if (dwSectorCount < 3)
                {
                    continue;
                }
                // In each request read at most 1/4th of the total sector count
                DWORD dwSectorsPerRequest = max(1, dwSectorCount / 4);
                DWORD dwRequestCount = 2;
                DWORD dwStepSize = dwSectorCount / 2;
                for (DWORD dwIndex = 0; dwIndex < dwRequestCount; dwIndex++)
                {
                    pFlashTransferRequests[dwIndex].SectorRun.StartSector =
                        ullStartSector + (dwIndex * dwStepSize);
                    pFlashTransferRequests[dwIndex].SectorRun.SectorCount = dwSectorsPerRequest;
                }
                // Use the ReadSectors() helper to read the sectors
                if (!ReadSectors(g_hDevice,
                    pFlashTransferRequests,
                    sizeof(FLASH_PDD_TRANSFER) * dwRequestCount))
                {
                    g_pKato->Log(LOG_FAIL,
                        _T("Read failed for region %u, sectors %u - %u"),
                        dwFlashRegionIndex,
                        (DWORD)pFlashTransferRequests[0].SectorRun.StartSector,
                        (DWORD)pFlashTransferRequests[dwRequestCount-1].SectorRun.StartSector + 
                        pFlashTransferRequests[dwRequestCount-1].SectorRun.SectorCount);
                    goto done;
                }
                // We found at least one disjoint set of multiple sectors to read
                fSectorsRead = TRUE;
            }
        }

        // Check if any eligible blocks were found in the region
        if (!fSectorsRead)
        {
            g_pKato->Log(LOG_FAIL,
                _T("No sectors were read because no valid sector ranges of suffient size were discovered in region %u"),
                dwFlashRegionIndex);
            goto done;
        }

        // Clear the block status table and sector buffers
        CHECK_FREE(pAllBlocksStatusBuffer);
        CHECK_FREE(pSectorData);
        CHECK_FREE(pSectorSpare);
    }

    // Print success message
    g_pKato->Log(LOG_DETAIL, _T("The IOCTL_FLASH_PDD_READ_PHYSICAL_SECTORS requests were successful"));

    // success
    dwRetVal = TPR_PASS;
done:
    CHECK_FREE(pSectorData);
    CHECK_FREE(pSectorSpare);
    CHECK_FREE(pAllBlocksStatusBuffer);
    CHECK_FREE(pFlashTransferRequests);
    return dwRetVal;
}

// ----------------------------------------------------------------------------
// This test case verifies the IOCTL_FLASH_PDD_WRITE_PHYSICAL_SECTORS IOCTL.
// The writes are configured to be single-sector, multi-sector, or multiple
// discontiguous sectors, depending on the test parameters.
//
// The test succeeds if the following conditions are met:
//
// 1) Every DeviceIoControl succeeds.
//
// 2) Blocks that are selected for writing are succesfully erased
//
// 3) Sector writes succeed
//
// 4) Written data can be confirmed by reading the sectors back
// ----------------------------------------------------------------------------
TESTPROCAPI Tst_WriteSectors(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    UNREFERENCED_PARAMETER(tpParam);
    DWORD dwRetVal = TPR_FAIL;
    BOOL fMultiSectorWrites = FALSE;
    BOOL fDiscontiguousWrites = FALSE;
    PULONG pAllBlocksStatusBuffer = 0;
    FLASH_PDD_TRANSFER * pFlashTransferRequests = NULL;
    PBYTE pSectorData = NULL;
    PBYTE pSectorSpare = NULL;
    BLOCK_RUN blockRun = {0};
    BOOL fSectorsWritten = FALSE;

    if( TPM_EXECUTE != uMsg )
    {
        return TPR_NOT_HANDLED;
    }

    // Check for the user test consent flag
    if (!TestConsent(lpFTE->dwUniqueID))
    {
        dwRetVal = TPR_SKIP;
        goto done;
    }

    // Check if the test volume has been successfully initialized.
    if (!InitVolume())
    {
        SKIP("The volume has not been successfully initialized, skipping test case");
        dwRetVal = TPR_SKIP;
        goto done;
    }

    // Read the tux parameter that specifies whether this test should
    // read single, multiple, or multiple discontiguous sectors
    if (SINGLE_UNIT_REQUEST == (DWORD)lpFTE->dwUserData)
    {
        fMultiSectorWrites = FALSE;
        fDiscontiguousWrites = FALSE;
    }
    else if (MULTI_UNIT_REQUEST == (DWORD)lpFTE->dwUserData)
    {
        fMultiSectorWrites = TRUE;
        fDiscontiguousWrites = FALSE;
    }
    else if (MULTI_UNIT_SPARSE_REQUEST == (DWORD)lpFTE->dwUserData)
    {
        fMultiSectorWrites = TRUE;
        fDiscontiguousWrites = TRUE;
    }

    // Allocate a flash transfer array for up to MAX_TRANSFER_REQUESTS i/o requests
    pFlashTransferRequests = (FLASH_PDD_TRANSFER*) malloc(MAX_TRANSFER_REQUESTS * sizeof(FLASH_PDD_TRANSFER));
    if (NULL == pFlashTransferRequests)
    {
        FAIL("Out of memory");
        goto done;
    }

    // Iterate over all the regions on the flash device. The information about
    // the region geometries is stored in global variables set in InitVolume()
    for (DWORD dwFlashRegionIndex = 0;
        dwFlashRegionIndex < g_dwFlashRegionCount;
        dwFlashRegionIndex++)
    {
        blockRun.RegionIndex = dwFlashRegionIndex;
        // Get geometry for this region
        ULONGLONG ullBlockCount = g_pFlashRegionInfoTable[dwFlashRegionIndex].BlockCount;
        ULONGLONG ullCurrentBlock = 0;
        // Get sector geometry and allocate buffers for spare and data areas
        ULONG ulSectorsPerBlock = g_pFlashRegionInfoTable[dwFlashRegionIndex].SectorsPerBlock;
        ULONG ulDataBytesPerSector = g_pFlashRegionInfoTable[dwFlashRegionIndex].DataBytesPerSector;
        ULONG ulSpareBytesPerSector = g_pFlashRegionInfoTable[dwFlashRegionIndex].SpareBytesPerSector;
        // Allocate enough buffer to Write at least 2 block's worth of data
        // for up to MAX_TRANSFER_REQUESTS requests
        DWORD dwSectorDataPerRequest = ulDataBytesPerSector * ulSectorsPerBlock * 2;
        DWORD dwSpareDataPerRequest = ulSpareBytesPerSector * ulSectorsPerBlock * 2;
        DWORD dwDataBufferSize = 0;
        DWORD dwSpareBufferSize = 0;
        if ((S_OK != DWordMult(dwSectorDataPerRequest, MAX_TRANSFER_REQUESTS, &dwDataBufferSize)) ||
            (S_OK != DWordMult(dwSpareDataPerRequest, MAX_TRANSFER_REQUESTS, &dwSpareBufferSize)) )
        {
            FAIL("Integer overflow while calculating buffer size");
            goto done;
        }
        pSectorData = (PBYTE) malloc(dwDataBufferSize);
        if (NULL == pSectorData)
        {
            FAIL("Out of memory");
            goto done;
        }
        pSectorSpare = (PBYTE) malloc(dwSpareBufferSize);
        if (NULL == pSectorSpare)
        {
            FAIL("Out of memory");
            goto done;
        }

        // Pre-fill sector data with a random load
        BCryptGenRandom(NULL, (PUCHAR) pSectorData, dwDataBufferSize, BCRYPT_USE_SYSTEM_PREFERRED_RNG);

        // Get a block status array for this region
        if (!GetStatusForAllBlocks(g_hDevice,
            g_pFlashRegionInfoTable[dwFlashRegionIndex],
            dwFlashRegionIndex,
            &pAllBlocksStatusBuffer))
        {
            g_pKato->Log(LOG_FAIL,
                _T("Failed to get block status for blocks in region %u"),
                dwFlashRegionIndex);
            goto done;
        }

        // Initialize common portions of the pFlashTransferRequests structure
        for (DWORD dwIndex = 0; dwIndex < MAX_TRANSFER_REQUESTS; dwIndex++)
        {
            pFlashTransferRequests[dwIndex].pData = pSectorData +
                (dwIndex) * dwSectorDataPerRequest;
            pFlashTransferRequests[dwIndex].pSpare = pSectorSpare + 
                (dwIndex) * dwSpareDataPerRequest;
            pFlashTransferRequests[dwIndex].RegionIndex = dwFlashRegionIndex;
        }

        // Keep track of percentage for current region
        ULONGLONG ullPercent = 0;
        g_pKato->Log(LOG_DETAIL,
            "Reading data from all reserved/good blocks in region %u",
            dwFlashRegionIndex);

        // Iterate over all the blocks
        while(ullCurrentBlock < ullBlockCount)
        {
            // Display progress
            ShowProgress(&ullPercent, ullCurrentBlock, ullBlockCount);

            ULONG ulNextBlockRunSize =
                GetGoodBlockRun(((fMultiSectorWrites)? MAX_TRANSFER_REQUESTS : 1),
                ullBlockCount,
                &ullCurrentBlock,
                pAllBlocksStatusBuffer,
                &blockRun,
                FALSE);

            // If ulNextBlockRunSize is 0 that means we reached the end of the flash
            // and no more eligible block runs have been found. Break out of the loop
            if (!ulNextBlockRunSize)
            {
                break;
            }

            // Erase the group of blocks that will be written to
            if (!EraseBlocks(g_hDevice,
                &blockRun,
                sizeof(BLOCK_RUN)))
            {
                g_pKato->Log(LOG_FAIL,
                    _T("Failed to erase region %u blocks %u - %u"),
                    dwFlashRegionIndex,
                    (DWORD)blockRun.StartBlock,
                    (DWORD)blockRun.StartBlock + blockRun.BlockCount - 1);
                goto done;
            }

            // Pick sectors to write and initialize the transfer structure
            if (!fMultiSectorWrites && !fDiscontiguousWrites)
            {
                DWORD dwRequestCount = 0;
                // Write all the sectors in the next block run individually
                pFlashTransferRequests[0].SectorRun.SectorCount = 1;
                ULONGLONG ullStartSector = blockRun.StartBlock * ulSectorsPerBlock;
                ULONGLONG ullEndSector = ullStartSector + (ulSectorsPerBlock * ulNextBlockRunSize);

                for (ULONGLONG ullCurrentSector = ullStartSector;
                    ullCurrentSector < ullEndSector;
                    ullCurrentSector++)
                {
                    pFlashTransferRequests[0].SectorRun.StartSector = ullCurrentSector;

                    // Use the WriteReadVerify() routine to write and verify the current sectors
                    if (!WriteReadVerify(g_hDevice,
                        pFlashTransferRequests,
                        sizeof(FLASH_PDD_TRANSFER),
                        dwDataBufferSize,
                        ulDataBytesPerSector))
                    {
                        g_pKato->Log(LOG_FAIL,
                            _T("Write failed for region %u, sectors %u - %u"),
                            dwFlashRegionIndex,
                            (DWORD)pFlashTransferRequests[0].SectorRun.StartSector,
                            (DWORD)pFlashTransferRequests[dwRequestCount-1].SectorRun.StartSector + 
                            pFlashTransferRequests[dwRequestCount-1].SectorRun.SectorCount);
                        goto done;
                    }
                }
                // We found at least one set of blocks eligible for read
                fSectorsWritten = TRUE;
            }
            else if (fMultiSectorWrites && !fDiscontiguousWrites)
            {
                // Issue up to MAX_TRANSFER_REQUESTS to read all sectors in the block run
                ULONGLONG ullStartSector = blockRun.StartBlock * ulSectorsPerBlock;
                DWORD dwSectorCount = (ulSectorsPerBlock * ulNextBlockRunSize);
                // If there are fewer than 2 sectors, this iteration does not apply
                if (dwSectorCount < 2)
                {
                    continue;
                }
                // In each request write 1/MAX_TRANSFER_REQUESTS of the total sector count
                DWORD dwSectorsPerRequest = max(1,dwSectorCount / MAX_TRANSFER_REQUESTS);
                // Use the lower number if there are fewer sectors per block than MAX_TRANSFER_REQUESTS
                DWORD dwRequestCount = min(MAX_TRANSFER_REQUESTS, ulSectorsPerBlock);
                for (DWORD dwIndex = 0; dwIndex < dwRequestCount; dwIndex++)
                {
                    pFlashTransferRequests[dwIndex].SectorRun.StartSector =
                        ullStartSector + (dwIndex * dwSectorsPerRequest);
                    pFlashTransferRequests[dwIndex].SectorRun.SectorCount = dwSectorsPerRequest;
                }
                // Use the WriteReadVerify() routine to write and verify the current sectors
                if (!WriteReadVerify(g_hDevice,
                    pFlashTransferRequests,
                    sizeof(FLASH_PDD_TRANSFER) * dwRequestCount,
                    dwDataBufferSize,
                    ulDataBytesPerSector * dwSectorsPerRequest))
                {
                    g_pKato->Log(LOG_FAIL,
                        _T("Write failed for region %u, sectors %u - %u"),
                        dwFlashRegionIndex,
                        (DWORD)pFlashTransferRequests[0].SectorRun.StartSector,
                        (DWORD)pFlashTransferRequests[dwRequestCount-1].SectorRun.StartSector + 
                        pFlashTransferRequests[dwRequestCount-1].SectorRun.SectorCount);
                    goto done;
                }
                // We found at least 2 sectors eligible for read
                fSectorsWritten = TRUE;
            }
            else if (fMultiSectorWrites && fDiscontiguousWrites)
            {
                // Issue writes for discontiguous groups of sectors
                ULONGLONG ullStartSector = blockRun.StartBlock * ulSectorsPerBlock;
                DWORD dwSectorCount = (ulSectorsPerBlock * ulNextBlockRunSize);
                // If there are fewer than 3 sectors, this iteration does not apply
                if (dwSectorCount < 3)
                {
                    continue;
                }
                // In each request read at most 1/4th of the total sector count
                DWORD dwSectorsPerRequest = max(1, dwSectorCount / 4);
                DWORD dwRequestCount = 2;
                DWORD dwStepSize = dwSectorCount / 2;
                for (DWORD dwIndex = 0; dwIndex < dwRequestCount; dwIndex++)
                {
                    pFlashTransferRequests[dwIndex].SectorRun.StartSector =
                        ullStartSector + (dwIndex * dwStepSize);
                    pFlashTransferRequests[dwIndex].SectorRun.SectorCount = dwSectorsPerRequest;
                }
                // Use the WriteReadVerify() routine to write and verify the current sectors
                if (!WriteReadVerify(g_hDevice,
                    pFlashTransferRequests,
                    sizeof(FLASH_PDD_TRANSFER) * dwRequestCount,
                    dwDataBufferSize,
                    ulDataBytesPerSector * dwSectorsPerRequest))
                {
                    g_pKato->Log(LOG_FAIL,
                        _T("Write failed for region %u, sectors %u - %u"),
                        dwFlashRegionIndex,
                        (DWORD)pFlashTransferRequests[0].SectorRun.StartSector,
                        (DWORD)pFlashTransferRequests[dwRequestCount-1].SectorRun.StartSector + 
                        pFlashTransferRequests[dwRequestCount-1].SectorRun.SectorCount);
                    goto done;
                }
                // We found at least one disjoint set of sectors to write
                fSectorsWritten = TRUE;
            }
        }

        // Check if any eligible blocks were found in the region
        if (!fSectorsWritten)
        {
            g_pKato->Log(LOG_FAIL,
                _T("No sectors were written because no valid sector ranges of suffient size were discovered in region %u"),
                dwFlashRegionIndex);
            goto done;
        }

        // Clear the block status table and sector buffers
        CHECK_FREE(pAllBlocksStatusBuffer);
        CHECK_FREE(pSectorData);
        CHECK_FREE(pSectorSpare);
    }

    // Print success message
    g_pKato->Log(LOG_DETAIL, _T("The IOCTL_FLASH_PDD_WRITE_PHYSICAL_SECTORS requests were successful"));

    // success
    dwRetVal = TPR_PASS;
done:
    CHECK_FREE(pSectorData);
    CHECK_FREE(pSectorSpare);
    CHECK_FREE(pAllBlocksStatusBuffer);
    CHECK_FREE(pFlashTransferRequests);
    return dwRetVal;
}

// ----------------------------------------------------------------------------
// This test case verifies the IOCTL_FLASH_PDD_ERASE_BLOCKS IOCTL.
// The erases are configured for single block runs, multiple block runs,
// or multiple discontiguous blocks runs, based on the test parameters
//
// The test succeeds if the following conditions are met:
//
// 1) Every DeviceIoControl succeeds.
//
// 2) All groupings of "good" blocks can be succesfully erased
//
// 3) All byte values in the sectors of the erased blocks are 0xFF (1's)
// ----------------------------------------------------------------------------
TESTPROCAPI Tst_EraseBlocks(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    UNREFERENCED_PARAMETER(tpParam);
    DWORD dwRetVal = TPR_FAIL;
    BOOL fMultiBlockErases = FALSE;
    BOOL fDiscontiguousErases = FALSE;
    PULONG pAllBlocksStatusBuffer = NULL;
    BLOCK_RUN blockRun = {0};
    BLOCK_RUN * pBlockRun = NULL;
    BOOL fBlocksErased = FALSE;
    DWORD dwRequestCount = 0;

    if( TPM_EXECUTE != uMsg )
    {
        return TPR_NOT_HANDLED;
    }

    // Check for the user test consent flag
    if (!TestConsent(lpFTE->dwUniqueID))
    {
        dwRetVal = TPR_SKIP;
        goto done;
    }

    // Check if the test volume has been successfully initialized.
    if (!InitVolume())
    {
        SKIP("The volume has not been successfully initialized, skipping test case");
        dwRetVal = TPR_SKIP;
        goto done;
    }

    // Read the tux parameter that specifies whether this test should
    // erase single, multiple, or multiple sets of block
    if (SINGLE_UNIT_REQUEST == (DWORD)lpFTE->dwUserData)
    {
        fMultiBlockErases = FALSE;
        fDiscontiguousErases = FALSE;
    }
    else if (MULTI_UNIT_REQUEST == (DWORD)lpFTE->dwUserData)
    {
        fMultiBlockErases = TRUE;
        fDiscontiguousErases = FALSE;
    }
    else if (MULTI_UNIT_SPARSE_REQUEST == (DWORD)lpFTE->dwUserData)
    {
        fMultiBlockErases = TRUE;
        fDiscontiguousErases = TRUE;
    }

    // Allocate a block run array for up to MAX_TRANSFER_REQUESTS
    pBlockRun = (BLOCK_RUN*) malloc(MAX_TRANSFER_REQUESTS * sizeof(BLOCK_RUN));
    if (NULL == pBlockRun)
    {
        FAIL("Out of memory");
        goto done;
    }

    // Iterate over all the regions on the flash device. The information about
    // the region geometries is stored in global variables set in InitVolume()
    for (DWORD dwFlashRegionIndex = 0;
        dwFlashRegionIndex < g_dwFlashRegionCount;
        dwFlashRegionIndex++)
    {  
        // Get geometry for this region
        ULONGLONG ullBlockCount = g_pFlashRegionInfoTable[dwFlashRegionIndex].BlockCount;
        ULONGLONG ullCurrentBlock = 0;

        // Get a block status array for this region
        if (!GetStatusForAllBlocks(g_hDevice,
            g_pFlashRegionInfoTable[dwFlashRegionIndex],
            dwFlashRegionIndex,
            &pAllBlocksStatusBuffer))
        {
            g_pKato->Log(LOG_FAIL,
                _T("Failed to get block status for blocks in region %u"),
                dwFlashRegionIndex);
            goto done;
        }

        // Initialize common portions of the block run array for erase requests
        for(DWORD dwIndex = 0; dwIndex < MAX_TRANSFER_REQUESTS; dwIndex++)
        {
            pBlockRun[dwIndex].RegionIndex = dwFlashRegionIndex;
        }

        // Keep track of percentage for current region
        ULONGLONG ullPercent = 0;
        g_pKato->Log(LOG_DETAIL,
            "erasing all good blocks in region %u",
            dwFlashRegionIndex);

        // Iterate over all the blocks
        while(ullCurrentBlock < ullBlockCount)
        {
            // Display progress
            ShowProgress(&ullPercent, ullCurrentBlock, ullBlockCount);

            ULONG ulNextBlockRunSize =
                GetGoodBlockRun(((fMultiBlockErases)? MAX_BLOCK_RUN_SIZE : 1),
                ullBlockCount,
                &ullCurrentBlock,
                pAllBlocksStatusBuffer,
                &blockRun,
                FALSE);

            // If ulNextBlockRunSize is 0 that means we reached the end of the flash
            // and no more eligible block runs have been found. Break out of the loop
            if (!ulNextBlockRunSize)
            {
                break;
            }

            // Pick sets of blocks to read
            if (!fMultiBlockErases && !fDiscontiguousErases)
            {
                // Erase one block
                dwRequestCount = 1;
                pBlockRun[0].BlockCount = 1;
                pBlockRun[0].StartBlock = blockRun.StartBlock;
            }
            else if (fMultiBlockErases && !fDiscontiguousErases)
            {
                // Make sure there are at least 2 blocks in the run
                if (ulNextBlockRunSize < 2)
                {
                    continue;
                }

                // Erase pairs of blocks in the run
                dwRequestCount = 0;
                ULONG ulBlocksRemaining = ulNextBlockRunSize;
                for (DWORD dwIndex = 0;
                    ((dwIndex < MAX_TRANSFER_REQUESTS) && (ulBlocksRemaining));
                    dwIndex++,dwRequestCount++)
                {
                    ULONG ulRunSize = min(2, ulBlocksRemaining);
                    pBlockRun[dwIndex].BlockCount = ulRunSize;
                    pBlockRun[dwIndex].StartBlock = blockRun.StartBlock + (dwIndex * 2);
                    ulBlocksRemaining -= ulRunSize;
                }
            }
            else if (fMultiBlockErases && fDiscontiguousErases)
            {
                // Grab at least 5 blocks
                if (ulNextBlockRunSize < 5)
                {
                    continue;
                }

                // Erase blocks the first two and the last two blocks in the group of 5
                // skipping over the middle block
                dwRequestCount = 2;
                pBlockRun[0].StartBlock = blockRun.StartBlock;
                pBlockRun[0].BlockCount = 2;
                pBlockRun[1].StartBlock = blockRun.StartBlock + 3;
                pBlockRun[1].BlockCount = 2;
            }

            // Erase the chosen blocks
            if (!EraseAndVerify(g_hDevice,
                g_pFlashRegionInfoTable[dwFlashRegionIndex],
                pBlockRun,
                sizeof(BLOCK_RUN) * dwRequestCount))
            {
                g_pKato->Log(LOG_FAIL,
                    _T("Erase failed for region %u, blocks %u - %u"),
                    dwFlashRegionIndex, 
                    (DWORD)pBlockRun[0].StartBlock,
                    (DWORD)(pBlockRun[dwRequestCount-1].StartBlock + pBlockRun[dwRequestCount-1].BlockCount - 1));
                goto done;
            }

            // Set the current block to the next block after the erase
            ullCurrentBlock = pBlockRun[dwRequestCount-1].StartBlock + pBlockRun[dwRequestCount-1].BlockCount;

            // Record that at least one set of eligible blocks was erased successfully
            fBlocksErased = TRUE;
        }

        // Check if any eligible blocks were found in the region
        if (!fBlocksErased)
        {
            g_pKato->Log(LOG_FAIL,
                _T("No blocks were erased because no valid 'good' block groups were found in region %u"),
                dwFlashRegionIndex);
            goto done;
        }

        // Clear the block status table and sector buffers
        CHECK_FREE(pAllBlocksStatusBuffer);
    }

    // Print success message
    g_pKato->Log(LOG_DETAIL, _T("The IOCTL_FLASH_PDD_ERASE_BLOCKS requests were successful"));

    // success
    dwRetVal = TPR_PASS;
done:
    CHECK_FREE(pAllBlocksStatusBuffer);
    CHECK_FREE(pBlockRun);
    return dwRetVal;
}


// ----------------------------------------------------------------------------
// This test case verifies the IOCTL_FLASH_PDD_COPY_PHYSICAL_SECTORS IOCTL.
//
// The test succeeds if the following conditions are met:
//
// 1) Every DeviceIoControl succeeds.
//
// 2) If a region does not advertize the IOCTL, the call must return FALSE
// with GetLastError() == ERROR_NOT_SUPPORTED
//
// 3) If a region supports the IOCTL, every copy request should succeed.
// ----------------------------------------------------------------------------
TESTPROCAPI Tst_CopySectors(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    UNREFERENCED_PARAMETER(tpParam);
    DWORD dwRetVal = TPR_FAIL;
    PULONG pAllBlocksStatusBuffer = 0;
    FLASH_PDD_COPY * pFlashPddCopy = NULL;
    BLOCK_RUN blockRun = {0};
    BOOL fSectorsCopied = FALSE;

    if( TPM_EXECUTE != uMsg )
    {
        return TPR_NOT_HANDLED;
    }

    // Check for the user test consent flag
    if (!TestConsent(lpFTE->dwUniqueID))
    {
        dwRetVal = TPR_SKIP;
        goto done;
    }

    // Check if the test volume has been successfully initialized.
    if (!InitVolume())
    {
        SKIP("The volume has not been successfully initialized, skipping test case");
        dwRetVal = TPR_SKIP;
        goto done;
    }

    // Allocate a FLASH_PDD_COPY array for up to MAX_TRANSFER_REQUESTS
    pFlashPddCopy = (FLASH_PDD_COPY*) malloc(MAX_TRANSFER_REQUESTS * sizeof(FLASH_PDD_COPY));
    if (NULL == pFlashPddCopy)
    {
        FAIL("Out of memory");
        goto done;
    }

    // Iterate over all the regions on the flash device. The information about
    // the region geometries is stored in global variables set in InitVolume()
    for (DWORD dwFlashRegionIndex = 0;
        dwFlashRegionIndex < g_dwFlashRegionCount;
        dwFlashRegionIndex++)
    {
        // Check if the region supports does not support the copy operation
        if (!(g_pFlashRegionInfoTable[dwFlashRegionIndex].FlashFlags & FLASH_FLAG_SUPPORTS_COPY))
        {
            SECTOR_RUN sourceRun = {0};
            SECTOR_RUN destRun = {0};
            sourceRun.SectorCount = 1;
            sourceRun.StartSector = 0;
            destRun.SectorCount = 1;
            sourceRun.StartSector = 1;
            pFlashPddCopy[0].RegionIndex = dwFlashRegionIndex;
            pFlashPddCopy[0].SourceSectorRun = sourceRun;
            pFlashPddCopy[0].DestinationSectorRun = destRun;
            // Call the CopySectors() helper to check that it correctly captures ERROR_NOT_SUPPORTED
            if (!CopySectors(g_hDevice,
                pFlashPddCopy,
                sizeof(FLASH_PDD_COPY),
                FALSE))
            {
                g_pKato->Log(LOG_FAIL,
                    _T("Region %u does not advertize FLASH_FLAG_SUPPORTS_COPY IOCTL but appears to support it"),
                    dwFlashRegionIndex);
                goto done;
            }
            g_pKato->Log(LOG_DETAIL,
                _T("Region %u does not support the FLASH_FLAG_SUPPORTS_COPY IOCTL"),
                dwFlashRegionIndex);
            continue;
        }

        // Get geometry for this region
        ULONGLONG ullBlockCount = g_pFlashRegionInfoTable[dwFlashRegionIndex].BlockCount;
        ULONGLONG ullCurrentBlock = 0;
        ULONG ulSectorsPerBlock = g_pFlashRegionInfoTable[dwFlashRegionIndex].SectorsPerBlock;

        // Sanity check the geometry
        if ((ulSectorsPerBlock  < 1) || (ullBlockCount < 1))
        {
            g_pKato->Log(LOG_FAIL,
                _T("Invalid flash geometry in region %u"),
                dwFlashRegionIndex);
            goto done;
        }

        // Get a block status array for this region
        if (!GetStatusForAllBlocks(g_hDevice,
            g_pFlashRegionInfoTable[dwFlashRegionIndex],
            dwFlashRegionIndex,
            &pAllBlocksStatusBuffer))
        {
            g_pKato->Log(LOG_FAIL,
                _T("Failed to get block status for blocks in region %u"),
                dwFlashRegionIndex);
            goto done;
        }

        // Initialize common portions of the copy request
        for (DWORD dwIndex = 0; dwIndex < MAX_TRANSFER_REQUESTS; dwIndex++)
        {
            pFlashPddCopy[dwIndex].RegionIndex = dwFlashRegionIndex;
        }

        // Keep track of percentage for current region
        ULONGLONG ullPercent = 0;
        g_pKato->Log(LOG_DETAIL,
            "Copying sectors in region %u",
            dwFlashRegionIndex);

        // Iterate over all the blocks
        while(ullCurrentBlock < ullBlockCount)
        {
            // Display progress
            ShowProgress(&ullPercent, ullCurrentBlock, ullBlockCount);

            // Try to find at least a pair of good blocks
            ULONG ulNextBlockRunSize =
                GetGoodBlockRun(2,
                ullBlockCount,
                &ullCurrentBlock,
                pAllBlocksStatusBuffer,
                &blockRun,
                FALSE);

            // If ulNextBlockRunSize is 0 that means we reached the end of the flash
            // and no more eligible block runs have been found. Break out of the loop
            if (!ulNextBlockRunSize)
            {
                break;
            }

            // We don't want to copy within a block, so we need at least 2
            if (ulNextBlockRunSize < 2)
            {
                continue;
            }

            // Copy pairs of sectors from the first block to the second block
            DWORD dwRequestCount = 0;
            ULONGLONG ullSourceStartSector = (blockRun.StartBlock) * ulSectorsPerBlock;
            ULONGLONG ullDestinationStartSector = ullSourceStartSector + ulSectorsPerBlock;
            for(ULONGLONG ullCurrentSector = 0;
                ((ullCurrentSector < (ulSectorsPerBlock-1)) && (dwRequestCount < MAX_TRANSFER_REQUESTS));
                ullCurrentSector+=2,dwRequestCount++)
            {
                pFlashPddCopy[dwRequestCount].SourceSectorRun.StartSector = ullSourceStartSector + ullCurrentSector;
                pFlashPddCopy[dwRequestCount].SourceSectorRun.SectorCount = 2;
                pFlashPddCopy[dwRequestCount].DestinationSectorRun.StartSector = ullDestinationStartSector + ullCurrentSector;
                pFlashPddCopy[dwRequestCount].DestinationSectorRun.SectorCount = 2;
            }

            // Execute the copy request
            if (!WriteCopyAndVerify(g_hDevice,
                g_pFlashRegionInfoTable[dwFlashRegionIndex],
                pFlashPddCopy,
                dwRequestCount * sizeof(FLASH_PDD_COPY)))
            {
                g_pKato->Log(LOG_FAIL,
                    _T("Sector copy failed in region %u"),
                    dwFlashRegionIndex);
                goto done;
            }
            fSectorsCopied = TRUE;
        }

        // Check if any eligible blocks were found in the region
        if (!fSectorsCopied)
        {
            g_pKato->Log(LOG_FAIL,
                _T("No sectors were found to be copied in region %u"),
                dwFlashRegionIndex);
            goto done;
        }
        // Clear the block status table
        CHECK_FREE(pAllBlocksStatusBuffer);
    }

    // Print success message
    g_pKato->Log(LOG_DETAIL, _T("The IOCTL_FLASH_PDD_COPY_PHYSICAL_SECTORS requests were successful"));

    // success
    dwRetVal = TPR_PASS;
done:;
    CHECK_FREE(pAllBlocksStatusBuffer);
    CHECK_FREE(pFlashPddCopy);
    return dwRetVal;
}