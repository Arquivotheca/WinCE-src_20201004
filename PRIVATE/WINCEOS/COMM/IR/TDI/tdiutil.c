//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


 Module Name:    tdiutil.c

 Abstract:       Utility for converting connection and address object handles
                 into pointers to the objects with added reference count to
                 ensure that object doesn't disappear while in use.

 Contents:
 
    IrdaAcquireAddrObj
    IrdaAcquireConnObj

--*/

#include "irdatdi.h"

#ifdef DEBUG
    static DWORD dwAddrId = 1;
#endif 

PIRLMP_ADDR_OBJ
IrdaAllocAddrObj(
    OUT PHANDLE phAddr
    )
{
    PIRLMP_ADDR_OBJ pAddr;

    EnterCriticalSection(&csIrObjList);

    pAddr = IrdaAlloc(sizeof(IRLMP_ADDR_OBJ), MT_TDI_ADDRESS);

    if (pAddr)
    {
        pAddr->cRefs = 1;
    #ifdef DEBUG
        pAddr->Sig  = TDIADDRSIG;
        pAddr->dwId = dwAddrId++;
    #endif // DEBUG
    
        DEBUGMSG(ZONE_ALLOC,
            (TEXT("IrDA: Allocate Address - %#x (%d)\r\n"), pAddr, pAddr->dwId));
        
        // Put address in address list.
        InsertHeadList(&IrAddrObjList, (PLIST_ENTRY)pAddr);
    
        // For now the handle is just the pointer.
        *phAddr = (HANDLE)pAddr;
    }

    LeaveCriticalSection(&csIrObjList);
    
    return (pAddr);
}

/*++                      

 Function:       IrdaAcquireAddrObj

 Description:    Converts address object handle to address object pointer.

 Arguments:
 
    hAddr - Handle to address object.

 Returns:
 
    pAddr - Pointer to address object. NULL if invalid handle.

 Comments:
 
    For now the handle is the pointer, but we verify that we can find the 
    object in the address list.
    
    Caller must call REFDEL to free reference acquired by this function.

--*/

PIRLMP_ADDR_OBJ   
IrdaAcquireAddrObj(
    IN HANDLE hAddr
    )
{
    PIRLMP_ADDR_OBJ pAddr = NULL;
    PLIST_ENTRY     pEntry; 

    EnterCriticalSection(&csIrObjList);

    for (pEntry = IrAddrObjList.Flink;
         pEntry != &IrAddrObjList;
         pEntry = pEntry->Flink)
    {
        if ((PIRLMP_ADDR_OBJ)pEntry == (PIRLMP_ADDR_OBJ)hAddr)
        {
            pAddr = (PIRLMP_ADDR_OBJ)pEntry;
            REFADDADDR(pAddr);
            break;
        }
    }

    LeaveCriticalSection(&csIrObjList);

    return (pAddr);
}

/*++                      

 Function:       IrdaAcquireConnObj

 Description:    Converts connection object handle to connection object pointer.

 Arguments:
 
    hConn - Handle to connection object.

 Returns:
 
    pConn - Pointer to connection object. NULL if invalid handle.

 Comments:
 
    For now the handle is the pointer, but we verify that we can find the 
    object associated with an address or in the unassociated connection
    list.
    
    Caller must call REFDEL to free reference acquired by this function.

--*/

PIRLMP_CONNECTION 
IrdaAcquireConnObj(
    IN HANDLE hConn
    )
{
    PIRLMP_CONNECTION pConn = NULL;
    PIRLMP_ADDR_OBJ   pAddr = NULL;
    PLIST_ENTRY       pAddrEntry;
    PLIST_ENTRY       pConnEntry;

    EnterCriticalSection(&csIrObjList);

    for (pAddrEntry = IrAddrObjList.Flink;
         pAddrEntry != &IrAddrObjList;
         pAddrEntry = pAddrEntry->Flink)
    {
        pAddr = (PIRLMP_ADDR_OBJ)pAddrEntry;

        GET_ADDR_LOCK(pAddr);

        for (pConnEntry = pAddr->ConnList.Flink;
             pConnEntry != &pAddr->ConnList;
             pConnEntry = pConnEntry->Flink)
        {
            if ((PIRLMP_CONNECTION)pConnEntry == (PIRLMP_CONNECTION)hConn)
            {
                pConn = (PIRLMP_CONNECTION)pConnEntry;
                REFADDCONN(pConn);
                break;
            }
        }

        FREE_ADDR_LOCK(pAddr);

        if (pConn) break;
    }

    // If not associated with an address, then it must be on the 
    // unassociated connection list.
    if (pConn == NULL)
    {
        for (pConnEntry = IrConnObjList.Flink;
             pConnEntry != &IrConnObjList;
             pConnEntry = pConnEntry->Flink)
        {
            if ((PIRLMP_CONNECTION)pConnEntry == (PIRLMP_CONNECTION)hConn)
            {
                pConn = (PIRLMP_CONNECTION)pConnEntry;
                REFADDCONN(pConn);
                break;
            }
        }
    }

    LeaveCriticalSection(&csIrObjList);

    return (pConn);
}


