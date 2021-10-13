//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#ifndef _DLLHEAP_H
#define _DLLHEAP_H

typedef struct _MODULEHEAPINFO
{
        // heap globals (populated by coredll on init)
        HANDLE hProcessHeap;
        LPCRITICAL_SECTION lpcsHeapList;
        HANDLE* pphpListAll;
        DWORD dwNextHeapOffset;
        UINT     nDllHeapModules; // number of dlls in this process which are using custom dll heap

        // module heap filled in by dll heap implementation
        HANDLE hModuleHeap;
        
        // get process heap override
        PFNVOID pfnGetProcessHeap;

        // LocalAllocxxx function overrides
        PFNVOID pfnLocalAlloc;
        PFNVOID pfnLocalFree;
        PFNVOID pfnLocalAllocTrace;
        PFNVOID pfnLocalReAlloc;
        PFNVOID pfnLocalSize;

        // string function overrides
        PFNVOID pfnStrDup;
        PFNVOID pfnWcsDup;

        // crt function overrides
        PFNVOID pfnMalloc;
        PFNVOID pfnFree;
        PFNVOID pfnNew;
        PFNVOID pfnNew2;
        PFNVOID pfnDelete;
        PFNVOID pfnDelete2;
        PFNVOID pfnCalloc;
        PFNVOID pfnRealloc; 
        PFNVOID pfnmsize;
        PFNVOID pfnRecalloc;

        // Custom heap function overrides when HEAP_SENTINEL is set
        PFNVOID pfnHeapReAlloc;
        PFNVOID pfnHeapAlloc;
        PFNVOID pfnHeapAllocTrace;
        PFNVOID pfnHeapFree;
}MODULEHEAPINFO, *PMODULEHEAPINFO;

#endif // _DLLHEAP_H

