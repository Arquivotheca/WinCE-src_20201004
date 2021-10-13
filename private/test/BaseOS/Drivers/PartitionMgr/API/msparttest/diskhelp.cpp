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
#include "DiskHelp.H"
#include <DiskIO.H> 

static const DWORD maxIndex = 9;
static const LONG maxDiskSize = (32*1024);
static const LONG minDiskSize = 1;

// structure for device information
typedef struct _DEVICE_INFO 
{
    HANDLE hDevice;
    HANDLE hStore;
    WCHAR szName[8];
    // link
    struct _DEVICE_INFO *pNext;

} DEVICE_INFO, *PDEVICE_INFO, **PPDEVICE_INFO;

// some registry definitions
#define WRITE_REG_SZ(hKey, Name, Value) RegSetValueEx( hKey, Name, 0, REG_SZ, (LPBYTE)Value, (wcslen(Value)+1)*sizeof(WCHAR))
#define WRITE_REG_DW(hKey, Name, Value) do { DWORD dwValue = Value; RegSetValueEx( hKey, Name, 0, REG_DWORD, (LPBYTE)&dwValue, sizeof(DWORD)); } while(0)

#define LOGPARTTYPE(bPartType, type) if(bPartType == type) LOG(L#type)
#define LOGFLAG(flags, match) if(flags & match) LOG(L#match)

// --------------------------------------------------------------------
void LogStoreInfo(STOREINFO *pInfo)
//
//  Log the info contained in a STOREINFO struct
// --------------------------------------------------------------------
{
    SYSTEMTIME sysTime = {0};
    
    if(pInfo)
    {
        LOG(L"STOREINFO");
        LOG(L"  snNumSectors:               %I64u", pInfo->snNumSectors);
        LOG(L"  dwBytesPerSector:           %u",    pInfo->dwBytesPerSector);
        LOG(L"  snFreeSectors:              %I64u", pInfo->snFreeSectors);
        LOG(L"  snBiggestPartCreatable:     %I64u", pInfo->snBiggestPartCreatable);
    
        FileTimeToSystemTime(&pInfo->ftCreated, &sysTime);
        LOG(L"  ftCreated:                  %u/%u/%u, %u:%u:%u:%u",
            sysTime.wMonth, sysTime.wDay, sysTime.wYear, sysTime.wHour, 
            sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds);
        
        FileTimeToSystemTime(&pInfo->ftLastModified, &sysTime);
        LOG(L"  ftLastModified:             %u/%u/%u, %u:%u:%u:%u",
            sysTime.wMonth, sysTime.wDay, sysTime.wYear, sysTime.wHour, 
            sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds);
        
        LOG(L"  dwAttributes:               0x%08x", pInfo->dwAttributes);
    }
}

// --------------------------------------------------------------------
void LogPartInfo(PARTINFO *pInfo)
//
//  Log the info contained in a PARTINFO struct
// --------------------------------------------------------------------
{
    SYSTEMTIME sysTime = {0};
    
    if(pInfo)
    {
        LOG(L"PARTINFO");
        LOG(L"  szPartitionName             %s",    pInfo->szPartitionName);
        LOG(L"  snNumSectors:               %I64u", pInfo->snNumSectors);
    
        FileTimeToSystemTime(&pInfo->ftCreated, &sysTime);        
        LOG(L"  ftCreated:                  %u/%u/%u, %u:%u:%u:%u",
            sysTime.wMonth, sysTime.wDay, sysTime.wYear, sysTime.wHour, 
            sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds);
        
        FileTimeToSystemTime(&pInfo->ftLastModified, &sysTime);        
        LOG(L"  ftLastModified:             %u/%u/%u, %u:%u:%u:%u",
            sysTime.wMonth, sysTime.wDay, sysTime.wYear, sysTime.wHour, 
            sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds);
        
        LOG(L"  dwAttributes:               0x%08x", pInfo->dwAttributes);

        LOG(L"  bPartType:                  0x%02x", pInfo->bPartType);
        
        LOGPARTTYPE(pInfo->bPartType, PART_DOS2_FAT);
        LOGPARTTYPE(pInfo->bPartType, PART_DOS3_FAT);
        LOGPARTTYPE(pInfo->bPartType, PART_DOS4_FAT);
        LOGPARTTYPE(pInfo->bPartType, PART_DOS32);
        LOGPARTTYPE(pInfo->bPartType, PART_DOS32X13);
        LOGPARTTYPE(pInfo->bPartType, PART_DOSX13);
        LOGPARTTYPE(pInfo->bPartType, PART_DOSX13X);
        
    }
}

