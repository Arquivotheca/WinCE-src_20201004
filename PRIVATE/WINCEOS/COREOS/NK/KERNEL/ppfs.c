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

#if 1
extern CRITICAL_SECTION ppfscs;
extern int NoPPFS;                      // parallel port not present flag

// If PPFS is running over ethernet, buffer up as much data as possible (up to
// KITL_MAX_DATA_SIZE) to send to the other side.  The PpfsEthBuf memory is set
// up through the KITLRegisterDfltClient call.
extern BOOL BufferedPPFS;
extern UCHAR *PpfsEthBuf;

// Local state variables for handling translation of the packet based ethernet
// commands to the byte read write commands used by the ppfs engine.
static UCHAR *pWritePtr, *pReadPtr;
static DWORD dwRemainingBytes;
static DWORD dwBytesWrittenToBuffer;

void  (* lpParallelPortSendByteFunc)(BYTE ch);
int   (* lpParallelPortGetByteFunc)(void);


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
fill_KITL_buffer()
{
    DWORD dwLen = KITL_MAX_DATA_SIZE;
    if (!pKITLRecv(KITL_SVC_PPSH, PpfsEthBuf,&dwLen, INFINITE)) {
        NoPPFS = TRUE;
        return FALSE;
    }
    dwRemainingBytes = dwLen;
    pReadPtr = PpfsEthBuf;
    return TRUE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL 
write_KITL_buffer()
{
    if (dwBytesWrittenToBuffer &&
        !pKITLSend(KITL_SVC_PPSH,PpfsEthBuf,dwBytesWrittenToBuffer)) {
        NoPPFS = TRUE;
        return FALSE;
    }
    dwBytesWrittenToBuffer = 0;
    pWritePtr = PpfsEthBuf;
    return TRUE;
}


// Functions for lpParallelPortxxxByteFunc, set up by calling
// SetKernelCommDev(KERNEL_SVC_PPSH,KERNEL_COMM_ETHER).


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
ppfs_send_byte_ether(
    BYTE ch
    )
{
    *pWritePtr++ = ch;
    if (++dwBytesWrittenToBuffer == KITL_MAX_DATA_SIZE)
        write_KITL_buffer();
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
ppfs_get_byte_ether()
{
    // Refill buffer if necessary and return next byte
    if (dwRemainingBytes == 0) 
         fill_KITL_buffer();
    dwRemainingBytes--;
    return *pReadPtr++;
}

#endif

void 
SC_PPSHRestart(void) 
{
}


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



//------------------------------------------------------------------------------

// Routine for sending debug messages over PPSH.  Note the problem with the PPFS CS here -
// if we are called to put out a debug message while in a sys call, we can't block on the CS,
// but we can't just start sending data, because if someone is called into the loader, the
// other end will get confused (PPFS protocol can't handle packets within other packets).

#define MAX_DEBUGSTRING_LEN 512


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void 
PpfsWriteDebugString(
    unsigned short *name
    ) {

    PBYTE lpInBuf;
    DWORD nInBufSize, nBytesReturned;
    HANDLE hReg;


    // Temp fix to wait until RELFSD is loaded
    if (!OpenEvent (EVENT_ALL_ACCESS, FALSE, TEXT("WAIT_RELFSD2")))
        return;

    hReg = CreateFileW(TEXT("\\Release\\reg:"), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );

    // Pack name into input buffer
    nInBufSize = strlenW(name);
    lpInBuf = (PBYTE)name;

    SC_DeviceIoControl (hReg, IOCTL_WRITE_DEBUG,  lpInBuf,  nInBufSize, NULL, 0, &nBytesReturned, NULL);

    CloseHandle (hReg);
}


