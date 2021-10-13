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

#include <windows.h>
#include "toolhelp.h"



BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    UNREFERENCED_PARAMETER (lpvReserved);
    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls((HMODULE) hinstDLL);
    }
    return TRUE; 
} 

//
// call this function to validate if the heap block we get from
// other process is valid
//
BOOL THHeapValidate (PTH32HEAPLIST php32 , DWORD cbTotalSize )
{
    UNREFERENCED_PARAMETER (php32);
    UNREFERENCED_PARAMETER (cbTotalSize);
    return TRUE;
}

DWORD WINAPI GetHeapSnapshot (PTHSNAP pSnap);

HANDLE WINAPI CreateToolhelp32Snapshot (DWORD dwFlags, DWORD th32ProcessID)
{
    PTHSNAP pSnap = THCreateSnapshot (dwFlags,th32ProcessID);

    if (pSnap
        && (TH32CS_SNAPHEAPLIST & dwFlags)
        && (!th32ProcessID || (GetCurrentProcessId () == th32ProcessID))) {
        GetHeapSnapshot (pSnap);
    }

    return pSnap? (HANDLE)pSnap : INVALID_HANDLE_VALUE;
}

BOOL WINAPI CloseToolhelp32Snapshot(HANDLE hSnapshot)
{
    if (INVALID_HANDLE_VALUE != hSnapshot) {
        VirtualFree (hSnapshot, 0, MEM_RELEASE);
    }
    return TRUE;
}

BOOL WINAPI Toolhelp32ReadProcessMemory (
    DWORD th32ProcessID,
    LPCVOID lpBaseAddress,
    LPVOID lpBuffer,
    DWORD cbRead,
    LPDWORD lpNumberOfBytesRead)
{
    BOOL bRet;
    HANDLE hProc;

    if (!th32ProcessID)
        th32ProcessID = GetCurrentProcessId();

    if (lpNumberOfBytesRead)
        *lpNumberOfBytesRead = 0;

    if (NULL == (hProc = OpenProcess(0,0,th32ProcessID))) {
        hProc = (HANDLE)th32ProcessID;
        if (!(th32ProcessID & 0x3)) {
            // passed in handle is not a process handle
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }
    }
    
    bRet = ReadProcessMemory(hProc,lpBaseAddress,lpBuffer,cbRead,lpNumberOfBytesRead);
    if (hProc != (HANDLE)th32ProcessID)
        CloseHandle(hProc);

    return bRet;
}
    
BOOL WINAPI Process32First (HANDLE hSnapshot, LPPROCESSENTRY32 lppe)
{
    PTHSNAP pSnap = (PTHSNAP)hSnapshot;
    if (hSnapshot
        && (INVALID_HANDLE_VALUE != hSnapshot)
        && lppe
        && (lppe->dwSize >= sizeof(PROCESSENTRY32))) {

        if (pSnap->dwProcCnt) {
            pSnap->dwProcIdx = 1;
            *lppe = TH32FIRSTPROC (pSnap)[0];
            return TRUE;
        }
        SetLastError (ERROR_NO_MORE_FILES);
    } else {
        SetLastError (ERROR_INVALID_PARAMETER);
    }
    return FALSE;
}