// --------------------------------------------------------------------
void LogDiskInfo(DISK_INFO *pInfo)
//
//  Log the info contained in a DISK_INFO struct
// --------------------------------------------------------------------
{
    if(pInfo)
    {
        LOG(L"DISK_INFO");
        LOG(L"  di_total_sectors            %u",        pInfo->di_total_sectors);
        LOG(L"  di_bytes_per_sect           %u",        pInfo->di_bytes_per_sect);
        LOG(L"  di_cylinders                %u",        pInfo->di_cylinders);
        LOG(L"  di_heads                    %u",        pInfo->di_heads);
        LOG(L"  di_sectors                  %u",        pInfo->di_sectors);
        LOG(L"  di_flags                    0x%08x",    pInfo->di_flags);
        LOGFLAG(pInfo->di_flags, DISK_INFO_FLAG_MBR);
        LOGFLAG(pInfo->di_flags, DISK_INFO_FLAG_CHS_UNCERTAIN);
        LOGFLAG(pInfo->di_flags, DISK_INFO_FLAG_UNFORMATTED);
        LOGFLAG(pInfo->di_flags, DISK_INFO_FLAG_PAGEABLE);
    }
}

// --------------------------------------------------------------------
UINT DumpDiskInfo(HANDLE hStore)
// --------------------------------------------------------------------
{
    DISK_INFO di = {0};
    UINT cPart = 0;
    DWORD dwBytes = 0;
    WCHAR szDiskName[MAX_PATH] = L"";

    HANDLE hSearch = INVALID_HANDLE_VALUE;
    PARTINFO partInfo = {0};
    STOREINFO storeInfo = {0};

    partInfo.cbSize = sizeof(PARTINFO);
    storeInfo.cbSize = sizeof(STOREINFO);
    
    if(INVALID_HANDLE(hStore))
        goto Error;

    // there are a few IOCTL's we can query the disk driver with
    
    // try each IOCTL using current and legacy IOCTLs (IOCTL_DISK_*, DISK_IOCTL_*)
    // request data as both input and output parameters

    // IOCTL_DISK_GETINFO 
    if(DeviceIoControl(hStore, IOCTL_DISK_GETINFO, &di, sizeof(DISK_INFO), NULL, 0, &dwBytes, NULL))
        LogDiskInfo(&di);
    else if(DeviceIoControl(hStore, IOCTL_DISK_GETINFO, NULL, 0, &di, sizeof(DISK_INFO), &dwBytes, NULL))
        LogDiskInfo(&di);
    else if(DeviceIoControl(hStore, DISK_IOCTL_GETINFO, &di, sizeof(DISK_INFO), NULL, 0, &dwBytes, NULL))
        LogDiskInfo(&di);
    else if(DeviceIoControl(hStore, DISK_IOCTL_GETINFO, NULL, 0, &di, sizeof(DISK_INFO), &dwBytes, NULL))
        LogDiskInfo(&di);

    // IOCTL_DISK_GETNAME
    if(DeviceIoControl(hStore, IOCTL_DISK_GETNAME, szDiskName, sizeof(szDiskName), NULL, 0, &dwBytes, NULL))
        LOG(L"Disk is named: %s", szDiskName);
    else if(DeviceIoControl(hStore, IOCTL_DISK_GETNAME, NULL, 0, szDiskName, sizeof(szDiskName), &dwBytes, NULL))
        LOG(L"Disk is named: %s", szDiskName);
    else if(DeviceIoControl(hStore, DISK_IOCTL_GETNAME, szDiskName, sizeof(szDiskName), NULL, 0, &dwBytes, NULL))
        LOG(L"Disk is named: %s", szDiskName);
    else if(DeviceIoControl(hStore, DISK_IOCTL_GETNAME, NULL, 0, szDiskName, sizeof(szDiskName), &dwBytes, NULL))
        LOG(L"Disk is named: %s", szDiskName);   

#if defined(IOCTL_DISK_DEVICE_INFO) && defined(STOREAGEDEVICEINFO)
    STORAGEDEVICEINFO sdi = {0};
    sdi.cbSize = sizeof(STORAGEDEVICEINFO);
    
    // IOCTL_DISK_DEVICE_INFO -- there is no legacy DISK_IOCTL_DEVICE_INFO IOCTL
    if(DeviceIoControl(hStore, IOCTL_DISK_DEVICE_INFO, &sdi, sizeof(STORAGEDEVICEINFO), NULL, 0, NULL))
        LogStorageDeviceInfo(&sdi);
    else if(DeviceIoControl(hStore, IOCTL_DISK_DEVICE_INFO, NULL, 0, &sdi, sizeof(STORAGEDEVICEINFO), NULL))
        LogStorageDeviceInfo(&sdi);
#endif

    // query and log store info struct for the disk
    if(!GetStoreInfo(hStore, &storeInfo))
    {
        ERR("GetStoreInfo()");
        goto Error;
    }
    LogStoreInfo(&storeInfo);
        
    // enumerate and log partition info
    hSearch = FindFirstPartition(hStore, &partInfo);
    if(INVALID_HANDLE_VALUE == hSearch)
    {
        ERR("FindFirstPartition()");
        goto Error;
    }
    do 
    {
        LOG(L"PARTITION NUMBER %u, NAME=%s SIZE=%.2fMB", ++cPart, partInfo.szPartitionName, 
            (float)((float)((DWORD)partInfo.snNumSectors * di.di_bytes_per_sect) / (float)(1<<20)));
        LogPartInfo(&partInfo);
    } while(FindNextPartition(hSearch, &partInfo));

Error:

    if(INVALID_HANDLE_VALUE != hSearch)
    {
        FindClosePartition(hSearch);
        hSearch = INVALID_HANDLE_VALUE;
    }

    return cPart;
}

