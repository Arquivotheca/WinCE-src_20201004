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
#include "celog.h"

// Make sure that no macro are redirecting LocalAlloc & HeapAlloc
#undef LocalAlloc
#undef HeapAlloc


//
// SanitizeSize - returns
//          0 - if size requested is invalid ( > HEAP_MAX_ALLOC)
//          1 - if size requested is 0 (to allow HeapAlloc/LocalAlloc of size 0)
//          otherwise returns size requested
//
static DWORD SanitizeSize (DWORD cbSize)
{
    return (cbSize > HEAP_MAX_ALLOC)
                ? 0
                : (cbSize? cbSize : 1);
}


/*
    @doc BOTH EXTERNAL

    @func HLOCAL | LocalAlloc | Allocates a block of memory from the local heap
    @parm UINT | uFlags | flags for alloc operation
    @parm UINT | uBytes | desired size of block

    @comm Only fixed memory is supported, thus LMEM_MOVEABLE is invalid.
*/
HLOCAL WINAPI LocalAlloc(UINT uFlags, UINT uBytes) {
    LPVOID p;
    DWORD  cbSize = SanitizeSize (uBytes);
    if (!cbSize || (uFlags & ~LMEM_VALID_FLAGS)) {
        DEBUGMSG(DBGLMEM, (L"   LocalAlloc %8.8lx %8.8lx ==> %8.8lx (invalid parameter)\r\n",uBytes,uFlags,0));
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    p = RHeapAlloc(g_hProcessHeap, (uFlags & LMEM_ZEROINIT) ? HEAP_ZERO_MEMORY : 0, cbSize);
    DEBUGMSG(DBGLMEM, (L"   LocalAlloc %8.8lx %8.8lx ==> %8.8lx [%8.8lx]\r\n",uBytes,uFlags,p,RHeapSize(g_hProcessHeap,0,p)));
    return (HLOCAL)p;
}

HLOCAL WINAPI LocalAllocTrace(UINT uFlags, UINT uBytes, UINT dwLineNum, LPCWSTR szFilename) {
    return LocalAlloc (uFlags, uBytes);
}


/*
    @doc BOTH EXTERNAL

    @func HLOCAL | LocalRealloc | Realloc's a LocalAlloc'ed block of memory
    @parm HLOCAL | hMem | handle of local memory object to realloc
    @parm UINT | uBytes | desired new size of block
    @parm UINT | uFlags | flags for realloc operation

    @comm Follows the Win32 reference description without restrictions
    or modifications.
*/
HLOCAL WINAPI LocalReAlloc(HLOCAL hMem, UINT uBytes, UINT uFlags) {
    LPVOID retval;
    UINT newflags;
    DWORD  cbSize = SanitizeSize (uBytes);
    if( !cbSize
        || (uFlags & ~(LMEM_VALID_FLAGS | LMEM_MODIFY ))
        || ((uFlags & LMEM_DISCARDABLE) && !(uFlags & LMEM_MODIFY))) {
        DEBUGMSG(DBGLMEM, (L"   LocalRealloc %8.8lx %8.8lx %8.8lx ==> %8.8lx (invalid parameter)\r\n",hMem,uBytes,uFlags,0));
        SetLastError(ERROR_INVALID_PARAMETER);
        return(NULL);
    }
    newflags = ((uFlags & LMEM_ZEROINIT) ? HEAP_ZERO_MEMORY : 0) | ((uFlags & LMEM_MOVEABLE) ? 0 : HEAP_REALLOC_IN_PLACE_ONLY);
    retval = HeapReAlloc(g_hProcessHeap, newflags, hMem, cbSize);
    DEBUGMSG(DBGLMEM, (L"   LocalRealloc %8.8lx %8.8lx %8.8lx ==> %8.8lx [%8.8lx]\r\n",hMem,uBytes,uFlags,retval,RHeapSize(g_hProcessHeap,0,retval)));
    return (HLOCAL)retval;
}

/*
    @doc BOTH EXTERNAL

    @func UINT | LocalSize | Returns the size of a LocalAlloc'ed block of memory
    @parm HLOCAL | hMem | handle of local memory object to get size of

    @comm Follows the Win32 reference description without restrictions
    or modifications.
*/
UINT WINAPI LocalSize(HLOCAL hMem) {
    UINT retval;
    // hMem could be null (pre-6.0 CRT functions called with NULL parameter)
    if (((DWORD)hMem < 0x10000) || ((retval = RHeapSize(g_hProcessHeap,0,hMem)) == (DWORD)-1)) {
        retval = 0;
        SetLastError (ERROR_INVALID_PARAMETER);
    }
    DEBUGMSG(DBGLMEM, (L"   LocalSize %8.8lx ==> %8.8lx\r\n",hMem,retval));
    return retval;
}

/*
    @doc BOTH EXTERNAL

    @func HLOCAL | LocalFree | Frees a LocalAlloc'ed block of memory
    @parm HLOCAL | hMem | handle of local memory object to free

    @comm Follows the Win32 reference description without restrictions
    or modifications.
*/
HLOCAL WINAPI LocalFree(HLOCAL hMem) {
    HLOCAL retval = NULL;
    if (hMem && !RHeapFree(g_hProcessHeap,0,(LPVOID)hMem)) {
        retval = hMem;
        SetLastError(ERROR_INVALID_PARAMETER);
    }    
    DEBUGMSG(DBGLMEM, (L"   LocalFree %8.8lx ==> %8.8lx\r\n",hMem,retval));
    return retval;
}

/*
    @doc BOTH EXTERNAL

    @func HANDLE | HeapCreate    | Creates a new local heap
    @parm DWORD  | flOptions     | no flags supported
    @parm DWORD  | dwInitialSize | Initial committed heap size
    @parm DWORD  | dwMaximumSize | Maximum heap size
*/
HANDLE WINAPI HeapCreate(DWORD flOptions, DWORD dwInitialSize, DWORD dwMaximumSize)
{
    if ((flOptions & ~(HEAP_NO_SERIALIZE|HEAP_SHARED_READONLY|HEAP_CREATE_ENABLE_EXECUTE))
        || (dwMaximumSize > HEAP_MAX_ALLOC)) {
        DEBUGMSG(DBGFIXHP, (L"   HeapCreate: Invalid parameter\r\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    return RHeapCreate (flOptions, dwInitialSize, dwMaximumSize);
}

//
// CeHeapCreate: Create a heap with custom allocator/deallocator
//
HANDLE WINAPI CeHeapCreate(DWORD flOptions,
    DWORD dwInitialSize,
    DWORD dwMaximumSize,
    PFN_AllocHeapMem pfnAlloc,
    PFN_FreeHeapMem pfnFree)
{
    DEBUGMSG(DBGFIXHP, (L"CeHeapCreate %8.8lx %8.8lx %8.8lx %8.8lx %.8lx\r\n", flOptions, dwInitialSize, dwMaximumSize, pfnAlloc, pfnFree));

    if (flOptions & ~(HEAP_SUPPORT_FIX_BLKS|HEAP_CREATE_ENABLE_EXECUTE)) {
        DEBUGMSG(DBGFIXHP, (L"   CeHeapCreate: Invalid parameter\r\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    
    return DoRHeapCreate (flOptions, dwInitialSize, dwMaximumSize, pfnAlloc, pfnFree, GetCurrentProcessId ());
}


/*
    @doc BOTH EXTERNAL

    @func LPVOID | HeapReAlloc | Realloc's a HeapAlloc'ed block of memory
    @parm HANDLE | hHeap     | Handle returned from CreateHeap()
    @parm DWORD  | dwFlags   | HEAP_NO_SERIALIZE, HEAP_REALLOC_IN_PLACE_ONLY and HEAP_ZERO_MEM supported
    @parm LPVOID | lpMem     | pointer to memory to realloc
    @parm DWORD  | dwBytes   | Desired size of block
*/
LPVOID WINAPI HeapReAlloc(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, DWORD dwBytes)
{
    PRHEAP php = (PRHEAP) hHeap;
    DWORD  cbSize = SanitizeSize (dwBytes);
    if ((dwFlags & ~(HEAP_GENERATE_EXCEPTIONS | HEAP_NO_SERIALIZE | HEAP_REALLOC_IN_PLACE_ONLY | HEAP_ZERO_MEMORY))
        || !cbSize
        || !php
        || (php->dwSig != HEAPSIG)) {
        DEBUGMSG(DBGFIXHP, (L"   2:HeapReAlloc %8.8lx %8.8lx %8.8lx %8.8lx ==> %8.8lx (invalid parameter)\r\n", hHeap,dwFlags,lpMem,dwBytes,0));
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    if (dwFlags & HEAP_GENERATE_EXCEPTIONS) {
        DEBUGMSG(DBGFIXHP, (L"   3:HeapReAlloc %8.8lx %8.8lx %8.8lx %8.8lx ==> %8.8lx (not supported)\r\n", hHeap,dwFlags,lpMem,dwBytes,0));
        SetLastError(ERROR_NOT_SUPPORTED);
        return NULL;
    }

    return RHeapReAlloc (hHeap, dwFlags, lpMem, cbSize);
}

/*
    @doc BOTH EXTERNAL

    @func LPVOID | HeapAlloc | Allocates a block of memory from specified heap
    @parm HANDLE | hHeap     | Handle returned from CreateHeap()
    @parm DWORD  | dwFlags   | HEAP_NO_SERIALIZE and HEAP_ZERO_MEM supported
    @parm DWORD  | dwBytes   | Desired size of block
*/
LPVOID WINAPI HeapAlloc(HANDLE hHeap, DWORD dwFlags, DWORD dwBytes)
{
    PRHEAP php = (PRHEAP) hHeap;
    DWORD  cbSize = SanitizeSize (dwBytes);
    if ((dwFlags & ~(HEAP_NO_SERIALIZE | HEAP_ZERO_MEMORY))
        || !cbSize
        || !hHeap
        || (php->dwSig != HEAPSIG)) {
        DEBUGMSG(DBGFIXHP, (L"   HeapAlloc %8.8lx %8.8lx %8.8lx ==> %8.8lx (invalid parameter)\r\n", hHeap,dwFlags,dwBytes,0));
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }

    return RHeapAlloc(hHeap, dwFlags, cbSize);
}

LPVOID WINAPI HeapAllocTrace(HANDLE hHeap, DWORD dwFlags, DWORD dwBytes, DWORD dwLineNum, PCCH szFilename)
{
    return HeapAlloc (hHeap, dwFlags, dwBytes);
}

/*
    @doc BOTH EXTERNAL

    @func BOOL   | HeapFree | Frees a HeapAlloc'ed block of memory
    @parm HANDLE | hHeap    | Handle returned from CreateHeap()
    @parm DWORD  | dwFlags  | HEAP_NO_SERIALIZE only
    @parm LPVOID | lpMem    | Pointer to memory block
*/
BOOL WINAPI HeapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem)
{
    PRHEAP php = (PRHEAP) hHeap;
    if ((dwFlags & ~HEAP_NO_SERIALIZE) || !hHeap || php->dwSig != HEAPSIG || (DWORD)lpMem < 0x10000) {
        DEBUGMSG(DBGFIXHP||lpMem, (L"   HeapFree %8.8lx %8.8lx %8.8lx ==> %8.8lx (invalid parameter)\r\n", hHeap,dwFlags,lpMem,0));

        // debugchk if lpMem is not NULL - mostly freeing something wrong.
        DEBUGCHK (!lpMem);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    return RHeapFree(hHeap, dwFlags, lpMem);
}
/*
    @doc BOTH EXTERNAL

    @func DWORD | HeapSize | Returns the size of a HeapAlloc'ed block of memory
    @parm HANDLE | hHeap | Heap from which memory was alloc'ed
    @parm DWORD | dwFlags | can be HEAP_NO_SERIALIZE
    @parm LPVOID | lpMem | handle of local memory object to get size of

    @comm Follows the Win32 reference description without restrictions
    or modifications.
*/

DWORD WINAPI HeapSize(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem)
{
    PRHEAP php = (PRHEAP) hHeap;
    if ((dwFlags & ~HEAP_NO_SERIALIZE) || !hHeap || php->dwSig != HEAPSIG || (DWORD)lpMem < 0x10000) {
        DEBUGMSG(DBGFIXHP, (L"   HeapSize %8.8lx: Invalid parameter\r\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return (DWORD)-1;
    }

    return RHeapSize (hHeap, dwFlags, lpMem);
}

/*
    @doc BOTH EXTERNAL

    @func BOOL   | HeapDestroy | Destroys a heap and releases its memory
    @parm HANDLE | hHeap       | Handle returned from CreateHeap()
*/
BOOL WINAPI HeapDestroy(HANDLE hHeap)
{
    PRHEAP php = (PRHEAP) hHeap;
    if (!hHeap || php->dwSig != HEAPSIG || (php->flOptions & HEAP_IS_PROC_HEAP)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        DEBUGMSG(DBGFIXHP, (L"   HeapDestroy %8.8lx ==> %8.8lx (invalid parameter)\r\n", hHeap, 0));
        return FALSE;
    }
    return RHeapDestroy(hHeap);
}


UINT WINAPI HeapCompact(HANDLE hHeap, DWORD dwFlags)
{
    if (dwFlags & ~HEAP_NO_SERIALIZE) {
        DEBUGMSG(DBGFIXHP, (L"   HeapCompact %8.8lx %8.8lx ==> %8.8lx (invalid parameter)\r\n", hHeap, dwFlags, 0));
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    if (!hHeap) {
        SetLastError(ERROR_INVALID_PARAMETER);
        DEBUGMSG(DBGFIXHP, (L"   HeapCompact %8.8lx %8.8lx ==> %8.8lx (invalid parameter)\r\n", hHeap, dwFlags, 0));
        return 0;
    }

    return RHeapCompact ((PRHEAP) hHeap, dwFlags);
}


/*
    @doc BOTH EXTERNAL

    @func HANDLE   | GetProcessHeap | Obtains a handle to the heap of the
    calling process
*/
HANDLE WINAPI GetProcessHeap(VOID)
{
    return g_hProcessHeap;
}


BOOL WINAPI HeapValidate(HANDLE hHeap, DWORD dwFlags, LPCVOID lpMem)
{
    PRHEAP php = (PRHEAP) hHeap;

    if ((dwFlags & ~HEAP_NO_SERIALIZE) || !hHeap || php->dwSig != HEAPSIG
            || (lpMem && lpMem < (LPCVOID)0x10000)) {
        DEBUGMSG(DBGFIXHP, (L"   HeapValidate %8.8lx: Invalid parameter\r\n", hHeap));
        return FALSE;
    }

    return RHeapValidate (php, dwFlags, lpMem);
}

