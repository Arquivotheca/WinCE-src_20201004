//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*****************************************************************************
*
* 
*
*       @doc
*       @module irdatdi.c | IrDA stack interface to TDI
*
*               11/11/1997
*               - Modified for new irdastk. New locking stategy. 
*                         Everything else is same.
*
*               3/16/1998
*               - Modified to used handles instead of pointers with
*                         AFD (AFD can call in to close connections at any
*                         time. We need to make sure that we ref count and
*                         not to delete any connection objects in use.)
*
*       see AFD\peginit.c
*
*       Locking Strategy:
*           Lock TDI connection objects while using.
*           Lock TDI address objects while using.
*           (Lock connection objects before address objects).
*           Don't call into IrLMP with locks held.
*           IrLmp locks the LMP link control blocks.
*
*           There is a specific lock for discoveries.
*           There is a specific lock for IAS queries.
*           There is a specific lock for manipulating object lists.
*
*/

#include "irdatdi.h"

#ifdef DEBUG

TCHAR *szIrlmpConnState[] = {
    TEXT("CONN_CREATED"),
    TEXT("CONN_CLOSING"),
    TEXT("CONN_OPENING"),
    TEXT("CONN_OPEN")
};

#define DUMP_RECVLIST(debugzone, pConn) if (debugzone) DumpRecvList(pConn)

void
DumpRecvList(PIRLMP_CONNECTION pConn)
{
    PIRDATDI_RECV_BUFF pRecv;
    PLIST_ENTRY        pRecvEntry;

    if (IsListEmpty(&pConn->RecvBuffList) == TRUE)
    {
        DEBUGMSG(1, (TEXT("*** IrTdi: No recv buffers are queued.\r\n")));
        return;
    }

    DEBUGMSG(1, (TEXT("*** IrTdi: Queued recv buffers:\r\n")));
    
    for (pRecvEntry = pConn->RecvBuffList.Flink;
         pRecvEntry != &pConn->RecvBuffList;
         pRecvEntry = pRecvEntry->Flink)
    {
        pRecv = (PIRDATDI_RECV_BUFF)pRecvEntry;

        DEBUGMSG(1,
            (TEXT("    %d B [%#x] %s\r\n"),
             pRecv->DataLen,
             pRecv->pRead,
             pRecv->FinalSeg == TRUE ? TEXT("F") : TEXT("")));
    }

    return;
}

#define DUMP_OBJECTS(debugzone) if (debugzone) DumpObjects()

void
DumpObjects()
{
    PIRLMP_ADDR_OBJ   pAddr;
    PIRLMP_CONNECTION pConn;
    PLIST_ENTRY       pAddrEntry, pConnEntry;

    EnterCriticalSection(&csIrObjList);

    DEBUGMSG(1,
        (TEXT("*** Dump Address and Connection Objects: %s\r\n"),
         (!IsListEmpty(&IrAddrObjList) && !IsListEmpty(&IrConnObjList)) ? 
          TEXT("NO ADDR or CONN objects.") : TEXT("")));
    
    for (pAddrEntry = IrAddrObjList.Flink;
         pAddrEntry != &IrAddrObjList;
         pAddrEntry = pAddrEntry->Flink)
    {    
        pAddr = (PIRLMP_ADDR_OBJ)pAddrEntry;

        VALIDADDR(pAddr);
        if (pAddr) {
            GET_ADDR_LOCK(pAddr);

            DEBUGMSG(1,
                (TEXT("  AddrObj:%#x (%d) RefCnt: %d Loc:\"%hs\",%d Next:%#x\r\n"),
                 pAddr, pAddr->dwId, pAddr->cRefs,
                 pAddr->SockAddrLocal.irdaServiceName,
                 pAddr->LSAPSelLocal,
                 pAddrEntry->Flink != &IrAddrObjList ? pAddrEntry->Flink : NULL));

            for (pConnEntry = pAddr->ConnList.Flink;
                 pConnEntry != &pAddr->ConnList;
                 pConnEntry = pConnEntry->Flink)
            {
                pConn = (PIRLMP_CONNECTION)pConnEntry;
                VALIDCONN(pConn);

                DEBUGMSG(1,
                    (TEXT("    ConnObj:%#x (%d) Ref: %d Loc:\"%hs\",%d Rem:\"%hs\",%d ")
                     TEXT("State:%s (%d) AddrObj:%#x Next:%#x\r\n"),
                     pConn, pConn->dwId, pConn->cRefs,
                     pConn->SockAddrLocal.irdaServiceName,
                     pConn->LSAPSelLocal,
                     pConn->SockAddrRemote.irdaServiceName,
                     pConn->LSAPSelRemote,
                     szIrlmpConnState[pConn->ConnState], pConn->ConnState,
                     pConn->pAddrObj,
                     pConnEntry->Flink != &pAddr->ConnList ? pConnEntry->Flink : NULL));
            }

            FREE_ADDR_LOCK(pAddr);
        } else {
            ASSERT(0);  // list is corrupt
            break;
        }
    }
    
    if (!IsListEmpty(&IrConnObjList))
    {
        DEBUGMSG(1,
            (TEXT("  Unassociated Connection Objects:\r\n")));
        
        for (pConnEntry = IrConnObjList.Flink;
             pConnEntry != &IrConnObjList;
             pConnEntry = pConnEntry->Flink)
        {
            pConn = (PIRLMP_CONNECTION)pConnEntry;
            VALIDCONN(pConn);
            DEBUGMSG(1,
                (TEXT("    ConnObj:%#x (%d) Ref: %d Loc:\"%hs\",%d Rem:\"%hs\",%d ")
                 TEXT("State:%s (%d) AddrObj:%#x pNext:%#x\r\n"),
                 pConn, pConn->dwId, pConn->cRefs,
                 pConn->SockAddrLocal.irdaServiceName,
                 pConn->LSAPSelLocal,
                 pConn->SockAddrRemote.irdaServiceName,
                 pConn->LSAPSelRemote,
                 szIrlmpConnState[pConn->ConnState], pConn->ConnState,
                 pConn->pAddrObj,
                 pConnEntry->Flink != &IrConnObjList ? pConnEntry->Flink : NULL));
        }
    }

    LeaveCriticalSection(&csIrObjList);
    return;
}

#else
#define DUMP_OBJECTS(debugzone)             ((void)0)
#define DUMP_RECVLIST(debugzone, pConn)     ((void)0)
#endif

//
// For sockets which are not bound to a service name - we will assign a 
// hard coded LSAP-SEL. We will store this in the irdaServiceName of the
// sockaddr.
//

VOID
SetLsapSelAddr(
    int LsapSel,
    CHAR *ServiceName)
{
    int     Digit, i;
    int     StrLen = 0;
    CHAR    Str[4];

    while (LsapSel > 0 && StrLen < 3)
    {
        Digit = LsapSel % 10;
        LsapSel = LsapSel / 10;
        Str[StrLen] = Digit + '0';
        StrLen++;
    }

    RtlCopyMemory(ServiceName, LSAPSEL_TXT, LSAPSEL_TXTLEN);

    for (i = 0; i < StrLen; i++)
    {
       ServiceName[i + LSAPSEL_TXTLEN] = Str[StrLen - 1 - i];
    }

    ServiceName[StrLen + LSAPSEL_TXTLEN] = 0;
}

#define IRDA_MIN_LSAP_SEL 1
#define IRDA_MAX_LSAP_SEL 127

int
ParseLsapSel(CHAR *pszDigits)
{
    int LsapSel = 0;
    int i;

    for (i = 0; i < 3; i++)
    {
        if (pszDigits[i] == 0)
            break;

        // Only real digits are valid.
        if ((pszDigits[i] < '0') || (pszDigits[i] > '9'))
            return (-1);

        // Calculate LSAP-SEL.
        LsapSel = (LsapSel * 10) + (pszDigits[i] - '0');
    }

    // Greater than 4 characters, < 1, > 127 are all invalid LSAP-SELs.
    if ((pszDigits[i] != 0) || 
        (LsapSel < IRDA_MIN_LSAP_SEL) || 
        (LsapSel > IRDA_MAX_LSAP_SEL))
    {
        return (-1);
    }

    return (LsapSel);
}

__inline BOOL 
IsLsapSelUsed(int LsapSel)
{
    PLIST_ENTRY pAddrEntry;

    // Assumes AddrObjList lock is held.

    // sh - We have hardcoded our IAS Lsap Sel as 3. Therefore, don't let 'em
    //      use it.
    if (LsapSel == 3 /*IAS_LOCAL_LSAP_SEL*/)
    {
        return (TRUE);
    }

    for (pAddrEntry = IrAddrObjList.Flink; 
         pAddrEntry != &IrAddrObjList; 
         pAddrEntry = pAddrEntry->Flink)
    {
        if (((PIRLMP_ADDR_OBJ)pAddrEntry)->LSAPSelLocal == LsapSel)
        {
            return (TRUE);
        }
    }
    return (FALSE);
}

int
GetUnusedLsapSel()
{
    int LsapSel;
    
    // Assumes AddrObjList lock is held.
    
    for (LsapSel = IRDA_MIN_LSAP_SEL; LsapSel <= IRDA_MAX_LSAP_SEL; LsapSel++)
    {
        if (IsLsapSelUsed(LsapSel) == FALSE)
        {
            return (LsapSel);
        }
    }
    return -1;
}    

/*****************************************************************************
*
*   @func   TDI_STATUS  |   IRLMP_OpenAddress   |
*           Called after bind(). Allocates a new IRLMP_ADDR_OBJ.
*
*	@rdesc
*           TDI_SUCCESS (tdistat.h) on success, or (tdistat.h)
*   @errors
*           @ecode  TDI_ADDR_IN_USE     |   Address is in use.
*           @ecode  TDI_BAD_ADDR        |   Address is bad.
*           @ecode  TDI_NO_FREE_ADDR    |   Address space is full.
*           @ecode  TDI_NO_RESOURCES    |   No memory.
*
*   @parm   PTDI_REQUEST        |   pTDIReq     |
*           (tdi.h) The address of the new IRLMP_ADDR_OBJ is returned in
*           pTDIReq->Handle.AddressHandle.
*   @parm   PTRANSPORT_ADDRESS  |   pAddr       |
*           Pointer to input TRANSPORT_ADDRESS (tdi.h) struct. IrLMP assumes
*           one TA_ADDRESS entry with the Address field containing the 
*           remaining bytes of a SOCKADDR_IRDA struct minus the first two
*           bytes.
*   @parm   uint                |   Protocol    |
*           (TBD, see tcp.h, udp.h> IrLMP will save this value in the new
*           IRLMP_ADDR_OBJ.
*   @parm   PVOID               |   pOptions    |
*           Pointer to options. IrLMP will ignore this value.
*/