// --------------------------------------------------------------------
BOOL GetRawDiskInfo(HANDLE hStore, DISK_INFO *diskInfo)
//
//  Query block driver for disk info -- tries all possible methods of
//  getting disk info
// --------------------------------------------------------------------
{
    DWORD dwBytes = 0;
    
    // query disk info -- driver may require any one of the following calls
    if(DeviceIoControl(hStore, IOCTL_DISK_GETINFO, NULL, 0, diskInfo, sizeof(DISK_INFO), &dwBytes, NULL))
        return TRUE;
    else if(DeviceIoControl(hStore, DISK_IOCTL_GETINFO, diskInfo, sizeof(DISK_INFO), NULL, 0, &dwBytes, NULL))
        return TRUE;
    else
    {
        ERR("DeviceIoControl(IOCTL_DISK_GETINFO)");
        return FALSE;
    }
}    

// -------------------------------------------------------------------
DWORD QueryDiskCount()
// --------------------------------------------------------------------
{
    UINT ct = 0;
    STOREINFO storeInfo;
    CE_VOLUME_INFO volumeInfo;
    
    storeInfo.cbSize = sizeof(STOREINFO);
    volumeInfo.cbSize = sizeof(CE_VOLUME_INFO);

    // Get the device name for the device
    // containing the root file system.
    // This query will run only once
    if (g_szRootDeviceName[0] == _T(' '))
    {
        if (!CeGetVolumeInfo(_T("\\"), CeVolumeInfoLevelStandard, &volumeInfo))
        {
            LOG(L"Unable to get volume information for root file system");
            g_szRootDeviceName[0] = _T('\0');
        } 
        else
        {
            // Copy the device name of root file system
            _tcsncpy_s(g_szRootDeviceName, STORENAMESIZE, volumeInfo.szStoreName, STORENAMESIZE);
        }
    }

    if(g_szStoreName[0]) {
        // use the specified store
        return 1;
    }
    
    HANDLE hFind = FindFirstStore(&storeInfo);
    if(!INVALID_HANDLE(hFind))
    {
        do
        {
            ct ++;
        } while (FindNextStore(hFind, &storeInfo));
        
        FindCloseStore(hFind);
    } else {
        LOG(L"there are no storage devices present!");
    }
    
    return ct;
}

