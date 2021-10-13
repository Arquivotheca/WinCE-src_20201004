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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*
 *      NK Kernel ppfs client code
 *
 *
 * Module Name:
 *
 *      ppfs.c
 *
 * Abstract:
 *
 *      This file implements the NK kernel ppfs client side interface
 *
 *
 */

#include "kernel.h"

// Constants for DeviceIOControl
enum {IOCTL_REG_OPEN, IOCTL_REG_CLOSE, IOCTL_REG_GET, IOCTL_REG_ENUM, IOCTL_WRITE_DEBUG};

// Registration database access functions for kernel debug support initialization

static HANDLE OpenRelfsdReg (PPROCESS pprc)
{
    HANDLE hReg;
//    HANDLE hEvt;
    
    // Temp fix to wait until RELFSD is loaded
//    if (!(hEvt = NKOpenEvent (EVENT_ALL_ACCESS, FALSE, TEXT("WAIT_RELFSD2"))))
//        return INVALID_HANDLE_VALUE;

//    VERIFY (HNDLCloseHandle (pprc, hEvt));

    hReg = FSCreateFile (TEXT("\\Release\\reg:"), GENERIC_READ, FILE_SHARE_READ,
                                NULL, OPEN_EXISTING, 0, NULL );  

    return (INVALID_HANDLE_VALUE == hReg)? NULL : hReg;
}

static BOOL CloseReflsdReg (HANDLE hReg)
{
    return HNDLCloseHandle (g_pprcNK, hReg);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
rRegOpen(
    DWORD hKey,
    const CHAR *szName,
    LPDWORD lphKey
    ) 
{

    int result = -1;
    HANDLE hReg;
    PPROCESS pprc = pActvProc;

    if (hReg = OpenRelfsdReg (pprc)) {
        
        PBYTE lpInBuf, lpOutBuf;
        DWORD nInBufSize, nOutBufSize, nBytesReturned;

        // Pack hKey and szName into input buffer
        nInBufSize = sizeof(DWORD) + strlen(szName)+ 1;
        lpInBuf = (PBYTE) _alloca (nInBufSize);
        *(DWORD*)lpInBuf = hKey;
        memcpy (lpInBuf+sizeof(DWORD), szName, strlen(szName)+ 1);

        nOutBufSize = sizeof(DWORD);
        lpOutBuf = (PBYTE) _alloca (nOutBufSize);

        result = FSIoctl (hReg, IOCTL_REG_OPEN,  lpInBuf,  nInBufSize,
                    lpOutBuf, nOutBufSize, &nBytesReturned, NULL);

        if (result != -1)
            memcpy( (BYTE *)lphKey, lpOutBuf, sizeof(DWORD));

        CloseReflsdReg (hReg);
        
    }
    return result;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
rRegClose(
    DWORD hKey
    ) 
{
    int result = -1;
    HANDLE hReg;
    PPROCESS pprc = pActvProc;

    if (hReg = OpenRelfsdReg (pprc)) {

        DWORD nBytesReturned;

        result = FSIoctl (hReg, IOCTL_REG_CLOSE, &hKey, sizeof (DWORD),
                    NULL, 0, &nBytesReturned, NULL);

        CloseReflsdReg (hReg);
    }
    return result;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
rRegGet(
    DWORD hKey,
    const CHAR *szName,
    LPDWORD lpdwType,
    LPBYTE lpbData,
    LPDWORD lpdwSize
    ) 
{
    int result = -1;
    HANDLE hReg;
    PPROCESS pprc = pActvProc;

    if (hReg = OpenRelfsdReg (pprc)) {

        PBYTE lpInBuf;
        DWORD nInBufSize, nOutBufSize, nBytesReturned;
        LPDWORD lpOutBuf;

        // Pack hKey and szName into input buffer
        nInBufSize = sizeof(DWORD) + strlen(szName)+ 1;
        lpInBuf = (PBYTE) _alloca (nInBufSize);
        *(DWORD*)lpInBuf = hKey;
        memcpy (lpInBuf+sizeof(DWORD), szName, strlen(szName)+ 1);

        nOutBufSize = sizeof(DWORD)*2 + MAX_PATH;
        lpOutBuf = (LPDWORD) _alloca (nOutBufSize);

        result = FSIoctl (hReg, IOCTL_REG_GET,  lpInBuf,  nInBufSize,
                    lpOutBuf, nOutBufSize, &nBytesReturned, NULL);

        *lpdwType = *lpOutBuf++;
        *lpdwSize = *lpOutBuf++;
        memcpy (lpbData, lpOutBuf, *lpdwSize);

        CloseReflsdReg (hReg);
    }

    return result;
}

