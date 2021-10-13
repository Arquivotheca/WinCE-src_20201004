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
#include "volume.h"
#include "globals.h"
#include <bcrypt.h>

// ----------------------------------------------------------------------------
// Initializes the flash volume
// ----------------------------------------------------------------------------
BOOL InitVolume()
{
    BOOL fRet = FALSE;
    HANDLE hDevice = INVALID_HANDLE_VALUE;
    CFSInfo * pFSInfo = NULL;
    LPTSTR szDeviceName = NULL;
    DWORD dwFlashRegionCount = 0;
    DWORD dwFlashRegionInfoTableSize = 0;

    // If the InitVolume function has already run, return the previous result
    if (g_fVolumeInitPerformed)
    {
        fRet = (g_hDevice != INVALID_HANDLE_VALUE);
        goto done;
    }

    // Get a handle to the flash device
    if (g_fPDDLibrarySpecified)
    {
        hDevice = LoadDevice(g_szPDDLibraryName, g_szPDDRegistryKey, &g_hActivateHandle);
    }
    else if (g_fDeviceNameSpecified)
    {
        hDevice = OpenDiskHandleByDiskName(g_pKato, g_fUseOpenStore, g_szDeviceName);
    }
    else if (g_fProfileSpecified)
    {
        hDevice = OpenDiskHandleByProfile(
            g_pKato, g_fUseOpenStore, g_szDeviceProfile, FALSE, g_szDeviceName, MAX_PATH);
    }
    else if (g_fPathSpecified)
    {
        // Use a store helper object to determine the name of this device
        pFSInfo = new CFSInfo(g_szPath);
        if (!VALID_POINTER(pFSInfo))
        {
            LOG(_T("Error : InitVolume() : could not create CFSInfo object"));
            goto done;
        }
        // Retrieve device information
        if (pFSInfo->StoreInfo())
        {
            szDeviceName = (pFSInfo->StoreInfo())->szDeviceName;
        }
        if (!VALID_POINTER(szDeviceName))
        {
            LOG(_T("Error : InitVolume() : could not retrieve device name from path %s"), g_szPath);
            goto done;
        }
        // Save the device name in global variable
        VERIFY(SUCCEEDED(StringCchCopy(g_szDeviceName, MAX_PATH, szDeviceName)));
        // Use device name to get handle
        hDevice = OpenDiskHandleByDiskName(g_pKato, g_fUseOpenStore, g_szDeviceName);
    }
    else
    {
        LOG(_T("Error : InitVolume() : no device profile, name, or path specified"));
        goto done;
    }

    // Check if the handle is valid
    if (INVALID_HANDLE(hDevice))
    {
        LOG(_T("Error : InitVolume() : could not get handle to device"));
        if (!g_fUseOpenStore && !g_fPDDLibrarySpecified)
        {
            LOG(_T("Info : InitVolume() : try using the /%s option to use OpenStore()"), FLAG_USE_OPEN_STORE);
        }
        goto done;
    }

    // Set the global handle
    g_hDevice = hDevice;

    // Get the number of regions on this flash device
    if (!GetFlashRegionCount(g_hDevice, &dwFlashRegionCount))
    {
        LOG(_T("Error : InitVolume() : failed to query the number of regions"));
        goto done;
    }
    if(!dwFlashRegionCount)
    {
        LOG(_T("Error : InitVolume() : The flash region count reported by IOCTL_FLASH_PDD_GET_REGION_COUNT was 0"));
        // Invalidate global handle if we there is an invalid number of regions reported.
        g_hDevice = INVALID_HANDLE_VALUE;
        goto done;
    }

    // Save region info to global variables
    g_dwFlashRegionCount = dwFlashRegionCount;
        // Allocate table to hold flash region info
    dwFlashRegionInfoTableSize = dwFlashRegionCount * sizeof(FLASH_REGION_INFO);
    g_pFlashRegionInfoTable = (FLASH_REGION_INFO*)malloc(dwFlashRegionInfoTableSize);
    if (NULL == g_pFlashRegionInfoTable)
    {
        LOG(_T("Error : InitVolume() : out of memory"));
        goto done;
    }

    // Retrieve region / partition information
    if (!GetFlashRegionInfo(g_hDevice, g_pFlashRegionInfoTable, dwFlashRegionInfoTableSize))
    {
        LOG(_T("Error : InitVolume() : GetFlashRegionInfo() failed"));
        // Invalidate global handle if we cannot read region information.
        g_hDevice = INVALID_HANDLE_VALUE;
        goto done;
    }

    // Display the region info
    ShowFlashRegionInfo(g_dwFlashRegionCount, g_pFlashRegionInfoTable);

    fRet = TRUE;
done:
    // Record that the InitVolume function was called, regardless of success
    g_fVolumeInitPerformed = TRUE;
    CHECK_DELETE(pFSInfo);
    return fRet;
}