// --------------------------------------------------------------------
BOOL IsAValidStore(DWORD dwDisk)
// --------------------------------------------------------------------
{
    HANDLE hStore = OpenStoreByIndex(dwDisk);
    DISK_INFO diskInfo = {0};
    STOREINFO storeInfo;
    DWORD cbReturned = 0;
    storeInfo.cbSize = sizeof(STOREINFO);

    if(INVALID_HANDLE(hStore))
        return FALSE;    

    // Compare the name of the device to the name of the root device
    if (!GetStoreInfo(hStore, &storeInfo))
    {
        return FALSE;
    }
    else if (0 == _tcsncmp(g_szRootDeviceName, storeInfo.szDeviceName, DEVICENAMESIZE))
    {
        // If this is a store with the root file system, it's not valid for testing
        return FALSE;
    }
    else if (0 == _tcsncmp(_T("VCEFSD"), storeInfo.sdi.szProfile, PROFILENAMESIZE))
    {
        // If this store is a host-mapped directory on an Emulator, it's not
        // valid for testing
        return FALSE;
    }

    // if either of the GETINFO calls succeeds, then this is a valid disk
    // CDROM Devices should theoretically not respond to this IOCTL
    if(DeviceIoControl(hStore, DISK_IOCTL_GETINFO, &diskInfo, sizeof(DISK_INFO), NULL, 0, &cbReturned, NULL) ||
       DeviceIoControl(hStore, IOCTL_DISK_GETINFO, NULL, 0, &diskInfo, sizeof(DISK_INFO), &cbReturned, NULL))
    {
        return TRUE;
    }
    return FALSE;
}

// --------------------------------------------------------------------
HANDLE OpenStoreByIndex(DWORD dwIndex)
// --------------------------------------------------------------------
{
    STOREINFO storeInfo;
    storeInfo.cbSize = sizeof(STOREINFO);
    LPTSTR pszStore = g_szStoreName;
    if(!pszStore[0]) 
    {
        HANDLE hFind = FindFirstStore(&storeInfo);
        if (INVALID_HANDLE_VALUE == hFind) {
            LOG(L"there are no storage devices present!");
            return INVALID_HANDLE_VALUE;
        }   
        for (DWORD i = 0; i < dwIndex; i++) {
            if (!FindNextStore(hFind, &storeInfo)) {
                LOG(L"there is no store at index %u!", dwIndex);
                FindCloseStore(hFind);
                return INVALID_HANDLE_VALUE;
            }
        }
        FindCloseStore(hFind);
        pszStore = storeInfo.szDeviceName;
    }
        
    LOG(L"Opening storage device \"%s\"...", pszStore);
    return OpenStore(pszStore);
}

// --------------------------------------------------------------------
BOOL CreateAndVerifyPartition(HANDLE hStore, LPCWSTR szPartName, BYTE bPartType, 
    SECTORNUM numSectors, BOOL fAuto, PARTINFO *pInfo)
