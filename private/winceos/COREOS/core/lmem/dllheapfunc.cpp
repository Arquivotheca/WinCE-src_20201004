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
#include <loader.h>
#include <coredll.h>
#include "loader_core.h"
#include "..\lmem\heap.h"
#include "dllheap.h"
#include <intsafe.h>

extern BOOL g_bIsAppVerifierActive;

#if HEAP_SENTINELS

extern "C" BOOL WINAPI RHeapFreeWithCaller (PRHEAP php, DWORD dwFlags, LPVOID ptr, DWORD dwCaller);

// compiler intrinsic to retrieve immediate caller
EXTERN_C void * _ReturnAddress(void);
#pragma intrinsic(_ReturnAddress)

//
// Helper function to set the allocpc/freepc sentinel value
//
void SetAllocSentinel (LPVOID lpMem, LPVOID lpvCallerAddress)
{
#ifdef x86
    // caller stack frames are set in the heap code
#else
    if (lpMem) {
        PRHEAP_SENTINEL_HEADER pHead = (PRHEAP_SENTINEL_HEADER) ((LPBYTE)lpMem - HEAP_SENTINELS);
        pHead->dwFrames[AllocPcIdx] = (DWORD)lpvCallerAddress;        
    }
#endif // x86
}

//
// Helper function to call local process alloc fn
//
HLOCAL DoAlloc (UINT uFlags, UINT uBytes, LPVOID lpvCallerAddress)
{
    HLOCAL hMem = LocalAlloc (uFlags, uBytes);
    SetAllocSentinel (hMem, lpvCallerAddress);
    return hMem;
}

//
// Helper function to call local process realloc fn
//
HLOCAL DoReAlloc (HLOCAL hMem, UINT uBytes, UINT uFlags, LPVOID lpvCallerAddress)
{
    HLOCAL hRet = LocalReAlloc (hMem, uBytes, uFlags);
    SetAllocSentinel (hRet, lpvCallerAddress);
    return hRet;
}

//
// Helper function to call local process free fn
//
static HLOCAL DoFree (HLOCAL hMem, LPVOID lpvCallerAddress)
{
    return (!hMem || RHeapFreeWithCaller ((PRHEAP)g_hProcessHeap, 0, (LPVOID)hMem, (DWORD)lpvCallerAddress)) ? NULL : hMem;
}

//
// LocalAlloc -- allocate bytes in the dll heap
//
HLOCAL WINAPI LocalAlloc_tag(UINT uFlags, UINT uBytes)
{
    return DoAlloc (uFlags, uBytes, _ReturnAddress());
}

//
// LocalFree -- free bytes in the dll heap
//
HLOCAL WINAPI LocalFree_tag(HLOCAL hMem)
{
    return DoFree (hMem, _ReturnAddress());
}

//
// LocalAllocTrace - ignore the extra trace info.
//
HLOCAL WINAPI LocalAllocTrace_tag(UINT uFlags, UINT uBytes, ULONG ulLineNumber, LPCWSTR aszFileName)
{
    return DoAlloc (uFlags, uBytes, _ReturnAddress());
}

//
// LocalReAlloc -- re-allocate within dll heap
//
HLOCAL WINAPI LocalReAlloc_tag(HLOCAL hMem, UINT uBytes, UINT uFlags)
{
    return DoReAlloc (hMem, uBytes, uFlags, _ReturnAddress());
}

//
// LocalSize -- size of the dll heap item
//
UINT WINAPI LocalSize_tag(HLOCAL hMem)
{
    return LocalSize (hMem);
}

//
// Alloc in specific heap
//
LPVOID WINAPI HeapAlloc_tag (HANDLE hHeap, DWORD dwFlags, DWORD dwBytes)
{
    LPVOID hMem = HeapAlloc (hHeap, dwFlags, dwBytes);
    SetAllocSentinel (hMem, _ReturnAddress());    
    return hMem;
}

//
// Realloc in specific heap
//
LPVOID WINAPI HeapReAlloc_tag (HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, DWORD dwBytes)
{
    LPVOID hMem = HeapReAlloc (hHeap, dwFlags, lpMem, dwBytes);
    SetAllocSentinel (hMem, _ReturnAddress());    
    return hMem;
}

