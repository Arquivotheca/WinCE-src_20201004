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
//
//    fscall.c - implementations kernel calls to filesys
//

#include <windows.h>
#include <kernel.h>
#include <fscall.h>
#include <loaderverifier.h>

#define HANDLE_CALL(pci, ftype, api, args)  (((ftype) pci->ppfnIntMethods[api]) args)
#define FILESYS_CALL(ftype, api, args)      (SystemAPISets[SH_FILESYS_APIS]? (((ftype) SystemAPISets[SH_FILESYS_APIS]->ppfnIntMethods[api]) args) : 0)

//
// FILE APIS (handle based)
//
#define FSAPI_READFILE                        ID_HCALL(0)     // 2 - ReadFile
#define FSAPI_WRITEFILE                       ID_HCALL(1)     // 3 - WriteFile
#define FSAPI_GETFILESIZE                     ID_HCALL(2)     // 4 - GetFileSize
#define FSAPI_SETFILEPOINTER                  ID_HCALL(3)     // 5 - SetFilePointer
#define FSAPI_GETFILEINFOBYHANDLE             ID_HCALL(4)     // 6 - GetFileInformationByHandle
#define FSAPI_FLUSHFILEBUFFERS                ID_HCALL(5)     // 7 - FlushFileBuffers
#define FSAPI_GETFILETIME                     ID_HCALL(6)     // 8 - GetFileTime
#define FSAPI_SETFILETIME                     ID_HCALL(7)     // 9 - SetFileTime
#define FSAPI_SETENDOFFILE                    ID_HCALL(8)     // 10 - SetEndOfFile
#define FSAPI_DEVICEIOCTL                     ID_HCALL(9)     // 11 - DeviceIoControl
#define FSAPI_READFILEWITHSEEK                ID_HCALL(10)    // 12 - ReadFileWithSeek
#define FSAPI_WRITEFILEWITHSEEK               ID_HCALL(11)    // 13 - WriteFileWithSeek

//
// FILESYS APIS (non-handle based)
//
#define FSYSAPI_FINDFIRSTFILE                   8           // FindFirstFileW
#define FSYSAPI_CREATEFILE                      9           // CreateFileW
#define FSYSAPI_REGCLOSE                        17          // RegCloseKey
#define FSYSAPI_REGCREATEEX                     18          // RegCreateKeyEx
#define FSYSAPI_REGOPENEX                       23          // RegOpenKeyEx
#define FSYSAPI_REGQUERYEX                      25          // RegQueryValueEx
#define FSYSAPI_REGSETEX                        26          // RegSetValueEx
#define FSYSAPI_REGFLUSH                        49          // RegCloseKey
#define FSYSAPI_NOTIFYFS                        50          // NotifyMountedFS
#define FSYSAPI_REGTESTSET                      134         // RegTestSetValue
#define FSYSAPI_OPENMODULE                      141

// The IsKernelVa checks below are to ensure that we never trust the file
// system (which could involve 3rd party code) to access a static-mapped
// address.  The file system could accidentally overflow into unrelated
// physical memory.


//--------------------------------------------------------------------------------
// handle based calls to filesys, using PHDATA
//--------------------------------------------------------------------------------

static BOOL DoSetFilePointer (PHDATA phdFile, LONG lToMove, PLONG lpToMoveHigh,
                              DWORD dwMoveMethod, DWORD* pdwRet)
{
    BOOL fResult = TRUE;
    DWORD dwRet = HANDLE_CALL (
        phdFile->pci,                                           // pci
        DWORD (*) (LPVOID, LONG, PLONG, DWORD),                 // function type
        FSAPI_SETFILEPOINTER,                                   // FILE API #5 == SetFilePointer
        (phdFile->pvObj, lToMove, lpToMoveHigh, dwMoveMethod)); // args
    
    if ((DWORD)-1 == dwRet) {
        // Have to do extra checking to see if the call failed, in case the file
        // is that size
        if (NO_ERROR != KGetLastError (pCurThread)) {
            fResult = FALSE;
        }
    }
    
    if (pdwRet) {
        *pdwRet = dwRet;
    }
    return fResult;
}

