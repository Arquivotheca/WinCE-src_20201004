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
#include "disk_common.h"

HANDLE OpenDiskHandleByDiskName(CKato* pKato,BOOL bOpenAsStore,TCHAR* szDiskName,BOOL bOpenAsPartition)
{
    HANDLE hDisk = INVALID_HANDLE_VALUE;
    if(!pKato)
    {
        NKMSG(_T("OpenDiskHandleByDiskName ERROR:  pKato is NULL\n"));
        return INVALID_HANDLE_VALUE;
    }
    if(!szDiskName)
    {
        pKato->Log(LOG_DETAIL,_T("OpenDiskHandleByDiskName ERROR:  No disk name specified\n"));
        return INVALID_HANDLE_VALUE;
    }
    //now open the disk
    hDisk = OpenDevice(szDiskName,bOpenAsStore,bOpenAsPartition);
    if(INVALID_HANDLE_VALUE == hDisk)
    {
        pKato->Log(LOG_DETAIL, _T("ERROR: unable to open user-specified mass storage device \"%s\"; error %u"), szDiskName, GetLastError());
        goto DONE;
    }
    else
    {
        if(!GetDiskInfo(hDisk,szDiskName,&g_diskInfo,pKato))
        {
            pKato->Log(LOG_DETAIL, _T("ERROR: user-specified device \"%s\" does not appear to be a mass storage device"), szDiskName);
            VERIFY(CloseHandle(hDisk));
            hDisk = INVALID_HANDLE_VALUE;
            goto DONE;
        }
        else
        {
            pKato->Log(LOG_DETAIL, _T("opened user-specified mass storage device: \"%s\""), szDiskName);
        }
    }

DONE:

    if(INVALID_HANDLE_VALUE == hDisk)
    {
        pKato->Log(LOG_DETAIL, _T("ERROR: found no mass storage devices!"));
    }
    else
    {
        pKato->Log(LOG_DETAIL, _T("    Total Sectors: %u"), g_diskInfo.di_total_sectors); 
        pKato->Log(LOG_DETAIL, _T("    Bytes Per Sector: %u"), g_diskInfo.di_bytes_per_sect); 
        pKato->Log(LOG_DETAIL, _T("    Cylinders: %u"), g_diskInfo.di_cylinders); 
        pKato->Log(LOG_DETAIL, _T("    Heads: %u"), g_diskInfo.di_heads); 
        pKato->Log(LOG_DETAIL, _T("    Sectors: %u"), g_diskInfo.di_sectors);             
        pKato->Log(LOG_DETAIL, _T("    Flags: 0x%08X"), g_diskInfo.di_flags);
    }
    return hDisk; 
}