//
// Alloc in specific heap
//
LPVOID WINAPI HeapAllocTrace_tag (HANDLE hHeap, DWORD dwFlags, DWORD dwBytes, DWORD dwLineNum, CONST PCHAR szFilename)
{
    LPVOID hMem = HeapAllocTrace (hHeap, dwFlags, dwBytes, dwLineNum, szFilename);
    SetAllocSentinel (hMem, _ReturnAddress());    
    return hMem;
}

//
// Free in specific heap
//
BOOL WINAPI HeapFree_tag (HANDLE hHeap, DWORD dwFlags, LPVOID lpMem)
{
    PRHEAP php = (PRHEAP) hHeap;
    if ((dwFlags & ~HEAP_NO_SERIALIZE) || !hHeap || php->dwSig != HEAPSIG || (DWORD)lpMem < 0x10000) {
        DEBUGMSG(DBGFIXHP||lpMem, (L"   HeapFree %8.8lx %8.8lx %8.8lx ==> %8.8lx (invalid parameter)\r\n", hHeap,dwFlags,lpMem,0));

        // debugchk if lpMem is not NULL - mostly freeing something wrong.
        DEBUGCHK (!lpMem);
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    return RHeapFreeWithCaller (php, dwFlags, lpMem, (DWORD)_ReturnAddress());
}

//
// string function overrides
//
char *strdup_tag (const char * string)
{
    DWORD cchString;
    char *newstring;
    
    if (!string)
        return NULL;

    cchString = strlen (string);

    newstring = (char*) DoAlloc (LMEM_ZEROINIT, cchString + 1, _ReturnAddress());
    if (newstring) {
        memcpy (newstring, string, cchString);
        newstring[cchString] = 0;
    }

    return newstring;
}

wchar_t *wcsdup_tag (const wchar_t * string)
{
    DWORD cchString;
    wchar_t *newstring;
    
    if (!string)
        return NULL;

    cchString = wcslen (string);

    newstring = (wchar_t*) DoAlloc (LMEM_ZEROINIT, (cchString + 1) * sizeof (WCHAR), _ReturnAddress());
    if (newstring) {
        memcpy (newstring, string, cchString * sizeof(WCHAR));
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
void *malloc_tag(size_t size)
{    
    return DoAlloc (0, size, _ReturnAddress());
}

void free_tag(void *ptr)
{
    DoFree (ptr, _ReturnAddress());
}

void *new_tag (UINT size)
{    
    return DoAlloc (0, size, _ReturnAddress());
}

void *new2_tag (UINT size)
{    
    return DoAlloc (0, size, _ReturnAddress());
}

void delete_tag (void* p)
{
    DoFree (p, _ReturnAddress());
}

void delete2_tag (void* p)
{
    DoFree (p, _ReturnAddress());
}

void *calloc_tag(size_t num, size_t size)
{
    UINT result;
    
    return SUCCEEDED (UIntMult (num, size, & result)) ? DoAlloc (LMEM_ZEROINIT, result, _ReturnAddress()) : NULL;
}

void *realloc_tag (void *OldPtr, size_t NewSize)
{
    LPVOID lpvCallerAddress = _ReturnAddress();

    if (!OldPtr)
        return DoAlloc (0, NewSize, lpvCallerAddress);

    if (!NewSize) {
        DoFree (OldPtr, lpvCallerAddress);
        return 0;
    }

    return DoReAlloc (OldPtr, NewSize, LMEM_MOVEABLE, lpvCallerAddress);   
}

UINT msize_tag(void *pBlock) 
{
    return LocalSize_tag(pBlock);
}

void *recalloc_tag(void *OldPtr, size_t NewCount, size_t NewUnit)
{
    LPVOID lpvCallerAddress = _ReturnAddress();
    size_t NewSize;

    if (!OldPtr) {
        UINT result;
        return SUCCEEDED (UIntMult (NewCount, NewUnit, & result)) ? DoAlloc (LMEM_ZEROINIT, result, lpvCallerAddress) : NULL;        
    }

    // check for overflow
    if (FAILED(UIntMult (NewUnit, NewCount, &NewSize))) {
        return NULL;
    }

    if (!NewSize) {
        DoFree (OldPtr, lpvCallerAddress);
        return NULL;
    }

    return DoReAlloc (OldPtr, NewSize, LMEM_MOVEABLE | LMEM_ZEROINIT, lpvCallerAddress);
}

#endif // HEAP_SENTINELS

//
// defined in rheap.cpp in coredll
//
extern MODULEHEAPINFO g_ModuleHeapInfo;

extern const o32_lite *FindRWOptrByRva (const o32_lite *optr, int nCnt, DWORD dwRva);

//------------------------------------------------------------------------------
// SetHeapId - useful for dll heap. "dwProcessId" field is assumed to be set to 
// the module base address passed in from loader.
//------------------------------------------------------------------------------
BOOL SetHeapId(HANDLE hHeap, DWORD dwId)
{
    PRHEAP php = (PRHEAP) hHeap;
    if (!hHeap || php->dwSig != HEAPSIG) {
        return FALSE;
    }
    
    php->dwProcessId = dwId;
    return TRUE;
}


//------------------------------------------------------------------------------
// DllHeapImportOneBlock: helper function to override the heap related functions
// to point to the dll heap implementation.
//------------------------------------------------------------------------------
static
DWORD
DllHeapImportOneBlock (
    LPDWORD         ltptr,        // starting ltptr
    LPDWORD         atptr,        // starting atptr
    PMODULEHEAPINFO pModuleHeapInfo
    )
{
    for (; *ltptr; ltptr ++, atptr ++) {

        if (*ltptr & 0x80000000) {
            DWORD ord = *ltptr & 0x7fffffff;

           switch(ord) {
                // local heap functions                
                case 33:
                    *atptr = (DWORD)pModuleHeapInfo->pfnLocalAlloc;
                break;
                case 34:
                    *atptr = (DWORD)pModuleHeapInfo->pfnLocalReAlloc;
                break;
                case 35:
                    *atptr = (DWORD)pModuleHeapInfo->pfnLocalSize;
                break;
                case 36:
                    *atptr = (DWORD)pModuleHeapInfo->pfnLocalFree;
                break;
                case 50:
                    *atptr = (DWORD)pModuleHeapInfo->pfnGetProcessHeap;
                break;
                case 2602:
                    *atptr = (DWORD)pModuleHeapInfo->pfnLocalAllocTrace;
                break;

                // custom heap functions
                case 46:
                    *atptr = (DWORD)pModuleHeapInfo->pfnHeapAlloc;
                    break;
                case 47:
                    *atptr = (DWORD)pModuleHeapInfo->pfnHeapReAlloc;
                    break;
                case 49:
                    *atptr = (DWORD)pModuleHeapInfo->pfnHeapFree;
                    break;
                case 1999:
                    *atptr = (DWORD)pModuleHeapInfo->pfnHeapAllocTrace;
                    break;
                    
                // crt function overrides
                case 1018:
                    *atptr = (DWORD)pModuleHeapInfo->pfnFree;
                break;
                case 1041:
                    *atptr = (DWORD)pModuleHeapInfo->pfnMalloc;
                break;
                case 1049:
                    *atptr = (DWORD)pModuleHeapInfo->pfnmsize;
                break;
                case 1054:
                    *atptr = (DWORD)pModuleHeapInfo->pfnRealloc;
                break;
                case 1094:
                    *atptr = (DWORD)pModuleHeapInfo->pfnDelete;
                break;
                case 1095:
                    *atptr = (DWORD)pModuleHeapInfo->pfnNew;
                break;
                case 1346:
                    *atptr = (DWORD)pModuleHeapInfo->pfnCalloc;
                break;
                case 1456:
                    *atptr = (DWORD)pModuleHeapInfo->pfnNew2;
                break;
                case 1457:
                    *atptr = (DWORD)pModuleHeapInfo->pfnDelete2;
                break;
                case 2656:
                    *atptr = (DWORD)pModuleHeapInfo->pfnRecalloc;
                break;

                // string function overrides
                case 74:
                    *atptr = (DWORD)pModuleHeapInfo->pfnWcsDup;
                break;
                case 1409:
                    *atptr = (DWORD)pModuleHeapInfo->pfnStrDup;
                break;
                    
                default:
                break;
            }
        }
    }

    return TRUE;
}

//
// Helper function to scan the import table
//
static void
DllHeapScanImports (
    PMODULE pKMod,
    DWORD dwBaseAddr,
    PCInfo pImpInfo,
    PMODULEHEAPINFO pModuleHeapInfo
)
{
    //
    // dll heap is enabled and the module heap is valid
    // re-wire all the heap functions from the import
    // table of this dll to the ones defined in dll heap
    // entry structure
    //
    if (pImpInfo->size && pModuleHeapInfo->pfnGetProcessHeap) {

        LPDWORD     ltptr, atptr;
        PCImpHdr    blockstart = (PCImpHdr) (dwBaseAddr+pImpInfo->rva);
        PCImpHdr    blockptr;
        DWORD       dwOfstImportTable = 0;

        if (pKMod) {
            // pKMod should only be non-null for k.coredll.dll
            DEBUGCHK (IsKModeAddr ((DWORD) DllHeapScanImports));
            
            // 'Z' flag modules has code/data split. Calculate the offset to get to import table
            const o32_lite *optr = FindRWOptrByRva (pKMod->o32_ptr, pKMod->e32.e32_objcnt, blockstart->imp_address);

            DEBUGCHK (optr);
            dwOfstImportTable = optr->o32_realaddr - (dwBaseAddr + optr->o32_rva);
        }

        for (blockptr = blockstart; blockptr->imp_lookup; blockptr++) {
            const char* impdllname = (LPCSTR)(dwBaseAddr+blockptr->imp_dllname);
            if (_stricmp(impdllname, "coredll.dll") && _stricmp(impdllname, "coredll")
                && _stricmp(impdllname, "k.coredll.dll") && _stricmp(impdllname, "k.coredll")) {
                // if this is not coredll block, skip
                continue;
            }

            ltptr = (LPDWORD) (dwBaseAddr + blockptr->imp_lookup);
            atptr = (LPDWORD) (dwBaseAddr + blockptr->imp_address + dwOfstImportTable);

            DllHeapImportOneBlock (ltptr, atptr, pModuleHeapInfo);
            pModuleHeapInfo->nDllHeapModules++;
            break;
        }
    }
}

//------------------------------------------------------------------------------
// DllHeapDoImports: called by loader to override any heap functions.
//------------------------------------------------------------------------------
BOOL
DllHeapDoImports (
    PMODULE pKMod,
    DWORD  dwBaseAddr,
    PCInfo pImpInfo
    )
{    
    // override heap functions only if app verifier is not loaded in this process    
    if (!g_bIsAppVerifierActive) {
        
#if HEAP_SENTINELS

        // enable heap item tagging with caller address

        static MODULEHEAPINFO s_ModuleHeapInfo = {
            HEAP_SENTINELS,
            NULL,
            NULL,
            NULL,
            0,
            0,
            NULL,
            (PFNVOID) GetProcessHeap,
            (PFNVOID) LocalAlloc_tag,
            (PFNVOID) LocalFree_tag,
            (PFNVOID) LocalAllocTrace_tag,
            (PFNVOID) LocalReAlloc_tag,
            (PFNVOID) LocalSize,
            (PFNVOID) HeapReAlloc_tag,
            (PFNVOID) HeapAlloc_tag,
            (PFNVOID) HeapAllocTrace_tag,
            (PFNVOID) HeapFree_tag,
            (PFNVOID) strdup_tag,
            (PFNVOID) wcsdup_tag,
            (PFNVOID) malloc_tag,
            (PFNVOID) free_tag,
            (PFNVOID) new_tag,
            (PFNVOID) new2_tag,
            (PFNVOID) delete_tag,
            (PFNVOID) delete2_tag,
            (PFNVOID) calloc_tag,
            (PFNVOID) realloc_tag,
            (PFNVOID) LocalSize,
            (PFNVOID) recalloc_tag
        };

        // override heap functions to allow tagging heap items with return address               
        DllHeapScanImports (pKMod, dwBaseAddr, pImpInfo, &s_ModuleHeapInfo);

#endif 

        // override heap functions if this dll is built with dllheap flag
        if (SetHeapId(g_ModuleHeapInfo.hModuleHeap, dwBaseAddr)) {
            DllHeapScanImports (pKMod, dwBaseAddr, pImpInfo, &g_ModuleHeapInfo);
        }
    }

    // reset dll heap pointer; this will be populated again when the next 
    // module which is using dll heap is loaded and dllmain is called.
    g_ModuleHeapInfo.hModuleHeap = NULL;
    
    return TRUE;
}

