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
// dllheap.cpp - Implementation for per-dll-heap construct
//
// This file compiles into an .obj file and any dll which wants to use custom heap
// can link with this .obj file. Dll code doesn't have to change. Dll code can still
// continue to use LocalAlloc and rest of the heap functions and at run-time,
// those calls will be routed to this file which has the stubs to use the custom
// dll heap in the heap functions.
//
#include <windows.h>
#include <pkfuncs.h>
#include <intsafe.h>
#include "dllheap.h"

void DllHeapInit(void);
void DllHeapDeInit(void);

// following is exported from coredll
extern "C" LPVOID CeGetModuleHeapInfo();

//
// The following three lines will create a new section
// which will be added to the chain of sections within
// crt. Also the code generates a global function entry
// point which gets automatically called by _cinit()
// when dllmain for any dll linked with this .obj file
// is called (before dllmain with PROCESS_ATTACH).
//
#pragma section(".CRT$XIAA",read)
__declspec(allocate(".CRT$XIAA"))
PFNVOID _g_pfnDllHeapInit = &DllHeapInit;

//
// The following three lines will create a new section
// which will be added to the chain of sections within
// crt. Also the code generates a global function entry
// point which gets automatically called by exit/_exit
// when dllmain for any dll linked with this .obj file
// is called (after dllmain with PROCESS_DETACH).
//
#pragma section(".CRT$XTY",read)
__declspec(allocate(".CRT$XTY"))
PFNVOID _g_pfnDllHeapDeInit = &DllHeapDeInit;

//
// dll specific heap and process heap
//
HANDLE g_hDllHeap;
HANDLE g_hLocalProcessHeap;
HANDLE* g_pphpListAll;
LPCRITICAL_SECTION g_lpcsHeapList;
DWORD g_dwNextHeapOffset;


//
// GetProcessHeap -- get the process heap
//
HANDLE WINAPI GetProcessHeap_stub(VOID)
{
    return (g_hDllHeap) ? g_hDllHeap : g_hLocalProcessHeap;
}

//
// LocalAlloc -- allocate bytes in the dll heap
//
HLOCAL WINAPI LocalAlloc_stub(UINT uFlags, UINT uBytes)
{
    return (HLOCAL) HeapAlloc(GetProcessHeap (), (uFlags & LMEM_ZEROINIT) ? HEAP_ZERO_MEMORY : 0, uBytes);
}

//
// LocalFree -- free bytes in the dll heap
//
HLOCAL WINAPI LocalFree_stub(HLOCAL hMem)
{
    HLOCAL hRet = NULL;
    int index = 0;
    BOOL bVal = FALSE;
    
    hRet = (!hMem 
            || HeapFree(GetProcessHeap (), 0, (LPVOID) hMem)) ? NULL : hMem;

    if (hRet) {

        // did not find the heap entry in the dll heap; try all other heaps in this process.
        // normally shouldn't happen but possible where dll 1 allocates something in
        // its own heap and hands over the object to dll 2 which then tries to call heap
        // functions on that object.
        
        // try to get the global heap critical section; if this fails, don't bother
        // with the failure to find the heap entry.
        while (index++ < 5) {
            bVal = TryEnterCriticalSection (g_lpcsHeapList);
            if (bVal)
                break;
            Sleep(10);
        }

        if (bVal) {                
            // if it fails to find the heap entry in the given heap try to see 
            // if the memory is in any of the local heaps only
            bVal = FALSE;
            for (HANDLE php = *g_pphpListAll; php && !bVal; php = (HANDLE)*((DWORD*)((LPBYTE*)php + g_dwNextHeapOffset))) {
                if (php == GetProcessHeap())
                    continue;            
                bVal = HeapFree(php, 0, hRet);
            }           

            LeaveCriticalSection (g_lpcsHeapList);     
        }
        
    }
    return hRet;
}

//
// LocalAllocTrace - ignore the extra trace info.
//
HLOCAL WINAPI LocalAllocTrace_stub(UINT uFlags, UINT uBytes, ULONG ulLineNumber, PCHAR aszFileName)
{
    // Ignore the extra trace info. Just call the hook for LocalAlloc.
    return (HLOCAL) HeapAlloc(GetProcessHeap (), (uFlags & LMEM_ZEROINIT) ? HEAP_ZERO_MEMORY : 0, uBytes);
}