// SetFilePointer
DWORD PHD_SetFilePointer (PHDATA phdFile, LONG lToMove, PLONG lpToMoveHigh, DWORD dwMoveMethod)
{
    DWORD dwRet;

    // change active process to filesys (nk)
    PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
    
    DEBUGMSG (ZONE_LOADER2, (L"PHD_SetFilePointer %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
        phdFile, lToMove, lpToMoveHigh, dwMoveMethod));

    // Sanity check, if file is large should use SetLargeFilePointer for rollover safety
    DEBUGCHK (!lpToMoveHigh && (lToMove >= 0));

    DoSetFilePointer (phdFile, lToMove, lpToMoveHigh, dwMoveMethod, &dwRet);

    // restore active process
    SwitchActiveProcess (pprc);
    return dwRet;
}

// Helper for very large files
BOOL PHD_SetLargeFilePointer (PHDATA phdFile, ULARGE_INTEGER liFileOffset)
{
    BOOL  fRet, fFirst;
    LONG  lMoveLow;
    LONG  lMoveHigh;

    // change active process to filesys (nk)
    PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
    
    DEBUGMSG (ZONE_MAPFILE, (L"PHD_SetLargeFilePointer %8.8lx %8.8lx %8.8lx\r\n",
        phdFile, liFileOffset.HighPart, liFileOffset.LowPart));

    // SetFilePointer uses signed numbers, while file sizes are unsigned.  Avoid
    // negative seeks by cutting down to positive numbers and moving up to 3 times.
    fFirst = TRUE;
    do {
        if (liFileOffset.HighPart & 0x80000000) {
            lMoveHigh = 0x7FFFFFFF;
        } else {
            lMoveHigh = liFileOffset.HighPart;
        }
        liFileOffset.HighPart -= lMoveHigh;
        
        if (liFileOffset.LowPart & 0x80000000) {
            lMoveLow = 0x7FFFFFFF;
        } else {
            lMoveLow = liFileOffset.LowPart;
        }
        liFileOffset.LowPart -= lMoveLow;

        // On the first seek, start at the beginning; else use current position.
        fRet = DoSetFilePointer (phdFile, lMoveLow, &lMoveHigh,
                                 fFirst ? FILE_BEGIN : FILE_CURRENT, NULL);
        if (!fRet) {
            break;
        }
        fFirst = FALSE;

    } while (liFileOffset.QuadPart);
    
    // restore active process
    SwitchActiveProcess (pprc);
    return fRet;
}

// Helper for very large files
BOOL PHD_GetLargeFileSize (PHDATA phdFile, ULARGE_INTEGER* pliFileSize)
{
    BOOL  fRet;

    // change active process to filesys (nk)
    PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
    
    DEBUGMSG (ZONE_MAPFILE, (L"PHD_GetLargeFileSize %8.8lx %8.8lx\r\n",
        phdFile, pliFileSize));

    pliFileSize->QuadPart = 0;
    fRet = DoSetFilePointer (phdFile, 0, (PLONG) &pliFileSize->HighPart, FILE_END,
                             &pliFileSize->LowPart);

    // restore active process
    SwitchActiveProcess (pprc);
    return fRet;
}

// ReadFile
BOOL PHD_ReadFile (PHDATA phdFile, LPVOID lpBuffer, DWORD cbToRead, LPDWORD pcbRead, LPOVERLAPPED lpOvp)
{
    BOOL fRet;
    // change active process to filesys (nk)
    PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
    
    DEBUGCHK (!IsKernelVa (lpBuffer));

    DEBUGMSG (ZONE_LOADER2, (L"PHD_ReadFile %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
        phdFile, lpBuffer, cbToRead, cbToRead, lpOvp));
    fRet = HANDLE_CALL (
        phdFile->pci,                                            // pci
        BOOL (*) (LPVOID, LPVOID, DWORD, LPDWORD, LPOVERLAPPED), // function type
        FSAPI_READFILE,                                          // FILE API #2 == ReadFile
        (phdFile->pvObj, lpBuffer, cbToRead, pcbRead, lpOvp));   // args
    
    // restore active process
    SwitchActiveProcess (pprc);
    return fRet;
}