HANDLE OpenDiskHandleByProfile(CKato* pKato,BOOL bOpenAsStore,TCHAR* szProfile,BOOL bOpenAsPartition,LPTSTR szDiskName,DWORD dwDiskNameSize)
{
    HANDLE hDetect = NULL;
    HANDLE hDisk = INVALID_HANDLE_VALUE;
    TCHAR szDisk[MAX_PATH] = _T("");
    STORAGEDEVICEINFO sdi = {0};
    DWORD cbReturned = 0;

    if(!pKato)
    {
        NKMSG(_T("OpenDiskHandleByProfile ERROR:  pKato is NULL\n"));
        goto DONE;
    }
    if(!szProfile)
    {
        pKato->Log(LOG_DETAIL,_T("OpenDiskHandleByProfile ERROR:  No profile name specified\n"));
        goto DONE;
    }


    //enumerate STORE_MOUNT_GUID or BLOCK_DRIVER_GUID devices
    //advertised by device manager
    hDetect = DEV_DetectFirstDevice((bOpenAsStore||bOpenAsPartition) ? &STORE_MOUNT_GUID : &BLOCK_DRIVER_GUID, szDisk, MAX_PATH);
    if(hDetect)
    {
        do
        {
            // open a handle to the enumerated device
            pKato->Log(LOG_DETAIL, _T("checking device \"%s\"..."), szDisk);
            hDisk = OpenDevice(szDisk,bOpenAsStore,bOpenAsPartition);
            if(INVALID_HANDLE_VALUE == hDisk)
            {
                pKato->Log(LOG_DETAIL, _T("unable to open mass storage device \"%s\"; error %u"), szDisk, GetLastError());
                continue;
            }
            if(!DeviceIoControl(hDisk, IOCTL_DISK_DEVICE_INFO, &sdi, sizeof(STORAGEDEVICEINFO), NULL, 0, &cbReturned, NULL))
            {
                pKato->Log(LOG_DETAIL, _T("device \"%s\" does not support IOCTL_DISK_DEVICE_INFO (required for /profile option); error %u"), szDisk, GetLastError());
                VERIFY(CloseHandle(hDisk));
                hDisk = INVALID_HANDLE_VALUE;
                continue;
            }
            else
            {
                // check for a profile match
                if(0 != _tcsicmp(szProfile, sdi.szProfile))
                {
                    pKato->Log(LOG_DETAIL, _T("device \"%s\" profile \"%s\" does not match specified profile \"%s\""), szDisk, sdi.szProfile, szProfile);
                    VERIFY(CloseHandle(hDisk));
                    hDisk = INVALID_HANDLE_VALUE;
                    continue;
                }
            }
            if(!GetDiskInfo(hDisk, szDisk, &g_diskInfo,pKato))
            {
                pKato->Log(LOG_DETAIL, _T("ERROR: device \"%s\" does not appear to be a mass storage device"), szDisk);
                VERIFY(CloseHandle(hDisk));
                hDisk = INVALID_HANDLE_VALUE;
                continue;
            }
            //if we get to here, we found a device
            pKato->Log(LOG_DETAIL, _T("found appropriate mass storage device: \"%s\""), szDisk);
            // copy the name of the disk to the output buffer it was provided
            if (szDiskName)
            {
                VERIFY(SUCCEEDED(StringCchCopy(szDiskName, dwDiskNameSize, szDisk)));
            }
            break;
        }while(DEV_DetectNextDevice(hDetect, szDisk, MAX_PATH));
        DEV_DetectClose(hDetect);
    }
    else
    {
        pKato->Log(LOG_DETAIL,_T("No devices of type %s are advertised by the storage manager\n"),bOpenAsStore ? _T("STORE_MOUNT_GUID") :_T("BLOCK_DRIVER_GUID"));
        goto DONE;
    }

DONE:

    if(INVALID_HANDLE_VALUE == hDisk)
    {
        pKato->Log(LOG_DETAIL, _T("ERROR: found no mass storage devices!"));
    }
    else
    {        
        pKato->Log(LOG_DETAIL, _T("    Total Sectors: %u"), g_diskInfo.di_total_sectors); 
        pKato->Log(LOG_DETAIL, _T("    Bytes Per Sector: %u"), g_diskInfo.di_bytes_per_sect); 
        pKato->Log(LOG_DETAIL, _T("    Cylinders: %u"), g_diskInfo.di_cylinders); 
        pKato->Log(LOG_DETAIL, _T("    Heads: %u"), g_diskInfo.di_heads); 
        pKato->Log(LOG_DETAIL, _T("    Sectors: %u"), g_diskInfo.di_sectors);             
        pKato->Log(LOG_DETAIL, _T("    Flags: 0x%08X"), g_diskInfo.di_flags); 
    }
    return hDisk;
}

