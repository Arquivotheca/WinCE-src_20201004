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
#include <coredll.h>
#include "heap.h"

//
// ALL RemoteHeapXXX APIs and LocalXXXInProc APIs are NOT SUPPORTED
//


// Make sure that no macro are redirecting LocalAlloc & HeapAlloc
#undef LocalAlloc
#undef HeapAlloc



HANDLE RemoteGetProcessHeap(HANDLE hProc)
{
    RETAILMSG (1, (L"!!!NOT SUPPORTED: Calling RemoteGetProcessHeap!!!!!\r\n"));
    DEBUGCHK(!DBGDEPRE);
    return NULL;
}

LPVOID RemoteCheckPtr(HANDLE hHeap, LPVOID lpMem)
{
    RETAILMSG (1, (L"!!!NOT SUPPORTED: Calling RemoteCheckPtr!!!!!\r\n"));
    DEBUGCHK(!DBGDEPRE);
    return NULL;
}


LPVOID RemoteHeapAlloc(HANDLE hProc, HANDLE hHeap, DWORD dwFlags, DWORD dwBytes)
{
    RETAILMSG (1, (L"!!!NOT SUPPORTED: Calling RemoteHeapAlloc!!!!!\r\n"));
    DEBUGCHK(!DBGDEPRE);
    return NULL;
}


LPVOID RemoteHeapReAlloc(HANDLE hProc, HANDLE hHeap, DWORD dwFlags, DWORD dwBytes, LPVOID lpMem)
{
    RETAILMSG (1, (L"!!!NOT SUPPORTED: Calling RemoteHeapReAlloc!!!!!\r\n"));
    DEBUGCHK(!DBGDEPRE);
    return NULL;
}


BOOL RemoteHeapFree(HANDLE hProc, HANDLE hHeap, DWORD dwFlags, LPVOID lpMem)
{
    RETAILMSG (1, (L"!!!NOT SUPPORTED: Calling RemoteHeapFree!!!!!\r\n"));
    DEBUGCHK(!DBGDEPRE);
    return FALSE;
}


DWORD RemoteHeapSize(HANDLE hProc, HANDLE hHeap, DWORD dwFlags, LPVOID lpMem)
{
    RETAILMSG (1, (L"!!!NOT SUPPORTED: Calling RemoteHeapSize!!!!!\r\n"));
    DEBUGCHK(!DBGDEPRE);
    return (DWORD)-1;
}



HLOCAL WINAPI LocalAllocInProcess (UINT uFlags, UINT uBytes, HPROCESS hProc)
{
    RETAILMSG (1, (L"!!!NOT SUPPORTED: Calling LocalAllocInProcess!!!!!\r\n"));
    DEBUGCHK(!DBGDEPRE);
    return NULL;
}


HLOCAL WINAPI LocalFreeInProcess (HLOCAL hMem, HPROCESS hProc)
{
    RETAILMSG (1, (L"!!!NOT SUPPORTED: Calling LocalFreeInProcess!!!!!\r\n"));
    DEBUGCHK(!DBGDEPRE);
    return hMem;
}


UINT WINAPI LocalSizeInProcess (HLOCAL hMem, HPROCESS hProc)
{
    RETAILMSG (1, (L"!!!NOT SUPPORTED: Calling LocalSizeInProcess!!!!!\r\n"));
    DEBUGCHK(!DBGDEPRE);
    return 0;
}

