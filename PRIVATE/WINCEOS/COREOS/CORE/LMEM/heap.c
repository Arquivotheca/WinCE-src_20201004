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
#include <celogcoredll.h>
#include "lmemdebug.h"

// Make sure that no macro are redirecting LocalAlloc & HeapAlloc
#undef LocalAlloc
#undef HeapAlloc

#ifndef FREE_POINTER_AT_LAST_FREE
#define FREE_POINTER_AT_LAST_FREE
#endif

#ifdef MEMTRACKING
DWORD dwHeapMemType;
#endif

#define PAGE_SIZE (UserKInfo[KINX_PAGESIZE])
#define HEAP_MAX_ALLOC 0x40000000   // Maximum allocation request is 1GB.

DWORD PageMask;
LPVOID PSlot;       // Address of the current process's "memory slot"
HANDLE hProcessHeap;
pheap phpListAll;
CRITICAL_SECTION csHeapList;

#ifdef HEAP_STATISTICS
#define NUM_ALLOC_BUCKETS 64
#define BUCKET_VIRTUAL (NUM_ALLOC_BUCKETS - 1)
#define BUCKET_LARGE   (NUM_ALLOC_BUCKETS - 2)
LONG lNumAllocs[NUM_ALLOC_BUCKETS];
#endif

PFN_HeapCreate ptrHeapCreate = Int_HeapCreate;
PFN_HeapDestroy ptrHeapDestroy = Int_HeapDestroy;
PFN_HeapAlloc ptrHeapAlloc = Int_HeapAlloc;
PFN_HeapAllocTrace ptrHeapAllocTrace;
PFN_HeapReAlloc ptrHeapReAlloc = Int_HeapReAlloc;
PFN_HeapSize ptrHeapSize = Int_HeapSize;
PFN_HeapFree ptrHeapFree = Int_HeapFree;
extern PFN_CloseHandle v_pfnCloseHandle;
extern PFN_CreateEventW v_pfnCreateEventW;
extern PFN_LMEMAddTrackedItem v_pfnLMEMAddTrackedItem;
extern PFN_LMEMRemoveTrackedItem v_pfnLMEMRemoveTrackedItem;
extern PFN_SetAllocAdditionalData v_pfnSetAllocAdditionalData;

static LONG fFullyCompacted;

BOOL WINAPI LMemInit(void)
{
    HMODULE hLMemDebug;
    FARPROC ptrProc;

    PageMask = UserKInfo[KINX_PAGESIZE] - 1;
    DEBUGMSG(DBGFIXHP, (L"LMemInit\r\n"));
    // Uncomment the line below for "mapped heaps"
    PSlot = (char*)MapPtrUnsecure(&PSlot, GetCurrentProcess()) - (DWORD)&PSlot;

    // Check to see if there is a LMemDebug library.
    hLMemDebug = (HMODULE)LoadDriver(LMEMDEBUG_DLL);
    if (NULL != hLMemDebug) {
        RETAILMSG (1, (TEXT("Loaded LMemDebug in process\r\n")));

        // Fetch the debug alloc functions.
        ptrProc = GetProcAddress(hLMemDebug, TEXT("HeapCreate"));
        if (ptrProc) {
            ptrHeapCreate = (PFN_HeapCreate)ptrProc;
        }
        ptrProc = GetProcAddress(hLMemDebug, TEXT("HeapDestroy"));
        if (ptrProc) {
            ptrHeapDestroy = (PFN_HeapDestroy)ptrProc;
        }
        ptrProc = GetProcAddress(hLMemDebug, TEXT("HeapAlloc"));
        if (ptrProc) {
            ptrHeapAlloc = (PFN_HeapAlloc)ptrProc;
        }
        ptrProc = GetProcAddress(hLMemDebug, TEXT("HeapAllocTrace"));
        if (ptrProc) {
            ptrHeapAllocTrace = (PFN_HeapAllocTrace)ptrProc;
        }
        ptrProc = GetProcAddress(hLMemDebug, TEXT("HeapReAlloc"));
        if (ptrProc) {
            ptrHeapReAlloc = (PFN_HeapReAlloc)ptrProc;
        }
        ptrProc = GetProcAddress(hLMemDebug, TEXT("HeapFree"));
        if (ptrProc) {
            ptrHeapFree = (PFN_HeapFree)ptrProc;
        }
        ptrProc = GetProcAddress(hLMemDebug, TEXT("HeapSize"));
        if (ptrProc) {
            ptrHeapSize = (PFN_HeapSize)ptrProc;
        }
        ptrProc = GetProcAddress(hLMemDebug, TEXT("CloseHandle"));
        if (ptrProc) {
            v_pfnCloseHandle = (PFN_CloseHandle)ptrProc;
        }
        ptrProc = GetProcAddress(hLMemDebug, TEXT("CreateEventW"));
        if (ptrProc) {
            v_pfnCreateEventW = (PFN_CreateEventW)ptrProc;
        }
        ptrProc = GetProcAddress(hLMemDebug, TEXT("LMEMAddTrackedItem"));
        if (ptrProc) {
            v_pfnLMEMAddTrackedItem = (PFN_LMEMAddTrackedItem)ptrProc;
        }
        ptrProc = GetProcAddress(hLMemDebug, TEXT("LMEMRemoveTrackedItem"));
        if (ptrProc) {
            v_pfnLMEMRemoveTrackedItem = (PFN_LMEMRemoveTrackedItem)ptrProc;
        }
        ptrProc = GetProcAddress(hLMemDebug, TEXT("SetAllocAdditionalData"));
        if (ptrProc) {
            v_pfnSetAllocAdditionalData = (PFN_SetAllocAdditionalData)ptrProc;
        }
    }
#ifdef MEMTRACKING
    dwHeapMemType = RegisterTrackedItem(L"Heaps");
#endif
    InitializeCriticalSection(&csHeapList);
    if ((hProcessHeap = HeapCreate(0, 0, 0)) == NULL)
        return FALSE;
    ((pheap)hProcessHeap)->flOptions |= HEAP_IS_PROC_HEAP;
#ifdef MEMTRACKING
    ((pheap)hProcessHeap)->dwMemType = RegisterTrackedItem(L"Local Memory");
#endif
    if (IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
        CELOG_HeapCreate(HEAP_IS_PROC_HEAP, 0, CE_FIXED_HEAP_MAXSIZE, hProcessHeap);
    }
    DEBUGMSG(DBGFIXHP, (L"   LMemInit: Done   PSlot = %08x hProcessHeap = %08x\r\n", PSlot, hProcessHeap));
#ifdef HEAP_STATISTICS
    NKDbgPrintfW (L"Heap Statistics at 0x%08X", (DWORD) lNumAllocs | (DWORD) PSlot);
#endif
    return TRUE;
}


void LMemDeInit (void)
{
    // restore everything back to default since LMEMDebug has already been freed
    ptrHeapCreate = Int_HeapCreate;
    ptrHeapDestroy = Int_HeapDestroy;
    ptrHeapAlloc = Int_HeapAlloc;
    ptrHeapAllocTrace;
    ptrHeapReAlloc = Int_HeapReAlloc;
    ptrHeapSize = Int_HeapSize;
    ptrHeapFree = Int_HeapFree;
    v_pfnCloseHandle = NULL;
    v_pfnCreateEventW = NULL;
    v_pfnLMEMAddTrackedItem = NULL;
    v_pfnLMEMRemoveTrackedItem = NULL;
    v_pfnSetAllocAdditionalData = NULL;

}

#if defined(MEMTRACKING) && defined(MEM_ACCOUNT)
void TrackerCallback (DWORD dwFlags, DWORD dwType, HANDLE hItem, DWORD dwProc, BOOL fDeleted,
    DWORD dwSize, DWORD dw1, DWORD dw2) {
    PBYTE p = (PBYTE)hItem;
    if (!fDeleted && *(int*)p == 0x8482906) {
        NKDbgPrintfW (L"%a %i", p+4, *(int*)(p+20));
        if (!memcmp (p+4, "String", 7))
            NKDbgPrintfW (L"  :\"%s\"", p+24);
    }
    NKDbgPrintfW (L"\r\n");
}
#define TCB TrackerCallback
#else
#define TCB NULL
#endif