// --------------------------------------------------------------------
{
    BOOL fRet = FALSE;
    PARTINFO partInfo = {0};
    STOREINFO storeInfo = {0};

    HANDLE hPart = INVALID_HANDLE_VALUE;
    partInfo.cbSize = sizeof(PARTINFO);
    storeInfo.cbSize = sizeof(STOREINFO);

    // validate parameters
    if(NULL == hStore)
    {
        FAIL("CreateAndVerifyPartition(): Invalid HANDLE");
        goto Error;
    }
    if(!szPartName || !szPartName[0])
    {
        FAIL("CreateAndVerifyPartition(): Invalid partition name");
        goto Error;
    }

    // create partition
    LOG(L"Creating partition \"%s\" of %u sectors", szPartName, numSectors);
    if(fAuto) 
    {
        if(!CreatePartition(hStore, szPartName, numSectors))
        {
            DETAIL("CreateAndVerifyPartition(): failed CreatePartition()");
            goto Error;
        }
    } else {
        if(!CreatePartitionEx(hStore, szPartName, bPartType, numSectors))
        {
            DETAIL("CreateAndVerifyPartition(): failed CreatePartitionEx()");
            goto Error;
        }
    }

    hPart = OpenPartition(hStore, szPartName);
    if(INVALID_HANDLE(hPart))
    {
        FAIL("OpenPartition()");
        goto Error;
    }

    if(!FormatPartitionEx(hPart, bPartType, fAuto))
    {
        FAIL("CreateAndVerifyPartition(): FormatPartitionEx()");
        goto Error;
    }

    // query partition info and verify
    DETAIL("Verifying partition info");
    if(!GetPartitionInfo(hPart, &partInfo))
    {
        FAIL("CreateAndVerifyPartition(): GetPartitionInfo()");
        goto Error;
    }
    // verify sector count
    DETAIL("Verifying sector count");
    if(partInfo.snNumSectors < numSectors)
    {
        LOG(L"requested sectors %I64u is less than actual sectors %I64u",
            numSectors, partInfo.snNumSectors);
        FAIL("insufficient sector count");
        goto Error;
    }
    // verify partition name
    DETAIL("Verifying partition name");
    if(wcscmp(partInfo.szPartitionName, szPartName))
    {
        LOG(L"requested partition name %s does not match actual name %s",
            szPartName, partInfo.szPartitionName);
        FAIL("Partition name is incorrect");
        goto Error;
    }

    // if a partition type was requested, verify it 
    if(!fAuto)
    {
        DETAIL("Verifying partition type");
        if(partInfo.bPartType != bPartType)
        {
            LOG(L"requested partition type 0x%02x not the same as actual partition type 0x%02x",
                bPartType, partInfo.bPartType);
            FAIL("Partition type is incorrect");
            goto Error;
        }
    }

    // copy partition info into output buffer, if requested
    if(pInfo)
    {
        memcpy(pInfo, &partInfo, sizeof(partInfo));
    }

    // success
    fRet = TRUE;

Error:
    if(VALID_HANDLE(hPart))
    {
        CloseHandle(hPart);
    }
    return fRet;
}

// --------------------------------------------------------------------
HANDLE OpenAndFormatStore(DWORD dwDiskNumber, STOREINFO *pInfo)
//
//  returns NULL on failure, valid HANDLE on success
// --------------------------------------------------------------------
{
    HANDLE hStore = INVALID_HANDLE_VALUE;

    hStore = OpenStoreByIndex(dwDiskNumber);
    if(INVALID_HANDLE(hStore))
    {
        FAIL("OpenStoreByIndex()");
        goto Error;
    }
    LOG(L"Opened store 0x%08x", hStore);

    if(!DismountStore(hStore))
    {
        FAIL("DismountStore()");
        CloseHandle(hStore);
        hStore = INVALID_HANDLE_VALUE;
        goto Error;
    }

    // format the store for use
    LOG(L"Formatting store 0x%08x", hStore);
    if(!FormatStore(hStore))
    {
        FAIL("FormatStore()");
        CloseHandle(hStore);
        hStore = INVALID_HANDLE_VALUE;
        goto Error;
    }

    if(NULL != pInfo)
    {
        // query store information, if requested
        LOG(L"Getting store info for store 0x%08x", hStore);
        if(!GetStoreInfo(hStore, pInfo))
        {
            FAIL("GetStoreInfo()");
            CloseHandle(hStore);
            hStore = INVALID_HANDLE_VALUE;
            goto Error;
        }
    }

Error:
    return hStore;
}