//
// LocalReAlloc -- re-allocate within dll heap
//
HLOCAL WINAPI LocalReAlloc_stub(HLOCAL hMem, UINT uBytes, UINT uFlags)
{
    LPVOID retval;
    UINT newflags;

    if ((uFlags & ~(LMEM_VALID_FLAGS | LMEM_MODIFY )) ||
        ((uFlags & LMEM_DISCARDABLE) && !(uFlags & LMEM_MODIFY))) {
        return NULL;
    }
    newflags = ((uFlags & LMEM_ZEROINIT) ? HEAP_ZERO_MEMORY : 0) | ((uFlags & LMEM_MOVEABLE) ? 0 : HEAP_REALLOC_IN_PLACE_ONLY);
    retval = HeapReAlloc(GetProcessHeap (), newflags, hMem, uBytes);

    return (HLOCAL) retval;
}

//
// LocalSize -- size of the dll heap region
//
UINT WINAPI LocalSize_stub(HLOCAL hMem)
{
    UINT retval;
    int index = 0;
    BOOL bVal = FALSE;
    if ((retval = HeapSize(GetProcessHeap (), 0, hMem)) == (DWORD)-1) {

        // did not find the heap entry in the dll heap; try all other heaps in this process.
        // normally shouldn't happen but possible where dll 1 allocates something in
        // its own heap and hands over the object to dll 2 which then tries to call heap
        // functions on that object.

        // try to get the global heap critical section; if this fails, don't bother
        // with the failure to find the heap entry.
        while (index++ < 5) {
            bVal = TryEnterCriticalSection (g_lpcsHeapList);
            if (bVal)
                break;
            Sleep(10);
        }

        if (bVal) {                
            // if it fails to find the heap entry in the given heap try to see 
            // if the memory is in any of the local heaps only
            for (HANDLE php = *g_pphpListAll; php && (retval != -1) ; php = (HANDLE)*((DWORD*)((LPBYTE*)php + g_dwNextHeapOffset))) {
                if (php == GetProcessHeap())
                    continue;
                retval = HeapSize(php, 0, hMem);
            }           

            LeaveCriticalSection (g_lpcsHeapList);            
        }
    }

    return retval;
}

//
// string function overrides
//
char *strdup_stub (const char * string)
{
    DWORD cchString;
    char *newstring;

    if (!string)
        return NULL;

    cchString = strlen (string);

    newstring = (char*) LocalAlloc_stub (LMEM_ZEROINIT, cchString + 1);
    if (newstring) {
        memcpy (newstring, string, cchString);
        newstring[cchString] = 0;
    }

    return newstring;
}
wchar_t *wcsdup_stub (const wchar_t * string)
{
    DWORD cchString;
    wchar_t *newstring;

    if (!string)
        return NULL;

    cchString = wcslen (string);

    newstring = (wchar_t*) LocalAlloc_stub (LMEM_ZEROINIT, (cchString + 1) * sizeof (WCHAR));
    if (newstring) {
        memcpy (newstring, string, cchString*sizeof(WCHAR));
        newstring[cchString] = 0;
    }

    return newstring;
}

//
// crt function overrides
// Note: These function implementations are not quite upto date
// with how crt implements it; especially in low memory conditions
// where crt supports a callback function where caller can free up
// some heap memory and re-call this function.
//
void *malloc_stub(size_t size)
{
    return LocalAlloc_stub (0, size);
}

void free_stub(void *ptr)
{
    LocalFree_stub (ptr);
}

void *new_stub (UINT size)
{
    return LocalAlloc_stub (0, size);
}

void *new2_stub (UINT size)
{
    return LocalAlloc_stub (0, size);
}

void delete_stub (void* p)
{
    LocalFree_stub (p);
}

void delete2_stub (void* p)
{
    LocalFree_stub (p);
}

void *calloc_stub(size_t num, size_t size)
{
    UINT result;
    return SUCCEEDED (SizeTMult (num, size, & result)) ? LocalAlloc_stub (LMEM_ZEROINIT, result) : NULL;
}

