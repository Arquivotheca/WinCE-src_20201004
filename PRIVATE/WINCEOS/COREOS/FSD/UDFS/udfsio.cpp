//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

#include "udfs.h"

//HANDLE  g_hIOEvent;

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
            //  If it is StateInUse mark it as StateWindUp so that 
            //  findfile() function will return before clean.
            //
            if (InterlockedTestExchange( &(m_State), StateInUse, StateWindUp) == StateInUse) {
                DEBUGMSG(ZONE_MEDIA,(TEXT("UDFSDeviceIoControl: changing state from StateInUse -> StateWindUp \r\n")));
            }

            DEBUGMSG(ZONE_MEDIA,(TEXT("UDFSDeviceIoControl: Check Volume State\r\n")));
            if (InterlockedTestExchange( &(m_State), StateClean, StateDirty) == StateClean) {
                //
                //  Mark All Disk Handles as dirty!!!
                //
				DEBUGMSG(ZONE_MEDIA,(TEXT("UDFSDeviceIoControl: Clean Volume State \r\n")));
				SetLastError(ERROR_MEDIA_CHANGED);
                EnterCriticalSection(&m_csListAccess);
                Clean();
                LeaveCriticalSection(&m_csListAccess);

            }

        }
    }

    return (fRet);
}