HANDLE OpenDevice(LPCTSTR pszDiskName,BOOL OpenAsStore,BOOL OpenAsPartition)
{
    HANDLE hStore = INVALID_HANDLE_VALUE;
    HANDLE hPartition = INVALID_HANDLE_VALUE;
    HANDLE hFindPartition = INVALID_HANDLE_VALUE;
    PARTINFO partInfo = {0};
    partInfo.cbSize = sizeof(PARTINFO);

    // open the device as either a partition, store or a stream device
    if(OpenAsPartition)
    {
        // Opent the store and get the first partition
        hStore = OpenStore(pszDiskName);
        if (INVALID_HANDLE_VALUE == hStore)
        {
            return INVALID_HANDLE_VALUE;
        }
        // Find the first partition in the store
        hFindPartition = FindFirstPartition(hStore, &partInfo);
        if (INVALID_HANDLE_VALUE == hFindPartition)
        {
            return INVALID_HANDLE_VALUE;
        }
        hPartition = OpenPartition(hStore, partInfo.szPartitionName);
        FindClosePartition(hFindPartition);
        return hPartition;
    }
    if(OpenAsStore)
    {
        return OpenStore(pszDiskName);
    }
    else
    {
        return CreateFile(pszDiskName, GENERIC_READ, FILE_SHARE_READ, 
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
}


HANDLE OpenDiskHandleByDiskNameRW(CKato* pKato,BOOL bOpenAsStore,TCHAR* szDiskName,BOOL bOpenAsPartition)
{
    HANDLE hDisk = INVALID_HANDLE_VALUE;
    if(!pKato)
    {
        NKMSG(_T("OpenDiskHandleByDiskNameRW ERROR:  pKato is NULL\n"));
        return INVALID_HANDLE_VALUE;
    }
    if(!szDiskName)
    {
        pKato->Log(LOG_DETAIL,_T("OpenDiskHandleByDiskNameRW ERROR:  No disk name specified\n"));
        return INVALID_HANDLE_VALUE;
    }
    //now open the disk
    hDisk = OpenDeviceRW(szDiskName,bOpenAsStore,bOpenAsPartition);
    if(INVALID_HANDLE_VALUE == hDisk)
    {
        pKato->Log(LOG_DETAIL, _T("ERROR: unable to open user-specified mass storage device \"%s\"; error %u"), szDiskName, GetLastError());
        goto DONE;
    }
    else
    {
        if(!GetDiskInfo(hDisk,szDiskName,&g_diskInfo,pKato))
        {
            pKato->Log(LOG_DETAIL, _T("ERROR: user-specified device \"%s\" does not appear to be a mass storage device"), szDiskName);
            VERIFY(CloseHandle(hDisk));
            hDisk = INVALID_HANDLE_VALUE;
            goto DONE;
        }
        else
        {
            pKato->Log(LOG_DETAIL, _T("opened user-specified mass storage device: \"%s\""), szDiskName);
        }
    }

DONE:

    if(INVALID_HANDLE_VALUE == hDisk)
    {
        pKato->Log(LOG_DETAIL, _T("ERROR: found no mass storage devices!"));
    }
    else
    {
        pKato->Log(LOG_DETAIL, _T("    Total Sectors: %u"), g_diskInfo.di_total_sectors); 
        pKato->Log(LOG_DETAIL, _T("    Bytes Per Sector: %u"), g_diskInfo.di_bytes_per_sect); 
        pKato->Log(LOG_DETAIL, _T("    Cylinders: %u"), g_diskInfo.di_cylinders); 
        pKato->Log(LOG_DETAIL, _T("    Heads: %u"), g_diskInfo.di_heads); 
        pKato->Log(LOG_DETAIL, _T("    Sectors: %u"), g_diskInfo.di_sectors);             
        pKato->Log(LOG_DETAIL, _T("    Flags: 0x%08X"), g_diskInfo.di_flags);
    }
    return hDisk; 
}

HANDLE OpenDiskHandleByProfileRW(CKato* pKato,BOOL bOpenAsStore,TCHAR* szProfile,BOOL bOpenAsPartition,LPTSTR szDiskName,DWORD dwDiskNameSize)
{
    HANDLE hDetect = NULL;
    HANDLE hDisk = INVALID_HANDLE_VALUE;
    TCHAR szDisk[MAX_PATH] = _T("");
    STORAGEDEVICEINFO sdi = {0};
    DWORD cbReturned = 0;

    if(!pKato)
    {
        NKMSG(_T("OpenDiskHandleByProfileRW ERROR:  pKato is NULL\n"));
        goto DONE;
    }
    if(!szProfile)
    {
        pKato->Log(LOG_DETAIL,_T("OpenDiskHandleByProfileRW ERROR:  No profile name specified\n"));
        goto DONE;
    }


    //enumerate STORE_MOUNT_GUID or BLOCK_DRIVER_GUID devices
    //advertised by device manager
    hDetect = DEV_DetectFirstDevice((bOpenAsStore||bOpenAsPartition) ? &STORE_MOUNT_GUID : &BLOCK_DRIVER_GUID, szDisk, MAX_PATH);
    if(hDetect)
    {
        do
        {
            // open a handle to the enumerated device
            pKato->Log(LOG_DETAIL, _T("checking device \"%s\"..."), szDisk);
            hDisk = OpenDeviceRW(szDisk,bOpenAsStore,bOpenAsPartition);
            if(INVALID_HANDLE_VALUE == hDisk)
            {
                pKato->Log(LOG_DETAIL, _T("unable to open mass storage device \"%s\"; error %u"), szDisk, GetLastError());
                continue;
            }
            if(!DeviceIoControl(hDisk, IOCTL_DISK_DEVICE_INFO, &sdi, sizeof(STORAGEDEVICEINFO), NULL, 0, &cbReturned, NULL))
            {
                pKato->Log(LOG_DETAIL, _T("device \"%s\" does not support IOCTL_DISK_DEVICE_INFO (required for /profile option); error %u"), szDisk, GetLastError());
                VERIFY(CloseHandle(hDisk));
                hDisk = INVALID_HANDLE_VALUE;
                continue;
            }
            else
            {
                // check for a profile match
                if(0 != _tcsicmp(szProfile, sdi.szProfile))
                {
                    pKato->Log(LOG_DETAIL, _T("device \"%s\" profile \"%s\" does not match specified profile \"%s\""), szDisk, sdi.szProfile, szProfile);
                    VERIFY(CloseHandle(hDisk));
                    hDisk = INVALID_HANDLE_VALUE;
                    continue;
                }
            }
            if(!GetDiskInfo(hDisk, szDisk, &g_diskInfo,pKato))
            {
                pKato->Log(LOG_DETAIL, _T("ERROR: device \"%s\" does not appear to be a mass storage device"), szDisk);
                VERIFY(CloseHandle(hDisk));
                hDisk = INVALID_HANDLE_VALUE;
                continue;
            }
            //if we get to here, we found a device
            pKato->Log(LOG_DETAIL, _T("found appropriate mass storage device: \"%s\""), szDisk);
            // copy the name of the disk to the output buffer it was provided
            if (szDiskName)
            {
                VERIFY(SUCCEEDED(StringCchCopy(szDiskName, dwDiskNameSize, szDisk)));
            }
            break;
        }while(DEV_DetectNextDevice(hDetect, szDisk, MAX_PATH));
        DEV_DetectClose(hDetect);
    }
    else
    {
        pKato->Log(LOG_DETAIL,_T("No devices of type %s are advertised by the storage manager\n"),bOpenAsStore ? _T("STORE_MOUNT_GUID") :_T("BLOCK_DRIVER_GUID"));
        goto DONE;
    }

DONE:

    if(INVALID_HANDLE_VALUE == hDisk)
    {
        pKato->Log(LOG_DETAIL, _T("ERROR: found no mass storage devices!"));
    }
    else
    {        
        pKato->Log(LOG_DETAIL, _T("    Total Sectors: %u"), g_diskInfo.di_total_sectors); 
        pKato->Log(LOG_DETAIL, _T("    Bytes Per Sector: %u"), g_diskInfo.di_bytes_per_sect); 
        pKato->Log(LOG_DETAIL, _T("    Cylinders: %u"), g_diskInfo.di_cylinders); 
        pKato->Log(LOG_DETAIL, _T("    Heads: %u"), g_diskInfo.di_heads); 
        pKato->Log(LOG_DETAIL, _T("    Sectors: %u"), g_diskInfo.di_sectors);             
        pKato->Log(LOG_DETAIL, _T("    Flags: 0x%08X"), g_diskInfo.di_flags); 
    }
    return hDisk;
}

HANDLE OpenDeviceRW(LPCTSTR pszDiskName,BOOL OpenAsStore,BOOL OpenAsPartition)
{
    HANDLE hStore = INVALID_HANDLE_VALUE;
    HANDLE hPartition = INVALID_HANDLE_VALUE;
    HANDLE hFindPartition = INVALID_HANDLE_VALUE;
    PARTINFO partInfo = {0};
    partInfo.cbSize = sizeof(PARTINFO);

    // open the device as either a partition, store or a stream device
    if(OpenAsPartition)
    {
        // Opent the store and get the first partition
        hStore = OpenStore(pszDiskName);
        if (INVALID_HANDLE_VALUE == hStore)
        {
            return INVALID_HANDLE_VALUE;
        }
        // Find the first partition in the store
        hFindPartition = FindFirstPartition(hStore, &partInfo);
        if (INVALID_HANDLE_VALUE == hFindPartition)
        {
            return INVALID_HANDLE_VALUE;
        }
        hPartition = OpenPartition(hStore, partInfo.szPartitionName);
        FindClosePartition(hFindPartition);
        return hPartition;
    }
    if(OpenAsStore)
    {
        return OpenStore(pszDiskName);
    }
    else
    {
        return CreateFile( pszDiskName, 
            GENERIC_READ|GENERIC_WRITE, 
            FILE_SHARE_READ|FILE_SHARE_WRITE, 
            NULL, 
            OPEN_EXISTING, 
            FILE_ATTRIBUTE_NORMAL, 
            NULL);
    }
}

BOOL GetDiskInfo( HANDLE hDisk,LPWSTR pszDisk,DISK_INFO *pDiskInfo,CKato* pKato)
{
    ASSERT(pDiskInfo);

    DWORD cbReturned;

    // first, try IOCTL_DISK_GETINFO call
    if(!DeviceIoControl(hDisk, IOCTL_DISK_GETINFO, NULL, 0, pDiskInfo, sizeof(DISK_INFO), &cbReturned, NULL))        
    {
        pKato->Log(LOG_DETAIL, _T("\"%s\" does not respond to IOCTL_DISK_GETINFO"), pszDisk);

        // the disk may only support the legacy DISK_IOCTL_GETINFO call
        if(!DeviceIoControl(hDisk, DISK_IOCTL_GETINFO, pDiskInfo, sizeof(DISK_INFO), NULL, 0, &cbReturned, NULL))
        {
            pKato->Log(LOG_DETAIL, _T("\"%s\" does not respond to DISK_IOCTL_GETINFO"), pszDisk);
            return FALSE;
        }
    }
    return TRUE;
}