//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++/*++


 Module Name:    mem.h

 Abstract:       Memory allocation and tracking for IrDA.

 Contents:

--*/

#ifndef _MEM_H_
#define _MEM_H_

//
// Memory tracking ids for different objects.
//

#define MT_NOT_SPECIFIED        0x00000000
#define MT_TDI_ADDRESS          0x00010000
#define MT_TDI_CONNECTION       0x00010001
#define MT_TDI_RECV_BUFF        0x00010002
#define MT_TDI_MESSAGE          0x00010004
#define MT_WSH_SOCKET_CTXT      0x00020000

#ifdef MEMTRACKING

// Variable initialized in DllEntry.
extern DWORD IrdaMemTypeId;
extern HANDLE g_hIrdaHeap;

// Callback for mem tracking.
__inline VOID
IrdaTrackerCallback(
    DWORD  dwFlags,
    DWORD  dwMemTypeId,
    HANDLE hItem,
    DWORD  Proc,
    BOOL   fDeleted,
    DWORD  dwSize,
    DWORD  dw1,
    DWORD  dw2
    )
{
    RETAILMSG(1, (TEXT("%08lx:%08lx "), dw1, dw2));

    if (fDeleted == TRUE)
    {
        RETAILMSG(1, (TEXT("Freed.\r\n")));
    }
    else
    {
        RETAILMSG(1, (TEXT("\r\n")));
    }
    return;
}

__inline void *
IrdaTrackedAlloc(
    DWORD dwId,
    size_t stAllocateBlock
    )
{
    void *ptr;

    ptr = HeapAlloc(g_hIrdaHeap, HEAP_ZERO_MEMORY, stAllocateBlock);

    if (ptr != NULL)
    {
        AddTrackedItem(
            IrdaMemTypeId,
            ptr,
            IrdaTrackerCallback,
            GetCurrentProcessId(),
            stAllocateBlock,
            HIWORD(dwId),
            LOWORD(dwId)
            );
    }
    return (ptr);
}

__inline void *
IrdaTrackedFree(
    void *ptr
    )
{
    void *ptrTemp;

    ptrTemp = HeapFree(g_hIrdaHeap, 0, ptr);

    if (ptrTemp == NULL)
    {
        DeleteTrackedItem(
            IrdaMemTypeId,
            ptr
            );
    }

    return (ptrTemp);
}

__inline void
IrdaAddTrackedItem(
    DWORD dwId,
    LPVOID lpPtr,
    size_t cbPtr
    )
{
    AddTrackedItem(
        IrdaMemTypeId,
        lpPtr,
        IrdaTrackerCallback,
        GetCurrentProcessId(),
        cbPtr,
        HIWORD(dwId),
        LOWORD(dwId)
        );
}

__inline void
IrdaDelTrackedItem(
    LPVOID lpPtr
    )
{
    DeleteTrackedItem(IrdaMemTypeId, lpPtr);
}

#define IrdaAlloc(_size, _dwId) IrdaTrackedAlloc(_dwId, _size)
#define IrdaFree(_ptr)          IrdaTrackedFree(_ptr)

#else // MEMTRACKING

#define IrdaAlloc(_size, _dwId) HeapAlloc(g_hIrdaHeap, HEAP_ZERO_MEMORY, _size)
#define IrdaFree(_ptr)          HeapFree(g_hIrdaHeap, 0, _ptr)

#define IrdaAddTrackedItem(_dwId, _ptr, _size) ((void)0)
#define IrdaDelTrackedItem(_ptr)               ((void)0)

#endif // !MEMTRACKING

#endif // _MEM_H_