/*
    @doc BOTH EXTERNAL

    @func HLOCAL | LocalAlloc | Allocates a block of memory from the local heap
    @parm UINT | uFlags | flags for alloc operation
    @parm UINT | uBytes | desired size of block

    @comm Only fixed memory is supported, thus LMEM_MOVEABLE is invalid.
*/
HLOCAL WINAPI LocalAlloc(UINT uFlags, UINT uBytes) {
    LPVOID p;
    if(uFlags & ~LMEM_VALID_FLAGS) {
        DEBUGMSG(DBGLMEM, (L"   LocalAlloc %8.8lx %8.8lx ==> %8.8lx (invalid parameter)\r\n",uBytes,uFlags,0));
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    p = ptrHeapAlloc(hProcessHeap, (uFlags & LMEM_ZEROINIT) ? HEAP_ZERO_MEMORY : 0, uBytes);
    DEBUGMSG(DBGLMEM, (L"   LocalAlloc %8.8lx %8.8lx ==> %8.8lx [%8.8lx]\r\n",uBytes,uFlags,p,Int_HeapSize(hProcessHeap,0,p)));
    return (HLOCAL)p;
}
HLOCAL WINAPI LocalAllocTrace(UINT uFlags, UINT uBytes, UINT dwLineNum, PCHAR szFilename) {
    LPVOID p;
    if(uFlags & ~LMEM_VALID_FLAGS) {
        DEBUGMSG(DBGLMEM, (L"   LocalAllocTrace %8.8lx %8.8lx ==> %8.8lx (invalid parameter)\r\n",uBytes,uFlags,0));
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    p = HeapAllocTrace(hProcessHeap, (uFlags & LMEM_ZEROINIT) ? HEAP_ZERO_MEMORY : 0, uBytes, dwLineNum, szFilename);
    DEBUGMSG(DBGLMEM, (L"   LocalAllocTrace %8.8lx %8.8lx ==> %8.8lx [%8.8lx]\r\n",uBytes,uFlags,p,Int_HeapSize(hProcessHeap,0,p)));
    return (HLOCAL)p;
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
    if((uFlags & ~(LMEM_VALID_FLAGS | LMEM_MODIFY )) ||
        ((uFlags & LMEM_DISCARDABLE) && !(uFlags & LMEM_MODIFY))) {
        DEBUGMSG(DBGLMEM, (L"   LocalRealloc %8.8lx %8.8lx %8.8lx ==> %8.8lx (invalid parameter)\r\n",hMem,uBytes,uFlags,0));
        SetLastError(ERROR_INVALID_PARAMETER);
        return(NULL);
    }
    newflags = ((uFlags & LMEM_ZEROINIT) ? HEAP_ZERO_MEMORY : 0) | ((uFlags & LMEM_MOVEABLE) ? 0 : HEAP_REALLOC_IN_PLACE_ONLY);
    retval = HeapReAlloc(hProcessHeap, newflags, hMem, uBytes);
    DEBUGMSG(DBGLMEM, (L"   LocalRealloc %8.8lx %8.8lx %8.8lx ==> %8.8lx [%8.8lx]\r\n",hMem,uBytes,uFlags,retval,Int_HeapSize(hProcessHeap,0,retval)));
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
    if ((retval = ptrHeapSize(hProcessHeap,0,hMem)) == (DWORD)-1)
        retval = 0;
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
    HLOCAL retval;
    if (retval = (!hMem || ptrHeapFree(hProcessHeap,0,(LPVOID)hMem)) ? NULL : hMem)
        SetLastError(ERROR_INVALID_PARAMETER);
    DEBUGMSG(DBGLMEM, (L"   LocalFree %8.8lx ==> %8.8lx\r\n",hMem,retval));
    return retval;
}


#if HEAP_SENTINELS
/** CheckSentinels() - verify allocated heap block sentinels
 */
BOOL CheckSentinels(pitem pit, BOOL bStopOnErr)
{
    int chSig = 0xA5;
    PBYTE pTail = (char*)(pit+1) + pit->cbTrueSize;
    int cbTail = (pit->size&~1) - sizeof(item) - pit->cbTrueSize;

    if (pit->dwSig != ALLOCSIGHEAD) {
        RETAILMSG(1, (L"CheckSentinels: Bad head signature @0x%08X  --> 0x%08X SHDB 0x%08X\r\n",
                pit, pit->dwSig, ALLOCSIGHEAD));
#ifdef DEBUG
        if (bStopOnErr)
            DebugBreak();
#endif
        return FALSE;
    }
    while (cbTail--) {
        if (*pTail != chSig) {
            RETAILMSG(1, (L"CheckSentinels: Bad tail signature item: 0x%08X  @0x%08X -> 0x%02x SHDB 0x%02x\r\n",
                    pit, pTail, *pTail, chSig));
#ifdef DEBUG
            if (bStopOnErr)
                DebugBreak();
#endif
            return FALSE;
        }
        ++pTail;
        chSig = chSig+1 & 0xFF;
    }
    return TRUE;
}

/** SetSentinels() - set allocated heap block sentinels
 */
void SetSentinels(pitem pit, int cbRequest)
{
    int chSig = 0xA5;
    int cbTail = (pit->size&~1) - cbRequest - sizeof(item);
    PBYTE pTail = (PBYTE)(pit+1) + cbRequest;

    DEBUGCHK((int)(pit->size - sizeof(pitem)) >= cbRequest+cbTail);
    pit->cbTrueSize = cbRequest;
    pit->dwSig = ALLOCSIGHEAD;
    while (cbTail--)
        *pTail++ = chSig++;
}

/** CheckFreeSentinels() - verify free heap block sentinels
 */
BOOL CheckFreeSentinels(pitem pit, BOOL bStopOnErr)
{
    PBYTE pTail = (PBYTE)(pit+1);
    int cbTail = sizeof(item) - pit->size;

    if (pit->dwSig != FREESIGHEAD) {
        RETAILMSG(1, (L"CheckFreeSentinels: Bad head signature @0x%08X  --> 0x%08X SHDB 0x%08X\r\n",
                pit, pit->dwSig, FREESIGHEAD));
#ifdef DEBUG
        if (bStopOnErr)
            DebugBreak();
#endif
        return FALSE;
    }
    if (pit->size > -(int)sizeof(item) || (pit->size & ALIGNBYTES-1) != 0) {
        RETAILMSG(1, (L"CheckFreeSentinels: Bogus size @0x%08X  --> 0x%08X \r\n", pit, pit->size));
#ifdef DEBUG
        if (bStopOnErr)
            DebugBreak();
#endif
        return FALSE;
    }
    return TRUE;
}

/** SetSentinels() - set heap block sentinels
 */
void SetFreeSentinels(pitem pit)
{
    PBYTE pTail = (PBYTE)(pit+1);
    int cbTail = -pit->size - sizeof(item);

    DEBUGCHK(pit->size <= -(int)sizeof(item) && (pit->size & ALIGNBYTES-1) == 0);
    pit->dwSig = FREESIGHEAD;
    pit->cbTrueSize = cbTail;
}
#endif

static LPVOID AllocMemShare (LPVOID pAddr, DWORD cbSize, DWORD fdwAction, LPDWORD pdwUnused)
{
    return CeVirtualSharedAlloc ((MEM_RESERVE & fdwAction)? NULL : pAddr, cbSize, fdwAction);
}

static LPVOID AllocMemVirt (LPVOID pAddr, DWORD cbSize, DWORD fdwAction, LPDWORD pdwUnused)
{
    return VirtualAlloc (pAddr, cbSize, fdwAction, PAGE_READWRITE);
}

static BOOL FreeMem (LPVOID pAddr, DWORD cbSize, DWORD fdwAction, DWORD dwUnused)
{
    return VirtualFree (pAddr, cbSize, fdwAction);
}

BOOL InitNewHeap(pheap php, 
    DWORD flOptions,
    DWORD dwInitialSize,
    DWORD dwMaximumSize,
    DWORD cbRegion,
    PFN_AllocHeapMem pfnAlloc,
    PFN_FreeHeapMem pfnFree,
    DWORD dwRgnData)
{
    DWORD dwTemp;
    pitem pit;
#ifdef MEMTRACKING
    WCHAR Name[14];
#endif
    DEBUGCHK (pfnAlloc && pfnFree);

    if (!pfnAlloc (php, dwInitialSize, MEM_COMMIT, &dwRgnData))
        return FALSE;

    php->dwSig = HEAPSIG;
    php->flOptions = (WORD)flOptions;
    php->pfnAlloc = pfnAlloc;
    php->pfnFree = pfnFree;
    if (dwMaximumSize)
        php->cbMaximum = dwMaximumSize;

    InitializeCriticalSection(&php->cs);

    php->rgn.dwRgnData = dwRgnData;

    // Initialize first free item in the starting region.
    pit = FIRSTITEM(&php->rgn);
    php->rgn.pitFree = php->rgn.pitLast = pit;
    dwTemp = (dwInitialSize - SIZE_HEAP_HEAD - sizeof(item)) & ~(ALIGNBYTES-1);
    pit->size = -(int)dwTemp;
    pit->prgn = &php->rgn;
#if HEAP_SENTINELS
    memset (pit+1, 0xcc, dwTemp - sizeof(item));
    SetFreeSentinels(pit);
#endif
    DEBUGMSG(DBGFIXHP, (L"InitNewHeap: first item=%8.8lx size=%lx\r\n", pit, -pit->size));

    // Initialize the endmarker.
    pit = (pitem)((char*)pit + dwTemp);
    pit->size = 0;
    pit->cbTail = cbRegion - dwInitialSize;
#if HEAP_SENTINELS
    pit->dwSig = TAILSIG;
#endif
    DEBUGMSG(DBGFIXHP, (L"InitNewHeap: endmarker=%8.8lx size=%d extra=%lx\r\n", pit, pit->size, pit->cbTail));

    php->rgn.cbMaxFree = dwTemp;
    php->rgn.phpOwner = php;
    php->rgn.prgnLast = &php->rgn;      // point to ourself (first == last)
#ifdef MEMTRACKING
    AddTrackedItem(dwHeapMemType,php,NULL,GetCurrentProcessId(),sizeof(heap),0,0);
    swprintf(Name, L"Heap %08X", php);
    php->wMemType = (WORD)RegisterTrackedItem(Name);
#endif

    if ( !(flOptions & HEAP_IS_SHARED)) {
        // Add the new heap to the list of heaps for the current process.
        EnterCriticalSection(&csHeapList);
        php->phpNext = phpListAll;
        phpListAll = php;
        LeaveCriticalSection(&csHeapList);
    }
    return TRUE;
}


LPBYTE InitSharedHeap(LPBYTE pMem, DWORD size, DWORD reserve)
{
    pheap php;
    pitem pit;
    LPBYTE pbAlloc;

    php = (pheap)pMem;
    pit = FIRSTITEM(&php->rgn);
DEBUGMSG(1, (L"InitSharedHeap: %8.8lx %lx %lx\r\n", pMem, size, reserve));
    if (size) {
        InitNewHeap(php, HEAP_IS_SHARED, PAGE_SIZE, size, size, AllocMemVirt, FreeMem, 0);
        pbAlloc = HeapAlloc((HANDLE)php, 0, reserve);
        DEBUGCHK(pbAlloc == (LPBYTE)(pit+1));
    }
DEBUGMSG(1, (L"  InitSharedHeap: return %8.8lx\r\n", pit+1));
    return (LPBYTE)(pit+1);
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
    return ptrHeapCreate (flOptions, dwInitialSize, dwMaximumSize);
}

//
// DoHeapCreate: Worker function to create a heap, parameter already validated
//
static HANDLE DoHeapCreate(DWORD flOptions,
    DWORD dwInitialSize,
    DWORD dwMaximumSize,
    PFN_AllocHeapMem pfnAlloc,
    PFN_FreeHeapMem pfnFree)
{
    pheap php;
    DWORD cbRegion;
    DWORD dwRgnData = 0;

    // Compute size of initial memory region based upon the requested initial and maxiumum sizes.
    dwInitialSize = (dwInitialSize + PAGE_SIZE - 1) & -PAGE_SIZE;
    dwMaximumSize = (dwMaximumSize + PAGE_SIZE - 1) & -PAGE_SIZE;
    cbRegion = (dwMaximumSize == 0) ? CE_FIXED_HEAP_MAXSIZE : dwMaximumSize;
    if (dwInitialSize == 0)
        dwInitialSize = PAGE_SIZE;
    else if (dwInitialSize > cbRegion)
        dwInitialSize = cbRegion;

    // Reserve address space for the initial region and commit the initial # of pages.
    if (!(php = pfnAlloc (PSlot, cbRegion, MEM_RESERVE, &dwRgnData))
            || !InitNewHeap(php, flOptions, dwInitialSize, dwMaximumSize, cbRegion, pfnAlloc, pfnFree, dwRgnData)) {
        if (php)
            pfnFree (php, 0, MEM_RELEASE, dwRgnData);
        DEBUGMSG(DBGFIXHP, (L"   HeapCreate: Unable to allocate initial pages\r\n"));
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }
    DEBUGMSG(DBGFIXHP, (L"   HeapCreate %8.8lx %8.8lx %8.8lx ==> %8.8lx\r\n", flOptions, dwInitialSize, dwMaximumSize, php));
    if (IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
        CELOG_HeapCreate(flOptions, dwInitialSize, dwMaximumSize, (HANDLE)php);
    }
    return (HANDLE)php;
}

HANDLE WINAPI Int_HeapCreate(DWORD flOptions, DWORD dwInitialSize, DWORD dwMaximumSize)
{
    DEBUGMSG(DBGFIXHP, (L"HeapCreate %8.8lx %8.8lx %8.8lx\r\n", flOptions, dwInitialSize, dwMaximumSize));
    if (flOptions & ~(HEAP_NO_SERIALIZE|HEAP_SHARED_READONLY)) {
        DEBUGMSG(DBGFIXHP, (L"   HeapCreate: Invalid parameter\r\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    return DoHeapCreate (flOptions, dwInitialSize, dwMaximumSize, (flOptions & HEAP_SHARED_READONLY)? AllocMemShare : AllocMemVirt, FreeMem);
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
    if (flOptions || !pfnAlloc || !pfnFree) {
        DEBUGMSG(DBGFIXHP, (L"   CeHeapCreate: Invalid parameter\r\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    return DoHeapCreate (flOptions, dwInitialSize, dwMaximumSize, pfnAlloc, pfnFree);
}

/** Create a new memory region for an existing heap.
 */
static pregion CreateNewRegion(LPVOID lpSlot, pheap php, int cbSeek)
{
    pregion prgn, prgnLast;
    pitem pit;
    DWORD dwRgnData = 0;

    DEBUGCHK(php->cbMaximum == 0 && php->pfnAlloc);
    cbSeek = (cbSeek + SIZE_REGION_HEAD + sizeof(item) + PAGE_SIZE-1) & -PAGE_SIZE;

    // Reserve address space for the region and commit the initial # of pages.
    if (!(prgn = php->pfnAlloc (lpSlot, CE_FIXED_HEAP_MAXSIZE, MEM_RESERVE, &dwRgnData))
            || !php->pfnAlloc (prgn, cbSeek, MEM_COMMIT, &dwRgnData)) {
        if (prgn)
            php->pfnFree (prgn, 0, MEM_RELEASE, dwRgnData);
        DEBUGMSG(DBGHEAP2, (L"   HeapAllocate: Unable to allocate new region\r\n"));
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }

    // Initialize the first free item in the region.
    pit = FIRSTITEM(prgn);
    prgn->pitFree = prgn->pitLast = pit;
    prgn->prgnNext = 0;           // we'll be the last in the list
    prgn->dwRgnData = dwRgnData;
    pit->prgn = prgn;
    cbSeek -= SIZE_REGION_HEAD + sizeof(item);
    pit->size = -cbSeek;
#if HEAP_SENTINELS
    memset (pit+1, 0xcc, cbSeek - sizeof(item));
    SetFreeSentinels(pit);
#endif
    DEBUGMSG(DBGHEAP2, (L"CreateNewRegion: first item=%8.8lx size=%lx\r\n", pit, -pit->size));

    // Initizize the endmarker.
    pit = (pitem)((char*)pit + cbSeek);
    pit->size = 0;
    pit->cbTail = (CE_FIXED_HEAP_MAXSIZE - cbSeek) & -PAGE_SIZE;
#if HEAP_SENTINELS
    pit->dwSig = TAILSIG;
#endif
    DEBUGMSG(DBGHEAP2, (L"CreateNewRegion: endmarker=%8.8lx size=%d extra=%lx\r\n", pit, pit->size, pit->cbTail));

    // Initialize the region structure and link it into the heap's region list.
    prgn->cbMaxFree = cbSeek;
    prgn->phpOwner = php;

    prgnLast = php->rgn.prgnLast;
    php->rgn.prgnLast = prgnLast->prgnNext = prgn;

    return prgn;
}

/** Delete an extended memory region for a heap
 */
static void DeleteRegion(pregion prgnDel)
{
    pheap php = prgnDel->phpOwner;
    pregion prgn;

    DEBUGCHK(php->dwSig == HEAPSIG);
    prgn = &php->rgn;
    do {
        if (prgn->prgnNext == prgnDel) {
            DWORD dwRgnData = prgnDel->dwRgnData;
            prgn->prgnNext = prgnDel->prgnNext;
            php->pfnFree (prgnDel, CE_FIXED_HEAP_MAXSIZE, MEM_DECOMMIT, dwRgnData);
            php->pfnFree (prgnDel, 0, MEM_RELEASE, dwRgnData);
            return;
        }
    } while ((prgn = prgn->prgnNext) != NULL);
    DEBUGMSG(1, (L"DeleteRegion: 0x%08X not found in Heap 0x%08X\r\n", prgnDel, php));
    DEBUGCHK(0);
}

//
// MergeFreeItems: Merge all free blocks and return the next item (or self if already at end)
//
static pitem MergeFreeItems (pregion prgn, pitem pit)
{
    pitem pitTrav;
    int cbItem = -pit->size;
    DEBUGCHK (cbItem >= 0);

    if (cbItem <= 0)
        return pit;

    // combine all the contigious free blocks
    while ((pitTrav = (pitem)((char*)pit + cbItem))->size < 0) {
        cbItem -= pitTrav->size;
#if HEAP_SENTINELS
        // block merged, fill 0xcc for the merged item header debug
        memset (pitTrav, 0xcc, sizeof(item));
#endif
    }


    pit->size = -cbItem;

    if (!pitTrav->size) {
        prgn->pitLast = pit;
        fFullyCompacted = FALSE;
    }

#ifdef FREE_POINTER_AT_LAST_FREE
    prgn->pitFree = pit;
#else
    if (prgn->pitFree > pit)
        prgn->pitFree = pit;
#endif

#if HEAP_SENTINELS
    SetFreeSentinels(pit);
#endif

    return pitTrav;
}

//
// Carve a required size from an item and, possibly, create a leftover item
//
static pitem CarveItem (pregion prgn, pitem pit, int cbSeek, DWORD dwBytes, pitem pitcombine)
{
    int cbItem = -pit->size;
    pitem pitNext;
#if HEAP_SENTINELS == 0
    int cbEx;
#endif

    DEBUGCHK (cbItem >= 0);

    if (cbItem < cbSeek) {

        // this is the case that we need to commit more memory
        pitem pitEnd = (pitem) ((char *) pit + cbItem);
        int cbEx;

        DEBUGCHK ((pitEnd->size == 0) && ((((DWORD)pitEnd + sizeof(item)) & (PAGE_SIZE-1)) == 0));

        // calculate much many bytes, round up to pagesize, we need to commit
        // NOTE: we need extra room for an end marker
        cbEx  = (cbSeek - cbItem + sizeof(item) + PAGE_SIZE-1) & -PAGE_SIZE;

        // commit extra memory.
        if (!prgn->phpOwner->pfnAlloc ((char *)pitEnd + sizeof(item), cbEx, MEM_COMMIT, &prgn->dwRgnData)) {
            return NULL;    // out of memory
        }

        DEBUGMSG(DBGHEAP2, (L"CarveItem: Committed more memory pitEnd = %8.8lx cbSeek = %8.8lx, cbEx = %8.8lx\r\n", pitEnd, cbSeek, cbEx));

        // create a new end marker
        pitNext = (pitem)((char*)pitEnd + cbEx);
        pitNext->size = 0;
        pitNext->cbTail = pitEnd->cbTail - cbEx;
#if HEAP_SENTINELS
        pitNext->dwSig = TAILSIG;
#endif

        DEBUGMSG(DBGHEAP2, (L"CarveItem: new endmarker %8.8lx(%lx)\r\n", pitNext, pitNext->cbTail));

        // update the size of the item
        pit->size -= cbEx;
        pit->prgn = prgn;        // it's possible (pit == pitEnd), so we need to update pit->prgn
        cbItem = -pit->size;

        if (cbItem - cbSeek > prgn->cbMaxFree) {
            // if we get more after committing...
            prgn->cbMaxFree = (cbItem - cbSeek) | (prgn->cbMaxFree & REGION_MAX_IS_ESTIMATE);
        }

        DEBUGMSG(DBGHEAP2, (L"CarveItem: pit = %8.8lx pit->size = %8.8lx\r\n", pit, pit->size));
    }

    // must have enough room
    DEBUGCHK (prgn && (prgn == pit->prgn) && (cbSeek <= cbItem));

    // if the leftover is too small, use it up
    if (cbItem <= (int) (cbSeek + sizeof(item) + HEAP_SENTINELS)) {
        cbSeek = cbItem;
    }

    // create a new item if necessary
    pitNext = (pitem)((char*) pit + cbSeek);

    if (cbItem > cbSeek) {

        // create an item for the leftover space
        pitNext->size = cbSeek - cbItem;    // Free block size is stored as negative number
        DEBUGMSG(DBGHEAP2, (L"CarveItem: Leftover %8.8lx(%lx)\r\n", pitNext, pitNext->size));
        pitNext->prgn = prgn;
#if HEAP_SENTINELS
        SetFreeSentinels(pitNext);
#endif
    }

    // update the size of the new item
    if (pitcombine) {
        DEBUGCHK ((pitcombine->size > 0) && ((DWORD) pitcombine + pitcombine->size == (DWORD) pit));
        (pit = pitcombine)->size += cbSeek;

    } else {
        pit->size = cbSeek;
    }

    // update the region's free pointer
    if (pitNext->size <= 0) {
        if (prgn->pitFree > pitNext)
            prgn->pitFree = pitNext;

        // update pitLast if we hit the end
        if (!((pitem) ((char *) pitNext - pitNext->size))->size) {
            prgn->pitLast = pitNext;
        }
    }

#if HEAP_SENTINELS
    SetSentinels(pit, dwBytes);
#else
    // zero the excess memory we allocated to it
    cbEx = pit->size - sizeof(item) - dwBytes;

    //
    // DEBUGCHK ((cbEx >= 0) && (cbEx < 16));
    if (cbEx > 0) {
        memset ((char *)(pit+1) + dwBytes, 0, cbEx);
    }
#endif
    DEBUGMSG(DBGHEAP2, (L"CarveItem returns: pit = %8.8lx pit->size = %8.8lx\r\n", pit, pit->size));
    return pit;
}


//
// Large allocation using VirtualAlloc
//
static pitem DoLargeHeapAlloc (LPVOID lpSlot, pheap php, DWORD dwBytes)
{
    pitem pitRet;
    pvaitem pva;
    DWORD dwReserve, cbSeek, dwData = 0;

    // Attempt to allocate the memory block before taking the heap critical section
    // to improve multi-threaded behavior.
    cbSeek = (dwBytes + ALIGNSIZE(sizeof(vaitem)) + PAGE_SIZE-1) & ~(PAGE_SIZE-1);
    dwReserve = (cbSeek + HEAP_SENTINELS + 0xFFFFL) & 0xFFFF0000L;
    if (!(pva = php->pfnAlloc (lpSlot, dwReserve, MEM_RESERVE, &dwData))
            || !php->pfnAlloc (pva, cbSeek, MEM_COMMIT, &dwData)) {
        if (pva)
            php->pfnFree (pva, 0, MEM_RELEASE, dwData);

        return NULL;
    }

    // New VA'ed item created.  Initialize the item structure within the vaitem.
    pva = (pvaitem)((char*)pva + (ALIGNSIZE(sizeof(vaitem)) - sizeof(vaitem)));
    pva->dwData = dwData;
    pitRet = &pva->it;
    pitRet->php = php;
    pitRet->size = (cbSeek - ALIGNSIZE(sizeof(vaitem)) + sizeof(item)) | 1;  // Tag item as VirtualAlloc'ed

#if HEAP_SENTINELS
    SetSentinels(pitRet, dwBytes);
#endif

    // Take the heap's CS and link it into the vaitem list.
    EnterCriticalSection(&php->cs);
    pva->pvaFwd = php->pvaList;
    if (php->pvaList)
        php->pvaList->pvaBak = pva;
    php->pvaList = pva;
    LeaveCriticalSection(&php->cs);

    return pitRet;
}

// find a free item of size >= cbSeek in a region (HOLDING HEAP CS).
static pitem FindFreeItemInRegion (LPVOID lpSlot, pregion prgn, int cbSeek)
{
    pitem pitStart = prgn->pitFree;
    pitem pit, pitRet, pitEnd;
    int   cbItem;
    int   nEndReached = 0;
    int   cbMax = 0;

    if (!pitStart->size)
        pitStart = FIRSTITEM (prgn);

    DEBUGMSG(DBGHEAP2, (L"1:FindFreeItemInRegion %8.8lx %8.8lx\r\n", prgn->pitFree, pitStart));
    pit = pitStart;
#ifndef FREE_POINTER_AT_LAST_FREE
    if (prgn->pitFree->size > 0)
        prgn->pitFree = prgn->pitLast;
#endif

    do {

        // find the next free item
        // NOTE: MUST SAVE THE SIZE IN LOCAL OR THERE'LL BE RACE CONDITION BECAUSE
        //       HeapReAlloc DOES NOT ACQUIRE CS while shrinking.
        while ((cbItem = pit->size) > 0) {
            pit = (pitem)((char*)pit + cbItem);
        }

        pit = MergeFreeItems (prgn, pitRet = pit);
        cbItem = -pitRet->size;

        // Is the block big enough?
        if (cbItem >= cbSeek) {

            DEBUGMSG(DBGHEAP2, (L"FindFreeItemInRegion returns pitRet = %8.8lx\r\n", pitRet));
            if (cbItem - cbSeek > cbMax)
                cbMax = cbItem - cbSeek;

            break;
        }

        if (cbMax < cbItem)
            cbMax = cbItem;

        if (nEndReached && (pit >= pitStart))
            break;

        // if we've reached the last block and the size is big enough, mark the end
        // but don't use it yet -- since it requires committing more memory
        if (!pit->size) {

            if (nEndReached ++) {
                // heap is corrupted -- faked end marker being inserted
                DEBUGMSG(DBGHEAP2, (L"!FindFreeItemInRegion: Heap is corrupted\r\n"));
                DEBUGCHK (0);
                return NULL;
            }
            pitEnd = pitRet;

            // continue search from the 1st item if not done the full pass
            pit = FIRSTITEM (prgn);
        }

    }while (!nEndReached || (pit < pitStart));

    if (cbItem < cbSeek) {

        // if we reach here, either we need to commit more memory or don't have enough room

        // if the last item examined is not the end, update it to the recorded end
        if (pit->size)
            pitRet = pitEnd;

        pitEnd = (pitem) ((char *)pitRet - pitRet->size);

        DEBUGCHK ((pitRet->size <= 0) && !pitEnd->size);

        // need an extra room for tail item. thus the check
        if (pitEnd->cbTail - pitRet->size < (int) (cbSeek + sizeof(item)))
            pitRet = NULL;

        prgn->cbMaxFree = 0;    // the number is no longer an estimate, wil be updated next
    }

    if (cbMax > prgn->cbMaxFree) {
        prgn->cbMaxFree = cbMax | (prgn->cbMaxFree & REGION_MAX_IS_ESTIMATE);
    }

    DEBUGMSG(DBGHEAP2, (L"FindFreeItemInRegion returns pitRet = %8.8lx\r\n", pitRet));
    return pitRet;
}


// find a free item of size >= cbSeek in a heap (HOLDING HEAP CS).
static pitem FindFreeItem (LPVOID lpSlot, pheap php, DWORD cbSeek, pregion *pprgn)
{
    pregion prgn = &php->rgn;
    pitem   pitRet = NULL;

    DEBUGMSG(DBGHEAP2, (L"FindFreeItem %8.8lx %8.8lx %8.8lx\r\n", lpSlot, php, cbSeek));

    // pass 1: look for regions with items large enough for us
    do {
        if (prgn->cbMaxFree >= (int) cbSeek) {
            if (pitRet = FindFreeItemInRegion (lpSlot, prgn, cbSeek)) {
                DEBUGCHK (pitRet->size <= 0);
                break;
            }
        }
    } while (prgn = prgn->prgnNext);

    // pass 2: look for all region that are estimates or need to commit memory
    if (!pitRet) {
        prgn = &php->rgn;
        do {
            // if the region has an exact upper bound, return the pitLast if there are
            // enough room at the end after committing pages
            if (!(prgn->cbMaxFree & REGION_MAX_IS_ESTIMATE)) {
                pitem pitLast = prgn->pitLast;
                pitem pitTail = (pitem) ((char *) pitLast - pitLast->size);
                if (pitTail->cbTail - pitLast->size > (int) cbSeek) {
                    pitRet = pitLast;
                    break;
                }
            } else if (pitRet = FindFreeItemInRegion (lpSlot, prgn, cbSeek)) {
                DEBUGCHK (pitRet->size <= 0);
                break;
            }
        } while (prgn = prgn->prgnNext);
    }
    *pprgn = prgn;
    DEBUGMSG(DBGHEAP2, (L"FindFreeItem returns %8.8lx, prgn = %8.8lx\r\n", pitRet, prgn));
    return pitRet;
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
    return ptrHeapReAlloc (hHeap, dwFlags, lpMem, dwBytes);
}
LPVOID WINAPI Int_HeapReAlloc(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, DWORD dwBytes)
{
    LPVOID pRet = NULL;
    int cbItem;
    int cbOrigSize;
    int cbSeek;
    pitem pit;
    pheap php = (pheap)hHeap;
    pregion prgn;
    BOOL bZeroPtr = FALSE;      // set to TRUE if caller is using "zero based pointers"
#if HEAP_SENTINELS
    DWORD dwSize;
#endif

    if (((DWORD)hHeap>>SECTION_SHIFT) != ((DWORD)lpMem>>SECTION_SHIFT)) {
        // The ptr has been unmapped by someone or it doesn't belong to this heap.
        if ((DWORD)lpMem>>SECTION_SHIFT) {
            DEBUGMSG(DBGFIXHP, (L"   1:HeapReAlloc %8.8lx %8.8lx %8.8lx %8.8lx ==> %8.8lx (invalid parameter)\r\n", hHeap,dwFlags,lpMem,dwBytes,0));
            SetLastError(ERROR_INVALID_PARAMETER);
            return NULL;
        }
        // Restore the "slot mapping" bits to the pointer.
        lpMem = (LPVOID)((DWORD)lpMem | ((DWORD)hHeap & -(1<<SECTION_SHIFT)));
        bZeroPtr = TRUE;
    }
    pit = (pitem)((char*)lpMem - sizeof(item));
    if (dwFlags & ~(HEAP_GENERATE_EXCEPTIONS | HEAP_NO_SERIALIZE | HEAP_REALLOC_IN_PLACE_ONLY | HEAP_ZERO_MEMORY)) {
        DEBUGMSG(DBGFIXHP, (L"   2:HeapReAlloc %8.8lx %8.8lx %8.8lx %8.8lx ==> %8.8lx (invalid parameter)\r\n", hHeap,dwFlags,lpMem,dwBytes,0));
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    if (dwFlags & HEAP_GENERATE_EXCEPTIONS) {
        DEBUGMSG(DBGFIXHP, (L"   3:HeapReAlloc %8.8lx %8.8lx %8.8lx %8.8lx ==> %8.8lx (not supported)\r\n", hHeap,dwFlags,lpMem,dwBytes,0));
        SetLastError(ERROR_NOT_SUPPORTED);
        return NULL;
    }
    if (!hHeap || php->dwSig != HEAPSIG || (DWORD)ZeroPtr(lpMem) < 0x10000 || pit->size <= 0) {
        DEBUGMSG(DBGFIXHP, (L"   4:HeapReAlloc %8.8lx %8.8lx %8.8lx %8.8lx %8.8lx (invalid parameter)\r\n", hHeap,dwFlags,lpMem,dwBytes,pit->size));
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    if (dwBytes > HEAP_MAX_ALLOC) {
        DEBUGMSG(DBGFIXHP, (L"   5:HeapReAlloc %8.8lx %8.8lx %8.8lx %8.8lx ==> %8.8lx\r\n", hHeap,dwFlags,lpMem,dwBytes,0));
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }

    cbItem = pit->size;
#if HEAP_SENTINELS
    cbOrigSize = pit->cbTrueSize;
#else
    cbOrigSize = cbItem & ~1;
#endif

    if (cbItem & 1) {
        // VirtualAlloc'ed item.  Validate that the item pointer falls correctly within a
        // 64K aligned virtual memory block and that the heap owner information in the
        // item is correct.
        pvaitem pva;
        int cbAllocated;

        --cbItem;       // Fix the item size
        pva = (pvaitem)(((DWORD)lpMem&0xFFFF0000) + (ALIGNSIZE(sizeof(vaitem)) - sizeof(vaitem)));
        cbAllocated = cbItem + (ALIGNSIZE(sizeof(vaitem)) - sizeof(item));
        if (pit != &pva->it || pit->php != php || (cbAllocated & PageMask) != 0) {
            DEBUGMSG(DBGFIXHP, (L"   6:HeapReAlloc %8.8lx %8.8lx %8.8lx %8.8lx ==> %8.8lx (invalid parameter)\r\n", hHeap,dwFlags,lpMem,dwBytes,0));
            SetLastError(ERROR_INVALID_PARAMETER);
            return NULL;
        }
#if HEAP_SENTINELS
        CheckSentinels(pit, TRUE);
#endif
        cbSeek = (dwBytes + ALIGNSIZE(sizeof(vaitem)) + PAGE_SIZE-1) & ~(PAGE_SIZE-1);
        if (cbSeek <= cbAllocated) {
            // Just use the existing block and treat this as a NOP.
#if HEAP_SENTINELS
            // If the size is changing, we must update the sentinels and
            // possibly zero out a few bytes.
            if ((dwSize = pit->cbTrueSize) != dwBytes) {
                SetSentinels(pit, dwBytes);
                if (dwSize < dwBytes && (dwFlags & HEAP_ZERO_MEMORY))
                    memset((char*)(pit+1)+dwSize, 0, dwBytes-dwSize);
            }
#endif
            pRet = pit+1;
        }
        

    } else {
        // Normal heap allocation.  Verify the region parameters.
        prgn = pit->prgn;
        if (prgn == NULL || php != prgn->phpOwner || (cbItem & (ALIGNBYTES-1)) != 0) {
            DEBUGMSG(DBGFIXHP, (L"   7:HeapReAlloc %8.8lx %8.8lx %8.8lx %8.8lx ==> %8.8lx (invalid parameter)\r\n", hHeap,dwFlags,lpMem,dwBytes,0));
            SetLastError(ERROR_INVALID_PARAMETER);
            return NULL;
        }
#if HEAP_SENTINELS
        CheckSentinels(pit, TRUE);
#endif
        cbSeek = ALIGNSIZE(dwBytes + sizeof(item) + HEAP_SENTINELS);

        if (cbSeek > cbItem) {
            // Need to grow the block. Check to see if there is a free block following
            // this one that is large enough to satisfy the request.
            pitem pitTrav, pitNext = (pitem)((char*)pit + cbItem);
            int   cbNext;

            // grab the CS
            EnterCriticalSection(&php->cs);

            if (pitNext->size > 0) {
                // release CS
                LeaveCriticalSection(&php->cs);

            } else {

                pitTrav = MergeFreeItems (prgn, pitNext);
                cbNext = -pitNext->size;

                DEBUGMSG(DBGFIXHP, (L"   8:HeapReAlloc %8.8lx %8.8lx %8.8lx %8.8lx\n", cbSeek, cbItem, cbNext, pitTrav->size));
                // do we have room to do in-place realloc?
                if ((cbItem + cbNext >= cbSeek)                 // have room by merging with free blocks?
                    || (!pitTrav->size                          // reach the end marker and have enough room?
                        && (cbItem + cbNext + pitTrav->cbTail > cbSeek))) {
                    if (pit = CarveItem (prgn, pitNext, cbSeek - cbItem, dwBytes, pit)) {
                        pRet = pit + 1;   // return value is right after the item

                        if (prgn->pitFree > pit) {
                            prgn->pitFree = (pitem) ((char *)pit + pit->size);
                        }
                    }
                }

                // release CS
                LeaveCriticalSection(&php->cs);

                // zero memory if requested
                if (pRet && (dwFlags & HEAP_ZERO_MEMORY)) {
#if HEAP_SENTINELS
                    memset ((char*)pRet + cbOrigSize, 0, dwBytes - cbOrigSize);
#else
                    memset ((char*)pit + cbOrigSize, 0, cbSeek - cbOrigSize);
#endif
                }
            }

        } else {

            if (cbItem > (int) (cbSeek + sizeof(item) + HEAP_SENTINELS)) {
                pitem pitNew = (pitem)((char*)pit + cbSeek);
                cbItem -= cbSeek;
                pitNew->size = -cbItem;
                pitNew->prgn = prgn;
                DEBUGMSG(DBGFIXHP, (L"   11:HeapReAlloc pitNew = %8.8lx (%8.8lx)\n", pitNew, pitNew->size));

                EnterCriticalSection (&php->cs);
                pit->size = cbSeek;     // Shrink the orginal block

                // we seems to do better without merging it here
                //MergeFreeItems (prgn, pitNew);

                if (prgn->cbMaxFree < cbItem) {
                    // the state of estimate does not change because of this
                    prgn->cbMaxFree = cbItem | (prgn->cbMaxFree & REGION_MAX_IS_ESTIMATE);
                }

#if HEAP_SENTINELS
                SetFreeSentinels (pitNew);
                SetSentinels(pit, dwBytes);
#endif
                LeaveCriticalSection (&php->cs);
                DEBUGMSG(DBGFIXHP, (L"   12:HeapReAlloc pit = %8.8lx (%8.8lx)\n", pit, pit->size));
            }
#if HEAP_SENTINELS
            else {
                // If the size is changing, we must update the sentinels and
                // possibly zero out a few bytes.
                if ((dwSize = pit->cbTrueSize) != dwBytes) {
                    SetSentinels(pit, dwBytes);
                    if (dwSize < dwBytes && (dwFlags & HEAP_ZERO_MEMORY))
                        memset((char*)(pit+1)+dwSize, 0, dwBytes-dwSize);
                }
            }
#endif
            pRet = pit+1;

        }
    }

    if (pit && !pRet && !(dwFlags & HEAP_REALLOC_IN_PLACE_ONLY)) {
        if ((pRet = Int_HeapAlloc(hHeap, dwFlags & HEAP_GENERATE_EXCEPTIONS, dwBytes)) != 0) {
#if HEAP_SENTINELS
            cbItem = pit->cbTrueSize;
            memcpy(pRet, pit+1, cbItem);
            if (dwFlags & HEAP_ZERO_MEMORY)
                memset((char*)pRet+cbItem, 0, dwBytes - cbItem);
#else
            cbItem = (pit->size & ~1) - sizeof(item);
            memcpy(pRet, pit+1, cbItem);
            if (dwFlags & HEAP_ZERO_MEMORY) {
                // Zero out tail of data that wasn't copied.
                int cbNew = (((pitem)pRet)[-1].size & ~1) - sizeof(item);
                if (cbNew > cbItem)
                    memset((char*)pRet+cbItem, 0, cbNew - cbItem);
            }
#endif
            // The new block has been allocated and the contents of the old block have been
            // copied to it.  Now free the old block.
            Int_HeapFree(hHeap, dwFlags & HEAP_GENERATE_EXCEPTIONS, lpMem);
        }
    } else if (!pRet)
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);

    DEBUGMSG(DBGFIXHP, (L"   9:HeapReAlloc %8.8lx %8.8lx %8.8lx %8.8lx ==> %8.8lx\r\n", hHeap,dwFlags,lpMem,dwBytes,pRet));
    DEBUGCHK(!pRet || ((Int_HeapSize(hHeap,0,pRet) >= dwBytes) && (Int_HeapSize(hHeap,0,pRet) != (DWORD)-1)));
    if (IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
        CELOG_HeapRealloc(hHeap, dwFlags, dwBytes, (DWORD)lpMem, (DWORD)pRet);
    }
    return bZeroPtr ? (LPVOID)ZeroPtr(pRet) : pRet;
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
    return ptrHeapAlloc(hHeap, dwFlags, dwBytes);
}
LPVOID WINAPI HeapAllocTrace(HANDLE hHeap, DWORD dwFlags, DWORD dwBytes, DWORD dwLineNum, PCHAR szFilename)
{
    // If it's redirected then call it, otherwise ignore the trace params and call HeapAlloc
    if (ptrHeapAllocTrace) {
        return ptrHeapAllocTrace(hHeap, dwFlags, dwBytes, dwLineNum, szFilename);
    } else {
        return ptrHeapAlloc(hHeap, dwFlags, dwBytes);
    }
}
LPVOID WINAPI Int_HeapAlloc(HANDLE hHeap, DWORD dwFlags, DWORD dwBytes)
{
    pheap   php = (pheap) hHeap;
    pregion prgn;
    LPVOID  lpSlot;
    pitem   pitRet;

    //
    // check parameters
    //

    lpSlot = (char*)php - (DWORD)ZeroPtr(php);
    if ((dwFlags & ~(HEAP_NO_SERIALIZE | HEAP_ZERO_MEMORY)) || !hHeap || php->dwSig != HEAPSIG) {
        DEBUGMSG(DBGFIXHP, (L"   HeapAlloc %8.8lx %8.8lx %8.8lx ==> %8.8lx (invalid parameter)\r\n", hHeap,dwFlags,dwBytes,0));
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    if (dwBytes > HEAP_MAX_ALLOC) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return NULL;
    }



    if (!php->cbMaximum && (dwBytes+sizeof(vaitem) >= CE_VALLOC_MINSIZE)) {
        // size requested is greated that VA threshold, use VirtualAlloc
        pitRet = DoLargeHeapAlloc (lpSlot, php, dwBytes);
#ifdef HEAP_STATISTICS
        InterlockedIncrement (&lNumAllocs[BUCKET_VIRTUAL]);
#endif
    } else {
        // ordinary HeapAlloc
        int cbSeek = ALIGNSIZE(dwBytes + sizeof(item) + HEAP_SENTINELS);
#ifdef HEAP_STATISTICS
        int idx = (cbSeek >> 4) - 1;   // slot number

        if (idx > BUCKET_LARGE)      // idx BUCKET_LARGE is for items >= 1K in size
            idx = BUCKET_LARGE;

        InterlockedIncrement (&lNumAllocs[idx]);
#endif

        // grab CS if serialized.
        EnterCriticalSection (&php->cs);

        // try allocating in existing regions
        if (!(pitRet = FindFreeItem (lpSlot, php, cbSeek, &prgn))   // try existing region first
            && !php->cbMaximum                                      // can we grow?
            && (prgn = CreateNewRegion (lpSlot, php, cbSeek))) {    // enough memory to create a new region?
            // if we can't find it in existing regions, try to create new regions
            pitRet = FIRSTITEM (prgn);
        }

        if (pitRet) {
            // carve the size requested from the free item found
            pitRet = CarveItem (prgn, pitRet, cbSeek, dwBytes, NULL);

#ifdef MEMTRACKING
            AddTrackedItem(php->wMemType, pitRet+1, TCB, GetCurrentProcessId(), pitRet->size, 0, 0);
#endif

        }
        LeaveCriticalSection (&php->cs);

        // Zero the memory block if requested.
        if ((dwFlags & HEAP_ZERO_MEMORY) && pitRet) {
#if HEAP_SENTINELS
            memset(pitRet+1, 0, pitRet->cbTrueSize);
#else
            memset(pitRet+1, 0, pitRet->size - sizeof(item));
#endif
        }
    }

    if (pitRet)
        ++pitRet;   // return the pointer AFTER the item header.

    if (IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
        CELOG_HeapAlloc(hHeap, dwFlags, dwBytes, (DWORD)pitRet);
    }
    DEBUGMSG(DBGFIXHP, (L"   HeapAlloc %8.8lx %8.8lx %8.8lx ==> %8.8lx\r\n", hHeap, dwFlags, dwBytes, pitRet));

    return lpSlot!=PSlot ? pitRet : (pitem)ZeroPtr(pitRet);

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
    return ptrHeapFree(hHeap, dwFlags, lpMem);
}
BOOL WINAPI Int_HeapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem)
{
    pheap php;
    pitem pit;
    pregion prgn;
    int size;
    BOOL bRet = FALSE;

    php = (pheap)hHeap;
    if ((dwFlags & ~HEAP_NO_SERIALIZE) || !hHeap || php->dwSig != HEAPSIG || (DWORD)lpMem < 0x10000) {
        DEBUGMSG(DBGFIXHP, (L"   HeapFree %8.8lx %8.8lx %8.8lx ==> %8.8lx (invalid parameter)\r\n", hHeap,dwFlags,lpMem,0));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
        CELOG_HeapFree(hHeap, dwFlags, (DWORD)lpMem);
    }
    pit = (pitem)((char*)lpMem - sizeof(item));
    if (!((DWORD) pit >> VA_SECTION)) {
        // pit belongs to this heap, make sure it's slotized
        pit = (pitem) ((char *) pit + (DWORD)php - (DWORD)ZeroPtr(php));
    }

    size = pit->size;
    if (size > 0) {
#if HEAP_SENTINELS
        CheckSentinels(pit, TRUE);
#endif
        if (size & 1) {
            if (php == pit->php) {
                // VirtualAlloc'ed item.  Locate the vaitem structure for the block.
                // Remove the item from the heap's list, release the critical section (if serialized)
                // and then free the vitual memory block.
                pvaitem pva = (pvaitem)((char*)lpMem - sizeof(vaitem));
                DWORD   dwData = pva->dwData;

                EnterCriticalSection(&php->cs);
                if (pva->pvaBak == NULL) {
                    // This item is the head of the list.
                    DEBUGCHK(php->pvaList == pva || (pvaitem)ZeroPtr(php->pvaList) == pva);
                    if ((php->pvaList = pva->pvaFwd) != NULL)
                        php->pvaList->pvaBak = NULL;
                } else {
                    // Item in the middle or end of the list.
                    DEBUGCHK(php->pvaList != pva);
                    if ((pva->pvaBak->pvaFwd = pva->pvaFwd) != NULL)
                        pva->pvaFwd->pvaBak = pva->pvaBak;
                }
                LeaveCriticalSection(&php->cs);
                lpMem = (LPVOID)((DWORD)lpMem & -PAGE_SIZE);

                php->pfnFree (lpMem, size, MEM_DECOMMIT, dwData);
                php->pfnFree (lpMem, 0, MEM_RELEASE, dwData);
                bRet = TRUE;
            }

        } else if ((prgn = pit->prgn)->phpOwner == php) {

            EnterCriticalSection (&php->cs);

            pit->size = -size;
#if HEAP_SENTINELS
            // fill the item with 0xcc
            memset (pit+1, 0xcc, size - sizeof(item));
#endif

            MergeFreeItems (prgn, pit);

            if (prgn->cbMaxFree < -pit->size)
                prgn->cbMaxFree = -pit->size;

            prgn->cbMaxFree |= REGION_MAX_IS_ESTIMATE;

            LeaveCriticalSection (&php->cs);
            bRet = TRUE;
        }
    }

#ifdef MEMTRACKING
    if (bRet)
        DeleteTrackedItem(((pheap)hHeap)->dwMemType, lpMem);
#endif
    DEBUGMSG(DBGFIXHP, (L"   HeapFree %8.8lx %8.8lx %8.8lx ==> %8.8lx\r\n",hHeap,dwFlags,lpMem,bRet));
    return bRet;
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
    return ptrHeapSize(hHeap, dwFlags, lpMem);
}
DWORD WINAPI Int_HeapSize(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem)
{
    UINT cbRet;
    pitem pit;
    pheap php;

    if ((dwFlags & ~HEAP_NO_SERIALIZE) || !hHeap || ((pheap)hHeap)->dwSig != HEAPSIG || (DWORD)lpMem < 0x10000) {
        DEBUGMSG(DBGFIXHP, (L"   HeapSize %8.8lx: Invalid parameter\r\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return (DWORD)-1;
    }

    pit = (pitem)((char*)lpMem - sizeof(item));
    php = (pit->size&1) ? pit->php : pit->prgn->phpOwner;
    if (php == (pheap)hHeap && pit->size > 0) {
#if HEAP_SENTINELS
        CheckSentinels(pit, TRUE);
        cbRet = pit->cbTrueSize;
#else
        cbRet = (pit->size & -2) - sizeof(item);
#endif
    } else
        cbRet = (DWORD)-1;

    DEBUGMSG(DBGFIXHP, (L"   HeapSize %8.8lx ==> %8.8lx\r\n", lpMem, cbRet));
    return cbRet;
}

/*
    @doc BOTH EXTERNAL

    @func BOOL   | HeapDestroy | Destroys a heap and releases its memory
    @parm HANDLE | hHeap       | Handle returned from CreateHeap()
*/
BOOL WINAPI HeapDestroy(HANDLE hHeap)
{
    return ptrHeapDestroy(hHeap);
}
BOOL WINAPI Int_HeapDestroy(HANDLE hHeap)
{
    pheap php = (pheap)hHeap;
    pheap *pphp;
    pregion prgn;
    pvaitem pva;
    PFN_FreeHeapMem pfnFree;
    DWORD  dwRgnData;

    if (!hHeap || php->dwSig != HEAPSIG || (php->flOptions & HEAP_IS_PROC_HEAP)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        DEBUGMSG(DBGFIXHP, (L"   HeapDestroy %8.8lx ==> %8.8lx (invalid parameter)\r\n", hHeap, 0));
        return FALSE;
    }
    if (IsCeLogStatus(CELOGSTATUS_ENABLED_GENERAL)) {
        CELOG_HeapDestroy(hHeap);
    }

    // Remove the heap from the list of heaps for the current process.
    EnterCriticalSection(&csHeapList);
    for (pphp = &phpListAll ; *pphp != 0 ; pphp = &(*pphp)->phpNext) {
        if (*pphp == php) {
            *pphp = php->phpNext;
            break;
        }
    }
    LeaveCriticalSection(&csHeapList);

    // Free all VirtualAlloc'ed heap items.
    while ((pva = php->pvaList) != 0)
        HeapFree(hHeap, 0, pva+1);

#ifdef MEMTRACKING
    // If memory tracking is enabled, scan the heap and delete the tracking info.
    prgn = &php->rgn;
    do {
        pitem pit = FIRSTITEM(prgn);
        do {
            if (pit->size > 0) {
                if (pit->prgn == prgn)
                    DeleteTrackedItem(php->dwMemType, pit+1);
                else
                    DEBUGCHK(pit->prgn == 0);
                pit = (pitem)((char*)pit + pit->size);
            } else
                pit = (pitem)((char*)pit - pit->size);
        } while (pit->size);
    } while ((prgn = prgn->prgnNext) != 0);
#endif

    // Free all additional regions added to the heap.
    while ((prgn = php->rgn.prgnNext) != 0)
        DeleteRegion(prgn);

    DeleteCriticalSection(&php->cs);

#ifdef MEMTRACKING
    DeleteTrackedItem(dwHeapMemType, hHeap);
#endif
    pfnFree = php->pfnFree;
    dwRgnData = php->rgn.dwRgnData;
    pfnFree (php, (php->cbMaximum ? php->cbMaximum : CE_FIXED_HEAP_MAXSIZE), MEM_DECOMMIT, dwRgnData);
    pfnFree (php, 0, MEM_RELEASE, dwRgnData);

    DEBUGMSG(DBGFIXHP, (L"   HeapDestroy %8.8lx ==> %8.8lx\r\n", hHeap, 1));
    return TRUE;
}

UINT CompactRegion(pregion prgn)
{
    pitem pit, pitNext;
    pitem pitPrev;
    int cbMaxFree = 0;
    int cbItem;

    DEBUGMSG(DBGHEAP2, (L"   CompactRegion: 0x%08X\r\n", prgn));
    pit = pitPrev = FIRSTITEM(prgn);
    do {
        // Skip over allocated blocks (and holes)
        while ((cbItem = pit->size) > 0) {
            pitPrev = pit;
            pit = (pitem)((char*)pit + cbItem);
        }

        // Scan a possible run of free blocks and merge them.
        pitNext = MergeFreeItems (prgn, pit);
        cbItem = -pit->size;

        // if we're on the last block, free the extra pages
        if ((cbItem >= PAGE_SIZE) && !pitNext->size) {
            // move back the end marker and free the extra pages
            int cbHead = PAGE_SIZE - ((DWORD)pit & PageMask) - sizeof (item);
            int cbEx = ((cbItem - cbHead) & ~PageMask);
            pitem pitEnd = (pitem)((char*)pit + cbHead); // (pit) = ptr to tail
            DEBUGCHK (cbEx > 0);
            prgn->pitLast = pit;
            pit->size = -cbHead;
            pitEnd->size = 0;   // NOTE: this assignment must come after chaning pit->size because they might overlap
            pitEnd->cbTail = pitNext->cbTail + cbEx;
#if HEAP_SENTINELS
            pitEnd->dwSig = TAILSIG;
#endif
            DEBUGCHK (((DWORD)(pitEnd+1) & PageMask) == 0);
            DEBUGMSG(DBGHEAP2, (L"     CompactRegion: Combining tail item to make free pages 0x%08X [%06x]\r\n", pit, cbHead));
            prgn->phpOwner->pfnFree (pitEnd+1, cbEx, MEM_DECOMMIT, prgn->dwRgnData);
            cbItem = cbHead;
            pitNext = pitEnd;
        }

        if (cbItem > cbMaxFree)
            cbMaxFree = cbItem;

        pit = pitNext;
    } while (pit->size);
    prgn->cbMaxFree = cbMaxFree;
    DEBUGMSG(DBGHEAP2, (L"   CompactRegion: 0x%08X   MaxFree=0x%07X  pitFree=0x%08X\r\n", prgn, cbMaxFree, prgn->pitFree));
    return cbMaxFree;
}


UINT WINAPI HeapCompact(HANDLE hHeap, DWORD dwFlags)
{
    pheap php;
    pregion prgn;
    INT cbMaxFree;

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

    php = (pheap)hHeap;

    EnterCriticalSection(&php->cs);
    cbMaxFree = 0;
    for (prgn = &php->rgn ; prgn ; prgn = prgn->prgnNext) {
        int cbFree = CompactRegion(prgn);
        if (cbFree > cbMaxFree)
            cbMaxFree = cbFree;
    }
    LeaveCriticalSection(&php->cs);
    DEBUGMSG(DBGFIXHP, (L"   HeapCompact %8.8lx %8.8lx ==> %8.8lx\r\n", hHeap, dwFlags, cbMaxFree));
    return cbMaxFree;
}


void CompactAllHeaps(void)
{
    pheap php;

    DEBUGMSG(1, (L"CompactAllHeaps: starting\r\n"));
    if (!fFullyCompacted) {
        fFullyCompacted = TRUE;
        EnterCriticalSection(&csHeapList);
        for (php = phpListAll ; php ; php = php->phpNext)
            HeapCompact((HANDLE)php, 0);
        LeaveCriticalSection(&csHeapList);
    }
    DEBUGMSG(1, (L"CompactAllHeaps: done\r\n"));
}

/*
    @doc BOTH EXTERNAL

    @func HANDLE   | GetProcessHeap | Obtains a handle to the heap of the
    calling process
*/
HANDLE WINAPI GetProcessHeap(VOID)
{
    return hProcessHeap;
}


BOOL WINAPI HeapValidate(HANDLE hHeap, DWORD dwFlags, LPCVOID lpMem)
{
    pitem pitSeek, pit;
    pheap php = (pheap)hHeap;
    pregion prgn;
    BOOL bRet = FALSE;
    int cbFree;

    if ((dwFlags & ~HEAP_NO_SERIALIZE) || !hHeap || php->dwSig != HEAPSIG
            || (lpMem && lpMem < (LPCVOID)0x10000)) {
        DEBUGMSG(DBGFIXHP, (L"   HeapValidate %8.8lx: Invalid parameter\r\n", hHeap));
        return FALSE;
    }

    try {
        if (lpMem) {
            if (((DWORD)hHeap>>SECTION_SHIFT) != ((DWORD)lpMem>>SECTION_SHIFT)) {
                // The ptr has been unmapped by someone or it doesn't belong to this heap.
                if ((DWORD)lpMem>>SECTION_SHIFT) {
                    DEBUGMSG(DBGFIXHP, (L"   HeapValidate %8.8lx %8.8lx %8.8lx ==> %8.8lx\r\n", hHeap,dwFlags,lpMem,0));
                    return FALSE;
                }
                // Restore the "slot mapping" bits to the pointer.
                lpMem = (LPVOID)((DWORD)lpMem | ((DWORD)hHeap & -(1<<SECTION_SHIFT)));
            }
            pitSeek = (pitem)((char*)lpMem - sizeof(item));
            if (pitSeek->size > 0) {
#if HEAP_SENTINELS
                CheckSentinels(pitSeek, TRUE);
#endif
                if (pitSeek->size & 1) {
                    // VirtualAlloc'ed item.  Validate that the item pointer falls correctly within a
                    // 64K aligned virtual memory block and that the heap owner information in the
                    // item is correct.
                    pvaitem pva;

                    pva = (pvaitem)(((DWORD)lpMem&0xFFFF0000) + (ALIGNSIZE(sizeof(vaitem)) - sizeof(vaitem)));
                    if (pitSeek != &pva->it || pitSeek->php != php)
                        goto done;
                    // Make sure that the true size of the block is a multiple of a page.
                    if (((pitSeek->size-1 + ALIGNSIZE(sizeof(vaitem)) - sizeof(item)) & PageMask) != 0)
                        goto done;
                    bRet = TRUE;
                    goto done;

                } else {
                    // Ordinary item.  Validate that the region pointer points at a region
                    // that is part of the given heap and that the size of the block is a
                    // multiple of the heap alignment value.
                    prgn = pitSeek->prgn;
                    if (prgn->phpOwner != php || (pitSeek->size & ALIGNBYTES-1) != 0)
                        goto done;
                }
            }

        } else {
            pitSeek = NULL;
            prgn = &php->rgn;
        }

        // Walk the regions of the heap validating the linkages.  If the validation is for
        // a specific block then only the region containing that block will be scanned.

        EnterCriticalSection(&php->cs);

        do {
            pit = FIRSTITEM(prgn);
            cbFree = 0;
            do {
                if (pit->size > 0) {
                    // Allocated block or hole.  Check if this is the item we're looking for and
                    // quit if so.  Otherwise, validate the block and continue scanning.
                    if (pit == pitSeek) {
                        bRet = TRUE;
                        goto done;
                    }
                    if (pit->size & ALIGNBYTES-1) {
                        // Bogus block size.  Quit scanning now.
                        DEBUGMSG(1, (L"  HeapValidate: %08x busy item=%08x invalid block size: %08x\r\n",
                                php, pit, pit->size));
                        goto done;
                    }
                    if (pit->prgn && pit->prgn != prgn) {
                        // Invalid region pointer.  Quit scanning now.
                        DEBUGMSG(1, (L"  HeapValidate: %08x busy item=%08x invalid region: %08x SHDB %08x\r\n",
                                php, pit, pit->prgn, prgn));
                        goto done;
                    }
#if HEAP_SENTINELS
                    if (!CheckSentinels(pit, FALSE))
                        goto done;
#endif
                    // Advance to the next item.
                    pit = (pitem)((char*)pit + pit->size);

                } else if (pit->size < 0) {
                    // Free item.  Validate the item size.
                    if (pit->size & ALIGNBYTES-1) {
                        // Bogus block size.  Quit scanning now.
                        DEBUGMSG(1, (L"  HeapValidate: %08x free item=%08x invalid block size: %08x\r\n",
                                php, pit, -pit->size));
                        goto done;
                    }
                    if (pit->prgn != prgn) {
                        // Invalid region pointer.  Quit scanning now.
                        DEBUGMSG(1, (L"  HeapValidate: %08x free item=%08x invalid region: %08x SHDB %08x\r\n",
                                php, pit, pit->prgn, prgn));
                        goto done;
                    }
#if HEAP_SENTINELS
                    if (!CheckFreeSentinels(pit, FALSE))
                        goto done;
#endif
                    // Advance to the next item.
                    cbFree -= pit->size;
                    pit = (pitem)((char*)pit - pit->size);
                }
            } while (pit->size != 0);

            if (pitSeek) {
                // The item wasn't found in a scan of the heap region.
                DEBUGMSG(1, (L"  HeapValidate: %08x  item=%08x not found!\r\n", php, pitSeek));
                goto done;
            }
        }while ((prgn = prgn->prgnNext) != 0);

        bRet = TRUE;
done:  ;
    } except (EXCEPTION_EXECUTE_HANDLER) {
        bRet = FALSE;
    }

    LeaveCriticalSection(&php->cs);

#ifdef DEBUG
    if (!bRet) {
        _HeapDump(hHeap);
        DEBUGCHK(0);
    }
#endif
    DEBUGMSG(DBGFIXHP, (L"   HeapValidate %8.8lx %8.8lx %8.8lx ==> %8.8lx\r\n", hHeap,dwFlags,lpMem,1));
    return bRet;
}

#ifdef DEBUG

UINT WINAPI _HeapDump(HLOCAL hHeap) {
    UINT retval = 1;
    pheap  php = (pheap) hHeap;
    pregion prgn;
    pitem pit;
    pvaitem pva;

    try {
        if (php->dwSig != HEAPSIG) {
            DEBUGMSG(1, (TEXT("_HeapDump: %8.8lx is not a valid heap! Signature=%8.8lx\r\n"), php, php->dwSig));
            goto done;
        }
        EnterCriticalSection(&php->cs);
        DEBUGMSG(1, (TEXT("_HeapDump: %8.8lx\r\n"), php));
        DEBUGMSG(1, (TEXT(" flOptions: 0x%08X   cbMaximum: 0x%08X\r\n"), php->flOptions, php->cbMaximum));

        // Walk the region list dumping all of the items in the regions.
        prgn = &php->rgn;
        do {
            if (prgn->phpOwner != php) {
                DEBUGMSG(1, (TEXT("_HeapDump: region %8.8lx has incorrect owner (%.8.8lx)\r\n"), prgn, prgn->phpOwner));
                goto done;
            }
            DEBUGMSG(1, (TEXT("   Region: 0x%08X     cbMaxFree:     0x%08X\r\n"), prgn, prgn->cbMaxFree));
            DEBUGMSG(1, (TEXT("    pitFree: 0x%08X   pitLast:       0x%08X\r\n"), prgn->pitFree, prgn->pitLast));
            pit = FIRSTITEM(prgn);
            do {
                if (pit->size > 0) {
                    if (pit->prgn == NULL) {
                        // This is a hole.
                        DEBUGMSG(1, (TEXT("       Hole: 0x%08X size: 0x%08X\r\n"), pit, pit->size));
#if HEAP_SENTINELS
                        if (pit->dwSig != HOLESIG) {
                            DEBUGMSG(1, (TEXT("Invalid hole signature: 0x%08X SHDB 0x%08X\r\n"), pit->dwSig, HOLESIG));
                            goto done;
                        }
#endif
                    } else {
                        // This is an allocated item.
                        DEBUGMSG(1, (TEXT("       Item: 0x%08X size: 0x%08X\r\n"), pit, pit->size));
#if HEAP_SENTINELS
                        if (!CheckSentinels(pit, FALSE))
                            goto done;
#endif
                    }
                    pit = (pitem)((char*)pit + pit->size);

                } else {
                    // This is a free item.
                    DEBUGMSG(1, (TEXT("       Free: 0x%08X size: 0x%08X\r\n"), pit, -pit->size));
#if HEAP_SENTINELS
                    if (!CheckFreeSentinels(pit, FALSE))
                        goto done;
#endif
                    pit = (pitem)((char*)pit - pit->size);
                }
            } while (pit->size != 0);

            DEBUGMSG(1, (TEXT("       End:  0x%08X size: 0x%08X\r\n"), pit, pit->cbTail));
#if HEAP_SENTINELS
            if (pit->dwSig != TAILSIG) {
                DEBUGMSG(1, (TEXT("Invalid endmarker signature: 0x%08X SHDB 0x%08X\r\n"), pit->dwSig, TAILSIG));
                goto done;
            }
#endif

        } while ((prgn = prgn->prgnNext) != NULL);

        // Dump the VirtualAlloc'ed items.
        for (pva = php->pvaList ; pva ; pva = pva->pvaFwd) {
            DEBUGMSG(1, (TEXT("       VItem: 0x%08X size: 0x%08X\r\n"), pva, pva->it.size));
#if HEAP_SENTINELS
            if (!CheckSentinels(&pva->it, FALSE))
                goto done;
#endif
        }
        DEBUGMSG(1, (TEXT("-----------------------------------------------------\r\n")));
        retval = 0;
done:
        ;
    } except (EXCEPTION_EXECUTE_HANDLER) {
        retval = 1;
    }
    LeaveCriticalSection(&php->cs);
    return retval;
}

#endif

BOOL FillInOneHeap(THSNAP *pSnap, pheap php, TH32HEAPENTRY **ppNext, DWORD dwProcessId)
{
    pregion prgn;
    pitem pit;
    pvaitem pva;

    EnterCriticalSection(&php->cs);

    // Walk the region list dumping all of the items in the regions.
    prgn = &php->rgn;
    do {
        DEBUGCHK(prgn->phpOwner == php);
        pit = FIRSTITEM(prgn);
        do {
            if (!(*ppNext = (TH32HEAPENTRY *)THGrow(pSnap,sizeof(TH32HEAPENTRY)))) {
                LeaveCriticalSection(&php->cs);
                return FALSE;
            }
            (*ppNext)->heapentry.dwSize = sizeof(TH32HEAPENTRY);
            (*ppNext)->heapentry.hHandle = (HANDLE)(pit+1);
            (*ppNext)->heapentry.dwAddress = (DWORD)(pit+1);
            (*ppNext)->heapentry.dwResvd = 0;
            (*ppNext)->heapentry.th32ProcessID = dwProcessId;
            (*ppNext)->heapentry.th32HeapID = (DWORD)php;
            if (pit->size > 0) {
                (*ppNext)->heapentry.dwBlockSize = pit->size;
                if (pit->prgn == NULL) {
                    // This is a hole.
                    (*ppNext)->heapentry.dwFlags = LF32_DECOMMIT;
                    (*ppNext)->heapentry.dwLockCount = 0;
#if HEAP_SENTINELS
                    if (pit->dwSig != HOLESIG) {
                        DEBUGCHK(0);
                        break;
                    }
#endif
                } else {
                    // This is an allocated item.
                    (*ppNext)->heapentry.dwFlags = LF32_FIXED;
                    (*ppNext)->heapentry.dwLockCount = 1;
#if HEAP_SENTINELS
                    if (!CheckSentinels(pit, TRUE))
                        break;
#endif
                }
                pit = (pitem)((char*)pit + pit->size);

            } else {
                // This is a free item.
                (*ppNext)->heapentry.dwBlockSize = -pit->size;
                (*ppNext)->heapentry.dwFlags = LF32_FREE;
                (*ppNext)->heapentry.dwLockCount = 0;
#if HEAP_SENTINELS
                if (!CheckFreeSentinels(pit, TRUE))
                    break;
#endif
                pit = (pitem)((char*)pit - pit->size);
            }
            ppNext = &(*ppNext)->pNext;
        } while (pit->size != 0);

#if HEAP_SENTINELS
        if (pit->dwSig != TAILSIG) {
            DEBUGCHK(0);
            break;
        }
#endif
    } while ((prgn = prgn->prgnNext) != NULL);

    // Walk the list of VirtualAlloc'ed items.
    for (pva = php->pvaList ; pva ; pva = pva->pvaFwd) {
#if HEAP_SENTINELS
        if (!CheckSentinels(&pva->it, FALSE))
            break;
#endif
        if (!(*ppNext = (TH32HEAPENTRY *)THGrow(pSnap,sizeof(TH32HEAPENTRY)))) {
            LeaveCriticalSection(&php->cs);
            return FALSE;
        }
        (*ppNext)->heapentry.dwSize = sizeof(TH32HEAPENTRY);
        (*ppNext)->heapentry.hHandle = (HANDLE)(&pva->it + 1);
        (*ppNext)->heapentry.dwAddress = (DWORD)(&pva->it + 1);
        (*ppNext)->heapentry.dwBlockSize = pva->it.size - 1;
        (*ppNext)->heapentry.dwFlags = LF32_FIXED | LF32_BIGBLOCK;
        (*ppNext)->heapentry.dwLockCount = 1;
        (*ppNext)->heapentry.dwResvd = 0;
        (*ppNext)->heapentry.th32ProcessID = dwProcessId;
        (*ppNext)->heapentry.th32HeapID = (DWORD)php;
        ppNext = &(*ppNext)->pNext;
    }
    *ppNext = 0;
    LeaveCriticalSection(&php->cs);
    return TRUE;
}


// GetHeapSnapShot is now a direct call from Kerenl.
// Permission should've been set by Kernel before making the call.
BOOL GetHeapSnapshot(THSNAP *pSnap, BOOL bMainOnly, LPVOID *pDataPtr, HANDLE hProc)
{
    pheap php, phpList;
    BOOL bRet = TRUE;
    LPCRITICAL_SECTION pcsHeapList;

    if (!(pcsHeapList = (LPCRITICAL_SECTION) MapPtrUnsecure (&csHeapList, hProc))) {
        return FALSE;
    }

    EnterCriticalSection (pcsHeapList);
    if (bMainOnly) {
        if (!(php = *(pheap *) MapPtrUnsecure (&hProcessHeap, hProc))) {
            bRet = FALSE;
        } else {
            bRet = FillInOneHeap(pSnap, php, (TH32HEAPENTRY **)pDataPtr, (DWORD) hProc);
        }

    } else {
        TH32HEAPLIST **ppNext;

        if (!(phpList = *(pheap *) MapPtrUnsecure (&phpListAll, hProc)))
            bRet = FALSE;

        ppNext = (TH32HEAPLIST **)pDataPtr;
        for (php = phpList ; php && bRet ; php = php->phpNext) {
            if (!(*ppNext = (TH32HEAPLIST *)THGrow(pSnap,sizeof(TH32HEAPLIST))))
                bRet = FALSE;
            else {
                (*ppNext)->heaplist.dwSize = sizeof(HEAPLIST32);
                (*ppNext)->heaplist.th32ProcessID = (DWORD) hProc;
                (*ppNext)->heaplist.th32HeapID = (DWORD)php;
                (*ppNext)->heaplist.dwFlags = (php->flOptions & HEAP_IS_PROC_HEAP) ? HF32_DEFAULT : 0;
                if (!FillInOneHeap(pSnap, php, &(*ppNext)->pHeapEntry, (DWORD) hProc)) {
                    bRet = FALSE;
                    break;
                }
                ppNext = &(*ppNext)->pNext;
            }
        }
        *ppNext = 0;
    }
    LeaveCriticalSection (pcsHeapList);
    return bRet;
}