TDI_STATUS
IRLMP_OpenAddress(PTDI_REQUEST       pTDIReq,
                  PTRANSPORT_ADDRESS pAddr,
                  uint               Protocol,
                  PVOID              pOptions)
{
    TDI_STATUS      TdiStatus = TDI_SUCCESS;
    PIRLMP_ADDR_OBJ	pNewAddrObj, pAddrObj;
    int             LsapSel;
    PSOCKADDR_IRDA  pIrdaAddr;
    PLIST_ENTRY     pAddrEntry;

    DEBUGMSG(ZONE_FUNCTION,
        (TEXT("+IRLMP_OpenAddress(0x%X,0x%X,0x%X,0x%X)\r\n"), 
         pTDIReq, pAddr, Protocol, pOptions));

    pNewAddrObj = NULL;
    EnterCriticalSection(&csIrObjList);

    // Service name supplied. Ensure that an address object with same
    // name does not exist.
    pIrdaAddr = (PSOCKADDR_IRDA)&pAddr->Address[0].AddressType;

    if (pIrdaAddr->irdaServiceName[0] != 0)
    {
        // Truncate at 24 chars. Doc'd as a NULL terminated string in a 25
        // char buffer. Truncate without error.
        pIrdaAddr->irdaServiceName[24] = 0;

        for (pAddrEntry = IrAddrObjList.Flink;
             pAddrEntry != &IrAddrObjList;
             pAddrEntry = pAddrEntry->Flink)
        {
            pAddrObj = (PIRLMP_ADDR_OBJ)pAddrEntry;
            if (strcmp(pIrdaAddr->irdaServiceName,
                       pAddrObj->SockAddrLocal.irdaServiceName) == 0)
            {
                DEBUGMSG(ZONE_WARN,
                    (TEXT("IRLMP_OpenAddress, dup irdaServiceName\r\n")));
                
                TdiStatus = TDI_ADDR_IN_USE;
                goto ExitIRLMP_OpenAddress;
            }
        }
    }

	if (!memcmp (pIrdaAddr->irdaServiceName, LSAPSEL_TXT, LSAPSEL_TXTLEN)) 
    {
		DEBUGMSG(ZONE_WARN, 
            (TEXT("IRLMP_OpenAddress: Hard coded LSAP '%hs' requested.\r\n"),
             pIrdaAddr->irdaServiceName));

        LsapSel = ParseLsapSel(
            &pIrdaAddr->irdaServiceName[LSAPSEL_TXTLEN]);

        if (LsapSel == -1)
        {
            TdiStatus = TDI_ADDR_INVALID;
            goto ExitIRLMP_OpenAddress;
        }

        DEBUGMSG(ZONE_WARN, 
            (TEXT("IRLMP_OpenAddress: Have hardcoded LSAP %d(0x%X)\r\n"),
             LsapSel, LsapSel));

        // Ensure that our LSAP does not collide.
        if (IsLsapSelUsed(LsapSel) == TRUE)
        {
            DEBUGMSG(ZONE_WARN,
                (TEXT("IRLMP_OpenAddress: Can't give hardcoded LSAP %d -- in use!\r\n"),
                 LsapSel));
            TdiStatus = TDI_ADDR_IN_USE;
            goto ExitIRLMP_OpenAddress;
        }
	}
    else
    {
        // Get an unused LSAP-SEL for this socket address object.
        LsapSel = GetUnusedLsapSel();

        if (LsapSel == -1)
        {
            TdiStatus = TDI_NO_FREE_ADDR;
            goto ExitIRLMP_OpenAddress;
        }
    }

	pNewAddrObj = ALLOCADDR(&pTDIReq->Handle.AddressHandle);

    if (pNewAddrObj == NULL)
    {
        DEBUGMSG(ZONE_ERROR,
            (TEXT("IRLMP_OpenAddress() CTEAllocMem() failed\r\n")));

        TdiStatus = TDI_NO_RESOURCES;
        goto ExitIRLMP_OpenAddress;
    }

    // Init lock.
    CTEInitLock(&pNewAddrObj->Lock);

    // Initialize connection list head.
    InitializeListHead(&pNewAddrObj->ConnList);
    
    // These values are set by IRLMP_SetEvent().
    pNewAddrObj->pEventConnect                 = NULL;
    pNewAddrObj->pEventConnectContext          = NULL;
    pNewAddrObj->pEventDisconnect              = NULL;
    pNewAddrObj->pEventDisconnectContext       = NULL;
    pNewAddrObj->pEventError                   = NULL;
    pNewAddrObj->pEventDisconnectContext       = NULL;
    pNewAddrObj->pEventReceive                 = NULL;
    pNewAddrObj->pEventReceiveContext          = NULL;
    pNewAddrObj->pEventReceiveDatagram         = NULL;
    pNewAddrObj->pEventReceiveDatagramContext  = NULL;
    pNewAddrObj->pEventReceiveExpedited        = NULL;
    pNewAddrObj->pEventReceiveExpeditedContext = NULL;
    
    // If there is no service name, save LSAP-SELXxx.
    if (pIrdaAddr->irdaServiceName[0] == 0)
    {
        // Not a server by default. If the app then does a listen on the
        // socket we get a WSH_NOTIFY_LISTEN and change the addr obj to
        // a server.
        pNewAddrObj->IsServer = FALSE;
        SetLsapSelAddr(LsapSel, &pIrdaAddr->irdaServiceName[0]);
    }
    else
    {
        // This will allow us to return an error to the user if a
        // connect attempt is made after binding to a service name.
        pNewAddrObj->IsServer = TRUE;
    }

    // Save the address and protocol.
    // Assume SOCKADDR_IRDA is TA_ADDRESS.AddressType, TA_ADDRESS.Address.
    memcpy(&pNewAddrObj->SockAddrLocal, 
           pIrdaAddr,
           sizeof(SOCKADDR_IRDA));

    pNewAddrObj->Protocol         = Protocol;
    pNewAddrObj->LSAPSelLocal     = LsapSel;
    pNewAddrObj->UseExclusiveMode = FALSE;
    pNewAddrObj->UseIrLPTMode     = FALSE;
    pNewAddrObj->Use9WireMode     = FALSE;
    pNewAddrObj->UseSharpMode     = FALSE;

    // Keep our ref, since we are passing it off to AFD. We will 
    // remove this ref in IRLMP_CloseAddress.

ExitIRLMP_OpenAddress:

    LeaveCriticalSection(&csIrObjList);
    DEBUGMSG(ZONE_FUNCTION,
        (TEXT("-IRLMP_OpenAddress [Status: %d; hAddr: %#x; ")
         TEXT("pAddr: 0x%x, id: %d]\r\n"),
         TdiStatus, pTDIReq->Handle.AddressHandle, 
         pNewAddrObj, (NULL == pNewAddrObj) ? 0 : pNewAddrObj->dwId));
	return (TdiStatus);
}

/*****************************************************************************
*
*   @func   TDI_STATUS  |   IRLMP_CloseAddress  |
*           Called at socket close time after IRLMP_Disconnect()
*           and IRLMP_CloseConnection(). Frees the IRDA_ADDR_OBJ.
*
*	@rdesc
*           TDI_SUCCESS (tdistat.h) on success, or (tdistat.h)
*   @errors
*           @ecode  TDI_ADDR_INVALID            |  Bad address object.
*
*   @parm   PTDI_REQUEST        |   pTDIReq     |
*           (tdi.h) pTDIReq->Handle.AddressHandle is a handle to the
*           IRLMP_ADDR_OBJ. IRLMP_OpenAddress() assigns this value to be
*           a pointer to the IRLMP_ADDR_OBJ.
*/

TDI_STATUS
IRLMP_CloseAddress(PTDI_REQUEST pTDIReq)
{
    TDI_STATUS        TdiStatus = TDI_SUCCESS;
    PIRLMP_ADDR_OBJ   pAddrObj = NULL;
    PIRLMP_CONNECTION pConn    = NULL;

	DEBUGMSG(ZONE_FUNCTION,
        (TEXT("+IRLMP_CloseAddress(0x%X)\r\n"), pTDIReq));

    pAddrObj = GETADDR(pTDIReq->Handle.AddressHandle);

    if (pAddrObj == NULL)
    {
        DEBUGMSG(ZONE_WARN, (TEXT("+IRLMP_CloseAddress(0x%X) hAddr = 0x%X not found!\r\n"),
                    pTDIReq, pTDIReq->Handle.AddressHandle));

        TdiStatus = TDI_ADDR_INVALID;
        goto done;
    }

    VALIDADDR(pAddrObj);

    DUMP_OBJECTS(ZONE_TDI || ZONE_ALLOC);

    // SH - It is possible that we have connection objects which are not
    //      disassociated. Cleanup appropriately.
    DEBUGMSG(ZONE_ERROR && (IsListEmpty(&pAddrObj->ConnList) == FALSE), 
        (TEXT("IRLMP_CloseAddress WARNING: Connections not disassociated.\r\n")));

    EnterCriticalSection(&csIrObjList);

    while (IsListEmpty(&pAddrObj->ConnList) == FALSE)
    {
        pConn = (PIRLMP_CONNECTION)pAddrObj->ConnList.Flink;

        // Assume that we don't have any real clean up to do. Just disassociate.
        ASSERT(pConn->ConnState != IRLMP_CONN_OPEN);
        ASSERT(pConn->pUsrNDISBuff == NULL);
        ASSERT(IsListEmpty(&pConn->RecvBuffList) == TRUE);

        TdiStatus = TdiDisassociateAddress(pConn);
        ASSERT(TdiStatus == TDI_SUCCESS);
    }
    
    LeaveCriticalSection(&csIrObjList);

    if (pAddrObj->IsServer == TRUE)
    {
        IRDA_MSG IMsg;
        int      rc;

        IMsg.Prim                  = IRLMP_DEREGISTERLSAP_REQ;
        IMsg.IRDA_MSG_LocalLsapSel = pAddrObj->LSAPSelLocal;
        rc = IrlmpDown(NULL, &IMsg);
        ASSERT(rc == 0);

        IMsg.Prim                  = IRLMP_DELATTRIBUTE_REQ;
        IMsg.IRDA_MSG_AttribHandle = pAddrObj->IasAttribHandle;
        rc = IrlmpDown(NULL, &IMsg);
        ASSERT(rc == 0);
    }

    // One for reference in this function and one since AFD is giving up
    // it's reference.
    REFDELADDR(pAddrObj);
    REFDELADDR(pAddrObj);

done:
    DEBUGMSG(ZONE_FUNCTION,
        (TEXT("-IRLMP_CloseAddress [%#x (%d)]\r\n"), TdiStatus, TdiStatus));
    return(TdiStatus);
}


#ifdef DEBUG
    static DWORD dwConnId = 1;
#endif 

/*****************************************************************************
*
*   @func   TDI_STATUS  |   IRLMP_OpenConnection    |
*           Called after connect() and backlog times after listen(). Allocates 
*           a new IRLMP_CONNECTION.
*
*	@rdesc
*           TDI_SUCCESS (tdistat.h) on success, or (tdistat.h)
*   @errors
*           @ecode  TDI_NO_RESOURCES    |   No memory.
*
*   @parm   PTDI_REQUEST        |   pTDIReq     |
*           (tdi.h) pTDIReq->Handle.ConnectionContext returns the address of
*           the new IRLMP_CONNECTION.
*   @parm   PVOID               |   pContext    |
*           IrLMP saves this in the IRLMP_CONNECTION.
*/

TDI_STATUS          
IRLMP_OpenConnection(PTDI_REQUEST pTDIReq,
                     PVOID        pContext)
{
    PIRLMP_CONNECTION pNewConn;
    TDI_STATUS        TdiStatus = TDI_SUCCESS;

	DEBUGMSG(ZONE_FUNCTION,
        (TEXT("+IRLMP_OpenConnection(0x%X,0x%X)\r\n"),
         pTDIReq, pContext));

    pNewConn = IrdaAlloc(sizeof(IRLMP_CONNECTION), MT_TDI_CONNECTION);

	if (pNewConn == NULL)
    {
        TdiStatus = TDI_NO_RESOURCES;
        goto done;
    }

    pNewConn->cRefs = 1;
#ifdef DEBUG
    pNewConn->Sig  = TDICONNSIG;
    pNewConn->dwId = dwConnId++;
#endif // DEBUG

    DEBUGMSG(ZONE_ALLOC, (TEXT("IrDA: Allocate Connection - %#x (%d)\r\n"), pNewConn, pNewConn->dwId));
    pNewConn->ConnState               = IRLMP_CONN_CREATED;
    pNewConn->fConnPending            = FALSE;
    pNewConn->pConnectionContext      = pContext;
    pNewConn->TdiStatus               = TDI_SUCCESS;
    InitializeListHead(&pNewConn->RecvBuffList);
    CTEInitLock(&pNewConn->Lock);

    // Put connection on unassociated connection list.
    EnterCriticalSection(&csIrObjList);
    InsertHeadList(&IrConnObjList, (PLIST_ENTRY)pNewConn);
    // For now the handle is just the pointer.
    pTDIReq->Handle.ConnectionContext =(HANDLE)pNewConn;
    LeaveCriticalSection(&csIrObjList);

    // Keep our ref since we are passing it off to AFD. We will remove this
    // ref in IRLMP_CloseConnection.

done:
    DEBUGMSG(ZONE_FUNCTION,
        (TEXT("-IRLMP_OpenConnection [Status: %#x; hConn = %#x;")
         TEXT(" pConn = %#x, id = %d]\r\n"),
         TdiStatus, pTDIReq->Handle.ConnectionContext, 
         pNewConn, (NULL == pNewConn) ? 0 : pNewConn->dwId));

    return (TdiStatus);
}

void TdiCleanupConnection(PIRLMP_CONNECTION pConn);


/*****************************************************************************
*
*   @func   TDI_STATUS  |   IRLMP_CloseConnection   |
*           Called at socket close time. If the connection is in a conencted
*           state, this call implies an abortive close - close the IRLMP
*           connection and free the IRDA_CONNECTION. If the connection is
*           not in the connected state, the IRLMP_DIsconnect() handler has
*           been called and only the IRLMP_CONNECTION will be freed.
*
*	@rdesc
*           TDI_SUCCESS (tdistat.h) on success, or (tdistat.h)
*   @errors
*           @ecode  TDI_INVALID_CONNECTION          |   Bad connection.
*
*   @parm   PTDI_REQUEST    |   pTDIReq             |
*           (tdi.h) pTDIReq->Handle.ConnectionContext contains the 
*           CONNECTION_CONTEXT (tdi.h, PVOID) of the IRLMP_CONNECTION. 
*           IRLMP_OpenConnection() assigns this value to be a pointer to
*           the IRLMP_CONNECTION.
*/

