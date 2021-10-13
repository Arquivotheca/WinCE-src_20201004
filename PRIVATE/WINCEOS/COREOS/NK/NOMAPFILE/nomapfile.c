//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*
 *              NK Kernel loader code
 *
 *
 * Module Name:
 *
 *              mapfile.c
 *
 * Abstract:
 *
 *              This file implements the NK kernel mapped file routines
 *
 */

#include "kernel.h"

// ??
const DWORD cinfMap;
HANDLE hMapList;
const PFNVOID MapMthds[] = {
    (PFNVOID)0,
    (PFNVOID)0,
    (PFNVOID)0,
};


LPVOID HugeVirtualReserve(DWORD dwSize, DWORD dwFlags)
{
//    KSetLastError(pCurThread, ERROR_OUTOFMEMORY);
    return (LPVOID)0;
}
BOOL 
HugeVirtualRelease(LPVOID pMem)
{
//    KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
    return FALSE;
}

LPVOID SC_MapViewOfFile(HANDLE hMap, DWORD fdwAccess, DWORD dwOffsetHigh, DWORD dwOffsetLow, DWORD cbMap)
{
//    KSetLastError(pCurThread, ERROR_OUTOFMEMORY);
    DEBUGMSG(1, (TEXT("MapViewOfFile failed: memory-mapped file component not present!\r\n")));
    return (LPVOID)0;
}
BOOL SC_UnmapViewOfFile(LPVOID lpvAddr)
{
//    KSetLastError(pCurThread, ERROR_INVALID_ADDRESS);
    return FALSE;
}
BOOL SC_FlushViewOfFile(LPCVOID lpBaseAddress, DWORD dwNumberOfBytesToFlush)
{
    return FALSE;
}
BOOL SC_FlushViewOfFileMaybe(LPCVOID lpBaseAddress, DWORD dwNumberOfBytesToFlush)
{
    return FALSE;
}
BOOL ChangeMapFlushing(LPCVOID lpBaseAddress, DWORD dwFlags)
{
    return FALSE;
}
int MappedPageIn(BOOL bWrite, DWORD addr)
{
    return 0 /*PAGEIN_FAILURE*/;
}
void PageOutFile(void)
{
}
BOOL TryCloseMappedHandle(HANDLE h)
{
//    KSetLastError(pCurThread, ERROR_INVALID_HANDLE);
    return FALSE;
}
void 
CloseMappedFileHandles()
{
}
HANDLE SC_CreateFileForMapping(LPCTSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                               LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                               DWORD dwFlagsAndAttributes, HANDLE hTemplateFile)
{
//    KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
    DEBUGMSG(1, (TEXT("CreateFileForMapping failed: memory-mapped file component not present!\r\n")));
    return INVALID_HANDLE_VALUE;
}
HANDLE SC_CreateFileMapping(HANDLE hFile, LPSECURITY_ATTRIBUTES lpsa, DWORD flProtect, DWORD dwMaxSizeHigh,
                            DWORD dwMaxSizeLow, LPCTSTR lpName)
{
//    KSetLastError(pCurThread, ERROR_INVALID_PARAMETER);
    DEBUGMSG(1, (TEXT("CreateFileMapping failed: memory-mapped file component not present!\r\n")));
    return INVALID_HANDLE_VALUE;
}
BOOL SC_MapCloseHandle(HANDLE hMap)
{
    return FALSE;
}

