//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
////////////////////////////////////////////////////////////////////////////////
//
//  BLKIOCTL TUX DLL
//  Copyright (c) Microsoft Corporation
//
//  Module: diskutil.cpp
//          Contains disk utility functions.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "globals.h"


////////////////////////////////////////////////////////////////////////////////
// GetDiskInfo
//  Gets disk info using either IOCTL_DISK_GETINFO or DISK_IOCTL_GETINFO
//
// Parameters:
//  hDisk       handle to the disk device
//  pswDisk     name of the disk (for debug info)
//  pDiskInfo   output buffer for disk information
//
// Return value:
//  TRUE if disk info was retrieved successfully, FALSE otherwise

static BOOL GetDiskInfo(HANDLE hDisk, LPWSTR pszDisk, DISK_INFO *pDiskInfo)
{
    ASSERT(pDiskInfo);
    
    DWORD cbReturned;
    
    // first, try new IOCTL_DISK_GETINFO call
    if(!DeviceIoControl(hDisk, IOCTL_DISK_GETINFO, NULL, 0, pDiskInfo, sizeof(DISK_INFO), &cbReturned, NULL))        
    {
        g_pKato->Log(LOG_DETAIL, _T("\"%s\" does not respond to IOCTL_DISK_GETINFO"), pszDisk);
   
        // the disk may only support the old DISK_IOCTL_GETINFO call
        if(!DeviceIoControl(hDisk, DISK_IOCTL_GETINFO, pDiskInfo, sizeof(DISK_INFO), NULL, 0, &cbReturned, NULL))
        {
            g_pKato->Log(LOG_DETAIL, _T("\"%s\" does not respond to DISK_IOCTL_GETINFO"), pszDisk);
            return FALSE;
        }
    }
    
    return TRUE;
}

////////////////////////////////////////////////////////////////////////////////
// GetDiskHandle
//  Gets handle to, and disk info for, the first available disk 
//
// Parameters:
//  pswDisk     name of the disk, or NULL to search
//  pDiskInfo   output buffer for disk information
//
// Return value:
//  handle to disk if opened successfully, INVALID_HANDLE_VALUE otherwise

HANDLE GetDiskHandle(LPWSTR pszDisk, DISK_INFO *pDiskInfo)
{
    TCHAR szDisk[MAX_PATH] = _T("");
    
    HANDLE hDisk = INVALID_HANDLE_VALUE;
    HANDLE hDetect = NULL;

    DWORD cbReturned = 0;

    // if a disk name was specified, try to open it
    if(pszDisk && pszDisk[0])
    {
        hDisk = CreateFile(pszDisk, GENERIC_READ, FILE_SHARE_READ, 
                NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if(INVALID_HANDLE_VALUE == hDisk)
        {
            g_pKato->Log(LOG_DETAIL, _T("ERROR: unable to open user-specified mass storage device \"%s\"; error %u"), pszDisk, GetLastError());
        }
        else
        {
            if(!GetDiskInfo(hDisk, pszDisk, pDiskInfo))
            {
                g_pKato->Log(LOG_DETAIL, _T("ERROR: user-specified device \"%s\" does not appear to be a mass storage device"), pszDisk);
                VERIFY(CloseHandle(hDisk));
                hDisk = INVALID_HANDLE_VALUE;
            }
            else
            {
                g_pKato->Log(LOG_DETAIL, _T("opened user-specified mass storage device: \"%s\""), pszDisk);
            }
        }
       
    } 
    else
    {
        // enumerate BLOCK_DRIVER_GUID devices
        hDetect = DEV_DetectFirstDevice(&BLOCK_DRIVER_GUID, szDisk, MAX_PATH);
        if(hDetect)
        {
            do
            {
                // open a handle to the enumerated device
                g_pKato->Log(LOG_DETAIL, _T("checking device \"%s\"..."), szDisk);
                hDisk = CreateFile(szDisk, GENERIC_READ, FILE_SHARE_READ, 
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if(INVALID_HANDLE_VALUE == hDisk)
                {
                    g_pKato->Log(LOG_DETAIL, _T("unable to open mass storage device \"%s\"; error %u"), szDisk, GetLastError());
                    continue;
                }

                if(!GetDiskInfo(hDisk, szDisk, pDiskInfo))
                {
                    g_pKato->Log(LOG_DETAIL, _T("ERROR: user-specified device \"%s\" does not appear to be a mass storage device"), szDisk);
                    VERIFY(CloseHandle(hDisk));
                    hDisk = INVALID_HANDLE_VALUE;
                    continue;
                }
                
                g_pKato->Log(LOG_DETAIL, _T("found mass storage device: \"%s\""), szDisk);

                break;

            } while(DEV_DetectNextDevice(hDetect, szDisk, MAX_PATH));
            DEV_DetectClose(hDetect);
        }
    }

    if(INVALID_HANDLE_VALUE == hDisk)
    {
        g_pKato->Log(LOG_DETAIL, _T("ERROR: found no mass storage devices!"));
    }
    else
    {        
        g_pKato->Log(LOG_DETAIL, _T("    Total Sectors: %u"), pDiskInfo->di_total_sectors); 
        g_pKato->Log(LOG_DETAIL, _T("    Bytes Per Sector: %u"), pDiskInfo->di_bytes_per_sect); 
        g_pKato->Log(LOG_DETAIL, _T("    Cylinders: %u"), pDiskInfo->di_cylinders); 
        g_pKato->Log(LOG_DETAIL, _T("    Heads: %u"), pDiskInfo->di_heads); 
        g_pKato->Log(LOG_DETAIL, _T("    Sectors: %u"), pDiskInfo->di_sectors);             
        g_pKato->Log(LOG_DETAIL, _T("    Flags: 0x%08X"), pDiskInfo->di_flags); 
    }

    return hDisk;
}