TDI_STATUS 
IRLMP_CloseConnection(PTDI_REQUEST pTDIReq)
{
    TDI_STATUS         TdiStatus = TDI_SUCCESS;
    PIRLMP_CONNECTION  pConn;
    IRDA_MSG           IrDAMsg;
    int                rc;

    pConn = GETCONN(pTDIReq->Handle.ConnectionContext);
    if (pConn == NULL)
    {
        DEBUGMSG(ZONE_WARN, (TEXT("IRLMP_CloseConnection(%#x) hConn = %#x, pConn = NULL!!!\r\n"), 
             pTDIReq, pTDIReq->Handle.ConnectionContext));
        DUMP_OBJECTS(ZONE_TDI || ZONE_ALLOC);
        return TdiStatus;
    }

    VALIDCONN(pConn);

    GET_CONN_LOCK(pConn);

	DEBUGMSG(ZONE_FUNCTION,
        (TEXT("+IRLMP_CloseConnection(%#x) hConn = %#x, pConn = %#x, id = %d\r\n"), 
         pTDIReq, pTDIReq->Handle.ConnectionContext, pConn, pConn->dwId));

    // Disconnect if required.
    if (pConn->ConnState == IRLMP_CONN_OPEN)
    {
        IrDAMsg.Prim                 = IRLMP_DISCONNECT_REQ;
        IrDAMsg.IRDA_MSG_pDiscData   = NULL;
        IrDAMsg.IRDA_MSG_DiscDataLen = 0;

        // Set state to closing.
        pConn->ConnState = IRLMP_CONN_CLOSING;
        IrdaAutoSuspend(FALSE);    // one less active connection
		
        rc = IrlmpDown(pConn->pIrLMPContext, &IrDAMsg);
        ASSERT(rc == 0);
    } else {
        DEBUGMSG(ZONE_WARN, (TEXT("IRLMP_CloseConnection() state = %d\r\n"), pConn->ConnState));
		// Set state to closing.
		pConn->ConnState = IRLMP_CONN_CLOSING;
	}


    TdiCleanupConnection(pConn);
    
	DEBUGMSG(ZONE_FUNCTION, (TEXT("-IRLMP_CloseConnection(%#x)\r\n"), pTDIReq));
    
    DUMP_OBJECTS(ZONE_TDI || ZONE_ALLOC);
	
    return(TdiStatus);
}

//
// Common worker used by both IRLMP_CloseConnection and IRLMP_Disconnect
//
void
TdiCleanupConnection(
    PIRLMP_CONNECTION pConn
    )
{
    TDI_STATUS         TdiStatus = TDI_SUCCESS;
    PIRDATDI_RECV_BUFF pRecvBuff;
    BOOL               fIndicateRecv = FALSE;

    //
    // Disassociate since our AFD does not call TdiDisassociateEntry.
    //

    // If the connect fails before AssociateAddress is called, it won't have
    // a pointer to an address object to disassociate.
    if (pConn->pAddrObj)
    {
        TdiStatus = TdiDisassociateAddress(pConn);
        ASSERT(TdiStatus == TDI_SUCCESS);
    }

    // Free memory.
    while (!IsListEmpty(&pConn->RecvBuffList))
    {
        pRecvBuff = (PIRDATDI_RECV_BUFF) 
                    RemoveTailList(&pConn->RecvBuffList);

        IrdaFree(pRecvBuff);
    }

    if (pConn->pUsrNDISBuff)
    {
        pConn->pUsrNDISBuff = NULL;
        pConn->UsrBuffLen   = 0;
        fIndicateRecv       = TRUE;
    }

    FREE_CONN_LOCK(pConn);

    if (fIndicateRecv == TRUE)
    {
        pConn->pRecvComplete(
            pConn->pRecvCompleteContext,
            TDI_SUCCESS,
            0);
    }

    // One for reference in this function and one since AFD is giving up
    // it's reference.
    REFDELCONN(pConn);
    REFDELCONN(pConn);    
}   // TdiCleanupConnection

/*****************************************************************************
*
*   @func   TDI_STATUS  |   IRLMP_AssociateAddress  |
*           Called after connect() and backlog times after listen(). 
*           Assigns pAddrObj in a newly created IRLMP_CONNECTION to point
*           to an IRLMP_ADDR_OBJ and adds the IRLMP_CONNECTION to pConnList
*           anchored on IRLMP_ADDR_OBJ. SockAddrLocal is copied from the
*           the IRLMP_ADDR_OBJ to the IRLMP_CONNECTION to handle the the
*           IsConn option of IRLMP_QueryInformation().
*
*	@rdesc
*           TDI_SUCCESS (tdistat.h) on success, or (tdistat.h)
*   @errors
*           @ecode TDI_ALREADY_ASSOCIATED   |   pAddrObj != NULL.
*
*   @parm   PTDI_REQUEST    |   pTDIReq     |
*           (tdi.h) pTDIReq->Handle.ConnectionContext contains the 
*           CONNECTION_CONTEXT (tdi.h, PVOID) of the IRLMP_CONNECTION. 
*           IRLMP_OpenConnection() assigns this value to be a pointer to
*           the IRLMP_CONNECTION.
*   @parm   HANDLE          |   AddrHandle  |
*           Handle to the IRLMP_ADDR_OBJ. IRLMP_OpenAddress() assigns this
*           value to be a pointer to the IRLMP_ADDR_OBJ.
*
*   @xref   <f IRLMP_OpenAddress>   <f IRLMP_OpenConnection>
*           <f IRLMP_QueryInformation>
*/

TDI_STATUS
IRLMP_AssociateAddress(PTDI_REQUEST pTDIReq,
                       HANDLE       AddrHandle)
{
    TDI_STATUS        TdiStatus = TDI_SUCCESS;
    PIRLMP_CONNECTION pConn;
    PIRLMP_ADDR_OBJ   pAddrObj;

	DEBUGMSG(ZONE_FUNCTION,
        (TEXT("+IRLMP_AssociateAddress(0x%X,0x%X)\r\n"), 
         pTDIReq, (unsigned) AddrHandle));

    pConn    = GETCONN(pTDIReq->Handle.ConnectionContext);
    pAddrObj = GETADDR(AddrHandle);

    if (pAddrObj == NULL)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("IRLMP_AssociateAddress(0x%X,0x%X) pAddrObj==NULL\r\n"), pTDIReq, (unsigned) AddrHandle));
        TdiStatus = TDI_ADDR_INVALID;
        goto done;
    }

    if (pConn == NULL)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("IRLMP_AssociateAddress(0x%X,0x%X) pConn==NULL\r\n"), pTDIReq, (unsigned) AddrHandle));
        REFDELADDR(pAddrObj);
        TdiStatus = TDI_INVALID_CONNECTION;
        goto done;
    }

    VALIDCONN(pConn);
    VALIDADDR(pAddrObj);

    DEBUGMSG(ZONE_TDI,
        (TEXT("Associate Addr:%#x, id:%d; Conn:%#x, id:%d\r\n"), 
         pAddrObj, pAddrObj->dwId, pConn, pConn->dwId));
    
    EnterCriticalSection(&csIrObjList);
    ASSERT(pConn->pAddrObj == NULL);

    // Connection should be on unassociated list.
    RemoveEntryList(&pConn->Linkage);

    // Associate address and connection.
    InsertHeadList(&pAddrObj->ConnList, (PLIST_ENTRY)pConn);

    GET_CONN_LOCK(pConn);

    memcpy(&pConn->SockAddrLocal, &pAddrObj->SockAddrLocal, 
           sizeof(SOCKADDR_IRDA));
    pConn->IsServer         = pAddrObj->IsServer;
    pConn->LSAPSelLocal     = pAddrObj->LSAPSelLocal;
    pConn->UseExclusiveMode = pAddrObj->UseExclusiveMode;
    pConn->UseIrLPTMode     = pAddrObj->UseIrLPTMode;
    pConn->Use9WireMode     = pAddrObj->Use9WireMode;
    pConn->UseSharpMode     = pAddrObj->UseSharpMode;
    
    // Link IRLMP_CONNECTION to IRLMP_ADDR_OBJ.
    pConn->pAddrObj         = pAddrObj;

    FREE_CONN_LOCK(pConn);

    LeaveCriticalSection(&csIrObjList);

    // Keep reference to Address since we have pAddrObj in connection.
    // But remove reference to connection.
    REFDELCONN(pConn);

done:
    
    DEBUGMSG(ZONE_FUNCTION,
        (TEXT("-IRLMP_AssociateAddress [%#x (%d)]\r\n"), TdiStatus, TdiStatus));
    
    return(TdiStatus);
}

/*****************************************************************************
*
*   @func   TDI_STATUS  |   IRLMP_DisAssociateAddress   | NOT DONE
*/

TDI_STATUS
IRLMP_DisAssociateAddress(PTDI_REQUEST pTDIReq)
{
	DEBUGMSG(ZONE_FUNCTION, 
        (TEXT("IRLMP_DisAssociateAddress(0x%X)\r\n"), pTDIReq));

    ASSERT(0);

    return(TDI_SUCCESS);
}

// Our own internal disassociate. 
TDI_STATUS
TdiDisassociateAddress(PIRLMP_CONNECTION pConn)
{
    PIRLMP_ADDR_OBJ   pAddr;

    EnterCriticalSection(&csIrObjList);

    VALIDCONN(pConn);
    if (pConn) {

        // Remove from address list.
        RemoveEntryList((PLIST_ENTRY)pConn);
        // Add to unassociated connection list.
        InsertHeadList(&IrConnObjList, (PLIST_ENTRY)pConn);

        GET_CONN_LOCK(pConn);
        pAddr = pConn->pAddrObj;
        VALIDADDR(pAddr); // Shouldn't be NULL here.
        pConn->pAddrObj = NULL;
        FREE_CONN_LOCK(pConn);

        REFDELADDR(pAddr);
    }
    LeaveCriticalSection(&csIrObjList);

    return (TDI_SUCCESS);
}

/*****************************************************************************
*
*   @func   TDI_STATUS  |   IRLMP_Connect   |
*           Called after connect(). Initiates an IRLMP connection. 
*           IRLMP will call ConnectComplete() when the connection is
*           established.
*
*	@rdesc
*           TDI_SUCCESS (tdistat.h) on success, or (tdistat.h)
*   @errors
*           @ecode TDI_PENDING  | ConnectComplete() will be called.
*
*   @parm   PTDI_REQUEST                |   pTDIReq     |
*           (tdi.h) pTDIReq->Handle.ConnectionContext contains the 
*           CONNECTION_CONTEXT (tdi.h, PVOID) of the IRLMP_CONNECTION. 
*           IRLMP_OpenConnection() assigns this value to be a pointer to
*           the IRLMP_CONNECTION.
*           <nl>pTDIReq->RequestNotifyObject is a pointer (void (*)(PVOID 
*           Context, TDI_STATUS FinalStatus, unsigned long ByteCount)) to a
*           Winsock routine to call if this function returns TDI_PENDING.
*           <nl>pTDIReq->RequestNotifyContext is a Winsock context to return in
*           the callback.
*   @parm   PVOID                       |   pTimeOut    |
*           sockutil.c appears to always set this -1. IrLMP will ignore it.
*   @parm   PTDI_CONNECTION_INFORMATION |   pReqAddr    |
*           Pointer to input TDI_CONNECTION_INFORMATION (tdi.h) struct.
*           IrLMP assumes RemoteAddress points to a TRANSPORT_ADDRESS (tdi.h)
*           struct. IrLMP assumes one TA_ADDRESS entry with the Address field
*           containing the remaining bytes of a SOCKADDR_IRDA struct minus the
*           first two bytes.
*   @parm   PTDI_CONNECTION_INFORMATION |   pRetAddr    |
*           Pointer to output TDI_CONNECTION_INFORMATION (tdi.h) struct.
*
*   @xref   <f ConnectComplete>
*/