BOOL WINAPI Process32Next (HANDLE hSnapshot, LPPROCESSENTRY32 lppe)
{
    PTHSNAP pSnap = (PTHSNAP)hSnapshot;

    if (hSnapshot
        && (INVALID_HANDLE_VALUE != hSnapshot)
        && pSnap->dwProcCnt
        && pSnap->dwProcIdx
        && lppe
        && (lppe->dwSize >= sizeof(PROCESSENTRY32))) {

        if (pSnap->dwProcIdx < pSnap->dwProcCnt) {
            *lppe = TH32FIRSTPROC(pSnap)[pSnap->dwProcIdx ++];
            return TRUE;
        }
        SetLastError(ERROR_NO_MORE_FILES);
    } else {
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    return FALSE;
}

BOOL WINAPI Thread32First(HANDLE hSnapshot, LPTHREADENTRY32 lpte)
{
    PTHSNAP pSnap = (PTHSNAP)hSnapshot;
    
    if (hSnapshot
        && (INVALID_HANDLE_VALUE != hSnapshot)
        && lpte
        && (lpte->dwSize >= sizeof(THREADENTRY32))) {
        if (pSnap->dwThrdCnt) {
            *lpte = TH32FIRSTTHRD(pSnap)[0];
            pSnap->dwThrdIdx = 1;
            return TRUE;
        }
        SetLastError(ERROR_NO_MORE_FILES);
    } else {
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    return FALSE;
}

BOOL WINAPI Thread32Next(HANDLE hSnapshot, LPTHREADENTRY32 lpte)
{
    PTHSNAP pSnap = (PTHSNAP)hSnapshot;

    if (hSnapshot
        && (INVALID_HANDLE_VALUE != hSnapshot)
        && pSnap->dwThrdCnt
        && pSnap->dwThrdIdx
        && lpte
        && (lpte->dwSize >= sizeof(THREADENTRY32))) {

        if (pSnap->dwThrdIdx < pSnap->dwThrdCnt) {
            *lpte = TH32FIRSTTHRD(pSnap)[pSnap->dwThrdIdx ++];
            return TRUE;
        }
        SetLastError(ERROR_NO_MORE_FILES);
    } else {
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    return FALSE;
}

BOOL WINAPI Module32First(HANDLE hSnapshot, LPMODULEENTRY32 lpme)
{
    PTHSNAP pSnap = (PTHSNAP)hSnapshot;

    if (hSnapshot
        && (INVALID_HANDLE_VALUE != hSnapshot)
        && lpme
        && (lpme->dwSize >= sizeof(MODULEENTRY32))) {

        if (pSnap->dwModCnt) {
            *lpme = TH32FIRSTMOD(pSnap)[0];
            pSnap->dwModIdx = 1;
            return TRUE;
        }
        SetLastError(ERROR_NO_MORE_FILES);
    } else {
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    return FALSE;
}

BOOL WINAPI Module32Next(HANDLE hSnapshot, LPMODULEENTRY32 lpme)
{
    PTHSNAP pSnap = (PTHSNAP)hSnapshot;

    if (hSnapshot
        && (INVALID_HANDLE_VALUE != hSnapshot)
        && pSnap->dwModCnt
        && pSnap->dwModIdx
        && lpme
        && (lpme->dwSize >= sizeof(MODULEENTRY32))) {

        if (pSnap->dwModIdx < pSnap->dwModCnt) {
            *lpme = TH32FIRSTMOD(pSnap)[pSnap->dwModIdx ++];
            return TRUE;
        }
        SetLastError(ERROR_NO_MORE_FILES);
    } else {
        SetLastError(ERROR_INVALID_PARAMETER);
    }
    return FALSE;
}


//
// Heap snapshots are filled by other processes, which we cannot trust fully. Kernel had validated the size fields
// in the snapshot is within bound. However, the rest of heap data are not guaranteed. Therefore we need to 
// try-except access to the heap entries, and make sure that we don't access data out of bound. The valid bound
// for an entry can be tested by "((DWORD) pEntry - (DWORD) pSnap <= pSnap->cbInuse)"
//

BOOL IsValidHeapList (PTHSNAP pSnap, PTH32HEAPLIST php32)
{
    DWORD dwOfst = (DWORD) php32 - (DWORD) pSnap;
    
    return ((int) dwOfst >= 0)
        && (dwOfst + sizeof (TH32HEAPLIST) <= pSnap->cbInuse)
        && ((int) php32->dwTotalSize >= sizeof (TH32HEAPLIST))
        && (dwOfst + php32->dwTotalSize <= pSnap->cbInuse);
}

BOOL WINAPI Heap32ListFirst (HANDLE hSnapshot, LPHEAPLIST32 lphl)
{
    PTHSNAP pSnap = (PTHSNAP)hSnapshot;
    DWORD   dwErr = 0;
    __try {
        if (hSnapshot
            && (INVALID_HANDLE_VALUE != hSnapshot)
            && lphl
            && (lphl->dwSize >= sizeof(HEAPLIST32))) {

            PTH32HEAPLIST php32 = TH32FIRSTHEAP (pSnap);

            if (pSnap->dwHeapListCnt && IsValidHeapList (pSnap, php32)) {
                *lphl = php32->heaplist;
                pSnap->dwOfstHeapList = (DWORD) php32 - (DWORD) pSnap;
            } else {

                dwErr = ERROR_NO_MORE_FILES;
            }
        } else {
            dwErr = ERROR_INVALID_PARAMETER;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dwErr = ERROR_INVALID_PARAMETER;
    }

    if (dwErr) {
        SetLastError (dwErr);
    }
    return !dwErr;
}

BOOL WINAPI Heap32ListNext(HANDLE hSnapshot, LPHEAPLIST32 lphl)
{
    PTHSNAP pSnap = (PTHSNAP)hSnapshot;
    DWORD   dwErr = 0;
    __try {

        if (hSnapshot
            && (INVALID_HANDLE_VALUE != hSnapshot)
            && pSnap->dwHeapListCnt
            && pSnap->dwOfstHeapList
            && lphl
            && (lphl->dwSize >= sizeof(HEAPLIST32))) {

            PTH32HEAPLIST php32 = (PTH32HEAPLIST) ((DWORD)pSnap + pSnap->dwOfstHeapList);
            php32 = TH32NEXTHEAPLIST (php32);

            if (IsValidHeapList (pSnap, php32)) {
                *lphl = php32->heaplist;
                pSnap->dwOfstHeapList = (DWORD) php32 - (DWORD) pSnap;
            } else {
                dwErr = ERROR_NO_MORE_FILES;
            }
        } else {
            dwErr = ERROR_INVALID_PARAMETER;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dwErr = ERROR_INVALID_PARAMETER;
    }

    if (dwErr) {
        SetLastError (dwErr);
    }
    return !dwErr;
}

__inline BOOL MatchHeap (PHEAPLIST32 php32, DWORD dwProcId, DWORD dwHeapId)
{
    return (dwProcId == php32->th32ProcessID) && (dwHeapId == php32->th32HeapID);
}

static PTH32HEAPLIST FindHeap (PTHSNAP pSnap, DWORD dwProcId, DWORD dwHeapId)
{
    PTH32HEAPLIST php32;
    DWORD dwIdx = 0;

    for (php32 = TH32FIRSTHEAP(pSnap); dwIdx < pSnap->dwHeapListCnt; php32 = TH32NEXTHEAPLIST (php32), dwIdx ++) {
        if ((DWORD) php32 - (DWORD) pSnap > pSnap->cbInuse) {
            // invalid heap list entry
            php32 = NULL;
            break;
        }
        if (MatchHeap (&php32->heaplist, dwProcId, dwHeapId)) {
            break;
        }
    }

    return (dwIdx < pSnap->dwHeapListCnt)? php32 : NULL;
}

BOOL WINAPI Heap32First (HANDLE hSnapshot, LPHEAPENTRY32 lphe, DWORD th32ProcessID, DWORD th32HeapID)
{
    PTHSNAP pSnap = (PTHSNAP)hSnapshot;
    PTH32HEAPLIST php32;
    DWORD   dwErr = 0;
    __try {

        if (hSnapshot
            && (INVALID_HANDLE_VALUE != hSnapshot)
            && lphe
            && (lphe->dwSize >= sizeof(HEAPENTRY32))
            && (NULL != (php32 = FindHeap (pSnap, th32ProcessID, th32HeapID)))) {

            pSnap->dwOfstEntryList = (DWORD) php32 - (DWORD) pSnap;
            if (php32->dwHeapEntryCnt) {
                *lphe = TH32FIRSTHEAPITEM(php32)[0];
                pSnap->dwHeapEntryIdx = 1;
            } else {
                dwErr = ERROR_NO_MORE_FILES;
            }
        } else {
            dwErr = ERROR_INVALID_PARAMETER;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dwErr = ERROR_INVALID_PARAMETER;
    }

    if (dwErr) {
        SetLastError (dwErr);
    }
    return !dwErr;
}

BOOL WINAPI Heap32Next(HANDLE hSnapshot, LPHEAPENTRY32 lphe) {
    PTHSNAP pSnap = (PTHSNAP)hSnapshot;
    DWORD   dwErr = 0;
    __try {

        if (hSnapshot
            && (INVALID_HANDLE_VALUE != hSnapshot)
            && pSnap->dwHeapEntryIdx
            && lphe
            && (lphe->dwSize >= sizeof(HEAPENTRY32))) {

            PTH32HEAPLIST php32 = (PTH32HEAPLIST) ((DWORD)pSnap + pSnap->dwOfstEntryList);
            PHEAPENTRY32  phe32 = TH32FIRSTHEAPITEM(php32) + pSnap->dwHeapEntryIdx;

            // validate the heap item is in-bound
            if (   ((DWORD) phe32 - (DWORD) pSnap < pSnap->cbInuse)
                && (pSnap->dwHeapEntryIdx < php32->dwHeapEntryCnt)) {
                *lphe = *phe32;
                pSnap->dwHeapEntryIdx ++;
            } else {
                dwErr = ERROR_NO_MORE_FILES;
            }
        } else {
            dwErr = ERROR_INVALID_PARAMETER;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        dwErr = ERROR_INVALID_PARAMETER;
    }

    if (dwErr) {
        SetLastError (dwErr);
    }
    return !dwErr;
}

