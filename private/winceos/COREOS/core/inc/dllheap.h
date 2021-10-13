//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

#ifndef _DLLHEAP_H
#define _DLLHEAP_H

typedef struct _MODULEHEAPINFO
{
    DWORD              dwHeapSentinels; // size of heap sentinels
    HANDLE             hProcessHeap;
    LPCRITICAL_SECTION lpcsHeapList;
    HANDLE*            pphpListAll;
    DWORD              dwNextHeapOffset;
    UINT               nDllHeapModules; // number of dlls in this process which are using custom dll heap

    // module heap filled in by dll heap implementation
    HANDLE             hModuleHeap;
    
    // get process heap override
    PFNVOID            pfnGetProcessHeap;

    // Process heap function overrides
    PFNVOID            pfnLocalAlloc;
    PFNVOID            pfnLocalFree;
    PFNVOID            pfnLocalAllocTrace;
    PFNVOID            pfnLocalReAlloc;
    PFNVOID            pfnLocalSize;

    // Custom heap function overrides
    PFNVOID            pfnHeapReAlloc;
    PFNVOID            pfnHeapAlloc;
    PFNVOID            pfnHeapAllocTrace;
    PFNVOID            pfnHeapFree;
    
    // string function overrides
    PFNVOID            pfnStrDup;
    PFNVOID            pfnWcsDup;

    // crt function overrides
    PFNVOID            pfnMalloc;
    PFNVOID            pfnFree;
    PFNVOID            pfnNew;
    PFNVOID            pfnNew2;
    PFNVOID            pfnDelete;
    PFNVOID            pfnDelete2;
    PFNVOID            pfnCalloc;
    PFNVOID            pfnRealloc; 
    PFNVOID            pfnmsize;
    PFNVOID            pfnRecalloc;
}MODULEHEAPINFO, *PMODULEHEAPINFO;


#endif // _DLLHEAP_H