TDI_STATUS
IRLMP_Connect(PTDI_REQUEST                  pTDIReq,
              PVOID                         pTimeOut,
              PTDI_CONNECTION_INFORMATION   pReqAddr,
              PTDI_CONNECTION_INFORMATION   pRetAddr)
{
    TDI_STATUS        TdiStatus = TDI_PENDING;
    PIRLMP_CONNECTION pConn;
    IRDA_MSG          IrDAMsg;
    int               rc;
    int               i;
	int				  RetryCount = 0;
    int               IrlptMode = 1;

    DWORD             dwTemp;	// For compiler 

    BOOL              bLPTMode;
    DWORD             dwAddr;
    BYTE              ServiceName[25];
    DWORD             dwLSAPSelRemote;

	DEBUGMSG(ZONE_FUNCTION,
        (TEXT("+IRLMP_Connect(0x%X,0x%X,0x%X,0x%X)\r\n"), 
        pTDIReq, pTimeOut, pReqAddr, pRetAddr));

    pConn = GETCONN(pTDIReq->Handle.ConnectionContext);

    if (pConn == NULL)
    {
        TdiStatus = TDI_INVALID_CONNECTION;
        goto done_nolocks;
    }

    GET_CONN_LOCK(pConn);

    if (pConn->IsServer)
    {
        TdiStatus = TDI_INVALID_REQUEST;
        goto ExitIRLMP_Connect;
    }

    memcpy(&pConn->SockAddrRemote, 
           (BYTE *) &((PTRANSPORT_ADDRESS) 
                      pReqAddr->RemoteAddress)->Address[0].AddressType,
           sizeof(SOCKADDR_IRDA));

    pConn->pConnectComplete        = pTDIReq->RequestNotifyObject;
    pConn->pConnectCompleteContext = NULL;

    pConn->ConnState = IRLMP_CONN_OPENING;
    pConn->fConnPending = FALSE;

    EnterCriticalSection(&csIasQuery);
    if (memcmp(&pConn->SockAddrRemote.irdaServiceName[0], LSAPSEL_TXT, LSAPSEL_TXTLEN) == 0)
    {
        pConn->LSAPSelRemote = ParseLsapSel(
            &pConn->SockAddrRemote.irdaServiceName[LSAPSEL_TXTLEN]);

        LeaveCriticalSection(&csIasQuery);
        if (pConn->LSAPSelRemote == -1)
        {
            TdiStatus = TDI_ADDR_INVALID;
            goto ExitIRLMP_Connect;
        }
    }
    else
    {
        LeaveCriticalSection(&csIasQuery);
        //
        // Let go of the connection during the query and reacquire it later.
        //
        memcpy(&dwAddr, &pConn->SockAddrRemote.irdaDeviceID, IRDA_DEV_ADDR_LEN);
        memcpy(ServiceName, pConn->SockAddrRemote.irdaServiceName, 25);
        bLPTMode = pConn->UseIrLPTMode;
        FREE_CONN_LOCK(pConn);
        REFDELCONN(pConn);
        pConn = NULL;
        EnterCriticalSection(&csIasQuery);

RetryIASQuery:
        
        if (IASQueryInProgress)
        {
			
			DEBUGMSG (ZONE_ERROR,
					  (TEXT("TDI:IRLMP_Connect returning TDI_IN_PROGRESS ")
					   TEXT("because IASQueryInProgress\r\n")));
            TdiStatus = TDI_IN_PROGRESS;
            goto ExitIRLMP_Connect_IASQuery;
        }

		// If we've been going around too many times then give up
		if (RetryCount++ > 20) {
			DEBUGMSG (ZONE_ERROR,
					  (TEXT("TDI:IRLMP_Connect returning TDI_NET_DOWN ")
					   TEXT("because too many retries of IAS Query\r\n")));
			TdiStatus = TDI_NET_DOWN;
			goto ExitIRLMP_Connect_IASQuery;
		}

        IASQueryInProgress = TRUE;

        pIASQuery = &ConnectIASQuery;

        IrDAMsg.Prim                   = IRLMP_GETVALUEBYCLASS_REQ;
        IrDAMsg.IRDA_MSG_pIasQuery     = pIASQuery;
        IrDAMsg.IRDA_MSG_AttribLen     = 1;

        memcpy(&pIASQuery->irdaDeviceID[0], &dwAddr, IRDA_DEV_ADDR_LEN);
        
        i = 0;
        // sh - Only allow 24 chars. We truncate the name because we
        //      have doc'd that it must be a NULL-terminated string
        //      in a 25 char array.
        while (ServiceName[i] && i < 24)
        {
            pIASQuery->irdaClassName[i] = ServiceName[i];
            i++;
        }
        pIASQuery->irdaClassName[i] = 0;

        if (bLPTMode)
        {
            // Two different attribute names for printers...most common
            // is IrDA:IrLMP:LsapSel.
            if (IrlptMode == 1)
            {
                strcpy(pIASQuery->irdaAttribName, "IrDA:IrLMP:LsapSel");
            }
            else
            {
                ASSERT(IrlptMode == 2);
                strcpy(pIASQuery->irdaAttribName, "IrDA:IrLMP:LSAPSel");            
            }
        }
        else
        {
            strcpy(pIASQuery->irdaAttribName, IasAttribName_TTPLsapSel);
        }

        LeaveCriticalSection(&csIasQuery);

        rc = IrlmpDown(NULL, &IrDAMsg);

        if (rc != 0)
        {
            EnterCriticalSection(&csIasQuery);
            IASQueryInProgress = FALSE;
            LeaveCriticalSection(&csIasQuery);

            if (rc == IRLMP_LINK_IN_USE)
            {
                TdiStatus = TDI_ADDR_IN_USE;
            }
            else if (rc == IRLMP_BAD_DEV_ADDR)
            {
                DEBUGMSG(ZONE_ERROR,
                    (TEXT("IrLMP: Can't connect - invalid address\r\n")));

                TdiStatus = TDI_ADDR_INVALID;
            }
            else    
            {
                ERRORMSG(1,
                    (TEXT("IrLMP: unrecoverable error %d\r\n"), rc));
                
                TdiStatus = WSANO_RECOVERY;
            }
            goto done_nolocks;
        }

		// We might get an IRLMP_IRLAP_REMOTE_DISCOVERY_IN_PROGRESS error
		// which means we want to wait until the discovery is complete.
		// We need to do this here to ensure that we don't miss the
		// Discovery Indication as we enter and leave the critical section
		ResetEvent(DscvIndEvent);

        WaitForSingleObject(IASQueryConfEvent, INFINITE);

        EnterCriticalSection(&csIasQuery);

        IASQueryInProgress = FALSE;

        if (IASQueryStatus == IRLMP_IAS_SUCCESS)
        {
            dwLSAPSelRemote = pIASQuery->irdaAttribute.irdaAttribInt;

            if (dwLSAPSelRemote > HIGHEST_LSAP_SEL)
            {
                TdiStatus = TDI_BAD_ADDR;
                goto ExitIRLMP_Connect_IASQuery;
            }

            LeaveCriticalSection(&csIasQuery);
        }
        else    
        {
            switch(IASQueryStatus)
            {
              case IRLMP_MAC_MEDIA_BUSY:
				
				DEBUGMSG (1, (TEXT("GOT IRLMP_MAC_MEDIA_BUSY in TDI:IRLMPConnect, retrying query\r\n")));
                *(volatile DWORD*)&dwTemp = 0;	// This avoids an SH3 compiler (#14740).
                goto RetryIASQuery;
                break;
                
              case IRLMP_IRLAP_REMOTE_DISCOVERY_IN_PROGRESS:
                /* wmz retry the query after DISCOVERY_IND */
				
				DEBUGMSG (1, (TEXT("GOT IRLMP_IRLAP_REMOTE_DISCOVERY_IN_PROGRESS in TDI:IRLMPConnect\r\n")));

                LeaveCriticalSection(&csIasQuery);

				WaitForSingleObject (DscvIndEvent, INFINITE);

                EnterCriticalSection(&csIasQuery);
		
				DEBUGMSG (1, (TEXT("GOT IRLMP_IRLAP_REMOTE_DISCOVERY_IN_PROGRESS in TDI:IRLMPConnect\r\n")));
                goto RetryIASQuery;
                break;
                
              case IRLMP_DISC_LSAP:
                TdiStatus = TDI_CONN_REFUSED;
                break;

              case IRLMP_NO_RESPONSE_LSAP:
                TdiStatus = TDI_TIMED_OUT;
                break;

              case IRLMP_IAS_NO_SUCH_OBJECT:
                TdiStatus = TDI_CONN_REFUSED;
                break;
            
              case IRLMP_IAS_NO_SUCH_ATTRIB:
                // This will retry the different Irlpt printer attribute names.
                if (IrlptMode == 1)
                {
                    IrlptMode++;
                    goto RetryIASQuery;
                }
                TdiStatus = TDI_CONN_REFUSED;
                break;

              default:
                TdiStatus = TDI_CONNECTION_ABORTED;
                break;
            }

            goto ExitIRLMP_Connect_IASQuery;
        }
    }

    if (pConn == NULL) {
        //
        // Reacquire conn after query
        //
        pConn = GETCONN(pTDIReq->Handle.ConnectionContext);
    
        if (pConn == NULL) {
            DEBUGMSG(ZONE_WARN, (TEXT("IRLMP_Connect - connection 0x%X gone after query\r\n"), pTDIReq->Handle.ConnectionContext));
            TdiStatus = TDI_CONNECTION_ABORTED;
            goto done_nolocks;
        }

        GET_CONN_LOCK(pConn);
        pConn->LSAPSelRemote = dwLSAPSelRemote;
    }

    IrDAMsg.Prim                   = IRLMP_CONNECT_REQ;

    memcpy(&IrDAMsg.IRDA_MSG_RemoteDevAddr, 
           &pConn->SockAddrRemote.irdaDeviceID,
           IRDA_DEV_ADDR_LEN);
    
    IrDAMsg.IRDA_MSG_RemoteLsapSel = pConn->LSAPSelRemote;
    IrDAMsg.IRDA_MSG_LocalLsapSel  = pConn->LSAPSelLocal;
    IrDAMsg.IRDA_MSG_pQos          = NULL;
    IrDAMsg.IRDA_MSG_pConnData     = NULL;
    IrDAMsg.IRDA_MSG_ConnDataLen   = 0;
    IrDAMsg.IRDA_MSG_pContext      = pConn;
    IrDAMsg.IRDA_MSG_UseTtp        = 
        ! (pConn->UseExclusiveMode || pConn->UseIrLPTMode);

    IrDAMsg.IRDA_MSG_TtpCredits    = TINY_TP_RECV_CREDITS;
    IrDAMsg.IRDA_MSG_MaxSDUSize    = TINY_TP_RECV_MAX_SDU;

    pConn->TinyTPRecvCreditsLeft = TINY_TP_RECV_CREDITS;

    // CONNECT_REQ is pending. We should get either a CONNECT_CONF or a
    // DISCONNECT_IND.
    pConn->fConnPending = TRUE;
    pConn->pConnectCompleteContext = pTDIReq->RequestContext;
    REFADDCONN(pConn);
    FREE_CONN_LOCK(pConn);

    rc = IrlmpDown(NULL, &IrDAMsg);

    GET_CONN_LOCK(pConn);

    if (rc) {
        pConn->fConnPending = FALSE;
        pConn->pConnectCompleteContext = NULL;
    }

    switch (rc) {
    case 0:
        break;

    case IRLMP_LINK_IN_USE:
        TdiStatus = TDI_ADDR_IN_USE;
        break;

    case IRLMP_IN_EXCLUSIVE_MODE:
        TdiStatus = TDI_LINK_BUSY;
        break;

    default:
        DEBUGMSG(ZONE_WARN,
            (TEXT("IRLMP_Connect - unexpected error - %#x (%d)\r\n"), rc, rc));

        TdiStatus = TDI_CONNECTION_ABORTED;
        break;
    }

    FREE_CONN_LOCK(pConn);
    REFDELCONN(pConn);
    

done_nolocks:

    //
    // If we are pending the connection, we need to keep a reference so that
    // we know it doesn't go away (someone closes before connect conf) so
    // that we can complete the connection to AFD (otherwise AFD blocks
    // waiting for the connection and we don't have our connection context
    // anymore).
    //    
    
    if (TdiStatus != TDI_PENDING)
    {
        if (pConn) REFDELCONN(pConn);
    }

    DEBUGMSG(ZONE_FUNCTION, (TEXT("-IRLMP_Connect [%#x (%d)]\r\n"), TdiStatus, TdiStatus));
    return(TdiStatus);
    

//
// Common failure exit path
//
ExitIRLMP_Connect:
    FREE_CONN_LOCK(pConn);
    REFDELCONN(pConn);
    DEBUGMSG(ZONE_FUNCTION, (TEXT("-IRLMP_Connect [%#x (%d)].\r\n"), TdiStatus, TdiStatus));
    return(TdiStatus);

ExitIRLMP_Connect_IASQuery:
    LeaveCriticalSection(&csIasQuery);
    DEBUGMSG(ZONE_FUNCTION, (TEXT("-IRLMP_Connect [%#x (%d)] (IAS Query failed).\r\n"), TdiStatus, TdiStatus));
    return(TdiStatus);
}


#define LINGER_TIMEOUT 1000

void
LingerTimerFunc(
    CTEEvent * pCTEEvent,
    void * pContext
    )
{
    PIRLMP_CONNECTION pConn;
    PIRLMP_ADDR_OBJ   pAddrObj;
    BOOL              bDisconnected;
    IRDA_MSG          IrDAMsg;
    int               rc;

    DEBUGMSG(ZONE_FUNCTION, (TEXT("+LingerTimerFunc(0x%X) at %d\r\n"), pContext, GetTickCount()));

    pConn = GETCONN(pContext);
    if (pConn == NULL)
    {
        DEBUGMSG(ZONE_ERROR|ZONE_FUNCTION, (TEXT("-LingerTimerFunc(0x%X - invalid) \r\n"), pContext));
        return;
    }

    VALIDCONN(pConn);
    GET_CONN_LOCK(pConn);

    bDisconnected = FALSE;

    if (pConn->ConnState == IRLMP_CONN_OPEN)
    {
        if (pConn->LingerTime <= LINGER_TIMEOUT) {
            //
            // The linger time has expired.
            //
            pConn->LingerTime = 0;
            pConn->LastSendsRemaining = 0;
            bDisconnected = TRUE;
        } else {
            pConn->LingerTime -= LINGER_TIMEOUT;
        }

        if (pConn->LastSendsRemaining <= pConn->SendsRemaining) {
            //
            // Didn't send anything in the last period
            //
            bDisconnected = TRUE;
        } else {
            pConn->LastSendsRemaining = pConn->SendsRemaining;
        }
    }

    if (bDisconnected) {
        pAddrObj = pConn->pAddrObj;
        VALIDADDR(pAddrObj);
        GET_ADDR_LOCK(pAddrObj);

        pConn->ConnState = IRLMP_CONN_CLOSING;
        IrdaAutoSuspend(FALSE);    // one less active connection

        DEBUGMSG(ZONE_WARN, (TEXT("LingerTimerFunc() Disconnecting pLsapCb 0x%x\r\n"), pConn->pIrLMPContext));

        pAddrObj->pEventDisconnect(
            pAddrObj->pEventDisconnectContext,
            pConn->pConnectionContext, 0, NULL, 
            0, NULL, TDI_DISCONNECT_ABORT);

        IrDAMsg.Prim                 = IRLMP_DISCONNECT_REQ;
        IrDAMsg.IRDA_MSG_pDiscData   = NULL;
        IrDAMsg.IRDA_MSG_DiscDataLen = 0;

        FREE_ADDR_LOCK(pAddrObj);
        FREE_CONN_LOCK(pConn);

        rc = IrlmpDown(pConn->pIrLMPContext, &IrDAMsg);
        if (rc) {
            DEBUGMSG(ZONE_ERROR, (TEXT("LingerTimerFunc() IrlmpDown(IRLMP_DISCONNECT_REQ) failed %d\r\n"), rc));
            GET_CONN_LOCK(pConn);
            TdiCleanupConnection(pConn);
        } else {
            REFDELCONN(pConn);
        }
    } else {
        CTEStartTimer(&pConn->LingerTimer, min(pConn->LingerTime, LINGER_TIMEOUT), LingerTimerFunc, pConn);
        FREE_CONN_LOCK(pConn);
        REFDELCONN(pConn);
    }


    DEBUGMSG(ZONE_FUNCTION, (TEXT("-LingerTimerFunc(0x%X) \r\n"), pContext));
}


