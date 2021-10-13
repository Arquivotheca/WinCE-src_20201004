//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>
#include <coredll.h>
#include "heap.h"
#include "lmemdebug.h"

// Make sure that no macro are redirecting LocalAlloc & HeapAlloc
#undef LocalAlloc
#undef HeapAlloc

HANDLE RemoteGetProcessHeap(HANDLE hProc)
{
    HANDLE *pHProcHeap;
    HANDLE hHeap;

    if (!hProc && !(hProc = GetCallerProcess()))
        hProc = (HANDLE)GetCurrentProcessId();
    pHProcHeap = (HANDLE *)MapPtrToProcess(&hProcessHeap, hProc);
    __try {
        hHeap = pHProcHeap ? *pHProcHeap : NULL;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        hHeap = NULL;
    }
    return hHeap;
}

LPVOID RemoteCheckPtr(HANDLE hHeap, LPVOID lpMem)
{
    if (!((DWORD)lpMem >> SECTION_SHIFT)) {
        // Pointer is not mapped.  Map into the same slot as the heap handle.
        lpMem = (LPVOID)((DWORD)lpMem | ((DWORD)hHeap & ~((1<<SECTION_SHIFT) - 1)));
    }
    // Make sure that the pointer is > 64K w/in the process's slot.
    if (ZeroPtr(lpMem) < 0x10000)
        lpMem = NULL;
    return lpMem;
}


LPVOID RemoteHeapAlloc(HANDLE hProc, HANDLE hHeap, DWORD dwFlags, DWORD dwBytes)
{
    LPVOID pv = NULL;

    // Heap pointers are mapped into the process specific slot so just
    // call the heap directly.
    if (!hHeap && !(hHeap = RemoteGetProcessHeap(hProc))) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    __try {
        pv = Int_HeapAlloc(hHeap, dwFlags, dwBytes);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        pv = NULL;
    }
    DEBUGMSG(DBGRMEM, (_T("--RemoteHeapAlloc (0x%08x, 0x%08x, 0x%08x, %u) ==> 0x%08x\r\n"),
        hProc, hHeap, dwFlags, dwBytes, pv));
    return pv;
}


LPVOID RemoteHeapReAlloc(HANDLE hProc, HANDLE hHeap, DWORD dwFlags, DWORD dwBytes, LPVOID lpMem)
{
    LPVOID pv = NULL;

    // Heap pointers are mapped into the process specific slot so just
    // call the heap directly.
    if ((!hHeap && !(hHeap = RemoteGetProcessHeap(hProc))) || !(lpMem = RemoteCheckPtr(hHeap, lpMem))) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    __try {
        pv = Int_HeapReAlloc(hHeap, dwFlags, lpMem, dwBytes);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        pv = NULL;
    }
    DEBUGMSG(DBGRMEM, (_T("--RemoteHeapReAlloc (0x%08x, 0x%08x, 0x%08x, %u, %08x) ==> 0x%08x\r\n"),
        hProc, hHeap, dwFlags, dwBytes, lpMem, pv));
    return pv;
}


BOOL RemoteHeapFree(HANDLE hProc, HANDLE hHeap, DWORD dwFlags, LPVOID lpMem)
{
    BOOL fRet = FALSE;

    // Heap pointers are mapped into the process specific slot so just
    // call the heap directly.
    if ((!hHeap && !(hHeap = RemoteGetProcessHeap(hProc))) || !(lpMem = RemoteCheckPtr(hHeap, lpMem))) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    __try {
        fRet = Int_HeapFree(hHeap, dwFlags, lpMem);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fRet = FALSE;
    }
    DEBUGMSG(DBGRMEM, (_T("--RemoteHeapFree (0x%08x, 0x%08x, 0x%08x, %08x) ==> %u\r\n"),
        hProc, hHeap, dwFlags, lpMem, fRet));
    return fRet;
}


DWORD RemoteHeapSize(HANDLE hProc, HANDLE hHeap, DWORD dwFlags, LPVOID lpMem)
{
    DWORD dwRet = (DWORD)-1;

    // Heap pointers are mapped into the process specific slot so just
    // call the heap directly.
    if ((!hHeap && !(hHeap = RemoteGetProcessHeap(hProc))) || !(lpMem = RemoteCheckPtr(hHeap, lpMem))) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return (DWORD)-1;
    }
    __try {
        dwRet = Int_HeapSize(hHeap, dwFlags, lpMem);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        dwRet = (DWORD)-1;
    }
    DEBUGMSG(DBGRMEM, (_T("--RemoteHeapSize (0x%08x, 0x%08x, 0x%08x, %08x) ==> %u\r\n"),
        hProc, hHeap, dwFlags, lpMem, dwRet));
    return dwRet;
}


HLOCAL WINAPI RemoteLocalAlloc(UINT uFlags, UINT uBytes)
{
    return RemoteHeapAlloc(NULL, NULL, ((uFlags & LMEM_ZEROINIT) ? HEAP_ZERO_MEMORY : 0), uBytes);
}


HLOCAL WINAPI RemoteLocalReAlloc(HLOCAL hMem, UINT uBytes, UINT uFlags)
{
    DWORD dwFlags;

    dwFlags = ((uFlags & LMEM_ZEROINIT) ? HEAP_ZERO_MEMORY : 0) | ((uFlags & LMEM_MOVEABLE) ? 0 : HEAP_REALLOC_IN_PLACE_ONLY);
    return RemoteHeapReAlloc(NULL, NULL, dwFlags, uBytes, (LPVOID)hMem);
}

UINT WINAPI RemoteLocalSize(HLOCAL hMem)
{
    DWORD dwSize;

    dwSize = RemoteHeapSize(NULL, NULL, 0, (LPVOID)hMem);
    return dwSize==(DWORD)-1 ? 0 : (UINT)dwSize;
}

HLOCAL WINAPI RemoteLocalFree(HLOCAL hMem)
{
    if (!hMem || RemoteHeapFree(NULL, NULL, 0, (LPVOID)hMem))
        hMem = NULL;
    else
        SetLastError(ERROR_INVALID_PARAMETER);
    return hMem;
}


HLOCAL WINAPI LocalAllocInProcess (UINT uFlags, UINT uBytes, HPROCESS hProc)
{
    return RemoteHeapAlloc(hProc, NULL, ((uFlags & LMEM_ZEROINIT) ? HEAP_ZERO_MEMORY : 0), uBytes);
}


HLOCAL WINAPI LocalFreeInProcess (HLOCAL hMem, HPROCESS hProc)
{
    if (!hMem || RemoteHeapFree(hProc, NULL, 0, (LPVOID)hMem))
        hMem = NULL;
    else
        SetLastError(ERROR_INVALID_PARAMETER);
    return hMem;
}


UINT WINAPI LocalSizeInProcess (HLOCAL hMem, HPROCESS hProc)
{
    DWORD dwSize;

    dwSize = RemoteHeapSize(hProc, NULL, 0, (LPVOID)hMem);
    return dwSize==(DWORD)-1 ? 0 : (UINT)dwSize;
}