// WriteFile
BOOL PHD_WriteFile (PHDATA phdFile, LPCVOID lpBuffer, DWORD cbToWrite, LPDWORD pcbWritten, LPOVERLAPPED lpOvp)
{
    BOOL fRet;
    // change active process to filesys (nk)
    PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
    
    DEBUGCHK (!IsKernelVa (lpBuffer));

    DEBUGMSG (ZONE_LOADER2, (L"PHD_WriteFile %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
        phdFile, lpBuffer, cbToWrite, pcbWritten, lpOvp));
    fRet = HANDLE_CALL (
        phdFile->pci,                                               // pci
        BOOL (*) (LPVOID, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED),   // function type
        FSAPI_WRITEFILE,                                            // FILE API #3 == WriteFile
        (phdFile->pvObj, lpBuffer, cbToWrite, pcbWritten, lpOvp));  // args
    // restore active process
    SwitchActiveProcess (pprc);
    return fRet;
}

// ReadFileWithSeek
BOOL PHD_ReadFileWithSeek (PHDATA phdFile, LPVOID lpBuffer, DWORD cbToRead, LPDWORD pcbRead, LPOVERLAPPED lpOvp, DWORD dwOfst, DWORD dwOfstHigh)
{
    BOOL fRet;
    // change active process to filesys (nk)
    PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
    
    DEBUGCHK (!IsKernelVa (lpBuffer));

    DEBUGMSG (ZONE_LOADER2, (L"PHD_ReadFileWithSeek %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
        phdFile, lpBuffer, cbToRead, pcbRead, lpOvp, dwOfst, dwOfstHigh));
    fRet = HANDLE_CALL (
        phdFile->pci,                                                           // pci
        BOOL (*) (LPVOID, LPVOID, DWORD, LPDWORD, LPOVERLAPPED, DWORD, DWORD),  // function type
        FSAPI_READFILEWITHSEEK,                                                 // FILE API #12 == ReadFileWithSeek
        (phdFile->pvObj, lpBuffer, cbToRead, pcbRead, lpOvp, dwOfst, dwOfstHigh));   // args
    // restore active process
    SwitchActiveProcess (pprc);
    return fRet;
}

// WriteFileWithSeek
BOOL PHD_WriteFileWithSeek (PHDATA phdFile, LPCVOID lpBuffer, DWORD cbToWrite, LPDWORD pcbWritten, LPOVERLAPPED lpOvp, DWORD dwOfst, DWORD dwOfstHigh)
{
    BOOL fRet;
    // change active process to filesys (nk)
    PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
    
    DEBUGCHK (!IsKernelVa (lpBuffer));

    DEBUGMSG (ZONE_LOADER2, (L"PHD_WriteFileWithSeek %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
            phdFile, lpBuffer, cbToWrite, pcbWritten, lpOvp, dwOfst, dwOfstHigh));
    fRet = HANDLE_CALL (
        phdFile->pci,                                                           // pci
        BOOL (*) (LPVOID, LPCVOID, DWORD, LPDWORD, LPOVERLAPPED, DWORD, DWORD), // function type
        FSAPI_WRITEFILEWITHSEEK,                                                // FILE API #13 == WriteFileWithSeek
        (phdFile->pvObj, lpBuffer, cbToWrite, pcbWritten, lpOvp, dwOfst, dwOfstHigh));  // args
    // restore active process
    SwitchActiveProcess (pprc);
    return fRet;
}

// GetFileInformationByHandle
BOOL PHD_GetFileInfo (PHDATA phdFile, PBY_HANDLE_FILE_INFORMATION pbhi)
{
    BOOL fRet;
    // change active process to filesys (nk)
    PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
    
    DEBUGCHK (!IsKernelVa (pbhi));

    DEBUGMSG (ZONE_LOADER2, (L"PHD_GetFileInfo %8.8lx %8.8lx\r\n", phdFile, pbhi));
    fRet = HANDLE_CALL (
        phdFile->pci,                                           // pci
        BOOL (*) (LPVOID, PBY_HANDLE_FILE_INFORMATION, DWORD),  // function type
        FSAPI_GETFILEINFOBYHANDLE,                              // FILE API #6 == GetFileInformationByHandle
        (phdFile->pvObj, pbhi, sizeof (BY_HANDLE_FILE_INFORMATION))); // args
    // restore active process
    SwitchActiveProcess (pprc);
    return fRet;
}

// FlushFileBuffers
BOOL PHD_FlushFileBuffers (PHDATA phdFile)
{
    BOOL fRet;
    // change active process to filesys (nk)
    PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
    
    DEBUGMSG (ZONE_LOADER2, (L"PHD_FlushFileBuffers %8.8lx\r\n", phdFile));
    fRet = HANDLE_CALL (
        phdFile->pci,                                           // pci
        BOOL (*) (LPVOID),                                      // function type
        FSAPI_FLUSHFILEBUFFERS,                                 // FILE API #7 == FlushFileBuffers
        (phdFile->pvObj));                                      // args
    // restore active process
    SwitchActiveProcess (pprc);
    return fRet;
}