// ----------------------------------------------------------------------------
// The function loads the Flash PDD drive directly without loading the MDD
// layer. The function first overwrites the Dll value in the registry settings
// that configure the PDD driver, then calls ActivateDevice.
// ----------------------------------------------------------------------------
HANDLE LoadDevice(
    IN LPTSTR szPDDLibraryName,
    IN LPTSTR szPDDRegistryKey,
    IN OUT PHANDLE phActivateHandle)
{
    HANDLE hActivateHandle = INVALID_HANDLE_VALUE;
    HANDLE hDevice = INVALID_HANDLE_VALUE;
    HANDLE hMsgQueue = INVALID_HANDLE_VALUE;
    HANDLE hNotifications = INVALID_HANDLE_VALUE;
    DWORD dwQueueData[QUEUE_ITEM_SIZE/sizeof(DWORD)] = {0};
    DWORD dwRead = 0;
    DWORD dwFlags = 0;
    DEVDETAIL *pDevDetail = (DEVDETAIL*)dwQueueData;
    MSGQUEUEOPTIONS msgqopts = {0};
    msgqopts.dwSize = sizeof(MSGQUEUEOPTIONS);
    msgqopts.dwFlags = 0;
    msgqopts.cbMaxMessage = QUEUE_ITEM_SIZE;
    msgqopts.bReadAccess = TRUE;

    // Create a message Queue
    hMsgQueue = CreateMsgQueue(NULL, &msgqopts);
    if (INVALID_HANDLE(hMsgQueue))
    {
        Debug(_T("Error :WaitForMount() : CreateMsgQueue() failed with error = %u"), GetLastError());
        goto done;
    }
    // register for FLASH_PDD_MOUNT_GUID
    hNotifications = RequestDeviceNotifications(&FLASH_PDD_MOUNT_GUID, hMsgQueue, FALSE);
    if (INVALID_HANDLE(hNotifications))
    {
        Debug(_T("Error : WaitForMount() : RequestDeviceNotifications() failed with error = %u"), GetLastError());
        goto done;
    }

    // Overwrite the Dll value in the driver's registry key configuration
    if (!WriteRegistryStringValue(szPDDRegistryKey, _T("Dll"), szPDDLibraryName))
    {
        LOG(_T("Error : LoadDevice() : WriteRegistryStringValue() failed"));
        goto done;
    }

    // Overwrite the IClass value in the driver's registry key configuration
    // This custom value will let the test to asynchronously detect
    // when the device has been fully activated
    if (!WriteRegistryMultiStringValue(szPDDRegistryKey, _T("IClass"), FLASH_PDD_MOUNT_GUID_STRING))
    {
        LOG(_T("Error : LoadDevice() : WriteRegistryStringValue() failed"));
        goto done;
    }

    // Call activate device on the PDD's registry key
    hActivateHandle = ActivateDevice(szPDDRegistryKey, 0);
    if (!hActivateHandle)
    {
        LOG(_T("Error : LoadDevice() : ActivateDevice() failed"));
        goto done;
    }
    *phActivateHandle = hActivateHandle;

    BOOL fDeviceFound = FALSE;
    // Wait until the FLASH_PDD_MOUNT_GUID is signaled for the FileFlash device
    while (ReadMsgQueue(hMsgQueue, pDevDetail, QUEUE_ITEM_SIZE, &dwRead, 30000, &dwFlags))
    {
        if (pDevDetail->fAttached)
        {
            fDeviceFound = TRUE;
            break;
        }
    }
    // Make sure the GUID was advertized
    if (!fDeviceFound)
    {
        LOG(_T("Error : LoadDevice() : failed to get device notification"));
        goto done;
    }

    // Use the name of the device to get a handle
    hDevice = CreateFile(pDevDetail->szName,
        GENERIC_READ|GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (INVALID_HANDLE_VALUE == hDevice)
    {
        LOG(_T("Error : LoadDevice() : CreateFile() failed"));
        goto done;
    }

done:
    CHECK_CLOSE_HANDLE(hMsgQueue);
    CHECK_STOP_DEVICE_NOTIFICATIONS(hNotifications);
    return hDevice;
}

// ----------------------------------------------------------------------------
// Queries the number of regions on the flash device.
// ----------------------------------------------------------------------------
BOOL GetFlashRegionCount(
    IN HANDLE hDevice,
    OUT LPDWORD lpdwFlashRegionCount)
{
    BOOL fRet = FALSE;
    DWORD dwFlashRegionCount = 0;

    // Query device for the number of regions
    if (!DeviceIoControl(hDevice,
        IOCTL_FLASH_PDD_GET_REGION_COUNT,
        NULL,
        0,
        &dwFlashRegionCount,
        sizeof(DWORD),
        NULL,
        NULL))
    {
        LOG(_T("Error : GetFlashRegionCount() : DeviceIoControl(IOCTL_FLASH_PDD_GET_REGION_COUNT) failed with error = %u"),
            GetLastError());
        goto done;
    }

    // Store the result
    *lpdwFlashRegionCount = dwFlashRegionCount;

    fRet = TRUE;
done:
    return fRet;
}

// ----------------------------------------------------------------------------
// Gets information about the regions on the flash device.
// ----------------------------------------------------------------------------
BOOL GetFlashRegionInfo(
    IN HANDLE hDevice,
    OUT FLASH_REGION_INFO * pFlashRegionInfoTable,
    IN DWORD dwFlashRegionInfoTableSize)
{
    BOOL fRet = FALSE;

    // Get the region table
    if (!DeviceIoControl(hDevice,
        IOCTL_FLASH_PDD_GET_REGION_INFO,
        NULL,
        0,
        pFlashRegionInfoTable,
        dwFlashRegionInfoTableSize,
        NULL,
        NULL))
    {
        LOG(_T("Error : GetFlashRegionInfo() : DeviceIoControl(IOCTL_FLASH_PDD_GET_REGION_INFO) failed with error = %u"),
            GetLastError());
        goto done;
    }

    fRet = TRUE;
done:
    return fRet;
}

// ----------------------------------------------------------------------------
// Prints region info from FLASH_REGION_INFO table
// ----------------------------------------------------------------------------
void ShowFlashRegionInfo(
    IN DWORD dwFlashRegionCount,
    IN FLASH_REGION_INFO * pFlashRegionInfoTable)
{
    FLASH_REGION_INFO flashRegionInfo = {0};
    LOG(_T("======================="));
    LOG(_T("Region Count = %u"), dwFlashRegionCount);
    for (DWORD dwRegionTableEntry = 0; dwRegionTableEntry < dwFlashRegionCount; dwRegionTableEntry++)
    {
        flashRegionInfo = pFlashRegionInfoTable[dwRegionTableEntry];
        LOG(_T("-----------------------"));
        LOG(_T("\tRegion Number = %u"), dwRegionTableEntry);
        LOG(_T("\tStartBlock = %u, FlashFlags = %u"),
            (DWORD) flashRegionInfo.StartBlock,
            flashRegionInfo.FlashFlags);
        LOG(_T("\tBlockCount = %u, SectorsPerBlock = %u"),
            (DWORD) flashRegionInfo.BlockCount,
            flashRegionInfo.SectorsPerBlock);
        LOG(_T("\tDataBytesPerSector = %u, SpareBytesPerSector = %u"),
            flashRegionInfo.DataBytesPerSector,
            flashRegionInfo.SpareBytesPerSector);
        LOG(_T("\tPageProgramLimit = %u, BadBlockHundredthPercent = %u"),
            flashRegionInfo.PageProgramLimit,
            flashRegionInfo.BadBlockHundredthPercent);
    }
}

// ----------------------------------------------------------------------------
// Gets block status information
// ----------------------------------------------------------------------------
BOOL GetBlockStatus(
    IN HANDLE hDevice,
    IN FLASH_GET_BLOCK_STATUS_REQUEST * pBlockStatusRequest,
    OUT PULONG pStatusBuffer,
    IN DWORD dwStatusBufferSize)
{
    BOOL fRet = FALSE;

    // Get the block status
    if (!DeviceIoControl(hDevice,
        IOCTL_FLASH_PDD_GET_BLOCK_STATUS,
        pBlockStatusRequest,
        sizeof(FLASH_GET_BLOCK_STATUS_REQUEST),
        pStatusBuffer,
        dwStatusBufferSize,
        NULL,
        NULL))
    {
        LOG(_T("Error : GetBlockStatus() : DeviceIoControl(IOCTL_FLASH_PDD_GET_BLOCK_STATUS) failed with error = %u"),
            GetLastError());
        goto done;
    }

    fRet = TRUE;
done:
    return fRet;
}

// ----------------------------------------------------------------------------
// Sets block status information
// ----------------------------------------------------------------------------
BOOL SetBlockStatus(
    IN HANDLE hDevice,
    IN FLASH_SET_BLOCK_STATUS_REQUEST * pBlockStatusRequest)
{
    BOOL fRet = FALSE;

    // Set the block status
    if (!DeviceIoControl(hDevice,
        IOCTL_FLASH_PDD_SET_BLOCK_STATUS,
        pBlockStatusRequest,
        sizeof(FLASH_SET_BLOCK_STATUS_REQUEST),
        NULL,
        0,
        NULL,
        NULL))
    {
        LOG(_T("Error : SetBlockStatus() : DeviceIoControl(IOCTL_FLASH_PDD_SET_BLOCK_STATUS) failed with error = %u"),
            GetLastError());
        goto done;
    }

    fRet = TRUE;
done:
    return fRet;
}

// ----------------------------------------------------------------------------
// Perform physical sector read
// ----------------------------------------------------------------------------
BOOL ReadSectors(
    IN HANDLE hDevice,
    IN FLASH_PDD_TRANSFER * pFlashTransferRequests,
    IN DWORD dwFlashTransferSize)
{
    BOOL fRet = FALSE;
    FLASH_READ_STATUS flashReadStatus;

    // Read sectors
    if (!DeviceIoControl(hDevice,
        IOCTL_FLASH_PDD_READ_PHYSICAL_SECTORS,
        pFlashTransferRequests,
        dwFlashTransferSize,
        &flashReadStatus,
        sizeof(ULONG),
        NULL,
        NULL))
    {
        LOG(_T("Error : ReadSectors() : DeviceIoControl(IOCTL_FLASH_PDD_READ_PHYSICAL_SECTORS) failed with error = %u"),
            GetLastError());
        goto done;
    }

    fRet = TRUE;
done:
    return fRet;
}

// ----------------------------------------------------------------------------
// Perform physical sector write
// ----------------------------------------------------------------------------
BOOL WriteSectors(
    IN HANDLE hDevice,
    IN FLASH_PDD_TRANSFER * pFlashTransferRequests,
    IN DWORD dwFlashTransferSize)
{
    BOOL fRet = FALSE;

    // Write sectors
    if (!DeviceIoControl(hDevice,
        IOCTL_FLASH_PDD_WRITE_PHYSICAL_SECTORS,
        pFlashTransferRequests,
        dwFlashTransferSize,
        NULL,
        0,
        NULL,
        NULL))
    {
        LOG(_T("Error : WriteSectors() : DeviceIoControl(IOCTL_FLASH_PDD_WRITE_PHYSICAL_SECTORS) failed with error = %u"),
            GetLastError());
        goto done;
    }

    fRet = TRUE;
done:
    return fRet;
}

// ----------------------------------------------------------------------------
// Erases the specified runs of blocks
// ----------------------------------------------------------------------------
BOOL EraseBlocks(
    IN HANDLE hDevice,
    IN BLOCK_RUN * pBlockRun,
    IN DWORD dwBlockRunArraySize)
{
    BOOL fRet = FALSE;

    // Erase blocks
    if (!DeviceIoControl(hDevice,
        IOCTL_FLASH_PDD_ERASE_BLOCKS,
        pBlockRun,
        dwBlockRunArraySize,
        NULL,
        0,
        NULL,
        NULL))
    {
        LOG(_T("Error : EraseBlocks() : DeviceIoControl(IOCTL_FLASH_PDD_ERASE_BLOCKS) failed with error = %u"),
            GetLastError());
        goto done;
    }

    fRet = TRUE;
done:
    return fRet;
}

// ----------------------------------------------------------------------------
// Copies the specified range of sectors
// ----------------------------------------------------------------------------
BOOL CopySectors(
    IN HANDLE hDevice,
    IN FLASH_PDD_COPY * pFlashPddCopy,
    IN DWORD dwPddCopyArraySize,
    IN BOOL fCopySupported)
{
    BOOL fRet = FALSE;
    DWORD dwLastError = ERROR_SUCCESS;

    // Copy sectors
    if (!DeviceIoControl(hDevice,
        IOCTL_FLASH_PDD_COPY_PHYSICAL_SECTORS,
        pFlashPddCopy,
        dwPddCopyArraySize,
        NULL,
        0,
        NULL,
        NULL))
    {
        dwLastError = GetLastError();
        // If copy support is not advertized for this region ensure that GetLastError() returns ERROR_NOT_SUPPORTED
        if (!fCopySupported)
        {
            if (ERROR_NOT_SUPPORTED != dwLastError)
            {
                LOG(_T("Error : CopySectors() : DeviceIoControl(IOCTL_FLASH_PDD_COPY_PHYSICAL_SECTORS) failed with error = %u, instead of %u (ERROR_NOT_SUPPORTED"),
                    dwLastError,
                    ERROR_NOT_SUPPORTED);
                goto done;
            }
            else
            {
                fRet = TRUE;
                goto done;
            }
        }
        LOG(_T("Error : CopySectors() : DeviceIoControl(IOCTL_FLASH_PDD_COPY_PHYSICAL_SECTORS) failed with error = %u"),
            dwLastError);
        goto done;
    }
    else
    {
        // If the operation succeeded, ensure that copy support was advertized
        if (!fCopySupported)
        {
            LOG(_T("Error : CopySectors() : DeviceIoControl(IOCTL_FLASH_PDD_COPY_PHYSICAL_SECTORS) succeeded, however the region does not advertize FLASH_FLAG_SUPPORTS_COPY"));
            goto done;
        }
    }

    fRet = TRUE;
done:
    return fRet;
}

// ----------------------------------------------------------------------------
// Creates a block status array and populates with with the status
// of all blocks in the specified region
// ----------------------------------------------------------------------------
BOOL GetStatusForAllBlocks(
    IN HANDLE hDevice,
    IN FLASH_REGION_INFO flashRegionInfo,
    IN DWORD dwFlashRegionIndex,
    IN OUT PULONG * ppulStatusBuffer)
{
    BOOL fRet = FALSE;
    PULONG pAllBlocksStatusBuffer = NULL;
    FLASH_GET_BLOCK_STATUS_REQUEST blockGetStatusRequest = {0};
    ULONGLONG ullBlockCount = flashRegionInfo.BlockCount;
    *ppulStatusBuffer = NULL;

    // Allocate table large enough to hold statuses for all blocks in the current region
    DWORD dwBufferSize = 0;
    if (S_OK != DWordMult((DWORD)ullBlockCount, sizeof(ULONG), &dwBufferSize))
    {
        LOG(_T("Error : GetStatusForAllBlocks() : Integer overflow while calculating buffer size"));
        goto done;
    }
    pAllBlocksStatusBuffer = (PULONG) malloc(dwBufferSize);
    if (NULL == pAllBlocksStatusBuffer)
    {
        LOG(_T("Error : GetStatusForAllBlocks() : out of memory!"));
        goto done;
    }
    *ppulStatusBuffer = pAllBlocksStatusBuffer;

    // Initialize structure to read status information for all blocks in current region
    blockGetStatusRequest.IsInitialFlash = FALSE;
    blockGetStatusRequest.BlockRun.RegionIndex = dwFlashRegionIndex;
    blockGetStatusRequest.BlockRun.StartBlock = 0;
    blockGetStatusRequest.BlockRun.BlockCount = (ULONG)(ullBlockCount);
    // Query the current region
    if (!GetBlockStatus(hDevice,
        &blockGetStatusRequest,
        pAllBlocksStatusBuffer,
        ((DWORD)ullBlockCount * sizeof(ULONG))))
    {
        LOG(_T("Error : GetStatusForAllBlocks() : The block status query failed for region %u, block range %u - %u"),
            dwFlashRegionIndex,
            blockGetStatusRequest.BlockRun.StartBlock,
            blockGetStatusRequest.BlockRun.StartBlock + (ullBlockCount - 1));
        goto done;
    }

    fRet = TRUE;
done:
    return fRet;
}

// ----------------------------------------------------------------------------
// Performs specified write then verifies the written data.
// The function assumes that all write requests have sequential pointers into
// a shared data buffer, and that each PDD_TRANSFER request writes out the
// same amount of data.
// ----------------------------------------------------------------------------
BOOL WriteReadVerify(
    IN HANDLE hDevice,
    IN FLASH_PDD_TRANSFER * pWriteRequest,
    IN DWORD dwRequestSize,
    IN DWORD dwDataBufferSize,
    IN DWORD dwBytesPerRequest)
{
    BOOL fRet = FALSE;
    FLASH_PDD_TRANSFER * pReadRequest = NULL;
    DWORD dwRequestCount = (dwRequestSize / sizeof(FLASH_PDD_TRANSFER));
    PBYTE pReadData = NULL;

    // Allocate corresponding read request
    pReadRequest = (FLASH_PDD_TRANSFER*) malloc(dwRequestSize);
    if (NULL == pReadRequest)
    {
        LOG(_T("Error: WriteReadVerify(): out of memory."));
        goto done;
    }

    // Allocate read buffer
    pReadData = (PBYTE) malloc(dwDataBufferSize);
    if (NULL == pReadData)
    {
        LOG(_T("Error: WriteReadVerify(): out of memory."));
        goto done;
    }

    // Copy write request into the read request
    memcpy(pReadRequest, pWriteRequest, dwRequestSize);

    // Update pointers into the read buffer
    for (DWORD dwIndex = 0; dwIndex < dwRequestCount; dwIndex++)
    {
        pReadRequest[dwIndex].pData = pReadData +
            (dwIndex * dwBytesPerRequest);
    }

    // Perform the write to flash
    if (!WriteSectors(hDevice,
        pWriteRequest,
        dwRequestSize))
    {
        LOG(_T("Error : WriteReadVerify() : write failed to region %u"),
            pWriteRequest[0].RegionIndex);
        goto done;
    }

    // Read the data back
    if (!ReadSectors(hDevice,
        pReadRequest,
        dwRequestSize))
    {
        LOG(_T("Error : WriteReadVerify() : read failed to region %u"),
            pReadRequest[0].RegionIndex);
        goto done;
    }

    // Compare the data
    if (0 != memcmp(pReadData, pWriteRequest[0].pData, dwBytesPerRequest))
    {
        LOG(_T("Error : WriteReadVerify() : data read/write data mismatch for region %u"),
            pReadRequest[0].RegionIndex);
        goto done;
    }

    fRet = TRUE;
done:
    CHECK_FREE(pReadRequest);
    CHECK_FREE(pReadData);
    return fRet;
}

// ----------------------------------------------------------------------------
// This function takes a copy request, erases the blocks that house both the
// the source and destination sectors, writes known patterns to source sectors,
// copies them, and verifies that the destination data matches the source
// ----------------------------------------------------------------------------
BOOL WriteCopyAndVerify(
    IN HANDLE hDevice,
    IN FLASH_REGION_INFO flashRegionInfo,
    IN FLASH_PDD_COPY * pFlashPddCopy,
    IN DWORD dwFlashPddCopySize)
{
    BOOL fRet = FALSE;
    DWORD dwNumRequests = dwFlashPddCopySize / sizeof(FLASH_PDD_COPY);
    DWORD dwRegionIndex = pFlashPddCopy[0].RegionIndex;
    BLOCK_RUN * pBlockRun = NULL;
    DWORD dwBlockRunArraySize = 0;
    // Get geometry infromation
    ULONG ulSectorsPerBlock = flashRegionInfo.SectorsPerBlock;
    ULONG ulDataBytesPerSector = flashRegionInfo.DataBytesPerSector;
    ULONG ulSpareBytesPerSector = flashRegionInfo.SpareBytesPerSector;
    // Read & Write request structures
    PBYTE pSectorData = NULL;
    PBYTE pSectorVerifyData = NULL;
    PBYTE pSpareData = NULL;
    PBYTE pSpareVerifyData = NULL;
    FLASH_PDD_TRANSFER flashTransfer = {0};
    flashTransfer.RegionIndex = dwRegionIndex;
    BYTE patternChange = 0;

    // Allocate enough block runs to cover all the erase requests
    // This is 2 runs per copy request (to erase the source and dest blocks)
    dwBlockRunArraySize = sizeof(BLOCK_RUN) * dwNumRequests * 2;
    pBlockRun = (BLOCK_RUN*) malloc(dwBlockRunArraySize);
    if (NULL == pBlockRun)
    {
        LOG(_T("Error : WriteCopyAndVerify() : out of memory"));
        goto done;
    }

    // Allocate data for sector writes/reads
    pSectorData = (PBYTE) malloc(ulDataBytesPerSector);
    if (NULL == pSectorData)
    {
        LOG(_T("Error : WriteCopyAndVerify() : out of memory"));
        goto done;
    }
    pSectorVerifyData = (PBYTE) malloc(ulDataBytesPerSector);
    if (NULL == pSectorVerifyData)
    {
        LOG(_T("Error : WriteCopyAndVerify() : out of memory"));
        goto done;
    }
    pSpareData = (PBYTE) malloc(ulSpareBytesPerSector);
    if (NULL == pSpareData)
    {
        LOG(_T("Error : WriteCopyAndVerify() : out of memory"));
        goto done;
    }
    pSpareVerifyData = (PBYTE) malloc(ulSpareBytesPerSector);
    if (NULL == pSpareVerifyData)
    {
        LOG(_T("Error : WriteCopyAndVerify() : out of memory"));
        goto done;
    }

    // Fill sector data with random data
    BCryptGenRandom(NULL, (PUCHAR) pSectorData, ulDataBytesPerSector, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    BCryptGenRandom(NULL, (PUCHAR) pSpareData, ulSpareBytesPerSector, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    memcpy(pSectorVerifyData, pSectorData, ulDataBytesPerSector);
    memcpy(pSpareVerifyData, pSpareData, ulSpareBytesPerSector);
    flashTransfer.pData = pSectorData;
    flashTransfer.pSpare = pSpareData;

    // Prepare block runs to erase all the blocks that are going to be involved in the copy
    // It is assumed there will be no overlap between source and destination blocks
    for (DWORD dwIndex = 0; dwIndex < dwNumRequests; dwIndex++)
    {
        // Calculate what blocks the source and target sector ranges correspond to
        ULONGLONG ullSourceStartBlock =
            pFlashPddCopy[dwIndex].SourceSectorRun.StartSector / ulSectorsPerBlock;
        ULONGLONG ullSourceEndBlock =
            (pFlashPddCopy[dwIndex].SourceSectorRun.StartSector + pFlashPddCopy[dwIndex].SourceSectorRun.SectorCount - 1) /
            ulSectorsPerBlock;
        ULONGLONG ullDestinationStartBlock =
            pFlashPddCopy[dwIndex].DestinationSectorRun.StartSector / ulSectorsPerBlock;
        ULONGLONG ullDestinationEndBlock =
            (pFlashPddCopy[dwIndex].DestinationSectorRun.StartSector + pFlashPddCopy[dwIndex].DestinationSectorRun.SectorCount - 1) /
            ulSectorsPerBlock;
        pBlockRun[dwIndex * 2].RegionIndex = dwRegionIndex;
        pBlockRun[dwIndex * 2].StartBlock = ullSourceStartBlock;
        pBlockRun[dwIndex * 2].BlockCount =  (ULONG)(ullSourceEndBlock - ullSourceStartBlock + 1); 
        pBlockRun[(dwIndex * 2) + 1].RegionIndex = dwRegionIndex;
        pBlockRun[(dwIndex * 2) + 1].StartBlock = ullDestinationStartBlock;
        pBlockRun[(dwIndex * 2) + 1].BlockCount = (ULONG)(ullDestinationEndBlock - ullDestinationStartBlock + 1); 
    }

    // Erase the blocks
    if (!EraseBlocks(hDevice,
        pBlockRun,
        dwBlockRunArraySize))
    {
        LOG(_T("Error : WriteCopyAndVerify() : EraseBlocks() failed"));
        goto done;
    }

    // Write known pattern data to source sectors
    patternChange = 0;
    for (DWORD dwIndex = 0; dwIndex < dwNumRequests; dwIndex++)
    {
        SECTOR_RUN sectorRun = pFlashPddCopy[dwIndex].SourceSectorRun;
        for (ULONGLONG ulSectorNumber = sectorRun.StartSector;
            ulSectorNumber < (sectorRun.StartSector + sectorRun.SectorCount);
            ulSectorNumber++)
        {
            flashTransfer.SectorRun.SectorCount = 1;
            flashTransfer.SectorRun.StartSector = ulSectorNumber;
            // Modify the pattern slightly for each sector
            pSectorData[0] = patternChange++;
            pSpareData[0] = patternChange++;
            // Execute write request
            if (!WriteSectors(hDevice,
                &flashTransfer,
                sizeof(FLASH_PDD_TRANSFER)))
            {
                LOG(_T("Error : WriteCopyAndVerify() : WriteSectors() failed"));
                goto done;
            }
        }
    }

    // Execute the copy request
    if (!CopySectors(hDevice,
        pFlashPddCopy,
        dwFlashPddCopySize,
        TRUE))
    {
        LOG(_T("Error : WriteCopyAndVerify() : CopySectors() failed"));
        goto done;
    }


    // Read data from destination sectors and verify the pattern
    patternChange = 0;
    flashTransfer.pData = pSectorVerifyData;
    flashTransfer.pSpare = pSpareVerifyData;
    for (DWORD dwIndex = 0; dwIndex < dwNumRequests; dwIndex++)
    {
        SECTOR_RUN sectorRun = pFlashPddCopy[dwIndex].DestinationSectorRun;
        for (ULONGLONG ulSectorNumber = sectorRun.StartSector;
            ulSectorNumber < (sectorRun.StartSector + sectorRun.SectorCount);
            ulSectorNumber++)
        {
            flashTransfer.SectorRun.SectorCount = 1;
            flashTransfer.SectorRun.StartSector = ulSectorNumber;
            // Execute read request
            if (!ReadSectors(hDevice,
                &flashTransfer,
                sizeof(FLASH_PDD_TRANSFER)))
            {
                LOG(_T("Error : WriteCopyAndVerify() : ReadSectors() failed"));
                goto done;
            }

            // Modify the pattern slightly for each sector
            pSectorData[0] = patternChange++;
            pSpareData[0] = patternChange++;

            // Verify the data
            if (0 != memcmp(pSectorData, pSectorVerifyData, ulDataBytesPerSector))
            {
                LOG(_T("Error : WriteCopyAndVerify() : CopySectors() failed : data area mismatch in sector %u"),
                    ulSectorNumber);
                goto done;
            }
            // Verify the data
            if (0 != memcmp(pSpareData, pSpareVerifyData, ulSpareBytesPerSector))
            {
                LOG(_T("Error : WriteCopyAndVerify() : CopySectors() failed : spare area mismatch in sector %u"),
                    ulSectorNumber);
                goto done;
            }
        }
    }

    fRet = TRUE;
done:
    CHECK_FREE(pBlockRun);
    CHECK_FREE(pSectorData);
    CHECK_FREE(pSectorVerifyData);
    CHECK_FREE(pSpareData);
    CHECK_FREE(pSpareVerifyData);
    return fRet;
}

// ----------------------------------------------------------------------------
// Erases the specified blocks, then reads sector data back to ensure
// that the values are all 1's
// ----------------------------------------------------------------------------
BOOL EraseAndVerify(
    IN HANDLE hDevice,
    IN FLASH_REGION_INFO flashRegionInfo,
    IN BLOCK_RUN * pBlockRun,
    IN DWORD dwBlockRunArraySize)
{
    BOOL fRet = FALSE;
    FLASH_PDD_TRANSFER flashTransfer = {0};
    ULONG ulSectorsPerBlock = flashRegionInfo.SectorsPerBlock;
    ULONG ulSpareBytesPerSector = flashRegionInfo.SpareBytesPerSector;
    ULONG ulDataBytesPerSector = flashRegionInfo.DataBytesPerSector;
    DWORD dwRequestSize = dwBlockRunArraySize / sizeof (BLOCK_RUN);
    PBYTE pSectorData = NULL;
    PBYTE pSpareData = NULL;

    // Allocate data for sector reads
    pSectorData = (PBYTE) malloc(ulDataBytesPerSector);
    if (NULL == pSectorData)
    {
        LOG(_T("Error : WriteCopyAndVerify() : out of memory"));
        goto done;
    }
    pSpareData = (PBYTE) malloc(ulSpareBytesPerSector);
    if (NULL == pSpareData)
    {
        LOG(_T("Error : WriteCopyAndVerify() : out of memory"));
        goto done;
    }
    flashTransfer.pData = pSectorData;
    flashTransfer.pSpare = pSpareData;
    flashTransfer.RegionIndex = pBlockRun[0].RegionIndex;
    flashTransfer.SectorRun.SectorCount = 1;

    // Erase the specified blocks
    if (!EraseBlocks(hDevice,
        pBlockRun,
        dwBlockRunArraySize))
    {
        LOG(_T("Error : EraseAndVerify() : EraseBlocks() failed"));
        goto done;
    }

    // Verify every sector in every erased block
    for (DWORD dwIndex = 0; dwIndex < dwRequestSize; dwIndex++)
    {
        ULONGLONG ullSectorCount = pBlockRun[dwIndex].BlockCount * ulSectorsPerBlock;
        ULONGLONG ullStartSector = pBlockRun[dwIndex].StartBlock * ulSectorsPerBlock;
        for (ULONGLONG ullCurrentSector = 0; ullCurrentSector < ullSectorCount; ullCurrentSector++)
        {
            flashTransfer.SectorRun.StartSector = ullCurrentSector + ullStartSector;
            // clear the data and spare buffers
            memset(pSpareData, 0, ulSpareBytesPerSector);
            memset(pSectorData, 0, ulDataBytesPerSector);
            if (!ReadSectors(hDevice, &flashTransfer, sizeof(FLASH_PDD_TRANSFER)))
            {
                LOG(_T("Error : EraseAndVerify() : ReadSectors() failed"));
                goto done;
            }
            // Check that all values are 1's
            for (ULONG ulIndex = 0; ulIndex < ulDataBytesPerSector; ulIndex++)
            {
                if (0xFF != pSectorData[ulIndex])
                {
                    LOG(_T("Error : EraseAndVerify() : sector data in block run %u - %u was not properly erased"),
                        pBlockRun[dwIndex].StartBlock,
                        pBlockRun[dwIndex].StartBlock + pBlockRun[dwIndex].BlockCount - 1);
                    goto done;
                }
            }
            for (ULONG ulIndex = 0; ulIndex < ulSpareBytesPerSector; ulIndex++)
            {
                if (0xFF != pSpareData[ulIndex])
                {
                    LOG(_T("Error : EraseAndVerify() : spare data in block run %u - %u was not properly erased"),
                        pBlockRun[dwIndex].StartBlock,
                        pBlockRun[dwIndex].StartBlock + pBlockRun[dwIndex].BlockCount - 1);
                    goto done;
                }
            }
        }
    }

    fRet = TRUE;
done:
    CHECK_FREE(pSectorData);
    CHECK_FREE(pSpareData);
    return fRet;
}

// ----------------------------------------------------------------------------
// Finds a range of "good" blocks up to specified size
// ----------------------------------------------------------------------------
ULONG GetGoodBlockRun(
    IN DWORD dwRequestedRunSize,
    IN ULONGLONG ullBlockCount,
    IN OUT PULONGLONG pullCurrentBlock,
    IN PULONG pBlockStatusBuffer,
    OUT BLOCK_RUN * pBlockRun,
    IN BOOL fUseReservedSectors)
{
    ULONG ulNextBlockRunSize = 0;
    ULONGLONG ullCurrentBlock = *pullCurrentBlock;
    ULONGLONG ullNextBlockRunStartBlock = *pullCurrentBlock;

    // Select a contiguous group of blocks with a good status
    // If this is a single block test, find the next good block
    while((ulNextBlockRunSize < dwRequestedRunSize) &&
        ((ullNextBlockRunStartBlock + ulNextBlockRunSize) < ullBlockCount))
    {
        ULONG ulCurrentBlockStatus = pBlockStatusBuffer[ullCurrentBlock];
        // Found a bad/reserved block
        if ((FLASH_BLOCK_STATUS_BAD & ulCurrentBlockStatus) ||
            (!fUseReservedSectors && (FLASH_BLOCK_STATUS_RESERVED & ulCurrentBlockStatus)))
        {
            // If we have not yet found any good blocks keep going
            if (!ulNextBlockRunSize)
            {
                // Restart search at next block
                ullCurrentBlock++;
                ullNextBlockRunStartBlock++;
                continue;
            }
            else
            {
                // Use the current non-0 block run size
                break;
            }
        }
        else
        {
            // Keep growing the block run size
            ulNextBlockRunSize++;
            ullCurrentBlock++;
        }
    }

    // Update values for caller
    *pullCurrentBlock = ullNextBlockRunStartBlock + ulNextBlockRunSize;
    pBlockRun->BlockCount = ulNextBlockRunSize;
    pBlockRun->StartBlock = ullNextBlockRunStartBlock;
    return ulNextBlockRunSize;
}

// --------------------------------------------------------------------
// Displays progress of operation
// --------------------------------------------------------------------
void ShowProgress(PULONGLONG pulPercent, ULONGLONG ulCurrent, ULONGLONG ulTotal)
{
    ULONGLONG ulCurrentPercent = (100 * ulCurrent) / (ulTotal);
    if (*pulPercent != ulCurrentPercent)
    {
        *pulPercent = ulCurrentPercent;
        LOG(_T("%02u%% complete"), (DWORD)*pulPercent);
    }
}

//-----------------------------------------------------------------------------
// Returns an HKEY for the specified key name.
//-----------------------------------------------------------------------------
HKEY OpenRegistryKey(
    IN LPCTSTR pszKeyName)
{
    HKEY hKey = NULL;
    DWORD dwDisposition = 0;

    if (ERROR_SUCCESS != RegCreateKeyEx(HKEY_LOCAL_MACHINE,
        pszKeyName,
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        0,
        NULL,
        &hKey,
        &dwDisposition))
    {
        Debug(_T("Error : OpenRegistryKey() : RegCreateKeyEx(HKLM, %s) failed : last error = %u"),
            pszKeyName, GetLastError());
        hKey = NULL;
        goto done;
    }

done:
    return hKey;
}

//-----------------------------------------------------------------------------
// Write a string value to the specified Key/Value.
//-----------------------------------------------------------------------------
BOOL WriteRegistryStringValue(
    IN LPCTSTR pszKeyName,
    IN LPCTSTR pszValueName,
    IN LPCTSTR pszStringValue)
{
    HKEY hKey = NULL;
    BOOL fRet = FALSE;
    size_t stStringLength = 0;
    DWORD dwValueSize = 0;

    // Calculate the length of the input string value
    VERIFY(SUCCEEDED(StringCchLength(pszStringValue, MAX_PATH, &stStringLength)));
    dwValueSize = (stStringLength+1)*sizeof(TCHAR);

    hKey = OpenRegistryKey(pszKeyName);
    if (NULL == hKey)
    {
        LOG(_T("Error : WriteRegistryStringValue() : OpenRegistryKey() failed"));
        goto done;
    }

    if(ERROR_SUCCESS != RegSetValueEx(hKey,
        pszValueName,
        0,
        REG_SZ,
        (LPBYTE)pszStringValue,
        dwValueSize))
    {
        Debug(_T("Error : FileFlashWrapper::WriteRegistryStringValue() : RegSetValueEx(%s) failed : last error = %u"),
            pszValueName, GetLastError());
        goto done;
    }

    fRet = TRUE;
done:
    CHECK_CLOSE_KEY(hKey);
    return fRet;
}

//-----------------------------------------------------------------------------
// Write a single multi-string value to the specified Key/Value.
//-----------------------------------------------------------------------------
BOOL WriteRegistryMultiStringValue(
    IN LPCTSTR pszKeyName,
    IN LPCTSTR pszValueName,
    IN LPCTSTR pszStringValue)
{
    HKEY hKey = NULL;
    BOOL fRet = FALSE;
    size_t stStringLength = 0;
    DWORD dwValueSize = 0;
    LPBYTE pMultiStringBuffer = NULL;

    // Calculate the length of the input string value
    VERIFY(SUCCEEDED(StringCchLength(pszStringValue, MAX_PATH, &stStringLength)));
    // The required buffer is the length of the string + 2 null terminators
    dwValueSize = (stStringLength+2)*sizeof(TCHAR);
    // Allocate buffer for multi_sz string
    pMultiStringBuffer = (LPBYTE) LocalAlloc(LPTR, dwValueSize);
    if (NULL == pMultiStringBuffer)
    {
        Debug(_T("Error : WriteRegistryMultiStringValue() : LocalAlloc() failed"));
        goto done;
    }
    // Zero out the buffer
    memset(pMultiStringBuffer, 0, dwValueSize);
    // Copy string into the buffer
    memcpy(pMultiStringBuffer, pszStringValue, dwValueSize-sizeof(TCHAR));

    hKey = OpenRegistryKey(pszKeyName);
    if (NULL == hKey)
    {
        Debug(_T("Error : WriteRegistryMultiStringValue() : OpenRegistryKey() failed"));
        goto done;
    }

    if(ERROR_SUCCESS != RegSetValueEx(hKey,
        pszValueName,
        0,
        REG_MULTI_SZ,
        (LPBYTE)pszStringValue,
        dwValueSize))
    {
        Debug(_T("Error : WriteRegistryMultiStringValue() : RegSetValueEx(%s) failed : last error = %u"),
            pszValueName, GetLastError());
        goto done;
    }

    fRet = TRUE;
done:
    CHECK_CLOSE_KEY(hKey);
    CHECK_LOCAL_FREE(pMultiStringBuffer);
    return fRet;
}

//-----------------------------------------------------------------------------
// Returns true if the argument is a power of 2
//-----------------------------------------------------------------------------
BOOL PowerOfTwo(
    IN DWORD dwNumber)
{
    BOOL fRet = FALSE;
    DWORD dwPowOfTwo = 1;

    while(dwPowOfTwo <= dwNumber)
    {
        if(dwNumber%dwPowOfTwo != 0)
        {
            goto done;
        }
        dwPowOfTwo = dwPowOfTwo << 1;
    }
    
    fRet = TRUE;
done:
    return fRet;
}