/*****************************************************************************
*
*   @func   TDI_STATUS  |   IRLMP_Disconnect        |
*           Called at socket close time. IrLMP will call EventDisconnect() to
*           to complete the disconnect. It appears that only graceful closes
*           use this mechanism. For abortive closes, Winsock calls only
*           IRLMP_CloseConnection().
*
*	@rdesc
*           TDI_SUCCESS (tdistat.h) on success, or (tdistat.h)
*   @errors
*           @ecode  TDI_INVALID_CONNECTION          |   Bad connection.
*           @ecode  TDI_INVALID_STATE               |   Bad state.
*
*   @parm   PTDI_REQUEST    |   pTDIReq             |
*           (tdi.h) pTDIReq->Handle.ConnectionContext contains the 
*           CONNECTION_CONTEXT (tdi.sock    h, PVOID) of the IRLMP_CONNECTION. 
*           IRLMP_OpenConnection() assigns this value to be a pointer to
*           the IRLMP_CONNECTION.
*   @parm   PVOID           | pTimeOut              | Ignored.
*   @parm   ushort          | Flags                 |
*           (tdi.h) One of
*           <nl>    TDI_DISCONNECT_WAIT
*           <nl>    TDI_DISCONNECT_ABORT
*           <nl>    TDI_DISCONNECT_RELEASE
*           <nl>    TDI_DISCONNECT_CONFIRM
*           <nl>    TDI_DISCONNECT_ASYNC
*   @parm   PTDI_CONNECTION_INFORMATION | pDiscInfo | Ignored.
*   @parm   PTDI_CONNECTION_INFORMATION | pRetInfo  | Ignored.
*/

TDI_STATUS 
IRLMP_Disconnect(PTDI_REQUEST                pTDIReq,
                 PVOID                       pTimeOut,
                 ushort                      Flags,
                 PTDI_CONNECTION_INFORMATION pDiscInfo,
                 PTDI_CONNECTION_INFORMATION pRetInfo)
{
    TDI_STATUS        TdiStatus = TDI_SUCCESS;
    PIRLMP_CONNECTION pConn;
    PIRLMP_ADDR_OBJ   pAddrObj;
    IRDA_MSG          IrDAMsg;
    int               rc;

	DEBUGMSG(ZONE_FUNCTION, 
        (TEXT("+IRLMP_Disconnect(0x%X,0x%X,0x%X,0x%X,0x%X)\r\n"), 
         pTDIReq, pTimeOut, (unsigned) Flags, pDiscInfo, pRetInfo));

    pConn = GETCONN(pTDIReq->Handle.ConnectionContext);
    if (pConn == NULL)
    {
        goto done;
    }

    rc = 0;
    VALIDCONN(pConn);
    GET_CONN_LOCK(pConn);

    switch (pConn->ConnState) {
    case IRLMP_CONN_OPEN:
        if (Flags != TDI_DISCONNECT_ABORT) {
            pConn->LastSendsRemaining = pConn->SendsRemaining;
            pConn->LingerTime = 0xffffffff;
            if (pTimeOut && (*(DWORD *)pTimeOut)) {
                pConn->LingerTime = *(DWORD *)pTimeOut;
            }
            CTEStopTimer(&pConn->LingerTimer);
            CTEInitTimer(&pConn->LingerTimer);
            CTEStartTimer(&pConn->LingerTimer, min(pConn->LingerTime, LINGER_TIMEOUT), LingerTimerFunc, pConn);
            FREE_CONN_LOCK(pConn);
        } else {
            pAddrObj = pConn->pAddrObj;
            VALIDADDR(pAddrObj);
            GET_ADDR_LOCK(pAddrObj);

            pConn->ConnState = IRLMP_CONN_CLOSING;
            IrdaAutoSuspend(FALSE);    // one less active connection

            DEBUGMSG(ZONE_WARN, (TEXT("IRLMP_Disconnect() pLsapCb 0x%x\r\n"), pConn->pIrLMPContext));

            TdiStatus = pAddrObj->pEventDisconnect(
                pAddrObj->pEventDisconnectContext,
                pConn->pConnectionContext, 0, NULL, 
                0, NULL, TDI_DISCONNECT_WAIT);

            if (TdiStatus != TDI_SUCCESS)
            {
                DEBUGMSG(ZONE_ERROR, (TEXT("IRLMP_Disconnect() EventDisconnect() failed\r\n")));
            }

            IrDAMsg.Prim                 = IRLMP_DISCONNECT_REQ;
            IrDAMsg.IRDA_MSG_pDiscData   = NULL;
            IrDAMsg.IRDA_MSG_DiscDataLen = 0;

            FREE_ADDR_LOCK(pAddrObj);
            FREE_CONN_LOCK(pConn);

            rc = IrlmpDown(pConn->pIrLMPContext, &IrDAMsg);
            if (rc) {
                DEBUGMSG(ZONE_ERROR, (TEXT("IRLMP_Disconnect() IrlmpDown() failed %d\r\n"), rc));
            }

        }
        break;

    case IRLMP_CONN_OPENING:
        DEBUGMSG(ZONE_ERROR, (TEXT("IRLMP_Disconnect() called in OPENING state!\n")));
        TdiStatus = TDI_INVALID_STATE;
        FREE_CONN_LOCK(pConn);
        break;

    default:
        FREE_CONN_LOCK(pConn);
        TdiStatus = TDI_INVALID_STATE;
        break;
    }

done:

    if (pConn)
    {
        if (rc) {
            GET_CONN_LOCK(pConn);
            TdiCleanupConnection(pConn);
        } else {
            REFDELCONN(pConn);
        }
    }

    DEBUGMSG(ZONE_FUNCTION,
        (TEXT("-IRLMP_Disconnect [%#x (%d)]\r\n"), TdiStatus, TdiStatus));
    return (TdiStatus);
}

/*****************************************************************************
*
*   @func   TDI_STATUS  |   IRLMP_Listen    | THIS IS NEVER CALLED.
*/

TDI_STATUS 
IRLMP_Listen(PTDI_REQUEST                pTDIReq,
             ushort                      Flags,
             PTDI_CONNECTION_INFORMATION pAcceptableInfo,
             PTDI_CONNECTION_INFORMATION pConnectedInfo)
{
	DEBUGMSG(ZONE_FUNCTION, 
        (TEXT("IRLMP_Listen(0x%X,0x%X,0x%X,0x%X)\r\n"), 
        pTDIReq, (unsigned) Flags, pAcceptableInfo, pConnectedInfo));

    ASSERT(0);

    return(TDI_SUCCESS);
}

/*****************************************************************************
*
*   @func   TDI_STATUS  |   IRLMP_Accept    | THIS IS NEVER CALLED.
*/

TDI_STATUS 
IRLMP_Accept(PTDI_REQUEST                pTDIReq,
             PTDI_CONNECTION_INFORMATION pAcceptInfo,
             PTDI_CONNECTION_INFORMATION pConnectedInfo)
{
	DEBUGMSG(ZONE_FUNCTION, 
        (TEXT("IRLMP_Accept(0x%X,0x%X,0x%X)\r\n"), 
        pTDIReq, pAcceptInfo, pConnectedInfo));

    ASSERT(0);

    return(TDI_SUCCESS);
}

/*****************************************************************************
*
*   @func   TDI_STATUS  |   IRLMP_Receive   | 
*           Called to recv data driectly into a user buffer or to signal
*           a flow off condition.
*
*	@rdesc
*           TDI_SUCCESS (tdistat.h) on success, or (tdistat.h)
*   @errors
*           @ecode  TDI_PENDING             |
*           @ecode  TDI_NO_RESOURCES        |
*           @ecode  TDI_INVALID_STATE       |
*           @ecode  TDI_INVALID_CONNECTION  |
*
*   @parm   PTDI_REQUEST    |   pTDIReq     |
*           (tdi.h) pTDIReq->Handle.ConnectionContext contains the 
*           CONNECTION_CONTEXT (tdi.h, PVOID) of the IRLMP_CONNECTION. 
*           IRLMP_OpenConnection() assigns this value to be a pointer to
*           the IRLMP_CONNECTION.
*           <nl>pTDIReq->RequestNotifyObject is a pointer (void (*)(PVOID 
*           Context, TDI_STATUS TdiStatus, DWORD BytesRecvd)) to a Winsock 
*           routine to call if this function returns TDI_PENDING.
*           <nl>pTDIReq->RequestNotifyContext is a Winsock context to return in
*           the callback.
*   @parm   ushort          |   Flags       |
*           (tdi.h) One of
*           <nl>TDI_RECEIVE_TRUNCATED
*           <nl>TDI_RECEIVE_FRAGMENT
*           <nl>TDI_RECEIVE_BROADCAST
*           <nl>TDI_RECEIVE_MULTICAST
*           <nl>TDI_RECEIVE_PARTIAL
*           <nl>TDI_RECEIVE_NORMAL
*           <nl>TDI_RECEIVE_EXPEDITED
*           <nl>TDI_RECEIVE_PEEK
*           <nl>TDI_RECEIVE_NO_RESPONSE_EXP
*           <nl>TDI_RECEIVE_COPY_LOOKAHEAD
*           <nl>TDI_RECEIVE_ENTIRE_MESSAGE
*           <nl>TDI_RECEIVE_AT_DISPATCH_LEVEL
*   @parm   unit *          |   RecvLen     |
*           Data size of the NDIS buffer (chain?).
*   @parm   PNDIS_BUFFER    | pNDISBuf      |
*           (ndis.h) Pointer to output NDIS_BUFFER.
*
*   @xref   <f EventReceive>
*/

#ifdef DEBUG
DWORD v_cRecvs;
#endif 