// SetEndOfFile
BOOL PHD_SetEndOfFile (PHDATA phdFile)
{
    BOOL fRet;
    // change active process to filesys (nk)
    PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
    
    DEBUGMSG (ZONE_LOADER2, (L"PHD_SetEndOfFile %8.8lx\r\n", phdFile));
    fRet = HANDLE_CALL (
        phdFile->pci,             // pci
        BOOL (*) (LPVOID),        // function type
        FSAPI_SETENDOFFILE,       // FILE API #10 == SetEndOfFile
        (phdFile->pvObj));        // args
    // restore active process
    SwitchActiveProcess (pprc);
    return fRet;
}

// DeviceIoControl
BOOL PHD_FSIoctl (PHDATA phdFile, DWORD dwCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD pcbRet, LPOVERLAPPED lpOverlapped)
{
    BOOL fRet;
    
    // change active process to filesys (nk)
    BOOL fOldDirectCall = NKSetDirectCall (TRUE);
    PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
    
    DEBUGCHK (!lpInBuf || !IsKernelVa (lpInBuf));

    DEBUGMSG (ZONE_LOADER2, (L"PHD_FSIoctl %8.8lx\r\n", phdFile));
    fRet = HANDLE_CALL (
        phdFile->pci,                                                                       // pci
        BOOL (*) (LPVOID, DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD, LPOVERLAPPED),      // function type
        FSAPI_DEVICEIOCTL,                                                                  // FILE API #11 == DeviceIoControl
        (phdFile->pvObj, dwCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, pcbRet, lpOverlapped));// args

    // restore active process
    SwitchActiveProcess (pprc);
    NKSetDirectCall(fOldDirectCall);
    
    return fRet;
}

static BOOL DoPHDClose (PHDATA phd, DWORD idxAPI)
{
    BOOL     fRet;
    DWORD    dwServerId = phd->pci->dwServerId;
    PPROCESS pprcActv   = SwitchActiveProcess (g_pprcNK);

    DEBUGMSG (ZONE_LOADER2, (L"DoPHDClose %8.8lx, server id = %8.8lx\r\n", phd, dwServerId? dwServerId : g_pprcNK->dwId));

    DEBUGCHK (idxAPI <= 1);

    if (!dwServerId || (dwServerId == g_pprcNK->dwId)) {
        // kernel mode server, call direct

        fRet = HANDLE_CALL (
            phd->pci,                   // pci
            BOOL (*) (LPVOID, DWORD),   // function type
            idxAPI,                     // Handle based API #0, or #1 - close handle, or pre-close handle
            (phd->pvObj, phd->dwData)); // args (Note: k-mode server gets an extra argument in close handle)

        
    } else {
        // user-mode server
        DHCALL_STRUCT hc;
        hc.args[0]  = (REGTYPE) phd->pvObj;
        hc.cArgs    = 0;
        hc.dwAPI    = idxAPI;
        hc.dwErrRtn = FALSE;
        __try {
            fRet = MDCallUserHAPI (phd, &hc);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            fRet = FALSE;
        }
    }

    SwitchActiveProcess (pprcActv);
    return fRet;
}

BOOL PHD_CloseHandle (PHDATA phd)
{
    return DoPHDClose (phd, 0);
}

BOOL PHD_PreCloseHandle (PHDATA phd)
{
    return DoPHDClose (phd, 1);
}


//--------------------------------------------------------------------------------
// handle based calls to filesys, using Handle
//--------------------------------------------------------------------------------

// SetFilePointer
DWORD FSSetFilePointer (HANDLE hFile, LONG lToMove, PLONG lpToMoveHigh, DWORD dwMoveMethod)
{
    PHDATA phd = LockHandleData (hFile, g_pprcNK);
    DWORD dwRet = (DWORD) -1;
    if (phd) {
        dwRet = PHD_SetFilePointer (phd, lToMove, lpToMoveHigh, dwMoveMethod);
        UnlockHandleData (phd);
    }
    return dwRet;
}