// --------------------------------------------------------------------
BOOL WritePartitionData(HANDLE hStore, LPCWSTR szPartName, SECTORNUM snSkip)
// --------------------------------------------------------------------
{
    BOOL fRet = FALSE;
    PBYTE pSector = NULL;
    UINT cByte = 0;

    HANDLE hPart = INVALID_HANDLE_VALUE;
    SECTORNUM cSector = 0;
    PARTINFO partInfo = {0};
    STOREINFO storeInfo = {0};
    
    partInfo.cbSize = sizeof(PARTINFO);
    storeInfo.cbSize = sizeof(STOREINFO);

    // get store info
    if(!GetStoreInfo(hStore, &storeInfo))
    {
        FAIL("T_GetStoreInfo()");
        goto Error;
    }

    hPart = OpenPartition(hStore, szPartName);
    if(INVALID_HANDLE(hPart)) 
    {
        FAIL("OpenPartition()");
        goto Error;
    }
    LOG(L"Opened partition %s on store 0x%08x as 0x%08x", szPartName, hStore, hPart);

    // get partition info
    if(!GetPartitionInfo(hPart, &partInfo))
    {
        FAIL("T_GetPartitionInfo()");
        goto Error;
    }   

    // allocate sector write buffer
    pSector = (PBYTE)VirtualAlloc(NULL, storeInfo.dwBytesPerSector, MEM_COMMIT, PAGE_READWRITE);
    if(NULL == pSector)
    {
        ERRFAIL("VirtualAlloc()");
        goto Error;
    }

    // write each sector of the partition
    LOG(L"Writing known data to partition 0x%08x", hPart);
    for(cSector = 0; cSector < partInfo.snNumSectors; cSector += snSkip)
    {
        // force the last sector to always get written on the last pass
        if(cSector + snSkip >= partInfo.snNumSectors)
            cSector = partInfo.snNumSectors - 1;
        
        // init our write buffer to the following pattern:
        // the first byte is cSector % 0xFF, increment by 1 
        // for every following byte
        for(cByte = 0; cByte < storeInfo.dwBytesPerSector; cByte++)
        {
            pSector[cByte] = (BYTE)(cSector + cByte);
        }
        if(!ReadWritePartition(hPart, DISK_IOCTL_WRITE, (UINT)cSector, 1, 
            storeInfo.dwBytesPerSector, pSector))
        {
            FAIL("ReadWritePartition()");
            goto Error;
        }        
    }

    fRet = TRUE;
    
Error:
    // free buffer
    if(pSector)
    {
        VirtualFree(pSector, 0, MEM_RELEASE);            
        pSector = NULL;
    }

    // close partition
    if(VALID_HANDLE(hPart))
    {
        CloseHandle(hPart);
    }
    
    return fRet;
}

// --------------------------------------------------------------------
BOOL VerifyPartitionData(HANDLE hStore, LPCWSTR szPartName, SECTORNUM snSkip)
// --------------------------------------------------------------------
{
    BOOL fRet = FALSE;
    PBYTE pSector = NULL;
    UINT cByte = 0;
    
    SECTORNUM cSector = 0;
    PARTINFO partInfo = {0};
    STOREINFO storeInfo = {0};    
    HANDLE hPart = INVALID_HANDLE_VALUE;

    partInfo.cbSize = sizeof(PARTINFO);
    storeInfo.cbSize = sizeof(STOREINFO);
    
    // get store info
    if(!GetStoreInfo(hStore, &storeInfo))
    {
        FAIL("GetStoreInfo()");
        goto Error;
    }

    
    // open a handle to the partition for read/write
    hPart = OpenPartition(hStore, szPartName);
    if(INVALID_HANDLE(hPart))
    {   
        LOG(L"Unable to open partition \"%s\"; partition might not exist", szPartName);
        FAIL("T_OpenPartition()");
        goto Error;
    }
    LOG(L"Opened partition %s on store 0x%08x as 0x%08x", szPartName, hStore, hPart);
    
    // get partition info
    if(!GetPartitionInfo(hPart, &partInfo))
    {
        FAIL("GetPartitionInfo()");
        goto Error;
    }
        
    // allocate sector read buffer
    pSector = (PBYTE)VirtualAlloc(NULL, storeInfo.dwBytesPerSector, MEM_COMMIT, PAGE_READWRITE);
    if(NULL == pSector)
    {
        ERRFAIL("VirtualAlloc()");
        goto Error;
    }
    
    // perform partition reads to verify the data
    LOG(L"Writing known data to partition 0x%08x", hPart);
    for(cSector = 0; cSector < partInfo.snNumSectors; cSector += snSkip)
    {
        // force the last sector to always get read on the last pass
        if(cSector + snSkip >= partInfo.snNumSectors)
            cSector = partInfo.snNumSectors - 1;
        
        if(!ReadWritePartition(hPart, DISK_IOCTL_READ, cSector, 1, 
            storeInfo.dwBytesPerSector, pSector))
        {
            FAIL("ReadWriteDiskRaw()");
            goto Error;
        }

        // verify the sector matches the correct pattern
        for(cByte = 0; cByte < storeInfo.dwBytesPerSector; cByte++)
        {
            if(pSector[cByte] != (BYTE)(cSector + cByte))
            {
                LOG(L"0x%02x != 0x%02x", pSector[cByte], (BYTE)(cSector + cByte));
                FAIL("raw sector data does not match data written to partition");
                goto Error;
            }
        }
    }

    fRet = TRUE;
    
Error:
    // free buffer
    if(pSector)
    {
        VirtualFree(pSector, 0, MEM_RELEASE);            
        pSector = NULL;
    }

    // close partition
    if(VALID_HANDLE(hPart))
    {
        CloseHandle(hPart);
    }
    
    return fRet; 
}

