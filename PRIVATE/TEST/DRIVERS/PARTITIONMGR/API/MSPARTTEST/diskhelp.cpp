//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "TPartDrv.H"
#include "DiskHelp.H"
#include <DiskIO.H> 

#if 0
#include <CardServ.H> // req'd for devload.h
#include <DevLoad.H>
#endif

static const DWORD maxIndex = 9;
static const LONG maxDiskSize = (32*1024);
static const LONG minDiskSize = 1;

// structure for device information
typedef struct _DEVICE_INFO 
{
    HANDLE hDevice;
    HANDLE hDisk;
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
void LogStoreInfo(PD_STOREINFO *pInfo)
//
//  Log the info contained in a PD_STOREINFO struct
// --------------------------------------------------------------------
{
    SYSTEMTIME sysTime = {0};
    
    if(pInfo)
    {
        LOG(L"PD_STOREINFO");
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
void LogPartInfo(PD_PARTINFO *pInfo)
//
//  Log the info contained in a PD_PARTINFO struct
// --------------------------------------------------------------------
{
    SYSTEMTIME sysTime = {0};
    
    if(pInfo)
    {
        LOG(L"PD_PARTINFO");
        LOG(L"  szPartitionName             %s",    pInfo->szPartitionName);
        //LOG(L"  snFirstSector:              %I64u", pInfo->snFirstSector);
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
UINT DumpDiskInfo(HANDLE hDisk)
// --------------------------------------------------------------------
{
    DISK_INFO di = {0};
    UINT cPart = 0;
    DWORD dwBytes = 0;
    WCHAR szDiskName[MAX_PATH] = L"";

    STOREID storeId = INVALID_STOREID;
    SEARCHID searchId= INVALID_SEARCHID;
    PD_PARTINFO partInfo = {0};
    PD_STOREINFO storeInfo = {0};

    partInfo.cbSize = sizeof(PD_PARTINFO);
    storeInfo.cbSize = sizeof(PD_STOREINFO);
    
    if(INVALID_HANDLE(hDisk))
        goto Error;

    // there are a few IOCTL's we can query the disk driver with
    
    // try each IOCTL using current and legacy IOCTLs (IOCTL_DISK_*, DISK_IOCTL_*)
    // request data as both input and output parameters

    // IOCTL_DISK_GETINFO 
    if(DeviceIoControl(hDisk, IOCTL_DISK_GETINFO, &di, sizeof(DISK_INFO), NULL, 0, &dwBytes, NULL))
        LogDiskInfo(&di);
    else if(DeviceIoControl(hDisk, IOCTL_DISK_GETINFO, NULL, 0, &di, sizeof(DISK_INFO), &dwBytes, NULL))
        LogDiskInfo(&di);
    else if(DeviceIoControl(hDisk, DISK_IOCTL_GETINFO, &di, sizeof(DISK_INFO), NULL, 0, &dwBytes, NULL))
        LogDiskInfo(&di);
    else if(DeviceIoControl(hDisk, DISK_IOCTL_GETINFO, NULL, 0, &di, sizeof(DISK_INFO), &dwBytes, NULL))
        LogDiskInfo(&di);

    // IOCTL_DISK_GETNAME
    if(DeviceIoControl(hDisk, IOCTL_DISK_GETNAME, szDiskName, sizeof(szDiskName), NULL, 0, &dwBytes, NULL))
        LOG(L"Disk is named: %s", szDiskName);
    else if(DeviceIoControl(hDisk, IOCTL_DISK_GETNAME, NULL, 0, szDiskName, sizeof(szDiskName), &dwBytes, NULL))
        LOG(L"Disk is named: %s", szDiskName);
    else if(DeviceIoControl(hDisk, DISK_IOCTL_GETNAME, szDiskName, sizeof(szDiskName), NULL, 0, &dwBytes, NULL))
        LOG(L"Disk is named: %s", szDiskName);
    else if(DeviceIoControl(hDisk, DISK_IOCTL_GETNAME, NULL, 0, szDiskName, sizeof(szDiskName), &dwBytes, NULL))
        LOG(L"Disk is named: %s", szDiskName);   

#if defined(IOCTL_DISK_DEVICE_INFO) && defined(STOREAGEDEVICEINFO)
    STORAGEDEVICEINFO sdi = {0};
    sdi.cbSize = sizeof(STORAGEDEVICEINFO);
    
    // IOCTL_DISK_DEVICE_INFO -- there is no legacy DISK_IOCTL_DEVICE_INFO IOCTL
    if(DeviceIoControl(hDisk, IOCTL_DISK_DEVICE_INFO, &sdi, sizeof(STORAGEDEVICEINFO), NULL, 0, NULL))
        LogStorageDeviceInfo(&sdi);
    else if(DeviceIoControl(hDisk, IOCTL_DISK_DEVICE_INFO, NULL, 0, &sdi, sizeof(STORAGEDEVICEINFO), NULL))
        LogStorageDeviceInfo(&sdi);
#endif

    // get open a storage handle to the drive
    storeId = T_OpenStore(hDisk);
    if(INVALID_STOREID == storeId)
    {
        ERR("T_OpenStore()");
        goto Error;
    }

    // query and log store info struct for the disk
    if(!T_GetStoreInfo(storeId, &storeInfo))
    {
        ERR("T_GetStoreInfo()");
        goto Error;
    }
    LogStoreInfo(&storeInfo);
        
    // enumerate and log partition info
    searchId = T_FindPartitionStart(storeId);
    if(INVALID_SEARCHID == searchId)
    {
        ERR("T_FindPartitionStart()");
        goto Error;
    }
    while(T_FindPartitionNext(searchId, &partInfo))
    {
        LOG(L"PARTITION NUMBER %u, NAME=%s SIZE=%.2fMB", ++cPart, partInfo.szPartitionName, 
            (float)((float)((DWORD)partInfo.snNumSectors * di.di_bytes_per_sect) / (float)(1<<20)));
        LogPartInfo(&partInfo);
    }
    
Error:

    if(INVALID_SEARCHID != searchId)
    {
        T_FindPartitionClose(searchId);
        searchId = INVALID_SEARCHID;
    }
    if(INVALID_STOREID != storeId)
    {
        T_CloseStore(storeId);
        storeId = INVALID_STOREID;
    }

    return cPart;
}

// --------------------------------------------------------------------
BOOL GetRawDiskInfo(HANDLE hDisk, DISK_INFO *diskInfo)
//
//  Query block driver for disk info -- tries all possible methods of
//  getting disk info
// --------------------------------------------------------------------
{
    DWORD dwBytes = 0;
    
    // query disk info -- driver may require any one of the following calls
    if(DeviceIoControl(hDisk, IOCTL_DISK_GETINFO, NULL, 0, diskInfo, sizeof(DISK_INFO), &dwBytes, NULL))
        return TRUE;
    else if(DeviceIoControl(hDisk, DISK_IOCTL_GETINFO, diskInfo, sizeof(DISK_INFO), NULL, 0, &dwBytes, NULL))
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
    HANDLE hDisk = INVALID_HANDLE_VALUE;

    while(INVALID_HANDLE_VALUE != (hDisk = OpenDisk(ct+1)))
    {        
        CloseHandle(hDisk);
        ct++;
    }
    
    return ct;
}

// --------------------------------------------------------------------
BOOL IsAValidDisk(DWORD dwDisk)
// --------------------------------------------------------------------
{
    HANDLE hDisk = OpenDisk(dwDisk);
    DISK_INFO diskInfo = {0};
    DWORD cbReturned = 0;

    if(INVALID_HANDLE(hDisk))
        return FALSE;    

    // if either of the GETINFO calls succeeds, then this is a valid disk
    // CDROM Devices should theoretically not respond to this IOCTL
    if(DeviceIoControl(hDisk, DISK_IOCTL_GETINFO, &diskInfo, sizeof(DISK_INFO), NULL, 0, &cbReturned, NULL) ||
       DeviceIoControl(hDisk, IOCTL_DISK_GETINFO, NULL, 0, &diskInfo, sizeof(DISK_INFO), &cbReturned, NULL))
    {
        return TRUE;
    }
    return FALSE;
}

// --------------------------------------------------------------------
HANDLE OpenDisk(DWORD dwDisk)
// --------------------------------------------------------------------
{
    WCHAR szDisk[DEVICENAMESIZE] = L"";

    swprintf(szDisk, L"DSK%u:", dwDisk);
    LOG(L"Opening disk %s", szDisk);
    return CreateFile(szDisk, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
}

// --------------------------------------------------------------------
BOOL CreateAndVerifyPartition(STOREID storeId, LPCWSTR szPartName, BYTE bPartType, 
    SECTORNUM numSectors, BOOL fAuto, PD_PARTINFO *pInfo)
// --------------------------------------------------------------------
{
    BOOL fRet = FALSE;
    PD_PARTINFO partInfo = {0};
    PD_STOREINFO storeInfo = {0};

    partInfo.cbSize = sizeof(PD_PARTINFO);
    storeInfo.cbSize = sizeof(PD_STOREINFO);

    // validate parameters
    if(NULL == storeId)
    {
        FAIL("CreateAndVerifyPartition(): Invalid STOREID");
        goto Error;
    }
    if(!szPartName || !szPartName[0])
    {
        FAIL("CreateAndVerifyPartition(): Invalid partition name");
        goto Error;
    }

    // create partition
    LOG(L"Creating partition \"%s\" of %u sectors", szPartName, numSectors);
    if(!T_CreatePartition(storeId, szPartName, bPartType, numSectors, fAuto))
    {
        FAIL("CreateAndVerifyPartition(): T_CreatePartition()");
        goto Error;
    }

    if(!T_FormatPartition(storeId, szPartName, bPartType, fAuto))
    {
        FAIL("CreateAndVerifyPartition(): T_FormatPartition()");
        goto Error;
    }

    // query partition info and verify
    DETAIL("Verifying partition info");
    if(!T_GetPartitionInfo(storeId, szPartName, &partInfo))
    {
        FAIL("CreateAndVerifyPartition(): T_GetPartitionInfo()");
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
    return fRet;
}

// --------------------------------------------------------------------
STOREID OpenAndFormatStore(HANDLE hDisk, PD_STOREINFO *pInfo)
//
//  Open a store on volume hDisk (returned from CreateFile on disk 
//  device), format the store, and query information. If pInfo is NULL,
//  the info query is skipped.
//
//  returns NULL on failure, valid HSTORE on success
// --------------------------------------------------------------------
{
    STOREID storeId = INVALID_STOREID;
    
    if(INVALID_HANDLE(hDisk))        
    {
        DETAIL("OpenAndFormatStore(): Invalid disk handle");
        goto Error;
    }

    // open handle to store on volume
    LOG(L"Opening store on disk 0x%08x", hDisk);
    storeId = T_OpenStore(hDisk);
    if(INVALID_STOREID == storeId)
    {
        FAIL("T_OpenStore()");
        goto Error;
    }
    LOG(L"Opened disk 0x%08x as store 0x%08x", hDisk, storeId);

    // format the store for use
    LOG(L"Formatting store 0x%08x", storeId);
    if(!T_FormatStore(storeId))
    {
        FAIL("T_FormatStore()");
        T_CloseStore(storeId);
        storeId = INVALID_STOREID;
        goto Error;
    }

    if(NULL != pInfo)
    {
        // query store information, if requested
        LOG(L"Getting store info for store 0x%08x", storeId);
        if(!T_GetStoreInfo(storeId, pInfo))
        {
            FAIL("T_GetStoreInfo()");
            T_CloseStore(storeId);
            storeId = INVALID_STOREID;
            goto Error;
        }
    }

Error:
    return storeId;
}

// --------------------------------------------------------------------
BOOL WritePartitionData(STOREID storeId, LPCWSTR szPartName, SECTORNUM snSkip)
// --------------------------------------------------------------------
{
    BOOL fRet = FALSE;
    PBYTE pSector = NULL;
    UINT cByte = 0;
    
    SECTORNUM cSector = 0;
    PD_PARTINFO partInfo = {0};
    PD_STOREINFO storeInfo = {0};
    PARTID partId = INVALID_PARTID;
    
    partInfo.cbSize = sizeof(PD_PARTINFO);
    storeInfo.cbSize = sizeof(PD_STOREINFO);

    // get store info
    if(!T_GetStoreInfo(storeId, &storeInfo))
    {
        FAIL("T_GetStoreInfo()");
        goto Error;
    }

    // get partition info
    if(!T_GetPartitionInfo(storeId, szPartName, &partInfo))
    {
        FAIL("T_GetPartitionInfo()");
        goto Error;
    }
    
    // open a handle to the partition for read/write
    partId = T_OpenPartition(storeId, szPartName);
    if(INVALID_PARTID == partId)
    {   
        LOG(L"Unable to open partition \"%s\"; partition might not exist", szPartName);
        FAIL("T_OpenPartition()");
        goto Error;
    }
    LOG(L"Opened partition %s on store 0x%08x as 0x%08x", szPartName, storeId, partId);

    // allocate sector write buffer
    pSector = (PBYTE)VirtualAlloc(NULL, storeInfo.dwBytesPerSector, MEM_COMMIT, PAGE_READWRITE);
    if(NULL == pSector)
    {
        ERRFAIL("VirtualAlloc()");
        goto Error;
    }

    // write each sector of the partition
    LOG(L"Writing known data to partition 0x%08x", partId);
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
        if(!ReadWritePartition(partId, DISK_IOCTL_WRITE, (UINT)cSector, 1, 
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
    if(INVALID_PARTID != partId)
    {
        T_ClosePartition(partId);
        partId = INVALID_PARTID;
    }
    
    return fRet;
}

// --------------------------------------------------------------------
BOOL VerifyPartitionData(STOREID storeId, LPCWSTR szPartName, SECTORNUM snSkip)
// --------------------------------------------------------------------
{
    BOOL fRet = FALSE;
    PBYTE pSector = NULL;
    UINT cByte = 0;
    
    SECTORNUM cSector = 0;
    PD_PARTINFO partInfo = {0};
    PD_STOREINFO storeInfo = {0};    
    PARTID partId = INVALID_PARTID;

    partInfo.cbSize = sizeof(PD_PARTINFO);
    storeInfo.cbSize = sizeof(PD_STOREINFO);
    
    // get store info
    if(!T_GetStoreInfo(storeId, &storeInfo))
    {
        FAIL("T_GetStoreInfo()");
        goto Error;
    }

    // get partition info
    if(!T_GetPartitionInfo(storeId, szPartName, &partInfo))
    {
        FAIL("T_GetPartitionInfo()");
        goto Error;
    }
    
    // open a handle to the partition for read/write
    partId = T_OpenPartition(storeId, szPartName);
    if(INVALID_PARTID == partId)
    {   
        LOG(L"Unable to open partition \"%s\"; partition might not exist", szPartName);
        FAIL("T_OpenPartition()");
        goto Error;
    }
    LOG(L"Opened partition %s on store 0x%08x as 0x%08x", szPartName, storeId, partId);

    // allocate sector read buffer
    pSector = (PBYTE)VirtualAlloc(NULL, storeInfo.dwBytesPerSector, MEM_COMMIT, PAGE_READWRITE);
    if(NULL == pSector)
    {
        ERRFAIL("VirtualAlloc()");
        goto Error;
    }
    
    // perform partition reads to verify the data
    LOG(L"Writing known data to partition 0x%08x", partId);
    for(cSector = 0; cSector < partInfo.snNumSectors; cSector += snSkip)
    {
        // force the last sector to always get read on the last pass
        if(cSector + snSkip >= partInfo.snNumSectors)
            cSector = partInfo.snNumSectors - 1;
        
        if(!ReadWritePartition(partId, DISK_IOCTL_READ, cSector, 1, 
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
    if(INVALID_PARTID != partId)
    {
        T_ClosePartition(partId);
        partId = INVALID_PARTID;
    }
    
    return fRet; 
}

// --------------------------------------------------------------------
BOOL WriteAndVerifyPartition(HANDLE hDisk, STOREID storeId, LPCWSTR szPartName, SECTORNUM snSkip)
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

    PARTID partId = INVALID_PARTID;
    SECTORNUM cSector = 0;
    PD_PARTINFO partInfo = {0};
    PD_STOREINFO storeInfo = {0};

    partInfo.cbSize = sizeof(PD_PARTINFO);
    storeInfo.cbSize = sizeof(PD_STOREINFO);

    if(INVALID_HANDLE(hDisk))
    {
        DETAIL("WriteAndVerifyPartition(): Invalid disk handle");
        goto Error;
    }

    // get partition and store information
    if(!T_GetPartitionInfo(storeId, szPartName, &partInfo))
    {
        LOG(L"Unable to get info for partition \"%s\"; parition may not exist", szPartName);
        FAIL("T_GetPartitionInfo()");
        goto Error;
    }
    if(!T_GetStoreInfo(storeId, &storeInfo))
    {
        FAIL("T_GetStoreInfo()");
        goto Error;
    }

    // allocate sector read buffer
    pSector = (PBYTE)VirtualAlloc(NULL, storeInfo.dwBytesPerSector, MEM_COMMIT, PAGE_READWRITE);
    if(NULL == pSector)
    {
        ERRFAIL("VirtualAlloc()");
        goto Error;
    }

#ifdef USE_BLOCK_DRIVER_CALLS
    // if the partition does not start on sector zero, read and
    // store the sector before it
    if(partInfo.snFirstSector > 0)
    {
        // allocate a buffer to store the sector preceeding the partition        
        pPreSector = (PBYTE)VirtualAlloc(NULL, storeInfo.dwBytesPerSector, MEM_COMMIT, PAGE_READWRITE);
        if(!pPreSector)
        {
            ERRFAIL("VirtualAlloc()");
            goto Error;
        }

        if(!ReadWriteDiskRaw(hDisk, DISK_IOCTL_READ, (DWORD)partInfo.snFirstSector - 1, 1,
            storeInfo.dwBytesPerSector, pPreSector))
        {
            FAIL("ReadWriteDiskRaw()");
            goto Error;
        }        
        LOG(L"Saved a copy of sector %I64u (before the partition)", partInfo.snFirstSector - 1);
    }
    else
    {
        LOG(L"Partition starts at first sector on disk");
    }

    // if the partition does not end on the last sector, read and
    // store the sector after it
    if((partInfo.snFirstSector + partInfo.snNumSectors) < (storeInfo.snNumSectors))
    {
        pPostSector = (PBYTE)VirtualAlloc(NULL, storeInfo.dwBytesPerSector, MEM_COMMIT, PAGE_READWRITE);
        if(!pPostSector)
        {
            ERRFAIL("VirtualAlloc()");
            goto Error;
        }

        if(!ReadWriteDiskRaw(hDisk, DISK_IOCTL_READ, (DWORD)(partInfo.snFirstSector + partInfo.snNumSectors),
            1, storeInfo.dwBytesPerSector, pPostSector))
        {
            FAIL("ReadWriteDiskRaw()");
            goto Error;
        }
        LOG(L"Saved a copy of sector %I64u (after the partition)", partInfo.snFirstSector + partInfo.snNumSectors);        
    }
    else
    {
        LOG(L"Partition ends on the last sector of disk");
    }
#endif // USE_BLOCK_DRIVER_CALLS

    if(!WritePartitionData(storeId, szPartName, snSkip))
    {
        FAIL("WritePartitionData()");
        goto Error;
    }

    // open a partition handle
    partId = T_OpenPartition(storeId, szPartName);
    if(INVALID_PARTID == partId)
    {
        FAIL("T_OpenPartition()");
        goto Error;
    }

    // write single out of bounds sectors
    for(cSector = partInfo.snNumSectors; cSector < partInfo.snNumSectors + MAX_SECTORS_OOB; cSector++)
    {   
        // try to write a sector past the end of the partition
        if(ReadWritePartition(partId, DISK_IOCTL_WRITE, partInfo.snNumSectors, 1,
            storeInfo.dwBytesPerSector, pSector))
        {
            FAIL("ReadWritePartition() succeeded past partition boundary");
            goto Error;   
        }
    }

    // write multi-sector buffer that crosses the partition boundaries
    for(cSector = MIN_MULTI_SECTOR; cSector <= MAX_MULTI_SECTOR; cSector++)
    {
        pMultiSector = (PBYTE)VirtualAlloc(NULL, (DWORD)cSector*storeInfo.dwBytesPerSector, MEM_COMMIT, PAGE_READWRITE);
        
        // center the buffer over the boundary
        if(ReadWritePartition(partId, DISK_IOCTL_WRITE, partInfo.snNumSectors - cSector/2, cSector,
            storeInfo.dwBytesPerSector, pSector))
        {
            FAIL("ReadWritePartition() succeeded past partition boundary");
            goto Error;
        }
        // start the buffer 1 sector before the boundary
        if(ReadWritePartition(partId, DISK_IOCTL_WRITE, partInfo.snNumSectors - 1, cSector,
            storeInfo.dwBytesPerSector, pSector))
        {
            FAIL("ReadWritePartition() succeeded past partition boundary");
            goto Error;
        }
        // end the buffer 1 sector past the boundary
        if(ReadWritePartition(partId, DISK_IOCTL_WRITE, partInfo.snNumSectors - cSector + 1, cSector,
            storeInfo.dwBytesPerSector, pSector))
        {
            FAIL("ReadWritePartition() succeeded past partition boundary");
            goto Error;
        }

        VirtualFree(pMultiSector, 0, MEM_RELEASE); 
    }

#ifdef USE_BLOCK_DRIVER_CALLS
    // read and verify the preceding sector, if applicable
    if(pPreSector)
    {
        if(!ReadWriteDiskRaw(hDisk, DISK_IOCTL_READ, (DWORD)partInfo.snFirstSector - 1,
            1, storeInfo.dwBytesPerSector, pSector))
        {
            FAIL("ReadWriteDiskRaw()");
            goto Error;
        }

        if(memcmp(pSector, pPreSector, storeInfo.dwBytesPerSector))
        {
            FAIL("sector preceeding the partition does not match previous value");
            goto Error;
        }
        else
        {
            DETAIL("sector preceeding remained intact");
        }
    }

    // read and verify the proceding sector, if applicable
    if(pPostSector)
    {
        if(!ReadWriteDiskRaw(hDisk, DISK_IOCTL_READ, (DWORD)(partInfo.snFirstSector + partInfo.snNumSectors),
            1, storeInfo.dwBytesPerSector, pSector))
        {
            FAIL("ReadWriteDiskRaw()");
            goto Error;
        }

        if(memcmp(pSector, pPostSector, storeInfo.dwBytesPerSector))
        {
            FAIL("sector proceeding the partition does not match previous value");
            goto Error;
        }
        else
        {
            DETAIL("sector proceeding remained intact");
        }
    }

    // now perform raw reads of the partition to verify the data
    // write each sector of the partition
    for(cSector = 0; cSector < partInfo.snNumSectors; cSector += snSkip)
    {
        // force the last sector to always get written on the last pass
        if(cSector + snSkip >= partInfo.snNumSectors && cSector < partInfo.snNumSectors)
            cSector = partInfo.snNumSectors - 1;
        
        if(!ReadWriteDiskRaw(hDisk, DISK_IOCTL_READ, (DWORD)partInfo.snFirstSector + cSector,
            1, storeInfo.dwBytesPerSector, pSector))
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

#endif // USE_BLOCK_DRIVER_CALLS

    // verify using partition reads
    if(!VerifyPartitionData(storeId, szPartName, snSkip))
    {
        FAIL("VerifyPartitionData()");
        goto Error;
    }

    fRet = TRUE;
    
Error:
    // close partition
    if(INVALID_PARTID != partId)
    {
        T_ClosePartition(partId);
        partId = INVALID_PARTID;
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
        pSector = NULL;
    }
    
    return fRet;
}

// --------------------------------------------------------------------
BOOL 
ReadWritePartition(
    PARTID partId, 
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
    
    if(INVALID_PARTID == partId)
    {
        FAIL("ReadWriteDiskRaw(): invalid PARTID");    
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
        if(!T_DeviceIoControl(partId, ctlCode, (PBYTE)&sgReq, sizeof(sgReq), NULL, 0, &cBytes))
        {
            goto Error;
        }

    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        DETAIL("T_DeviceIoControl() caused an unhandled exception");
        goto Error;
    }

    fRet = TRUE;

Error:
    return fRet;
}

// --------------------------------------------------------------------
BOOL 
ReadWriteDiskRaw(
    HANDLE hDisk, 
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
    
    if(INVALID_HANDLE(hDisk))
    {
        FAIL("ReadWriteDiskRaw(): invalid disk handle");    
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
        if(!DeviceIoControl(hDisk, ctlCode, &sgReq, sizeof(sgReq), NULL, 0, &cBytes, NULL))
        {
            ERR("DeviceIoControl()");
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