// ReadFile
BOOL FSReadFile (HANDLE hFile, LPVOID lpBuffer, DWORD cbToRead, LPDWORD pcbRead, LPOVERLAPPED lpOvp)
{
    PHDATA phd = LockHandleData (hFile, g_pprcNK);
    BOOL fRet = FALSE;
    if (phd) {
        fRet = PHD_ReadFile (phd, lpBuffer, cbToRead, pcbRead, lpOvp);
        UnlockHandleData (phd);
    }
    return fRet;
}

// WriteFile
BOOL FSWriteFile (HANDLE hFile, LPCVOID lpBuffer, DWORD cbToWrite, LPDWORD pcbWritten, LPOVERLAPPED lpOvp)
{
    PHDATA phd = LockHandleData (hFile, g_pprcNK);
    BOOL fRet = FALSE;
    if (phd) {
        fRet = PHD_WriteFile (phd, lpBuffer, cbToWrite, pcbWritten, lpOvp);
        UnlockHandleData (phd);
    }
    return fRet;
}

// ReadFileWithSeek
BOOL FSReadFileWithSeek (HANDLE hFile, LPVOID lpBuffer, DWORD cbToRead, LPDWORD pcbRead, LPOVERLAPPED lpOvp, DWORD dwOfst, DWORD dwOfstHigh)
{
    PHDATA phd = LockHandleData (hFile, g_pprcNK);
    BOOL fRet = FALSE;
    if (phd) {
        fRet = PHD_ReadFileWithSeek (phd, lpBuffer, cbToRead, pcbRead, lpOvp, dwOfst, dwOfstHigh);
        UnlockHandleData (phd);
    }
    return fRet;
}

// WriteFileWithSeek
BOOL FSWriteFileWithSeek (HANDLE hFile, LPCVOID lpBuffer, DWORD cbToWrite, LPDWORD pcbWritten, LPOVERLAPPED lpOvp, DWORD dwOfst, DWORD dwOfstHigh)
{
    PHDATA phd = LockHandleData (hFile, g_pprcNK);
    BOOL fRet = FALSE;
    if (phd) {
        fRet = PHD_WriteFileWithSeek (phd, lpBuffer, cbToWrite, pcbWritten, lpOvp, dwOfst, dwOfstHigh);
        UnlockHandleData (phd);
    }
    return fRet;
}

// GetFileInformationByHandle
BOOL FSGetFileInfo (HANDLE hFile, PBY_HANDLE_FILE_INFORMATION pbhi)
{
    PHDATA phd = LockHandleData (hFile, g_pprcNK);
    BOOL fRet = FALSE;
    if (phd) {
        fRet = PHD_GetFileInfo (phd, pbhi);
        UnlockHandleData (phd);
    }
    return fRet;
}

// check if a file is a system file
BOOL FSSetEndOfFile (HANDLE hFile)
{
    PHDATA phd = LockHandleData (hFile, g_pprcNK);
    BOOL fRet = FALSE;
    if (phd) {
        fRet = PHD_SetEndOfFile (phd);
        UnlockHandleData (phd);
    }
    return fRet;
}

// DeviceIoControl
BOOL FSIoctl (HANDLE hFile, DWORD dwCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD pcbRet, LPOVERLAPPED lpOverlapped)
{
    PHDATA phd = LockHandleData (hFile, g_pprcNK);
    BOOL fRet = FALSE;
    if (phd) {
        fRet = PHD_FSIoctl (phd, dwCode, lpInBuf, nInBufSize, lpOutBuf, nOutBufSize, pcbRet, lpOverlapped);
        UnlockHandleData (phd);
    }
    return fRet;
}


//--------------------------------------------------------------------------------
// Filesys APIs
//--------------------------------------------------------------------------------


//
// simple fscall - handle case where there is only 1 pointer argument
//
static LONG SimpleFsysCall (LPVOID pvArg0, DWORD idxAPI, LONG dwDfltRtn)
{
    if (IsSystemAPISetReady (SH_FILESYS_APIS)) {

        // change active process to filesys (nk)
        PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
        
        dwDfltRtn = FILESYS_CALL (
            LONG (*) (LPVOID),          // function type
            idxAPI,                     // api index
            (pvArg0));                  // args

        // restore active process
        SwitchActiveProcess (pprc);
    }
    return dwDfltRtn;
}


