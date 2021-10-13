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
//  File:       udfsio.cpp
//
//  Contents:
//
//  Classes:
//
//  Functions:
//
//--------------------------------------------------------------------------

#include "cdfs.h"

//+-------------------------------------------------------------------------
//
//  Member:     CReadOnlyFileSystemDriver::UDFSDeviceIoControl
//
//  Synopsis:
//
//  Arguments:  [dwIoControlCode] --
//              [lpInBuffer]      --
//              [nInBufferSize]   --
//              [lpOutBuffer]     --
//              [nOutBufferSize]  --
//              [lpBytesReturned] --
//              [lpOverlapped]    --
//
//  Returns:
//
//  Notes:
//
//--------------------------------------------------------------------------

BOOL CReadOnlyFileSystemDriver::UDFSDeviceIoControl(
        DWORD           dwIoControlCode,
        LPVOID          lpInBuffer,
        DWORD           nInBufferSize,
        LPVOID          lpOutBuffer,
        DWORD           nOutBufferSize,
        LPDWORD         lpBytesReturned,
        LPOVERLAPPED    lpOverlapped)
{
    BOOL        fRet;
    DWORD       dwErr;

    fRet = ::FSDMGR_DiskIoControl(
                m_hDsk,
                dwIoControlCode,
                lpInBuffer,
                nInBufferSize,
                lpOutBuffer,
                nOutBufferSize,
                lpBytesReturned,
                lpOverlapped);

    if (fRet == FALSE)  {
    
        dwErr = GetLastError();


        if	((dwErr == ERROR_MEDIA_CHANGED) || (dwErr == ERROR_NOT_READY) ||	(dwErr == ERROR_NO_MEDIA_IN_DRIVE)) {
            //
            //  If it is currently clean, mark it as dirty
            //  This way if it is being cleaned we won't mess that
            //  up.
            //
            DEBUGMSG(ZONE_MEDIA,(TEXT("UDFSDeviceIoControl: Check Volume State\r\n")));
            if (InterlockedTestExchange( &(m_State), StateClean, StateDirty) == StateClean) {
                //
                //  Mark All Disk Handles as dirty!!!
                //
				DEBUGMSG(ZONE_MEDIA,(TEXT("UDFSDeviceIoControl: Clean Volume State \r\n")));
				SetLastError(ERROR_MEDIA_CHANGED);
				Clean();

            }

        }
    }

    return (fRet);
}
