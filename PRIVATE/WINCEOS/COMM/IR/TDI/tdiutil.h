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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
/*++


 Module Name:    tdiutil.h

 Abstract:       Inline allocation and free utils for address and connection
                 objects.

 Contents:
 
    IrdaAllocAddrObj
    IrdaAllocConnObj
    IrdaFreeAddrObj
    IrdaFreeConnObj

--*/

#ifndef _TDI_UTIL_H_
#define _TDI_UTIL_H_

#ifdef DEBUG
#define TDIADDRSIG       0xfeedaaaa
#define TDICONNSIG       0xfeedeeee
#define VALIDADDR(pAddr) ASSERT(pAddr && pAddr->Sig == TDIADDRSIG);
#define VALIDCONN(pConn) ASSERT(pConn && pConn->Sig == TDICONNSIG);
#else // DEBUG
#define VALIDADDR(pAddr) ((void)0)
#define VALIDCONN(pConn) ((void)0)
#endif // !DEBUG

#define REFADDADDR(pAddr) AddressAddReference(pAddr)
#define REFDELADDR(pAddr) AddressRemoveReference(pAddr)
#define REFADDCONN(pConn) ConnectionAddReference(pConn)
#define REFDELCONN(pConn) ConnectionRemoveReference(pConn)

#define ALLOCADDR         IrdaAllocAddrObj
#define GETADDR           IrdaAcquireAddrObj
#define GETCONN           IrdaAcquireConnObj

PIRLMP_ADDR_OBJ   IrdaAllocAddrObj(OUT PHANDLE phAddr);
PIRLMP_ADDR_OBJ   IrdaAcquireAddrObj(IN HANDLE hAddr);
PIRLMP_CONNECTION IrdaAcquireConnObj(IN HANDLE hConn);

//
// Allocs have initialized with signature and reference count = 1.
//

// Don't need critical section since this will only be called from
// AddressRemoveReference.
__inline VOID
IrdaFreeAddrObj(PIRLMP_ADDR_OBJ pAddr)
{
    VALIDADDR(pAddr);

    // Remove the address from the list.
    RemoveEntryList((PLIST_ENTRY)pAddr);

    // Shouldn't be deleted if there are connections, since a connection has
    // a reference to the address object.
    ASSERT(pAddr->ConnList.Flink == &pAddr->ConnList);

    DEBUGMSG(ZONE_ALLOC,
        (TEXT("IrDA: Free Address - %#x (%d)\r\n"), pAddr, pAddr->dwId));
    
    CTEDeleteLock(&pAddr->Lock);
    IrdaFree(pAddr);
}

__inline VOID
AddressRemoveReference(PIRLMP_ADDR_OBJ pAddr)
{
    EnterCriticalSection(&csIrObjList);
    VALIDADDR(pAddr);
    if (--pAddr->cRefs == 0)
    {
        IrdaFreeAddrObj(pAddr);
    }
    LeaveCriticalSection(&csIrObjList);
}

// Don't need critical section since this will only be called from
// ConnectionRemoveReference.
__inline VOID
IrdaFreeConnObj(PIRLMP_CONNECTION pConn)
{
    VALIDCONN(pConn);

    // Remove connection from the list.
    RemoveEntryList((PLIST_ENTRY)pConn);

    // We should be disassociated already.
    if (pConn->pAddrObj) {
        PIRLMP_ADDR_OBJ pAddrObj = GETADDR(pConn->pAddrObj);
        ASSERT(pAddrObj);
        REFDELADDR(pAddrObj); // One for GETADDR
        REFDELADDR(pAddrObj); // One for Conn
        REFDELADDR(pAddrObj); // One for TDI
    }

    // Things should have been cleaned up before calling REFDEL.
    ASSERT(IsListEmpty(&pConn->RecvBuffList) == TRUE);
    ASSERT(pConn->pUsrNDISBuff == NULL);

    DEBUGMSG(ZONE_ALLOC,
        (TEXT("IrDA: Free Connection - %#x (%d)\r\n"), pConn, pConn->dwId));

    CTEStopTimer(&pConn->LingerTimer);
    CTEDeleteLock(&pConn->Lock);
    IrdaFree(pConn);
}

__inline VOID
ConnectionAddReference(PIRLMP_CONNECTION pConn)
{
    EnterCriticalSection(&csIrObjList);
    VALIDCONN(pConn);
    pConn->cRefs++;
    LeaveCriticalSection(&csIrObjList);
}

__inline VOID
ConnectionRemoveReference(PIRLMP_CONNECTION pConn)
{
    EnterCriticalSection(&csIrObjList);
    VALIDCONN(pConn);
    if (--pConn->cRefs == 0)
    {
        IrdaFreeConnObj(pConn);
    }
    LeaveCriticalSection(&csIrObjList);
}

__inline VOID
AddressAddReference(PIRLMP_ADDR_OBJ pAddr)
{
    EnterCriticalSection(&csIrObjList);
    VALIDADDR(pAddr);
    pAddr->cRefs++;
    LeaveCriticalSection(&csIrObjList);
}

#endif // _TDI_UTIL_H_
