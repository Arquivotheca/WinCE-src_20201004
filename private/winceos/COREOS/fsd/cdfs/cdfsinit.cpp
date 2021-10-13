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
//+-------------------------------------------------------------------------
//
//
//  File:       udfsinit.cpp
//
//  Contents:
//
//  Classes:
//
//  Functions:
//
//--------------------------------------------------------------------------

#include "cdfs.h"
#include <diskio.h>

//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::CReadOnlyFileSystemDriver
//
//  Synopsis:
//
//  Arguments:  (none)
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------

extern PUDFSDRIVER g_pHeadFSD;

//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::Initialize
//
//  Synopsis:
//
//  Arguments:  [hDsk] --
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------

#define MAX_DEVICE_NAME  32

BOOL CReadOnlyFileSystemDriver::Initialize(HDSK hDsk)
{
    BOOL            fSuccess = TRUE;
    BOOL            fRegistered;
//    HANDLE          hThread;
    DWORD           wAvail; 

    // First open the driver

    m_hDsk = hDsk;
    
    if (fSuccess = UDFSDeviceIoControl(DISK_IOCTL_GETNAME, NULL, 0, (LPVOID)m_szAFSName, MAX_DEVICE_NAME * sizeof(WCHAR), &wAvail, NULL)) { 
        if (!FSDMGR_GetRegistryValue(m_hDsk, L"MountLabel", (LPDWORD)&m_fRegisterLabel)) {
            m_fRegisterLabel= 0;
        }   
        m_hVolume = ::FSDMGR_RegisterVolume(m_hDsk, m_szAFSName, (PVOLUME)this);

        if (!m_hVolume) {
            goto error;
        }    

        fRegistered = TRUE;
        
    } else {
        DEBUGMSG(ZONE_INIT|ZONE_ERROR,(TEXT("UDFS!cdfsInit: RegisterAFSName failed (%d)\n"), GetLastError()));
        fRegistered = FALSE;
    }
error:
    if( !fSuccess) {
        if( fRegistered) {
            ::FSDMGR_DeregisterVolume(m_hVolume);
        }

       // delete m_pCache;
    }

    DEBUGMSG(ZONE_FUNCTION, (FS_MOUNT_POINT_NAME TEXT(" Initialize Done\r\n")));

    return fSuccess;
}