// the CreateFile API
HANDLE FSCreateFile (LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpsa, 
        DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
    // need to change current process to NK or else the handle created will not belong to NK
    PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
    HANDLE hRet;
    
    DEBUGMSG (ZONE_LOADER2, (L"FSCreateFile '%s'\r\n", lpFileName));
    hRet = FILESYS_CALL (
        HANDLE (*)  (LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE),      // function type
        FSYSAPI_CREATEFILE,                                                                  // FILESYS API #9 == CreateFileW
        (lpFileName, dwDesiredAccess, dwShareMode, lpsa, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile)); // args
        

    // restore active process
    SwitchActiveProcess (pprc);

    return hRet;
}

// RegQueryValueEx
LONG NKRegQueryValueExW (HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData)
{
    // This function is exposed to the OAL via NKGLOBAL, and to CeLog via
    // CeLogPrivateImports, so safety-check for filesys
    if (!IsSystemAPISetReady (SH_FILESYS_APIS)) {
        DEBUGMSG (ZONE_ERROR, (L"ERROR: NKRegQueryValueExW - Registry is not ready\r\n"));
        return ERROR_NOT_READY;
    } else {
        LONG lRet;

        // IN/OUT argument lpcbData is converted into separate IN and OUT arguments.
        DWORD cbData = lpcbData ? *lpcbData : 0;

        // change active process to filesys (nk)
        PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
        
        DEBUGMSG (ZONE_LOADER2, (L"NKRegQueryValueExW %8.8lx '%s' %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
            hKey, lpValueName, lpReserved, lpType, lpData, lpcbData));
        lRet = FILESYS_CALL (
            LONG (*) (HKEY, LPCWSTR, LPDWORD, LPDWORD, LPBYTE, DWORD, LPDWORD), // function type
            FSYSAPI_REGQUERYEX,                                                 // FILESYS #25 - RegQueryValueExW
            (hKey, lpValueName, lpReserved, lpType, lpData, cbData, lpcbData)); // args

        // restore active process
        SwitchActiveProcess (pprc);
        return lRet;
    }
}

LONG NKRegSetValueExW (HKEY hKey, LPCWSTR lpValueName, DWORD dwReserved, DWORD dwType, LPBYTE lpData, DWORD cbData)
{
    // This function is exposed to the OAL via NKGLOBAL so safety-check for filesys
    if (!IsSystemAPISetReady (SH_FILESYS_APIS)) {
        DEBUGMSG (ZONE_ERROR, (L"ERROR: NKRegSetValueExW - Registry is not ready\r\n"));
        return ERROR_NOT_READY;
    } else {
        LONG lRet;
        // change active process to filesys (nk)
        PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
        
        DEBUGMSG (ZONE_LOADER2, (L"NKRegSetValueExW %8.8lx '%s' %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
            hKey, lpValueName, dwReserved, dwType, lpData, cbData));
        lRet = FILESYS_CALL (
            LONG (*) (HKEY, LPCWSTR, DWORD, DWORD, LPBYTE, DWORD),              // function type
            FSYSAPI_REGSETEX,                                                   // FILESYS #26 - RegSetValueExW
            (hKey, lpValueName, dwReserved, dwType, lpData, cbData));           // args
        // restore active process
        SwitchActiveProcess (pprc);
        return lRet;
    }
}

LONG NKRegTestSetValueW (HKEY hKey, LPCWSTR lpValueName, DWORD dwType, LPBYTE lpOldData, DWORD cbOldData, LPBYTE lpNewData, DWORD cbNewData, DWORD dwFlags) {
    LONG lRet;
    // change active process to filesys (nk)
    PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
    
    DEBUGMSG (ZONE_LOADER2, (L"NKRegTestSetValueExW %8.8lx '%s' %8.8lx %8.8lx %d %8.8lx %d %8.8lx\r\n",
        hKey, lpValueName, dwType, lpOldData, cbOldData, lpNewData, cbNewData, dwFlags));
    lRet = FILESYS_CALL (
        LONG (*) (HKEY, LPCWSTR, DWORD, LPBYTE, DWORD, LPBYTE, DWORD, DWORD),               // function type
        FSYSAPI_REGTESTSET,                                                                 // FILESYS #134 - RegTestSetValueExW
        (hKey, lpValueName, dwType, lpOldData, cbOldData, lpNewData, cbNewData, dwFlags));  // args
    // restore active process
    SwitchActiveProcess (pprc);
    return lRet;
}