// --------------------------------------------------------------------
BOOL WriteAndVerifyPartition(HANDLE hStore, LPCWSTR szPartName, SECTORNUM snSkip)
//
//  Reads raw sectors before and after the partition (if available),
//  writes known data to the partition, verifies that the partition
//  was written correctly and the sectors before and after were not
//  altered. All reads/writes are done one sector at a time
// --------------------------------------------------------------------
{
    BOOL fRet = FALSE;
    PBYTE pPreSector = NULL;
    PBYTE pPostSector = NULL;
    PBYTE pSector = NULL;
    PBYTE pMultiSector = NULL;

    SECTORNUM cSector = 0;
    PARTINFO partInfo = {0};
    STOREINFO storeInfo = {0};
    HANDLE hPart = INVALID_HANDLE_VALUE;

    partInfo.cbSize = sizeof(PARTINFO);
    storeInfo.cbSize = sizeof(STOREINFO);

    if(INVALID_HANDLE(hStore))
    {
        DETAIL("WriteAndVerifyPartition(): Invalid store handle");
        goto Error;
    }

    hPart = OpenPartition(hStore, szPartName);
    if(INVALID_HANDLE(hPart))
    {
        FAIL("OpenPartition()");
        goto Error;
    }

    // get partition and store information
    if(!GetPartitionInfo(hPart, &partInfo))
    {
        LOG(L"Unable to get info for partition \"%s\"; parition may not exist", szPartName);
        FAIL("GetPartitionInfo()");
        goto Error;
    }
    if(!GetStoreInfo(hStore, &storeInfo))
    {
        FAIL("GetStoreInfo()");
        goto Error;
    }

    // allocate sector read buffer
    pSector = (PBYTE)VirtualAlloc(NULL, storeInfo.dwBytesPerSector, MEM_COMMIT, PAGE_READWRITE);
    if(NULL == pSector)
    {
        ERRFAIL("VirtualAlloc()");
        goto Error;
    }

    if(!WritePartitionData(hStore, szPartName, snSkip))
    {
        FAIL("WritePartitionData()");
        goto Error;
    }

    // write single out of bounds sectors
    for(cSector = partInfo.snNumSectors; cSector < partInfo.snNumSectors + MAX_SECTORS_OOB; cSector++)
    {   
        // try to write a sector past the end of the partition
        if(ReadWritePartition(hPart, DISK_IOCTL_WRITE, partInfo.snNumSectors, 1,
            storeInfo.dwBytesPerSector, pSector))
        {
            FAIL("ReadWritePartition() succeeded past partition boundary");
            goto Error;   
        }
    }

    // write multi-sector buffer that crosses the partition boundaries
    for(cSector = MIN_MULTI_SECTOR; cSector <= MAX_MULTI_SECTOR; cSector++)
    {
        DWORD dwTemp=0;
        if (S_OK != DWordMult((DWORD)cSector, storeInfo.dwBytesPerSector, &dwTemp))
         {
            FAIL("Integer overflow in calculating buffer size\r\n");
            goto Error;
         }
          
        pMultiSector = (PBYTE)VirtualAlloc(NULL, dwTemp, MEM_COMMIT, PAGE_READWRITE);
        
        // center the buffer over the boundary
        if(ReadWritePartition(hPart, DISK_IOCTL_WRITE, partInfo.snNumSectors - cSector/2, cSector,
            storeInfo.dwBytesPerSector, pSector))
        {
            FAIL("ReadWritePartition() succeeded past partition boundary");
            goto Error;
        }
        // start the buffer 1 sector before the boundary
        if(ReadWritePartition(hPart, DISK_IOCTL_WRITE, partInfo.snNumSectors - 1, cSector,
            storeInfo.dwBytesPerSector, pSector))
        {
            FAIL("ReadWritePartition() succeeded past partition boundary");
            goto Error;
        }
        // end the buffer 1 sector past the boundary
        if(ReadWritePartition(hPart, DISK_IOCTL_WRITE, partInfo.snNumSectors - cSector + 1, cSector,
            storeInfo.dwBytesPerSector, pSector))
        {
            FAIL("ReadWritePartition() succeeded past partition boundary");
            goto Error;
        }

        VirtualFree(pMultiSector, 0, MEM_RELEASE);
        pMultiSector = NULL;
    }

    // verify using partition reads
    if(!VerifyPartitionData(hStore, szPartName, snSkip))
    {
        FAIL("VerifyPartitionData()");
        goto Error;
    }

    fRet = TRUE;
    
Error:
    // close partition
    if(VALID_HANDLE(hPart))
    {
        CloseHandle(hPart);
    }

    // free buffer memory
    if(pPostSector)
    {
        VirtualFree(pPostSector, 0, MEM_RELEASE);
        pPostSector = NULL;
    }
    if(pPreSector)
    {
        VirtualFree(pPreSector, 0, MEM_RELEASE);
        pPreSector = NULL;
    }
    if(pSector)
    {
        VirtualFree(pSector, 0, MEM_RELEASE);
        pSector = NULL;
    }
    if(pMultiSector)
    {
        VirtualFree(pMultiSector, 0, MEM_RELEASE);
        pMultiSector = NULL;
    }
    
    return fRet;
}