TDI_STATUS
IRLMP_Receive(PTDI_REQUEST pTDIReq,
              ushort *     pFlags,
              uint *       pRecvLen, 
              PNDIS_BUFFER pNDISBuf)
{
    TDI_STATUS         TdiStatus = TDI_SUCCESS;
    PIRLMP_CONNECTION  pConn;
    PIRDATDI_RECV_BUFF pRecvBuff;
    int                SpaceLeft;
    IRDA_MSG           IrDAMsg;
    int                rc;
#ifdef DEBUG
    DWORD cRecvs = v_cRecvs++;
#endif 
    
	DEBUGMSG(ZONE_FUNCTION, 
        (TEXT("+IRLMP_Receive(0x%X,0x%X,0x%X,0x%X)\r\n"), 
         pTDIReq, pFlags, pRecvLen, pNDISBuf));

    pConn = GETCONN(pTDIReq->Handle.ConnectionContext);

    if (pConn == NULL)
    {
        TdiStatus = TDI_INVALID_CONNECTION;
        goto done;
    }

    VALIDCONN(pConn);
    GET_CONN_LOCK(pConn);

    if (! IsListEmpty(&pConn->RecvBuffList))
    {
		PVOID		VirtualAddress;
        UINT		BufferLength;

        SpaceLeft              = *pRecvLen;

        DUMP_RECVLIST(ZONE_TDI, pConn);
        
		NdisQueryBuffer(pNDISBuf, &VirtualAddress, &BufferLength);
		BufferLength = 0;
        while (SpaceLeft > 0 && ! IsListEmpty(&pConn->RecvBuffList))
        {
            pRecvBuff = 
                (PIRDATDI_RECV_BUFF) RemoveTailList(&pConn->RecvBuffList);

            DEBUGMSG(ZONE_TDI,
                (TEXT("IRLMP_Receive() forwarding %d bytes\r\n"),
                 min(pRecvBuff->DataLen, SpaceLeft)));

            memcpy(((BYTE *) VirtualAddress) + BufferLength, 
                   pRecvBuff->pRead, 
                   min(pRecvBuff->DataLen, SpaceLeft));

            BufferLength += min(pRecvBuff->DataLen, SpaceLeft);

            // requeue the remaining bytes
            if (pRecvBuff->DataLen > (int) SpaceLeft)
            {
                pRecvBuff->DataLen -= SpaceLeft;
                pRecvBuff->pRead   += SpaceLeft;

                DEBUGMSG(ZONE_TDI,
                    (TEXT("IRLMP_Receive() reQing remaining %d bytes\r\n"),
                    pRecvBuff->DataLen));

                InsertTailList(&pConn->RecvBuffList, &pRecvBuff->Linkage);
            }
            else
                IrdaFree(pRecvBuff);

            SpaceLeft = *pRecvLen - BufferLength;
        }

		NdisAdjustBufferLength(pNDISBuf, BufferLength);

        *pRecvLen = BufferLength;

        pConn->RecvQBytes -= BufferLength;

        if ((DWORD) pConn->IndicatedNotAccepted >= BufferLength)
            pConn->IndicatedNotAccepted -= BufferLength;
        else
        {
            pConn->NotIndicated -= BufferLength - pConn->IndicatedNotAccepted;
            pConn->IndicatedNotAccepted = 0;
        }

        if (pConn->IndicatedNotAccepted == 0 && pConn->NotIndicated > 0)
        {
            EventRcvBuffer EventRecvCompleteInfo;    
            DWORD          BytesTaken = 0;
            
            DEBUGMSG(ZONE_TDI,
                     (TEXT("%x IR : IRLMP_Receive (%d) calling TdiEventReceive\r\n"),
                      GetCurrentThreadId(), cRecvs)
                     );
            
            pConn->pAddrObj->pEventReceive(
                pConn->pAddrObj->pEventReceiveContext,
                pConn->pConnectionContext,
                0, pConn->NotIndicated, pConn->NotIndicated,
                &BytesTaken, NULL,
                &EventRecvCompleteInfo);

            pConn->IndicatedNotAccepted = pConn->NotIndicated - BytesTaken;
            pConn->NotIndicated         = 0;
        }

        // Is is time to disconnect?
        if (pConn->NotIndicated == 0 && 
            pConn->IndicatedNotAccepted == 0 && 
            pConn->TdiStatus == TDI_DISCONNECT_WAIT &&
            pConn->ConnState == IRLMP_CONN_CLOSING)
        {
            if (pConn->pAddrObj->pEventDisconnect(
                pConn->pAddrObj->pEventDisconnectContext,
                pConn->pConnectionContext, 0, NULL, 0, NULL, 
                pConn->TdiStatus)
                != STATUS_SUCCESS)
            {
                DEBUGMSG(ZONE_ERROR,
                    (TEXT("IRLMP_Receive() EventDisconnect() failed\r\n")));
            }

            FREE_CONN_LOCK(pConn);
            goto done;
        }
        
        if (pConn->RecvQBytes < IRDATDI_RECVQ_LEN &&
            pConn->TinyTPRecvCreditsLeft == 0 &&
			pConn->ConnState != IRLMP_CONN_CLOSING)
        {
            pConn->TinyTPRecvCreditsLeft = TINY_TP_RECV_CREDITS;

            IrDAMsg.Prim                 = IRLMP_MORECREDIT_REQ;
            IrDAMsg.IRDA_MSG_TtpCredits  = TINY_TP_RECV_CREDITS;

            FREE_CONN_LOCK(pConn);
            rc = IrlmpDown(pConn->pIrLMPContext, &IrDAMsg);
            ASSERT(rc == 0);
        }
        else
        {
            FREE_CONN_LOCK(pConn);
        }

        goto done;
    }

    pConn->pRecvComplete        = pTDIReq->RequestNotifyObject;
    pConn->pRecvCompleteContext = pTDIReq->RequestContext;

    pConn->pUsrNDISBuff = pNDISBuf;
    pConn->UsrBuffLen   = *pRecvLen;
    pConn->UsrBuffPerm  = pTDIReq->ProcPerm;

    FREE_CONN_LOCK(pConn);

    TdiStatus = TDI_PENDING;

done:

    if (pConn)
    {
        REFDELCONN(pConn);
    }

    DEBUGMSG(ZONE_FUNCTION,
        (TEXT("-IRLMP_Receive [%#x (%d)]\r\n"), TdiStatus, TdiStatus));
    
    return (TdiStatus);
}

/*****************************************************************************
*
*   @func   TDI_STATUS  |   IRLMP_Send     | 
*           Called to send data.
*
*	@rdesc
*           TDI_SUCCESS (tdistat.h) on success, or (tdistat.h)
*   @errors
*           @ecode  TDI_PENDING             |
*           @ecode  TDI_NO_RESOURCES        |
*           @ecode  TDI_INVALID_STATE       |
*           @ecode  TDI_INVALID_CONNECTION  |
*
*   @parm   PTDI_REQUEST    |   pTDIReq     |
*           (tdi.h) pTDIReq->Handle.ConnectionContext contains the 
*           CONNECTION_CONTEXT (tdi.h, PVOID) of the IRLMP_CONNECTION. 
*           IRLMP_OpenConnection() assigns this value to be a pointer to
*           the IRLMP_CONNECTION.
*           <nl>pTDIReq->RequestNotifyObject is a pointer (void (*)(PVOID 
*           Context, TDI_STATUS TdiStatus, DWORD BytesSent)) to a Winsock 
*           routine to call if this function returns TDI_PENDING.
*           <nl>pTDIReq->RequestNotifyContext is a Winsock context to return in
*           the callback.
*   @parm   ushort          |   Flags       |
*           (tdi.h) One of
*           <nl>TDI_SEND_EXPEDITED
*           <nl>TDI_SEND_PARTIAL
*           <nl>TDI_SEND_NO_RESPONSE_EXPECTED
*           <nl>TDI_SEND_NON_BLOCKING
*   @parm   unit            |   SendLen     |
*           Data size of the NDIS buffer (chain?).
*   @parm   PNDIS_BUFFER    | pNDISBuf      |
*           (ndis.h) Pointer to input NDIS_BUFFER.
*/

TDI_STATUS 
IRLMP_Send(PTDI_REQUEST pTDIReq,
           ushort       Flags,
           uint         SendLen,
           PNDIS_BUFFER pNDISBuf)
{
    TDI_STATUS        TdiStatus = TDI_PENDING;
    IRDA_MSG         *pIrDAMsg  = NULL;
    PIRLMP_CONNECTION pConn;
    int               rc;

	DEBUGMSG(ZONE_FUNCTION,
        (TEXT("+IRLMP_Send(0x%X,0x%X,0x%X,0x%X)\r\n"), 
         pTDIReq, (unsigned) Flags, SendLen, pNDISBuf));

    pConn = GETCONN(pTDIReq->Handle.ConnectionContext);

    if (pConn == NULL)
    {
        TdiStatus = TDI_INVALID_CONNECTION;
        goto done;
    }

    VALIDCONN(pConn);

    pIrDAMsg = IrdaAlloc(sizeof(IRDA_MSG), MT_TDI_MESSAGE);

	if (pIrDAMsg == NULL)
    {
        DEBUGMSG(ZONE_ERROR,
            (TEXT("IRLMP_Send() CTEAllocMem() failed\r\n")));

        FREE_CONN_LOCK(pConn);
        TdiStatus = TDI_NO_RESOURCES;
        goto done;
    }
    
    GET_CONN_LOCK(pConn);

    if (pConn->UseExclusiveMode || pConn->UseIrLPTMode)
    {
        if (SendLen > (uint) pConn->SendMaxSDU)
        {
            FREE_CONN_LOCK(pConn);
            TdiStatus = TDI_BUFFER_OVERFLOW;
            goto done;
        }
    }

    if (pConn->Use9WireMode) {
        if (SendLen > (uint) pConn->SendMaxPDU)
        {
            FREE_CONN_LOCK(pConn);
            TdiStatus = TDI_BUFFER_OVERFLOW;
            goto done;
        }
    }

    pIrDAMsg->Prim                      = IRLMP_DATA_REQ;
    pIrDAMsg->DataContext               = pNDISBuf;
    pIrDAMsg->IRDA_MSG_pTdiSendComp     = pTDIReq->RequestNotifyObject;
    pIrDAMsg->IRDA_MSG_pTdiSendCompCnxt = pTDIReq->RequestContext;
    pIrDAMsg->IRDA_MSG_SendLen          = SendLen;
    // send leading zero byte for IrCOMM?
    pIrDAMsg->IRDA_MSG_IrCOMM_9Wire     = pConn->Use9WireMode;

    switch(pConn->ConnState)
    {
        case IRLMP_CONN_OPEN:
            pConn->SendsRemaining++;
            FREE_CONN_LOCK(pConn);
            rc = IrlmpDown(pConn->pIrLMPContext, pIrDAMsg);
            ASSERT(rc == 0);
            break;
            
        default:
            FREE_CONN_LOCK(pConn);
            TdiStatus = TDI_INVALID_STATE;
            break;
    }

done:

    if (pConn)
    {
        REFDELCONN(pConn);

        // DataReq should always pend.
        ASSERT(TdiStatus != TDI_SUCCESS);
        if (TdiStatus != TDI_PENDING )
        {
            if (pIrDAMsg) IrdaFree(pIrDAMsg);
        }
    }

    DEBUGMSG(ZONE_FUNCTION,
        (TEXT("-IRLMP_Send [%#x (%d)]\r\n"), TdiStatus, TdiStatus));

    return(TdiStatus);
}

/*****************************************************************************
*
*   @func   TDI_STATUS  |   IRLMP_SendDatagram  | THIS IS NEVER CALLED. wmz
*/

TDI_STATUS 
IRLMP_SendDatagram(PTDI_REQUEST                pTDIReq,
                   PTDI_CONNECTION_INFORMATION pConnInfo,
                   uint                        SendLen,
                   ULONG *                     pBytesSent,
                   PNDIS_BUFFER                pNDISBuf)
{
	DEBUGMSG(ZONE_FUNCTION,
        (TEXT("IRLMP_SendDatagram(0x%X,0x%X,0x%X,0x%X,0x%X)\r\n"), 
        pTDIReq, pConnInfo, SendLen, pBytesSent, pNDISBuf));

    ASSERT(0);

    return(TDI_SUCCESS);
}

/*****************************************************************************
*
*   @func   TDI_STATUS  |   IRLMP_SendDatagram  | THIS IS NEVER CALLED. wmz
*/

TDI_STATUS 
IRLMP_ReceiveDatagram(PTDI_REQUEST                pTDIReq,
                      PTDI_CONNECTION_INFORMATION pConnInfo,
                      PTDI_CONNECTION_INFORMATION pRetInfo,
                      uint                        RecvLen,
                      uint *                      pBytesRecvd,
                      PNDIS_BUFFER                pNDISBuf)
{
    DEBUGMSG(ZONE_FUNCTION,
        (TEXT("IRLMP_ReceiveDatagram(0x%X,0x%X,0x%X,0x%X,0x%X,0x%X)\r\n"), 
        pTDIReq, pConnInfo, pRetInfo, RecvLen, pBytesRecvd, pNDISBuf));

    ASSERT(0);

    return(TDI_SUCCESS);
}

/*****************************************************************************
*
*   @func   TDI_STATUS  |   IRLMP_SetEvent  |
*           Informs the stack of each Winsock entry point (event handler).
*           Called after bind() for each event handler except
*           TDI_EVENT_CONNECT. Called for TDI_EVENT_CONNECT after listen().
*           Each Address Object can have unique event handlers. The mind
*           boggles.
*
*	@rdesc
*           TDI_SUCCESS (tdistat.h) on success, or (tdistat.h)
*   @errors
*           @ecode  TDI_BAD_EVENT_TYPE  |   Event not suppoerted.
*           
*   @parm   PVOID   |   pHandle     |
*           Pointer to the IRLMP_ADDR_OBJ struct that will be associated with
*           this event handler. IrLMP will store the address of the event
*           handler here.
*   @parm   int     |   Type        |   TDI_EVENT_ (tdi.h) handler being set.
*           <nl>    TDI_EVENT_CONNECT
*           <nl>    TDI_EVENT_DISCONNECT
*           <nl>    TDI_EVENT_ERROR
*           <nl>    TDI_EVENT_RECEIVE
*           <nl>    TDI_EVENT_RECEIVE_DATAGRAM
*           <nl>    TDI_EVENT_RECEIVE_EXPEDITED
*           <nl>    TDI_EVENT_SEND_POSSIBLE
*   @parm   PVOID   |   pHandler    |
*           Address of the Winsock event handler (tdice.h).
*   @parm   PVOID   |   pContext    |
*           Pointer to a context to return to Winsock when calling the event
*           handler. IrLMP will store this pointer in the IRLMP_ADDR_OBJ.
*/

