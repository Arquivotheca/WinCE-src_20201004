//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
#include <kitlpriv.h>

// Constants for DeviceIOControl
enum {IOCTL_REG_OPEN, IOCTL_REG_CLOSE, IOCTL_REG_GET, IOCTL_REG_ENUM, IOCTL_WRITE_DEBUG};

// Registration database access functions for kernel debug support initialization

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
rRegOpen(
    DWORD hKey,
    CHAR *szName,
    LPDWORD lphKey
    ) 
{
    PBYTE lpInBuf, lpOutBuf;
    DWORD nInBufSize, nOutBufSize, nBytesReturned;
    int result;
    HANDLE hReg;

    // Temp fix to wait until RELFSD is loaded
    if (!OpenEvent (EVENT_ALL_ACCESS, FALSE, TEXT("WAIT_RELFSD2")))
        return FALSE;

    hReg = CreateFileW(TEXT("\\Release\\reg:"), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );

    if (hReg == INVALID_HANDLE_VALUE)
        return FALSE;

    // Pack hKey and szName into input buffer
    nInBufSize = sizeof(DWORD) + strlen(szName)+ 1;
    lpInBuf = (PBYTE) _alloca (nInBufSize);
    *(DWORD*)lpInBuf = hKey;
    memcpy (lpInBuf+sizeof(DWORD), szName, strlen(szName)+ 1);

    nOutBufSize = sizeof(DWORD);
    lpOutBuf = (PBYTE) _alloca (nOutBufSize);

    result = SC_DeviceIoControl (hReg, IOCTL_REG_OPEN,  lpInBuf,  nInBufSize,
                lpOutBuf, nOutBufSize, &nBytesReturned, NULL);

    if (result != -1)
        memcpy( (BYTE *)lphKey, lpOutBuf, sizeof(DWORD));
    CloseHandle (hReg);
    return result;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
rRegClose(
    DWORD hKey
    ) 
{
    PBYTE lpInBuf;
    DWORD nInBufSize, nBytesReturned;
    int result;
    HANDLE hReg;

    // Temp fix to wait until RELFSD is loaded
    if (!OpenEvent (EVENT_ALL_ACCESS, FALSE, TEXT("WAIT_RELFSD2")))
        return FALSE;

    hReg = CreateFileW(TEXT("\\Release\\reg:"), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );

    if (hReg == INVALID_HANDLE_VALUE)
        return FALSE;

    // Pack hKey into input buffer
    nInBufSize = sizeof(DWORD);
    lpInBuf = (PBYTE) _alloca (nInBufSize);
    *(DWORD*)lpInBuf = hKey;

    result = SC_DeviceIoControl (hReg, IOCTL_REG_CLOSE,  lpInBuf,  nInBufSize,
                NULL, 0, &nBytesReturned, NULL);

    CloseHandle (hReg);
    return result;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
rRegGet(
    DWORD hKey,
    CHAR *szName,
    LPDWORD lpdwType,
    LPBYTE lpbData,
    LPDWORD lpdwSize
    ) 
{
    PBYTE lpInBuf, lpOutBuf;
    DWORD nInBufSize, nOutBufSize, nBytesReturned;
    DWORD* tempOutBuf;
    int result;
    HANDLE hReg;

    // Temp fix to wait until RELFSD is loaded
    if (!OpenEvent (EVENT_ALL_ACCESS, FALSE, TEXT("WAIT_RELFSD2")))
        return FALSE;

    hReg = CreateFileW(TEXT("\\Release\\reg:"), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );

    if (hReg == INVALID_HANDLE_VALUE)
        return FALSE;

    // Pack hKey and szName into input buffer
    nInBufSize = sizeof(DWORD) + strlen(szName)+ 1;
    lpInBuf = (PBYTE) _alloca (nInBufSize);
    *(DWORD*)lpInBuf = hKey;
    memcpy (lpInBuf+sizeof(DWORD), szName, strlen(szName)+ 1);

    nOutBufSize = sizeof(DWORD)*2 + MAX_PATH;
    lpOutBuf = (PBYTE) _alloca (nOutBufSize);

    result = SC_DeviceIoControl (hReg, IOCTL_REG_GET,  lpInBuf,  nInBufSize,
                lpOutBuf, nOutBufSize, &nBytesReturned, NULL);

    //if (result == 1) {
        tempOutBuf = (DWORD*)lpOutBuf;
        *lpdwType = *tempOutBuf++;
        *lpdwSize = *tempOutBuf++;
        memcpy (lpbData, lpOutBuf + 2*sizeof(DWORD), *lpdwSize);
    //}

    CloseHandle (hReg);
    return result;
}