void *realloc_stub (void *OldPtr, size_t NewSize)
{
    if (!OldPtr)
        return LocalAlloc_stub (0, NewSize);

    if (!NewSize) {
        LocalFree_stub (OldPtr);
        return 0;
    }

    return LocalReAlloc_stub (OldPtr, NewSize, LMEM_MOVEABLE);   
}

UINT msize_stub(void *pBlock) 
{
    return LocalSize_stub(pBlock);
}

void *recalloc_stub(void *OldPtr, size_t NewCount, size_t NewUnit)
{
    size_t NewSize;

    if (!OldPtr) {
        return calloc_stub(NewCount, NewUnit);
    }

    // check for overflow
    if (FAILED(SizeTMult (NewUnit, NewCount, &NewSize))) {
        return NULL;
    }

    if (!NewSize) {
        LocalFree(OldPtr);
        return NULL;
    }

    return LocalReAlloc_stub(OldPtr, NewSize, LMEM_MOVEABLE | LMEM_ZEROINIT);
}


//
// DllHeapDeInit -- This gets called when the dll is being unloaded.
// This should be the last function crt calls after DllMain for the dll
// linking with this .obj file is called.
//
void DllHeapDeInit(void)
{
    if (g_hDllHeap)
        HeapDestroy(g_hDllHeap);
}


//
// DllHeapInit -- This gets called by _cinit() when the dll is loaded
// This should be the first function crt calls before DllMain for the dll
// linking with this .obj file is called.
//
/*
HANDLE g_hDllHeap;
HANDLE g_hLocalProcessHeap;
HANDLE g_hPhpListAll;
LPCRITICAL_SECTION g_hlpcsHeap;
*/
void DllHeapInit(void)
{
    PMODULEHEAPINFO pInfo = (PMODULEHEAPINFO) CeGetModuleHeapInfo();
    
    // get the dll heap entry structure from coredll
    if (pInfo) {
        pInfo->hModuleHeap = NULL;

        // sanity check - these are supposed to be filled in by coredll 
        // before calling into the dllmain routine
        if ((NULL == pInfo->hProcessHeap)
            || (NULL == pInfo->pphpListAll)
            || (NULL == *pInfo->pphpListAll)
            || !pInfo->lpcsHeapList) {
            // module heap info structure is not properly setup
            // don't use a custom heap for this module.
            return;
        }

        // create our custom heap
        if (g_hDllHeap || (NULL != (g_hDllHeap = HeapCreate(0,0,0)))) {

            // set-up the function pointers
            g_hLocalProcessHeap        = pInfo->hProcessHeap;
            g_pphpListAll              = pInfo->pphpListAll;
            g_lpcsHeapList             = pInfo->lpcsHeapList;
            g_dwNextHeapOffset         = pInfo->dwNextHeapOffset;
            pInfo->hModuleHeap         = g_hDllHeap;
            pInfo->pfnGetProcessHeap   = (PFNVOID) &GetProcessHeap_stub;
            pInfo->pfnLocalAlloc       = (PFNVOID) &LocalAlloc_stub;
            pInfo->pfnLocalFree        = (PFNVOID) &LocalFree_stub;
            pInfo->pfnLocalAllocTrace  = (PFNVOID) &LocalAllocTrace_stub;
            pInfo->pfnLocalReAlloc     = (PFNVOID) &LocalReAlloc_stub;
            pInfo->pfnLocalSize        = (PFNVOID) &LocalSize_stub;                                       
            pInfo->pfnCalloc           = (PFNVOID) &calloc_stub;
            pInfo->pfnDelete           = (PFNVOID) &delete_stub;
            pInfo->pfnDelete2          = (PFNVOID) &delete2_stub;
            pInfo->pfnFree             = (PFNVOID) &free_stub;
            pInfo->pfnMalloc           = (PFNVOID) &malloc_stub;
            pInfo->pfnNew              = (PFNVOID) &new_stub;
            pInfo->pfnNew2             = (PFNVOID) &new2_stub;
            pInfo->pfnRealloc          = (PFNVOID) &realloc_stub;
            pInfo->pfnmsize           = (PFNVOID) &msize_stub;
            pInfo->pfnRecalloc       = (PFNVOID) &recalloc_stub;
            pInfo->pfnStrDup           = (PFNVOID) &strdup_stub;
            pInfo->pfnWcsDup           = (PFNVOID) &wcsdup_stub;
        }
    }
}