TDI_STATUS
IRLMP_SetEvent(PVOID pHandle,
               int   Type,
               PVOID pHandler,
               PVOID pContext)
{
    TDI_STATUS      TdiStatus = TDI_SUCCESS;
    PIRLMP_ADDR_OBJ pAddrObj;

	DEBUGMSG(ZONE_FUNCTION,
        (TEXT("+IRLMP_SetEvent(0x%X,0x%X,0x%X,0x%X)\r\n"), 
         pHandle, Type, pHandler, pContext));

    pAddrObj = GETADDR(pHandle);

    if (pAddrObj == NULL)
    {
        TdiStatus = TDI_ADDR_INVALID;
        goto done;
    }

    VALIDADDR(pAddrObj);
    GET_ADDR_LOCK(pAddrObj);

    switch (Type)
    {
      case TDI_EVENT_CONNECT:
        pAddrObj->pEventConnect                 = pHandler;
        pAddrObj->pEventConnectContext          = pContext;
        break;

      case TDI_EVENT_DISCONNECT:
        pAddrObj->pEventDisconnect              = pHandler;
        pAddrObj->pEventDisconnectContext       = pContext;
        break;

      case TDI_EVENT_ERROR:
        pAddrObj->pEventError                   = pHandler;
        pAddrObj->pEventDisconnectContext       = pContext;
        break;

      case TDI_EVENT_RECEIVE:
        pAddrObj->pEventReceive                 = pHandler;
        pAddrObj->pEventReceiveContext          = pContext;
        break;

      case TDI_EVENT_RECEIVE_DATAGRAM:
        pAddrObj->pEventReceiveDatagram         = pHandler;
        pAddrObj->pEventReceiveDatagramContext  = pContext;
        break;

      case TDI_EVENT_RECEIVE_EXPEDITED:
        pAddrObj->pEventReceiveExpedited        = pHandler;
        pAddrObj->pEventReceiveExpeditedContext = pContext;
        break;
        
      default:
        TdiStatus = TDI_BAD_EVENT_TYPE;
        break;
    }

    FREE_ADDR_LOCK(pAddrObj);

done:

    if (pAddrObj)
    {
        REFDELADDR(pAddrObj);
    }

    DEBUGMSG(ZONE_FUNCTION,
        (TEXT("-IRLMP_SetEvent [%#x (%d)]\r\n"), TdiStatus, TdiStatus));
    
    return (TdiStatus);
}

/*****************************************************************************
*
*   @func   TDI_STATUS  |   IRLMP_QueryInformation  |
*           Returns one of (tdi.h)
*           <nl>    TDI_CONNECTION_INFO
*           <nl>    TDI_ADDRESS_INFO
*           <nl>    TDI_PROVIDER_INFO
*           <nl>    TDI_PROVIDER_STATISTICS
*
*	@rdesc
*           TDI_SUCCESS (tdistat.h) on success, or (tdistat.h)
*   @errors
*           @ecode  TDI_INVALID_QUERY   |   QueryType not supported.
*           @ecode  TDI_BUFFER_OVERFLOW |   Return buffer too small.
*
*   @parm   PTDI_REQUEST        |   pTDIReq         |
*           (tdi.h) If (IsConn), pTDIReq->Handle.ConnectionContext contains
*           the CONNECTION_CONTEXT (tdi.h, PVOID) of the IRLMP_CONNECTION. 
*           IRLMP_OpenConnection() assigns this value to be a pointer to
*           the IRLMP_CONNECTION.
*           <nl> If (~IsConn), pTDIReq->Handle.AddressHandle is a handle to
*           the IRLMP_ADDR_OBJ. IRLMP_OpenAddress() assigns this value to be
*           a pointer to the IRLMP_ADDR_OBJ.
*   @parm   uint                |   QueryType       |
*           (tdi.h) One of
*           <nl>    TDI_QUERY_BROADCAST_ADDRESS
*           <nl>    TDI_QUERY_PROVIDER_INFO
*           <nl>    TDI_QUERY_ADDRESS_INFO
*           <nl>    TDI_QUERY_CONNECTION_INFO
*           <nl>    TDI_QUERY_PROVIDER_STATISTICS
*           <nl>IrLMP handles TDI_QUERY_ADDRESS_INFO and
*           TDI_QUERY_PROVIDER_INFO.
*   @parm   PNDIS_BUFFER        |   pNDISBuf        |
*           (ndis.h) Pointer to output NDIS_BUFFER.
*   @parm   unit *              | pNDISBufSize      |
*           On entry, the data size of the NDIS buffer chain. On return, 
*           sizeof() information struct copied into NDIS_BUFFER.
*   @parm   uint                |   IsConn          |
*           Indicates a query for connection information rather than Address
*           Object information (TDI_QUERY_ADDRESS_INFO only).
*
*/

TDI_STATUS
IRLMP_QueryInformation(PTDI_REQUEST pTDIReq,
                       uint         QueryType,
                       PNDIS_BUFFER pNDISBuf,
                       uint *       pNDISBufSize,
                       uint         IsConn)
{
    // This is large enough for TDI_QUERY_ADDRESS_INFO because
    // of the inclusion of TDI_PROVIDER_STATISTICS.
	union {
		TDI_CONNECTION_INFO		ConnInfo;
		TDI_ADDRESS_INFO		AddrInfo;
		TDI_PROVIDER_INFO		ProviderInfo;
		TDI_PROVIDER_STATISTICS	ProviderStats;
	} InfoBuf;

    TDI_STATUS  TdiStatus = TDI_SUCCESS;
    int         InfoSize  = 0;
    int         Offset;

    DEBUGMSG(ZONE_FUNCTION,
        (TEXT("+IRLMP_QueryInformation(0x%X,0x%X,0x%X,0x%X,0x%X)\r\n"), 
         pTDIReq, QueryType, pNDISBuf, pNDISBufSize, IsConn));

    if ((NULL == pNDISBuf) || (NULL == pNDISBufSize)) {
        TdiStatus = TDI_INVALID_PARAMETER;
        goto done;
    }
    
	switch (QueryType)
	{
        case TDI_QUERY_PROVIDER_INFO:
            InfoSize = sizeof(TDI_PROVIDER_INFO);
            InfoBuf.ProviderInfo.Version 				= 0x0100;
            InfoBuf.ProviderInfo.MaxSendSize			= 2048;
            InfoBuf.ProviderInfo.MaxConnectionUserData	= 0;
            InfoBuf.ProviderInfo.MaxDatagramSize		= 0;
    
            InfoBuf.ProviderInfo.ServiceFlags			= 0;
            InfoBuf.ProviderInfo.MinimumLookaheadData	= 0;
            InfoBuf.ProviderInfo.MaximumLookaheadData	= 0;
            InfoBuf.ProviderInfo.NumberOfResources		= 0;
            InfoBuf.ProviderInfo.StartTime.LowPart		= 0;
            InfoBuf.ProviderInfo.StartTime.HighPart	    = 0;
            break;
    
        case TDI_QUERY_ADDRESS_INFO:
            InfoSize = 
                offsetof(TDI_ADDRESS_INFO,
                         Address.Address[0].AddressType) +
                sizeof(SOCKADDR_IRDA);
    
            if (IsConn) // Extract the local address from the Connection
            {
                PIRLMP_CONNECTION pConn;
            
                pConn = GETCONN(pTDIReq->Handle.ConnectionContext);
    
                if (pConn == NULL)
                {
                    TdiStatus = TDI_INVALID_CONNECTION;
                    goto done;
                }
    
                VALIDCONN(pConn);
                GET_CONN_LOCK(pConn);
    
                InfoBuf.AddrInfo.ActivityCount          = 1; // What is this?
                InfoBuf.AddrInfo.Address.TAAddressCount = 1;
                InfoBuf.AddrInfo.Address.Address[0].AddressLength =
                    sizeof(SOCKADDR_IRDA) - 2;
                // Start copy at AddressType.
                memcpy((BYTE *) &InfoBuf.AddrInfo.Address.Address[0].AddressType, 
                       (BYTE *) &pConn->SockAddrLocal, sizeof(SOCKADDR_IRDA));
    
                FREE_CONN_LOCK(pConn);
                REFDELCONN(pConn);
            }
            else        // Extract the local address from the Address Object
            {
                PIRLMP_ADDR_OBJ pAddrObj;
    
                pAddrObj = GETADDR(pTDIReq->Handle.AddressHandle);
    
                if (pAddrObj == NULL)
                {
                    TdiStatus =  TDI_ADDR_INVALID;
                    goto done;
                }
    
                VALIDADDR(pAddrObj);
                GET_ADDR_LOCK(pAddrObj);
    
                InfoBuf.AddrInfo.ActivityCount          = 1; // What is this?
                InfoBuf.AddrInfo.Address.TAAddressCount = 1;
                InfoBuf.AddrInfo.Address.Address[0].AddressLength =
                    sizeof(SOCKADDR_IRDA) - 2;
                // Start copy at AddressType.
                memcpy((BYTE *) &InfoBuf.AddrInfo.Address.Address[0].AddressType,
                       (BYTE *) &pAddrObj->SockAddrLocal, sizeof(SOCKADDR_IRDA));
    
                FREE_ADDR_LOCK(pAddrObj);
                REFDELADDR(pAddrObj);
            }
    
            break;
    
        default:
            TdiStatus = TDI_INVALID_QUERY;
            goto done;
    }

    if (*pNDISBufSize < (ULONG) InfoSize)
    {
        TdiStatus = TDI_BUFFER_OVERFLOW;
        goto done;
    }

    Offset = 0;
    CopyFlatToNdis(pNDISBuf, (uchar *) &InfoBuf, InfoSize, &Offset);

    *pNDISBufSize = InfoSize;

done:

    DEBUGMSG(ZONE_FUNCTION,
        (TEXT("-IRLMP_QueryInformation [%#x (%d)]\r\n"), TdiStatus, TdiStatus));
    
    return (TdiStatus);
}

/*****************************************************************************
*
*   @func   TDI_STATUS  |   IRLMP_SetInformation    | THIS IS NEVER CALLED.
*/

TDI_STATUS 
IRLMP_SetInformation(PTDI_REQUEST pTDIReq,
                     uint         SetType, 
                     PNDIS_BUFFER pNDISBuf,
                     uint         BufLen,
                     uint         IsConn)
{
	DEBUGMSG(ZONE_FUNCTION,
        (TEXT("IRLMP_SetInformation(0x%X,0x%X,0x%X,0x%X,0x%X)\r\n"), 
        pTDIReq, SetType, pNDISBuf, BufLen, IsConn));

    return(TDI_SUCCESS);
}

/*****************************************************************************
*
*   @func   TDI_STATUS  |   IRLMP_Action    | THIS IS NEVER CALLED.
*/

TDI_STATUS 
IRLMP_Action(PTDI_REQUEST pTDIReq,
             uint         ActionType, 
             PNDIS_BUFFER pNDISBuf,
             uint         BufLen)
{
	DEBUGMSG(ZONE_FUNCTION,
        (TEXT("IRLMP_Action(0x%X,0x%X,0x%X,0x%X)\r\n"), 
        pTDIReq, ActionType, pNDISBuf, BufLen));

    return(TDI_SUCCESS);
}

/*****************************************************************************
*
*   @func   TDI_STATUS  |   IRLMP_QueryInformationEx    | 
*/

TDI_STATUS
IRLMP_QueryInformationEx(PTDI_REQUEST         pTDIReq, 
                         struct TDIObjectID * pObjId, 
                         PNDIS_BUFFER         pNDISBuf,
                         uint *               pBufLen,
                         void *               pContext)
{
	DEBUGMSG(ZONE_FUNCTION,
        (TEXT("IRLMP_QueryInformationEx(0x%X,0x%X,0x%X,0x%X,0x%X)\r\n"), 
        pTDIReq, pObjId, pNDISBuf, pBufLen, pContext));

    return(TDI_INVALID_REQUEST);
}

/*****************************************************************************
*
*   @func   TDI_STATUS  |   IRLMP_SetInformationEx  | 
*           Set info associated with ADDRESS_OBJECTS or CONNECTIONS.
*
*	@rdesc
*           TDI_SUCCESS (tdistat.h) on success, or (tdistat.h)
*   @errors
*           @ecode  TDI_INVALID_REQUEST         | Request not supported.
*           @ecode  TDI_INVALUD_PARAMETER       | Bad parameter.
*
*   @parm   PTDI_REQUEST            |   pTDIReq |
*           (tdi.h) pTDIReq->Handle.ConnectionContext contains the 
*           CONNECTION_CONTEXT (tdi.h, PVOID) of the IRLMP_CONNECTION. 
*           IRLMP_OpenConnection() assigns this value to be a pointer to
*           the IRLMP_CONNECTION.
*   @parm   struct TDIObjectID *    | pObjId    |
*           (tdiinfo.h) Pointer to input TDIObjectID struct. IrLMP ignores
*           the TCP_SOCKET_WINDOW (tcpinfo.h, ipexport.h) option that occurs
*           on a connect().
*   @parm   void *                  | pBuf      |
*           Pointer to input buffer.
*   @parm   unit                    | BufLen    |   
*           Input buffer len.
*/