// --------------------------------------------------------------------
BOOL 
ReadWritePartition(
    HANDLE hPart, 
    DWORD ctlCode,
    SECTORNUM sector, 
    SECTORNUM cSectors, 
    UINT cBytesPerSector,
    PBYTE pbBuffer)
// --------------------------------------------------------------------
{
    BOOL fRet = FALSE;
    SG_REQ sgReq = {0};
    DWORD cBytes = 0;
 
    if(ctlCode != IOCTL_DISK_READ && ctlCode != IOCTL_DISK_WRITE &&
       ctlCode != DISK_IOCTL_READ && ctlCode != DISK_IOCTL_WRITE)
    {
        FAIL("ReadWriteDiskRaw(): invalid IOCTL code");
        goto Error;
    }
    
    if(INVALID_HANDLE(hPart))
    {
        FAIL("ReadWriteDiskRaw(): invalid partition handle");    
        goto Error;
    }

    // build sg request buffer -- single sg buffer
    sgReq.sr_start = (DWORD)sector;
    sgReq.sr_num_sec = (DWORD)cSectors;
    sgReq.sr_num_sg = 1;
    sgReq.sr_callback = NULL; // no callback under CE
    sgReq.sr_sglist[0].sb_len = (DWORD)(cSectors * cBytesPerSector);
    sgReq.sr_sglist[0].sb_buf = pbBuffer;

    // make sure all exceptions are caught by the driver
    __try
    {
        if(!DeviceIoControl(hPart, ctlCode, (PBYTE)&sgReq, sizeof(sgReq), NULL, 0, &cBytes, NULL))
        {
            goto Error;
        }

    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        DETAIL("DeviceIoControl() caused an unhandled exception");
        goto Error;
    }

    fRet = TRUE;

Error:
    return fRet;
}