LONG NKRegOpenKeyExW (HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult)
{
    // This function is exposed to the OAL via NKGLOBAL so safety-check for filesys
    if (!IsSystemAPISetReady (SH_FILESYS_APIS)) {
        DEBUGMSG (ZONE_ERROR, (L"ERROR: NKRegOpenKeyExW - Registry is not ready\r\n"));
        return ERROR_NOT_READY;
    } else {
        LONG lRet;
        // change active process to filesys (nk)
        PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
        
        DEBUGMSG (ZONE_LOADER2, (L"NKRegOpenKeyExW %8.8lx '%s' %8.8lx %8.8lx %8.8lx\r\n",
            hKey, lpSubKey, ulOptions, samDesired, phkResult));
        lRet = FILESYS_CALL (
            LONG (*) (HKEY, LPCWSTR, DWORD, REGSAM, PHKEY),                     // function type
            FSYSAPI_REGOPENEX,                                                  // FILESYS #23 - RegOpenKeyExW
            (hKey, lpSubKey, ulOptions, samDesired, phkResult));                // args
        // restore active process
        SwitchActiveProcess (pprc);
        return lRet;
    }
}

LONG NKRegCreateKeyExW (HKEY hKey, LPCWSTR lpSubKey, DWORD Reserved, LPWSTR lpClass, DWORD dwOptions, 
    REGSAM samDesired, LPSECURITY_ATTRIBUTES lpsa, PHKEY phkResult, LPDWORD lpdwDisp)
{
    // This function is exposed to the OAL via NKGLOBAL so safety-check for filesys
    if (!IsSystemAPISetReady (SH_FILESYS_APIS)) {
        DEBUGMSG (ZONE_ERROR, (L"ERROR: NKRegCreateKeyExW - Registry is not ready\r\n"));
        return ERROR_NOT_READY;
    } else {
        LONG lRet;
        // change active process to filesys (nk)
        PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
        
        DEBUGMSG (ZONE_LOADER2, (L"NKRegCreateKeyExW %8.8lx '%s' %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx\r\n",
            hKey, lpSubKey, Reserved, lpClass, dwOptions, samDesired, lpsa, phkResult, lpdwDisp));
        lRet = FILESYS_CALL (
            LONG (*) (HKEY, LPCWSTR, DWORD, LPWSTR, DWORD, REGSAM, LPSECURITY_ATTRIBUTES, PHKEY, LPDWORD),  // function type
            FSYSAPI_REGCREATEEX,                                                                            // FILESYS #18 - RegCreateKeyEx
            (hKey, lpSubKey, Reserved, lpClass, dwOptions, samDesired, lpsa, phkResult, lpdwDisp));         // args
        // restore active process
        SwitchActiveProcess (pprc);
        return lRet;
    }
}

LONG NKRegCloseKey (HKEY hKey)
{
    DEBUGMSG (ZONE_LOADER2, (L"NKRegCloseKey %8.8lx\r\n", hKey));

    return SimpleFsysCall ((LPVOID) hKey, FSYSAPI_REGCLOSE, ERROR_NOT_READY);
    
}

LONG NKRegFlushKey (HKEY hKey)
{
    DEBUGMSG (ZONE_LOADER2, (L"NKRegCloseKey %8.8lx\r\n", hKey));

    return SimpleFsysCall ((LPVOID) hKey, FSYSAPI_REGFLUSH, ERROR_NOT_READY);
    
}

HANDLE FSOpenModule (LPCWSTR ModuleName, DWORD Flags)
{
    HANDLE h;

    if (!IsSystemAPISetReady (SH_FILESYS_APIS)) {
        DEBUGMSG (ZONE_LOADER2, (TEXT("FSOpenModule: bypass\r\n")));
        h = INVALID_HANDLE_VALUE;
        
    } else {
        // change active process to filesys (nk)
        PPROCESS pprc = SwitchActiveProcess (g_pprcNK);
        
        DEBUGMSG (ZONE_LOADER2, (L"FSOpenModule '%s' %8.8lx \r\n",
            ModuleName, Flags));
        h = FILESYS_CALL (
            HANDLE (*) (LPCWSTR, DWORD),
            FSYSAPI_OPENMODULE,
            (ModuleName, Flags));         // args
        // restore active process
        SwitchActiveProcess (pprc);
    }
    
    return h;
}

void FSNotifyMountedFS (DWORD dwFlags)
{
    SimpleFsysCall ((LPVOID) dwFlags, FSYSAPI_NOTIFYFS, 0);
}