TDI_STATUS 
IRLMP_SetInformationEx(PTDI_REQUEST         pTDIReq, 
                       struct TDIObjectID * pObjId,
                       void *               pBuf,
                       uint                 BufLen)
{
    DEBUGMSG(ZONE_FUNCTION,
        (TEXT("IRLMP_SetInformationEx(0x%X,0x%X,0x%X,0x%X)\r\n"), 
        pTDIReq, pObjId, pBuf, BufLen));

    if (pObjId->toi_entity.tei_entity != CO_TL_ENTITY &&
        pObjId->toi_entity.tei_entity != CL_TL_ENTITY)
        // return (*LocalNetInfo.ipi_setinfo)(ID, Buffer, Size);
        return(TDI_INVALID_REQUEST);

	if (pObjId->toi_entity.tei_instance != 0)
        return(TDI_INVALID_REQUEST);

    switch(pObjId->toi_class)
    {
      case INFO_CLASS_PROTOCOL:
        switch(pObjId->toi_type)
        {
          case INFO_TYPE_ADDRESS_OBJECT:
            switch(pObjId->toi_id)
            {
              default:
                return(TDI_INVALID_PARAMETER);
                break;
            }
            break;

          case INFO_TYPE_CONNECTION:
            switch(pObjId->toi_id)
            {
              case TCP_SOCKET_WINDOW:
                return(TDI_SUCCESS);
                break;

              default:
                return(TDI_INVALID_PARAMETER);
                break;
            }
            break;
            
          case INFO_TYPE_PROVIDER:
          default:
            return(TDI_INVALID_PARAMETER);
            break;
        }
        break;
        
      case INFO_CLASS_GENERIC:
        return(TDI_INVALID_PARAMETER);
        break;
        
      case INFO_CLASS_IMPLEMENTATION:
      default:
        return(TDI_INVALID_REQUEST);
        break;
    }

    return(TDI_SUCCESS);
}

/*****************************************************************************
*
*   @func   TDI_STATUS  |   EventConnect  |
*           Winsock connect event handler. Called by IrLMP through
*           pEventConnect stored in the IRLMP_ADDR_OBJ associated with
*           this connection. IrLMP will use an IRLMP_CONNECTION associated
*           with the new connection to save return information.
*           
*	@rdesc
*           TDI_MORE_PROCESSING (tdistat.h) on success, or (tdistat.h)
*   @errors
*           @ecode STATUS_REQUEST_NOT_ACCEPTED (ndis.h) | Not accepted.
*
*   @parm   PVOID               |   pEventContext   |
*           Pointer to context associated with the IRLMP_ADDR_OBJ and event
*           handler supplied by IRLMP_SetEvent().
*   @parm   uint                |   RemAddrLen      |
*           sizeof() buffer pointed to by pRemAddr.
*   @parm   PTRANSPORT_ADDRESS  |   pRemAddr        |
*           Pointer to input TRANSPORT_ADDRESS (tdi.h) struct. IrLMP uses
*           one TA_ADDRESS entry with the Address field containing a 
*           SOCKADDR_IRDA struct minus the first two bytes.
*   @parm   uint                |   UserDataLength  |
*           sizeof() buffer pointed to by pUserData. IrLMP will set to 0.
*   @parm   PVOID               |   pUserData       |
*           Pointer to user data. IrLMP will set to NULL.
*   @parm   uint                |   OptionsLength   |
*           sizeof() buffer pointed to by pOptions. IrLMP will set to 0.
*   @parm   PVOID               |   pOptions        |
*           Pointer to options. IrLMP will set to NULL.
*   @parm   PVOID *             |   ppAcceptingID   |
*           Returns the address of a Winsock endpoint context. IrLMP stores
*           this value in the IRLMP_CONNECTION and returns it to Winsock in 
*           the connect complete event handler.
*   @parm   ConnectEventInfo *  |   *EventInfo      |
*           Pointer to output ConnectEventInfo (tdice.h) struct. The 
*           connect complete event handler is returned in cei_rtn and the
*           Winsock endpoint context is returned in cei_context. The connect
*           complete event handler is called when the conection has been
*           established. IrLMP saves this struct in the IRLMP_CONNECTION.
*           
*   @xref   <f EventConnectComplete>
*/

/*****************************************************************************
*
*   @func   VOID    |   EventConnectComplete  |
*           Winsock connect event complete handler. Called by IrLMP through 
*           EventConectCompleteInfo.cei_rtn stored in the IRLMP_CONNECTION
*           associated with this connection. 
*           
*	@rdesc
*           No return value.
*
*   @parm   PVOID       |   pContext    |
*           Pointer to context associated with the Winsock endpoint (supplied
*           by ConnWinSockHandler).
*           handler. IrLMP saves this in the IRLMP_ADDR_OBJ.
*   @parm   TDI_STATUS  |   TdiStatus   |
*           Final status. Winsock likes a 0 here.
*   @parm   DWORD       |   Param       |
*           Unused.
*
*   @xref   <f EventConnect>
*/

/*****************************************************************************
*
*   @func   VOID    |   ConnectComplete  |
*           Winsock initiated connect complete handler. Called by IrLMP
*           through the pConnCompleteWinSockHandler stored in the 
*           IRLMP_CONNECTION associated with this connection. This pointer is
*           initialized in IRLMP_Connect() and a call to this function
*           completes the WinSock initiated connect.
*           
*	@rdesc
*           No return value.
*
*   @parm   PVOID       |   pContext    |
*           Pointer to context associated with the Winsock endpoint (supplied
*           by IRLMP_Connect and stored in pConnCompleteContext.
*           handler. IrLMP saves this in the IRLMP_ADDR_OBJ.
*   @parm   TDI_STATUS  |   TdiStatus   |
*           Final status. Winsock likes a 0 here.
*   @parm   DWORD       |   Param       |
*           Unused.
*
*   @xref   <f IRLMP_Connect>
*/

/*****************************************************************************
*
*   @func   TDI_STATUS  |   EventDisconnect  |
*           Winsock disconnect event handler. Called by IrLMP through
*           pEventDisconnect stored in the IRLMP_ADDR_OBJ associated with
*           this connection.
*           
*	@rdesc
*           STATUS_SUCCESS (ndis.h) on success.
*
*   @parm   PVOID               |   pEventContext           |
*           Pointer to context associated with the IRLMP_ADDR_OBJ and event
*           handler supplied by IRLMP_SetEvent().
*   @parm   PVOID               | pConnectionContext        |
*           Pointer to ConnectionContext supplied by OpenConnection(). IrLMP
*           saves this in pConnectionContext in the IRLMP_CONNECTION.
*   @parm   uint                | DisconnectDataLength      |
*           sizeof() buffer pointed to by pDisconnectData. IrLMP will set to 0.
*   @parm   PVOID               | pDisconnectData           |
*           Pointer to disconnect data. IrLMP will set to NULL.
*   @parm   uint                | OptionsLength             |
*           sizeof() buffer pointed to by pOptions. IrLMP will set to 0.
*   @parm   PVOID               | pOptions                  |
*           Pointer to options. IrLMP will set to NULL.
*   @parm   ulong               | Flags                     | 
*           (tdi.h) One of:
*           <nl>    TDI_DISCONNECT_WAIT
*           <nl>    TDI_DISCONNECT_ABORT
*           <nl>    TDI_DISCONNECT_RELEASE
*           <nl>    TDI_DISCONNECT_CONFIRM
*           <nl>    TDI_DISCONNECT_ASYNC
*/

/*****************************************************************************
*
*   @func   VOID    |   SendComplete  |
*           Winsock send complete handler. Called by IrLMP though a pointer
*           stored in the data buffer.
*
*	@rdesc
*           No return value.
*
*   @parm   PVOID       |   pContext    |
*           Pointer to context associated with the data buffer.
*   @parm   TDI_STATUS  |   TdiStatus   |
*           Final status. Winsock likes a 0 here.
*   @parm   DWORD       |   BytesSent   |
*           Unused.
*
*   @xref   <f IRLMP_Send>
*/

/*****************************************************************************
*
*   @func   VOID    |   ReceiveComplete  |
*           Winsock recv complete handler. Called by IrLMP though a pointer
*           stored in the data buffer.
*
*	@rdesc
*           No return value.
*
*   @parm   PVOID       |   pContext    |
*           Pointer to context associated with the data buffer.
*   @parm   TDI_STATUS  |   TdiStatus   |
*           Final status. Winsock likes a 0 here.
*   @parm   DWORD       |   BytesSent   |
*           Unused.
*
*   @xref   <f IRLMP_Receive>
*/

/*****************************************************************************
*
*   @func   TDI_STATUS  |   EventReceive |
*           Winsock recv event handler. Called by IrLMP through 
*           pEventReceive stored in the IRLMP_ADDR_OBJ associated with
*           this connection. This routine is called when:
*           <nl>    There is no outstanding receive request.
*           <nl>    The client has accepted all indicated data.
*<nl>
*<nl>                    Winsock
*<nl> WinSock App        / AFD              IrDATDI            IrLMP
*<nl> -----------        -------            -------            -----    
*<nl>
*<nl> Copy directly into user buffer
*<nl> ------------------------------
*<nl> recv(UserBuf1) --<gt> IRLMP_Recv(
*<nl>                      UserBuf1) -----<gt> save UserBuf1
*<nl>                               <lt>------ return
*<nl>                                                          IRLMP_DATA.Ind(
*<nl>                                                     <lt>----- Buf1)
*<nl>                                       copy(UserBuf1,
*<nl>                                         Buf1)
*<nl>                                       return ------------<gt>
*<nl>                           <lt>---------- RecvComplete()
*<nl>                    return ----------<gt>
*<nl>               <lt>--- return
*<nl>
*<nl> Copy into a Winsock allocated buffer
*<nl> ------------------------------------
*<nl>                                                          IRLMP_DATA.Ind(
*<nl>                                                   <lt>------- Buf2)
*<nl>                                       EventRecv(
*<nl>                                  <lt>----- Buf2)
*<nl>                    [ignore Buf2]
*<nl>                    alloc(WBuf1)
*<nl>                    return WBuf1 ----<gt> 
*<nl>                                       copy(WBuf1,
*<nl>                                         Buf2)
*<nl>                                       return ------------<gt>
*<nl>                           <lt>---------- EventRecvComplete()
*<nl>                    return ----------<gt>
*<nl>
*
* EventRecv Parms
* ---------------
*
* Indicated  = number of bytes IrLMP is passing to Winsock
* Available  = number of bytes IrLMP has available to pass to Winsock
* BytesTaken = number of bytes Winsock copied from IrLMP [always 0]
*
*
* If EventRecv() returns STATUS_MORE_PROCESSING_REQUIRED, Winsock can not
* accept more data. IrLMP is flow controlled until the next IrLMP_Recv().
*
* If, after the NDIS_BUFFER supplied by EventRecv() is filled and the 
* EventRecv() callback is complete, Winsock has not accepted all data that
* has been indicated by IrLMP, IrLMP is flow controlled until the next
* IrLMP_Recv().
*           
*	@rdesc
*           TDI_MORE_PROCESSING (tdistat.) - no data taken; erb returned
*   @errors
*           @ecode TDI_SUCCESS      |   *Taken = Available - data tossed
*           @ecode TDI_NOT_ACCEPTED |   Flow controlled?
*
*   @parm   PVOID               |   pEventContext       |
*           Pointer to context associated with the IRLMP_ADDR_OBJ and event
*           handler supplied by IRLMP_SetEvent().
*   @parm   PVOID               | pConnectionContext    |
*           Pointer to ConnectionContext supplied by OpenConnection(). IrLMP
*           saves this in pConnectionContext in the IRLMP_CONNECTION.
*   @parm   ulong               | Flags                 |
*           (tdi.h) One of:
*           <nl>    TDI_RECEIVE_TRUNCATED
*           <nl>    TDI_RECEIVE_FRAGMENT
*           <nl>    TDI_RECEIVE_BROADCAST
*           <nl>    TDI_RECEIVE_MULTICAST
*           <nl>    TDI_RECEIVE_PARTIAL
*           <nl>    TDI_RECEIVE_NORMAL
*           <nl>    TDI_RECEIVE_EXPEDITED
*           <nl>    TDI_RECEIVE_PEEK
*           <nl>    TDI_RECEIVE_NO_RESPONSE_EXP
*           <nl>    TDI_RECEIVE_COPY_LOOKAHEAD
*           <nl>    TDI_RECEIVE_ENTIRE_MESSAGE
*           <nl>    TDI_RECEIVE_AT_DISPATCH_LEVEL
*   @parm   unit                |   Indicated           |
*           Number of data bytes in the buffer.
*   @parm   uint                |   Available           |
*           Number of data bytes in the transport.
*   @parm   unit                |   *Taken              |
*           Returns the number of data bytes read.
*   @parm   uchar               |   *Data               |
*           Pointer to data.
*   @parm   EventRcvBuffer      |   *Buffer             |
*           Pointer to output EventRcvBuffer (tdice.h) struct. 
*           This struct is filled in as follows:
*           <nl>    erb_buffer  Pointer to an NDIS_BUFFER.
*	        uint			erb_size;
*	        CTEReqCmpltRtn	erb_rtn;			// Completion routine.
*	        PVOID			erb_context;		// User context.
*	        ushort			*erb_flags;			// Pointer to user flags.
*
*   @xref   <f EventReceiveComplete>
*/

/*****************************************************************************
*
*   @func   VOID    |   EventReceiveComplete  |
*           Winsock receive event complete handler. Called by IrLMP through 
*           EventRecieveCompleteInfo.erb_rtn stored in the data buffer 
*           previously received from WinSock.
*           
*	@rdesc
*           No return value.
*
*   @parm   PVOID       |   pContext    |
*           Pointer to context associated with the data buffer.
*   @parm   TDI_STATUS  |   TdiStatus   |
*           Final status. Winsock likes a 0 here.
*   @parm   DWORD       |   BytesRecvd  |
*           Unused.
*
*   @xref   <f EventReceive>
*/


