//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


 Module Name:    TdiUp.c

 Abstract:       Contains interface from IrLMP up to TDI.

 Contents:

--*/

#include "irdatdi.h"

extern BOOL AutoSuspend;
DWORD g_cActiveConnections = 0;

//
// Allow auto suspend while sockets are opened but there are no active connections.
//
void IrdaAutoSuspend(BOOL bAddConn)
{
    BOOL bCallCxport;
    if (TRUE == AutoSuspend) {
        bCallCxport = FALSE;
        EnterCriticalSection(&csIrObjList);
        if (bAddConn) {
            if (0 == g_cActiveConnections) {
                //
                // Only call the cxport function to disable suspend
                // if the number of active connections goes from 0 to 1.
                //
                bCallCxport = TRUE;
            }
            g_cActiveConnections++;
        } else{
            ASSERT(g_cActiveConnections);
            g_cActiveConnections--;
            if (0 == g_cActiveConnections) {
                //
                // Only call the cxport function to enable suspend
                // if the number of active connections goes from 1 to 0.
                //
                bCallCxport = TRUE;
            }
        }
        LeaveCriticalSection(&csIrObjList);

        if (bCallCxport) {
            CTEIOControl(
                bAddConn ? CTE_IOCTL_SET_IDLE_TIMER_RESET : CTE_IOCTL_CLEAR_IDLE_TIMER_RESET,
                NULL, 0, NULL, 0, NULL);
        }
    }
}   // IrdaAutoSuspend


/*++

 Function:       IrlmpDiscoveryConf

 Description:    Processes an IRLMP_DISCOVERY_CONFIRM. Completes a client
                 discovery request. The discovery request is stored in 
                 globals data.

 Comments:

--*/

VOID
IrlmpDiscoveryConf(IRDA_MSG *pIrDAMsg)
{
    IRDA_DEVICE       *pIrDADevice;         
    int                NickNameByte;
    int                NumDevices;
    DWORD              OldPermissions;

    DEBUGMSG(ZONE_TDI, (TEXT("TdiUp: +IRLMP_DISCOVERY_CONF\r\n")));
    
    EnterCriticalSection(&csDscv);

    if (DscvInProgress) {
    
        OldPermissions = SetProcPermissions(DscvCallersPermissions);
    
        pIrDADevice = (IRDA_DEVICE *) pIrDAMsg->IRDA_MSG_pDevList->Flink;
    
        if (pIrDAMsg->IRDA_MSG_pDevList == (LIST_ENTRY *) pIrDADevice)
        {
            NumDevices = 0;
        }
        else
        {
            NumDevices = 1;
            while (pIrDAMsg->IRDA_MSG_pDevList != (LIST_ENTRY *) pIrDADevice &&
                   NumDevices <= DscvNumDevices)
            {
                memcpy(&pDscvDevList->Device[NumDevices - 1].irdaDeviceID[0],
                       &pIrDADevice->DevAddr[0], IRDA_DEV_ADDR_LEN);
    
                // Retrieve the first 2 hint bytes
                pDscvDevList->Device[NumDevices-1].irdaDeviceHints1 = pIrDADevice->DscvInfo[0];
                pDscvDevList->Device[NumDevices-1].irdaDeviceHints2 = pIrDADevice->DscvInfo[1];
    
                // Skip additional hint bytes.
                NickNameByte = 0;
                while (pIrDADevice->DscvInfo[NickNameByte++] & 0x80);
            
                // Get the char set byte
                pDscvDevList->Device[NumDevices-1].irdaCharSet = pIrDADevice->DscvInfo[NickNameByte++];
    
                // copy the nickame
                memcpy(pDscvDevList->Device[NumDevices-1].irdaDeviceName, &(pIrDADevice->DscvInfo[NickNameByte]), IRDA_MAX_DEVICE_NAME);
                pDscvDevList->numDevice = NumDevices++;
            
                pIrDADevice = (IRDA_DEVICE *) pIrDADevice->Linkage.Flink;
            }
        }
    
        SetProcPermissions(OldPermissions);
        SetEvent(DscvConfEvent);
    }

    LeaveCriticalSection(&csDscv);

    DEBUGMSG(ZONE_TDI, (TEXT("TdiUp: -IRLMP_DISCOVERY_CONF\r\n")));
    return;
}

/*++

 Function:       IrlmpDiscoveryInd

 Description:    Process an IRLMP_DISCOVERY_IND.

 Comments:

--*/

__inline VOID
IrlmpDiscoveryInd()
{
    // Release all threads waiting on the DiscoveryIndication
    DEBUGMSG (ZONE_TDI, (TEXT("IRLMP_DISCOVERY_IND\r\n")));
    SetEvent(DscvIndEvent);

    // This is a manual reset event.  Anyone who wants to wait for the next
    // DISCOVERY_IND should reset the event before leaving the critical
    // section and doing the WaitForSingleObject()
}

/*++

 Function:       IrlmpConnectInd

 Description:    Processes an IRLMP_CONNECT_IND. Calls client connect
                 handler if we can find a matching address object.

 Comments:

--*/

VOID
IrlmpConnectInd(IRDA_MSG *pIrDAMsg)
{
    IRDA_MSG           IrDAMsg;
    TDI_STATUS         TdiStatus;
    int                rc;
    int                i;

    // IRLMP_CONNECT_IND
    BYTE               RemAddrBuf[sizeof(TRANSPORT_ADDRESS) +
                                  sizeof(SOCKADDR_IRDA) - 3];
    PTRANSPORT_ADDRESS pRemAddr = (PTRANSPORT_ADDRESS) RemAddrBuf;
    PSOCKADDR_IRDA     pSockAddr;
    void              *pConnectionContext;
    ConnectEventInfo   EventConnectCompleteInfo;
    int                Val, Digit;
    char               TmpStr[4];
    int                StrLen;
    PIRLMP_ADDR_OBJ    pAddrObj  = NULL;
    PIRLMP_CONNECTION  pConn     = NULL;
    PLIST_ENTRY        pAddrEntry, pConnEntry;

    DEBUGMSG(ZONE_TDI, (TEXT("TdiUp: +IRLMP_CONNECT_IND\r\n")));
    
    EnterCriticalSection(&csIrObjList);
    
    for (pAddrEntry = IrAddrObjList.Flink;
         pAddrEntry != &IrAddrObjList;
         pAddrEntry = pAddrEntry->Flink)
    {
        if (((PIRLMP_ADDR_OBJ)pAddrEntry)->LSAPSelLocal == pIrDAMsg->IRDA_MSG_LocalLsapSel)
        {
            pAddrObj = (PIRLMP_ADDR_OBJ)pAddrEntry;
            REFADDADDR(pAddrObj);
            VALIDADDR(pAddrObj);
            break;
        }
    }

    LeaveCriticalSection(&csIrObjList);

    if (NULL == pAddrObj)
    {
        //
        // No such address
        //
        IrDAMsg.Prim                 = IRLMP_DISCONNECT_REQ;
        IrDAMsg.IRDA_MSG_pDiscData   = NULL;
        IrDAMsg.IRDA_MSG_DiscDataLen = 0;
            
        rc = IrlmpDown(pIrDAMsg->IRDA_MSG_pContext, &IrDAMsg);
        ASSERT(rc == 0);
        goto done;
    }

    if (NULL == pAddrObj->pEventConnect)
    {
        //
        // Address not ready for connections (listen not called yet)
        //
        goto done;
    }

    pRemAddr->TAAddressCount           = 1;
    pRemAddr->Address[0].AddressLength = sizeof(SOCKADDR_IRDA) - 2;

    pSockAddr = (PSOCKADDR_IRDA) &pRemAddr->Address[0].AddressType;

    pSockAddr->irdaAddressFamily = AF_IRDA;

    memcpy(&pSockAddr->irdaDeviceID[0],
           &pIrDAMsg->IRDA_MSG_RemoteDevAddr[0], 
           IRDA_DEV_ADDR_LEN);
    
    Val = pIrDAMsg->IRDA_MSG_RemoteLsapSel;            
    StrLen = 0;
    while (Val > 0 && StrLen < 3)
    {
        Digit = Val % 10;
        Val = Val / 10;
        TmpStr[StrLen] = Digit + '0';
        StrLen++;
    }

    memcpy(&pSockAddr->irdaServiceName[0], LSAPSEL_TXT, LSAPSEL_TXTLEN);
        
    for (i = 0; i < StrLen; i++)
    {
        pSockAddr->irdaServiceName[i + 8] = TmpStr[StrLen - 1 - i];
    }

    pSockAddr->irdaServiceName[StrLen + 8] = 0;

    TdiStatus = pAddrObj->pEventConnect(
                    pAddrObj->pEventConnectContext,
                    offsetof(TRANSPORT_ADDRESS, 
                             Address[0].AddressType) + 
                    sizeof(SOCKADDR_IRDA),
                    pRemAddr,
                    0, NULL, 0, NULL,
                    &pConnectionContext,
                    &EventConnectCompleteInfo);

    if (TdiStatus != TDI_MORE_PROCESSING)
    {
        // The only error that we expect?
        ASSERT(TdiStatus == STATUS_REQUEST_NOT_ACCEPTED);
        goto done;
    }
    
    EnterCriticalSection(&csIrObjList);

    for (pConnEntry = pAddrObj->ConnList.Flink;
         pConnEntry != &pAddrObj->ConnList;
         pConnEntry = pConnEntry->Flink)
    {
        if (((PIRLMP_CONNECTION)pConnEntry)->pConnectionContext == pConnectionContext)
        {
            pConn = (PIRLMP_CONNECTION)pConnEntry;
            REFADDCONN(pConn);
            VALIDCONN(pConn);
            break;
        }
    }

    LeaveCriticalSection(&csIrObjList);

    if (pConn == NULL)
    {
        IrDAMsg.Prim                 = IRLMP_DISCONNECT_REQ;
        IrDAMsg.IRDA_MSG_pDiscData   = NULL;
        IrDAMsg.IRDA_MSG_DiscDataLen = 0;
            
        rc = IrlmpDown(pIrDAMsg->IRDA_MSG_pContext, &IrDAMsg);
        ASSERT(rc == 0);

        EventConnectCompleteInfo.cei_rtn(
            EventConnectCompleteInfo.cei_context,
            TDI_NO_FREE_ADDR, 
            0);
        goto done;
    }
    
    GET_CONN_LOCK(pConn);

    ASSERT(pConn->ConnState == IRLMP_CONN_CREATED);
    
    pConn->ConnState             = IRLMP_CONN_OPEN;
    pConn->LSAPSelRemote         = pIrDAMsg->IRDA_MSG_RemoteLsapSel;
    pConn->SendMaxSDU            = pIrDAMsg->IRDA_MSG_MaxSDUSize;
    pConn->SendMaxPDU            = pIrDAMsg->IRDA_MSG_MaxPDUSize;
    pConn->pIrLMPContext         = pIrDAMsg->IRDA_MSG_pContext;
    pConn->TinyTPRecvCreditsLeft = TINY_TP_RECV_CREDITS;
    // IRDA_MSG_pQos ignored

    memcpy(&pConn->SockAddrRemote, pSockAddr, sizeof(SOCKADDR_IRDA));

    FREE_CONN_LOCK(pConn);

    IrdaAutoSuspend(TRUE);    // new active connection

    IrDAMsg.Prim                 = IRLMP_CONNECT_RESP;
    IrDAMsg.IRDA_MSG_pConnData   = NULL;      
    IrDAMsg.IRDA_MSG_ConnDataLen = 0;      
    IrDAMsg.IRDA_MSG_pContext    = pConn;      
    IrDAMsg.IRDA_MSG_MaxSDUSize  = TINY_TP_RECV_MAX_SDU;
    IrDAMsg.IRDA_MSG_TtpCredits  = TINY_TP_RECV_CREDITS;

    rc = IrlmpDown(pIrDAMsg->IRDA_MSG_pContext, &IrDAMsg);
    ASSERT(rc == 0);

    EventConnectCompleteInfo.cei_rtn(
        EventConnectCompleteInfo.cei_context,
        TDI_SUCCESS, 
        0);

done:
    
    if (pAddrObj)   REFDELADDR(pAddrObj);
    if (pConn)      REFDELCONN(pConn);

    DEBUGMSG(ZONE_TDI, (TEXT("TdiUp: -IRLMP_CONNECT_IND\r\n")));

    return;
}


extern void
LingerTimerFunc(
    CTEEvent * pCTEEvent,
    void * pContext
    );

/*++

 Function:       IrlmpDisconnectInd

 Description:    Processes an IRLMP_DISCONNECT_IND.

 Comments:

--*/

VOID
IrlmpDisconnectInd(void *pConnContext, IRDA_MSG *pIrDAMsg)
{
    TDI_STATUS         TdiStatus;
    PIRLMP_CONNECTION  pConn;
    BOOL fConnPending;

    DEBUGMSG(ZONE_TDI, (TEXT("TdiUp: +IRLMP_DISCONNECT_IND\r\n")));
    
    pConn = GETCONN(pConnContext);
    
    if (pConn == NULL)
    {
        DEBUGMSG(ZONE_WARN,
            (TEXT("IrlmpDisconnectInd - invalid connection handle.\r\n")));
        
        return;
    }
    
    VALIDCONN(pConn);

    GET_CONN_LOCK(pConn);

    DEBUGMSG(ZONE_WARN,
             (TEXT("Got IrLMP Disconnect IND: ConnState=%d DiscReason=%d\r\n"),
              pConn->ConnState, pIrDAMsg->IRDA_MSG_DiscReason)
             );

    //
    // Make IRLMP_Disconnect quit its disconnect process because a disconnect indication
    // from the peer makes the LsapCb go away no matter what the connection state.
    //
    pConn->pIrLMPContext = NULL;

    if (pConn->ConnState == IRLMP_CONN_OPENING)
    {
        switch (pIrDAMsg->IRDA_MSG_DiscReason)
        {
            case IRLMP_MAC_MEDIA_BUSY:
                DEBUGMSG(ZONE_WARN, (TEXT("GOT IRLMP_MAC_MEDIA_BUSY in TDI:TdiUp\r\n")));
                /* wmz retry the connect */
                TdiStatus = TDI_NET_DOWN;
                break;

            case IRLMP_IRLAP_REMOTE_DISCOVERY_IN_PROGRESS:
                /* wmz retry the connect after DISCOVERY_IND */
                DEBUGMSG(ZONE_WARN, (TEXT("GOT IRLMP_IRLAP_REMOTE_DISCOVERY_IN_PROGRESS in TDI:TdiUp\r\n")));
                TdiStatus = TDI_NET_DOWN;
                break;

            case IRLMP_DISC_LSAP:
                TdiStatus = TDI_CONN_REFUSED;
                break;

            case IRLMP_NO_RESPONSE_LSAP:
                TdiStatus = TDI_TIMED_OUT;
                break;

            default:
                TdiStatus = TDI_CONNECTION_ABORTED;
                break;
        }

        fConnPending = pConn->fConnPending;
        pConn->fConnPending = FALSE;
        pConn->ConnState    = IRLMP_CONN_CLOSING;

        FREE_CONN_LOCK(pConn);

        if (fConnPending && pConn->pConnectCompleteContext) {
            pConn->pConnectComplete(pConn->pConnectCompleteContext, 
                                    TdiStatus, 0);
            // IRLMP_Connect left us an extra ref to take care of.
            REFDELCONN(pConn);
        }
    }
    else if (pConn->ConnState == IRLMP_CONN_OPEN)
    {
        switch (pIrDAMsg->IRDA_MSG_DiscReason)
        {
            case IRLMP_USER_REQUEST:
                TdiStatus = TDI_DISCONNECT_WAIT;
                break;

            case IRLMP_UNEXPECTED_IRLAP_DISC:
                if (pConn->UseSharpMode)
                {
                    // If special sharp mode is set then
                    // we pretend that an IRLAP_DISC is really
                    // just a normal close.
                    // Major hack but there are a lot of Zaurus's
                    // out in the world.
                    DEBUGMSG (ZONE_WARN, (TEXT("Got IRLAP_DISC in SharpMode\r\n")));
                    TdiStatus = TDI_DISCONNECT_WAIT;
                    break;
                }
                // Else fall into following
            case IRLMP_IRLAP_RESET:
            case IRLMP_IRLAP_CONN_FAILED:
            case IRLMP_UNSPECIFIED_DISC:
                TdiStatus = TDI_DISCONNECT_ABORT;
                break;

            default:
                TdiStatus = TDI_DISCONNECT_ABORT;
                break;
        }

        if (TdiStatus == TDI_DISCONNECT_WAIT)
        {
            DEBUGMSG(ZONE_WARN, (TEXT("IrlmpDisconnectInd - Starting linger timer.\r\n")));

            pConn->TdiStatus = TdiStatus;
            pConn->LastSendsRemaining = pConn->SendsRemaining;
            pConn->LingerTime = 10000;
            CTEStopTimer(&pConn->LingerTimer);
            CTEInitTimer(&pConn->LingerTimer);
            CTEStartTimer(&pConn->LingerTimer, 1000, LingerTimerFunc, pConn);
            FREE_CONN_LOCK(pConn);
        }
        else
        {
            BOOL fIndicateRecv = FALSE;

            pConn->ConnState = IRLMP_CONN_CLOSING;

            if (pConn->pUsrNDISBuff)
            {
                pConn->pUsrNDISBuff = NULL;
                pConn->UsrBuffLen   = 0;
                fIndicateRecv       = TRUE;
            }

            FREE_CONN_LOCK(pConn);

            IrdaAutoSuspend(FALSE);    // one less active connection

            if (fIndicateRecv == TRUE &&
                TdiStatus != TDI_DISCONNECT_ABORT)
            {
                pConn->pRecvComplete(
                    pConn->pRecvCompleteContext,
                    TDI_SUCCESS,
                    0);
            }

            if (pConn->pAddrObj->pEventDisconnect(
                 pConn->pAddrObj->pEventDisconnectContext,
                 pConn->pConnectionContext, 0, NULL, 0, NULL, 
                 TdiStatus) != STATUS_SUCCESS)
            {
                DEBUGMSG(ZONE_ERROR, (TEXT("TdiUp() EventDisconnect() failed\r\n")));
            }

            if (fIndicateRecv == TRUE &&
                TdiStatus == TDI_DISCONNECT_ABORT)
            {
                pConn->pRecvComplete(
                    pConn->pRecvCompleteContext,
                    TDI_CONNECTION_RESET,
                    0);
            }
        }
    }
    else 
    {
        FREE_CONN_LOCK(pConn);
        DEBUGMSG(ZONE_WARN, (TEXT("TdiUp() IRLMP_DISC_IND ignored\r\n")));
    }

    REFDELCONN(pConn);

    DEBUGMSG(ZONE_TDI, (TEXT("TdiUp: -IRLMP_DISCONNECT_IND\r\n")));
    return;
}

UINT ConnConfDiscReq(void  *pIrlmpContext) 
{
    // Disconnect because either the connection context has gone away, 
    // or is about to go away (IRLMP_CONN_CLOSING).

    IRDA_MSG IMsg;
    UINT     rc;
    
    DEBUGMSG(ZONE_WARN,
        (TEXT("ConnConfDiscReq - invalid connection handle or closing.\r\n")));

    IMsg.Prim                 = IRLMP_DISCONNECT_REQ;
    IMsg.IRDA_MSG_pDiscData   = NULL;
    IMsg.IRDA_MSG_DiscDataLen = 0;

    rc = IrlmpDown(pIrlmpContext, &IMsg);

    return (rc);
}

/*++

 Function:       IrlmpConnectConf

 Description:    Processes an IRLMP_CONNECT_CONF.

 Comments:

--*/

VOID
IrlmpConnectConf(void *pConnContext, IRDA_MSG *pIrDAMsg)
{
    int                 rc;
    IRDA_MSG            IrDAMsg;
    PIRLMP_CONNECTION   pConn;

    DEBUGMSG(ZONE_TDI, (TEXT("TdiUp: +IRLMP_CONNECT_CONF\r\n")));
    
    pConn = GETCONN(pConnContext);

    if (pConn == NULL)
    {
        // If we get a close while trying to connect, our connection
        // object.

        // We don't expect to get here, because we will keep an extra
        // ref on the connection if a connection is pending.
        // Normally we don't keep an extra ref on pending requests, but
        // we need to be able to do a connect complete to AFD, other
        // wise, AFD blocks....

        DEBUGMSG(ZONE_WARN,
            (TEXT("IrlmpConnectConf - connection gone.\r\n")));
        
        rc = ConnConfDiscReq(pIrDAMsg->IRDA_MSG_pContext);
        ASSERT(rc == 0);
        goto done;
    }

    VALIDCONN(pConn);
    GET_CONN_LOCK(pConn);

    if (pConn->ConnState == IRLMP_CONN_OPENING)
    {
        pConn->pIrLMPContext = pIrDAMsg->IRDA_MSG_pContext;
        pConn->SendMaxSDU    = pIrDAMsg->IRDA_MSG_MaxSDUSize;
        pConn->SendMaxPDU    = pIrDAMsg->IRDA_MSG_MaxPDUSize;

        if (pConn->UseExclusiveMode || pConn->UseIrLPTMode)
        {
            IrDAMsg.Prim                 = IRLMP_ACCESSMODE_REQ;
            IrDAMsg.IRDA_MSG_AccessMode  = IRLMP_EXCLUSIVE;

            IrDAMsg.IRDA_MSG_IrLPTMode = pConn->UseIrLPTMode;

            FREE_CONN_LOCK(pConn);
            if ((rc = IrlmpDown(pConn->pIrLMPContext, &IrDAMsg)) != 0)
            {
                pConn->fConnPending = FALSE;
                if (pConn->pConnectCompleteContext) {
                    if (rc == IRLMP_IN_MULTIPLEXED_MODE)
                    {
                        pConn->pConnectComplete(
                            pConn->pConnectCompleteContext, 
                            TDI_LINK_BUSY, 
                            0);
                    }
                    else if (rc == IRLMP_IN_EXCLUSIVE_MODE)
                    {
                        pConn->pConnectComplete(
                            pConn->pConnectCompleteContext, 
                            TDI_SUCCESS, 
                            0);
                    }
                    else
                    {
                        ASSERT(0);
                    }
                }
                // IRLMP_Connect left us an extra ref to take care of.
                REFDELCONN(pConn);
            }
            // If the ACCESSMODE_REQ returned success -- really pending. Keep
            // our extra reference.
        }
        else
        {
            pConn->ConnState = IRLMP_CONN_OPEN;
            pConn->fConnPending = FALSE;
            FREE_CONN_LOCK(pConn);
            if (pConn->pConnectCompleteContext) {
                IrdaAutoSuspend(TRUE);    // new active connection
                pConn->pConnectComplete(pConn->pConnectCompleteContext, 
                                        TDI_SUCCESS, 0);
            }
            // IRLMP_Connect left us an extra ref to take care of.
            REFDELCONN(pConn);
        }
    }
    else  
    {
        // ConnState = IRLMP_CONN_CLOSING, IRLMP_CONN_CREATED, IRLMP_CONN_OPEN
        ASSERT(pConn->ConnState == IRLMP_CONN_CLOSING);
        FREE_CONN_LOCK(pConn);

        DEBUGMSG(ZONE_WARN,
            (TEXT("IrlmpConnectConf - connection closing.\r\n")));
        
        rc = ConnConfDiscReq(pIrDAMsg->IRDA_MSG_pContext);
        ASSERT(rc == 0);

        if (pConn->pConnectCompleteContext) {
            pConn->pConnectComplete(
                pConn->pConnectCompleteContext,
                TDI_CONNECTION_ABORTED,
                0);
        }
        // IRLMP_Connect left us an extra ref to take care of.
        REFDELCONN(pConn);
    }

    REFDELCONN(pConn);

done:

    DEBUGMSG(ZONE_TDI, (TEXT("TdiUp: -IRLMP_CONNECT_CONF\r\n")));
    return;
}

/*++

 Function:       IrlmpGetValueByClassConf

 Description:    Process an IRLMP_GETVALUEBYCLASS_CONF.

 Arguments:

--*/

__inline VOID
IrlmpGetValueByClassConf(IRDA_MSG *pIrDAMsg)
{
    DEBUGMSG(ZONE_TDI, (TEXT("TdiUp: +IRLMP_GETVALUEBYCLASS_CONF\r\n")));
    
    EnterCriticalSection(&csIasQuery);
    IASQueryStatus = pIrDAMsg->IRDA_MSG_IASStatus;
    SetEvent(IASQueryConfEvent);
    LeaveCriticalSection(&csIasQuery);

    DEBUGMSG(ZONE_TDI, (TEXT("TdiUp: -IRLMP_GETVALUEBYCLASS_CONF\r\n")));
    return;
}

/*++

 Function:       IrlmpDataConf

 Description:    Process an IRLMP_DATA_CONF.

 Comments:

--*/

VOID
IrlmpDataConf(void *pConnContext, IRDA_MSG *pIrDAMsg)
{
    PIRLMP_CONNECTION  pConn;

    DEBUGMSG(ZONE_TDI, (TEXT("TdiUp: +IRLMP_DATA_CONF\r\n")));

    if (pIrDAMsg->IRDA_MSG_DataStatus == IRLMP_DATA_REQUEST_COMPLETED)
    {
        ((PIRLMP_SEND_COMPLETE) pIrDAMsg->IRDA_MSG_pTdiSendComp)(
            pIrDAMsg->IRDA_MSG_pTdiSendCompCnxt, 
            TDI_SUCCESS, 
            pIrDAMsg->IRDA_MSG_SendLen);

        pConn = GETCONN(pConnContext);

        if (pConn)
        {
            VALIDCONN(pConn);
            GET_CONN_LOCK(pConn);

            if (pConn->SendsRemaining) {
                pConn->SendsRemaining--;
            }

            FREE_CONN_LOCK(pConn);
            REFDELCONN(pConn);
        }
    }
    else
    {
        ((PIRLMP_SEND_COMPLETE) pIrDAMsg->IRDA_MSG_pTdiSendComp)(
            pIrDAMsg->IRDA_MSG_pTdiSendCompCnxt, 
            TDI_GRACEFUL_DISC, 
            0);
    }

    IrdaFree(pIrDAMsg);

    DEBUGMSG(ZONE_TDI, (TEXT("TdiUp: -IRLMP_DATA_CONF\r\n")));
    return;
}

/*++

 Function:       BufferRecv

 Description:    

 Arguments:

 Returns:

 Comments:

--*/

BOOL
BufferRecv(
    PIRLMP_CONNECTION pConn,
    PUCHAR            pData,
    DWORD             cbData,
    BYTE              bFinalSeg
    )
{
    PIRDATDI_RECV_BUFF pRecvBuf;

    pRecvBuf = IrdaAlloc(
        (offsetof(IRDATDI_RECV_BUFF, Data) + cbData),
        MT_TDI_RECV_BUFF);

    if (pRecvBuf == NULL)
    {
        ASSERT(0);
        return (FALSE);
    }

    pConn->RecvQBytes += cbData;
    pRecvBuf->DataLen  = cbData;
    pRecvBuf->pRead    = &pRecvBuf->Data[0];
    pRecvBuf->FinalSeg = bFinalSeg;
    memcpy(pRecvBuf->pRead, pData, cbData);
    InsertHeadList(&pConn->RecvBuffList, &pRecvBuf->Linkage);

    return (TRUE);
}

/*++

 Function:       IrlmpDataInd

 Description:    Process an IRLMP_DATA_IND.

 Comments:

--*/

VOID IrlmpDataInd(void *pConnContext, IRDA_MSG *pIrDAMsg)
{
    IRDA_MSG           IrDAMsg;
    TDI_STATUS         TdiStatus;
    int                rc;
    DWORD              OldPermissions;
    
    // IRLMP_DATA_IND
    EventRcvBuffer     EventRecvCompleteInfo;    
    int                BytesTaken;
    
    PIRLMP_CONNECTION  pConn;

    int             cbCopyToNdisBuf;
    int             cbCopyToRecvBuf;
    PNDIS_BUFFER    pNdisBuf;
    BOOL            fEventReceive;
    BYTE            FinalSeg;
    
    DEBUGMSG(ZONE_TDI, (TEXT("TdiUp: +IRLMP_DATA_IND\r\n")));
    
    pConn = GETCONN(pConnContext);

    if (pConn == NULL)
    {
        DEBUGMSG(ZONE_WARN,
            (TEXT("IrlmpDataInd - invalid connection handle.\r\n")));
        goto done;
    }

    VALIDCONN(pConn);
    GET_CONN_LOCK(pConn);

    if (pConn->ConnState != IRLMP_CONN_OPEN)
    {
        DEBUGMSG(ZONE_WARN, (TEXT("TdiUp() IRLMP_DATA_IND ignored\r\n")));
        // wmz open the window?

        goto done;
    }

    pConn->TinyTPRecvCreditsLeft--;

    //
    // Is this an IrCOMM connection?
    //

    if (pConn->Use9WireMode)
    {
        // 
        // When a packet arrives that has both contrl parameters and data, the 
        // control data will be acted up first, then the data will be 
        // processed. In this case, all processing for control parameters is to
        // ignore (discard). The first byte in an IrCOMM frame is the byte count
        // of the control parameters.
        // 

        BYTE cbControlPacket = *(BYTE *)pIrDAMsg->IRDA_MSG_pRead;

        // Increment past the byte count.
        pIrDAMsg->IRDA_MSG_pRead++;

        DEBUGMSG(ZONE_TDI,
            (TEXT("IrlmpDataInd: IrCOMM frame - ControlParm Len = %d/%d\r\n"),
             cbControlPacket,
             pIrDAMsg->IRDA_MSG_pWrite - pIrDAMsg->IRDA_MSG_pRead));
        
        //  Skip over any control parameters in the packet
        pIrDAMsg->IRDA_MSG_pRead += cbControlPacket;

        // Make sure that we didn't advance beyond the end of the buffer.
        // If so, the peer is violating the protocol.
        if (pIrDAMsg->IRDA_MSG_pRead > pIrDAMsg->IRDA_MSG_pWrite)
			DEBUGMSG(ZONE_ERROR,
				(TEXT("IrlmpDataInd: Invalid IrCOMM frame - ControlParm Len = %d too large\r\n"),
                 cbControlPacket));

        // If there is no data (i.e. there was only control parameters, then
        // skip the data ind path. 
        if (pIrDAMsg->IRDA_MSG_pRead >= pIrDAMsg->IRDA_MSG_pWrite)
        {
            goto IgnoreIRLMP_DATA_IND;
        }
    }
    
    //
    // Four main cases to deal with:
    // 1) Copy all data to pending NDIS buffer. Call CompleteReceive.
    // 2) No pending NDIS buffer. Other data has already been indicated
    //    but not accepted.
    //
    // The above two cases are easy, grab the locks copy the data, get out.
    //
    // 3) Copy some data (as much as possible) to pending NDIS buffer.
    //    Call CompleteReceive. Call IndicateReceive for remaining bytes and Q
    //    the remaining bytes.
    // 4) Need to indicate all the data. Call IndicateReceive and Q bytes.
    //
    // Cases 3 and 4 need to release locks to indicate receive, but before the
    // data is really queued. Therefore, when the locks are re-acquired
    // we need to check to see if a buffer is pending.
    //

    // Default - will buffer recv.
    fEventReceive   = TRUE;
    cbCopyToNdisBuf = 0;
    cbCopyToRecvBuf = pIrDAMsg->IRDA_MSG_pWrite - pIrDAMsg->IRDA_MSG_pRead;
    pNdisBuf        = pConn->pUsrNDISBuff;
    FinalSeg        = (pIrDAMsg->IRDA_MSG_SegFlags & SEG_FINAL) ? 1 : 0;

    DEBUGMSG(ZONE_TDI, 
        (TEXT("TdiUp() got %d bytes, pConn->UsrBuffLen %d\r\n"),
        cbCopyToRecvBuf, 
        pConn->UsrBuffLen));

    do
    {
        // First off, if we have a pending NDIS buffer, fill it.
        if (pNdisBuf)
        {
			PVOID VirtualAddress;
			UINT  BufferLength;

            cbCopyToNdisBuf = min(pConn->UsrBuffLen, 
                pIrDAMsg->IRDA_MSG_pWrite - pIrDAMsg->IRDA_MSG_pRead);
    
            OldPermissions = SetProcPermissions(pConn->UsrBuffPerm);
			NdisQueryBuffer(pNdisBuf, &VirtualAddress, &BufferLength);
            memcpy(
                VirtualAddress,
                pIrDAMsg->IRDA_MSG_pRead,
                cbCopyToNdisBuf);
            SetProcPermissions(OldPermissions);
    
            pIrDAMsg->IRDA_MSG_pRead += cbCopyToNdisBuf;
            cbCopyToRecvBuf          -= cbCopyToNdisBuf;
            
            if (pConn->IndicatedNotAccepted > 0)
            {
                pConn->IndicatedNotAccepted -= cbCopyToNdisBuf;
                ASSERT(pConn->IndicatedNotAccepted >= 0);
                
                // Should not have bytes indicated but not accepted on first
                // iteration through loop.
                ASSERT(fEventReceive == FALSE);
            }

            pConn->pUsrNDISBuff = NULL;
            pConn->UsrBuffLen   = 0;
        }
        else if (pConn->IndicatedNotAccepted > 0)
        {
            // Buffer data and get out -- no indications required.
            BufferRecv(
                pConn, 
                pIrDAMsg->IRDA_MSG_pRead, 
                cbCopyToRecvBuf,
                FinalSeg);
            pConn->NotIndicated += cbCopyToRecvBuf;
            break;
        }
    
        FREE_CONN_LOCK(pConn);
    
        // If we copied data to a pending NDIS buffer, we must complete 
        // the receive.
        if (pNdisBuf)
        {
            pConn->pRecvComplete(
                pConn->pRecvCompleteContext, 
                TDI_SUCCESS,
                cbCopyToNdisBuf);
            pNdisBuf = NULL;
        }
    
        // If we need to queue any data, we indicate it first.
        // Only indicate the first time since we are using the same data.
        if (cbCopyToRecvBuf && fEventReceive)
        {
            TdiStatus = pConn->pAddrObj->pEventReceive(
                pConn->pAddrObj->pEventReceiveContext,
                pConn->pConnectionContext,
                0, cbCopyToRecvBuf, cbCopyToRecvBuf,
                &BytesTaken, NULL,
                &EventRecvCompleteInfo);
        }
    
        GET_CONN_LOCK(pConn);
    
        if (cbCopyToRecvBuf && fEventReceive)
        {
            pConn->IndicatedNotAccepted = cbCopyToRecvBuf;
            fEventReceive = FALSE;
        }

        if (pConn->pUsrNDISBuff)
        {
            // Was a new NDIS buffer pended while we dropped the locks?
            // If so, we will loop and fill the buffer.
            pNdisBuf = pConn->pUsrNDISBuff;
        }
        else if (cbCopyToRecvBuf > 0)
        {
            // Else, queue any bytes.
            if (TDI_NOT_ACCEPTED == TdiStatus) {
                if (BufferRecv(
                    pConn, 
                    pIrDAMsg->IRDA_MSG_pRead, 
                    cbCopyToRecvBuf,
                    FinalSeg) == FALSE)
                {
                    // sh - drop data (failed to allocate buffer)?
                    break;
                }
            }
            cbCopyToRecvBuf = 0;
        }
    }
    while (cbCopyToRecvBuf && pNdisBuf);

#if 0
    if (pConn->pUsrNDISBuff)
    {
        DEBUGMSG(ZONE_TDI, (TEXT("%x IR : +IrlmpDataInd - copy %d bytes to buf\r\n"), 
                     GetCurrentThreadId(), 
                     min(pConn->UsrBuffLen, BytesRecvd)));
        
        DEBUGMSG(ZONE_TDI,
            (TEXT("TdiUp() copying %d bytes\r\n"),
            min(pConn->UsrBuffLen, BytesRecvd)));

        OldPermissions = SetProcPermissions(pConn->UsrBuffPerm);

        memcpy(pConn->pUsrNDISBuff->VirtualAddress,
            pIrDAMsg->IRDA_MSG_pRead,
            min(pConn->UsrBuffLen, BytesRecvd));

        pConn->pUsrNDISBuff->BufferLength =
            min(pConn->UsrBuffLen, BytesRecvd);

        SetProcPermissions(OldPermissions);

        FREE_CONN_LOCK(pConn);

        pConn->pRecvComplete(pConn->pRecvCompleteContext, TDI_SUCCESS,
            min(pConn->UsrBuffLen, BytesRecvd));

        GET_CONN_LOCK(pConn);

        // enqueue the remaining bytes
        if (BytesRecvd > (int) pConn->UsrBuffLen)
        {
            BytesRecvd               -= pConn->UsrBuffLen;
            pIrDAMsg->IRDA_MSG_pRead += pConn->UsrBuffLen;

            DEBUGMSG(ZONE_TDI,
                (TEXT("TdiUp() enQing remaining %d bytes\r\n"),
                BytesRecvd));

            pRecvBuff = IrdaAlloc((offsetof(IRDATDI_RECV_BUFF, Data) + BytesRecvd),
                                 MT_TDI_RECV_BUFF);

            if (pRecvBuff == NULL)
            {
                DEBUGMSG(ZONE_ERROR, (TEXT("TdiUp CTEAllocMem() failed\r\n")));
                ASSERT(0);
            }

            pRecvBuff->DataLen  = BytesRecvd;
            pRecvBuff->pRead    = &pRecvBuff->Data[0];
            pRecvBuff->FinalSeg = (pIrDAMsg->IRDA_MSG_SegFlags & SEG_FINAL) 
                                ? TRUE: FALSE;
            memcpy(pRecvBuff->pRead, pIrDAMsg->IRDA_MSG_pRead, BytesRecvd);

            pConn->RecvQBytes  += BytesRecvd;
            InsertHeadList(&pConn->RecvBuffList, &pRecvBuff->Linkage);

            FREE_CONN_LOCK(pConn);

            DEBUGMSG(ZONE_TDI,
                     (TEXT("%x IR : IrlmpDataInd calling TdiEventReceive %d bytes\r\n"),
                      GetCurrentThreadId(), BytesRecvd)
                     );
            
            TdiStatus = pConn->pAddrObj->pEventReceive(
                pConn->pAddrObj->pEventReceiveContext,
                pConn->pConnectionContext,
                0, BytesRecvd, BytesRecvd,
                &BytesTaken, NULL,
                &EventRecvCompleteInfo);

            ASSERT(TdiStatus == TDI_NOT_ACCEPTED);
            ASSERT(BytesTaken == 0);

            GET_CONN_LOCK(pConn);

            pConn->IndicatedNotAccepted = BytesRecvd;
        }

        pConn->pUsrNDISBuff = NULL;
        pConn->UsrBuffLen   = 0;
    }
    else
    {
        if (pConn->IndicatedNotAccepted > 0)
        {
            DEBUGMSG(ZONE_TDI,
                     (TEXT("%x IR : IrlmpDataInd Qing %d bytes.\r\n"),
                      GetCurrentThreadId(), BytesRecvd)
                     );
            
            pConn->NotIndicated += BytesRecvd;
        }
        else
        {
            FREE_CONN_LOCK(pConn);

            DEBUGMSG(ZONE_TDI,
                     (TEXT("%x IR : IrlmpDataInd2 calling TdiEventReceive %d bytes\r\n"),
                      GetCurrentThreadId(), BytesRecvd)
                     );
            
            TdiStatus = pConn->pAddrObj->pEventReceive(
                pConn->pAddrObj->pEventReceiveContext,
                pConn->pConnectionContext,
                0, BytesRecvd, BytesRecvd,
                &BytesTaken, NULL,
                &EventRecvCompleteInfo);

            GET_CONN_LOCK(pConn);

            pConn->IndicatedNotAccepted = BytesRecvd;

            ASSERT(TdiStatus == TDI_NOT_ACCEPTED);
        }

        DEBUGMSG(ZONE_TDI, (TEXT("TdiUp() enQing %d bytes\r\n"), BytesRecvd));

        pRecvBuff = IrdaAlloc((offsetof(IRDATDI_RECV_BUFF, Data) + BytesRecvd),
                     MT_TDI_RECV_BUFF);

        if (pRecvBuff == NULL)
        {
            DEBUGMSG(ZONE_ERROR, (TEXT("TdiUp CTEAllocMem() failed\r\n")));
            ASSERT(0);
        }
            
        pRecvBuff->DataLen  = BytesRecvd;
        pRecvBuff->pRead    = &pRecvBuff->Data[0];
        pRecvBuff->FinalSeg = (pIrDAMsg->IRDA_MSG_SegFlags & SEG_FINAL)
            ? TRUE : FALSE;
        memcpy(pRecvBuff->pRead, pIrDAMsg->IRDA_MSG_pRead, BytesRecvd);
        pConn->RecvQBytes  += BytesRecvd;
        InsertHeadList(&pConn->RecvBuffList, &pRecvBuff->Linkage);
    }
#endif // 0

IgnoreIRLMP_DATA_IND:

    if (pConn->RecvQBytes < IRDATDI_RECVQ_LEN &&
        pConn->TinyTPRecvCreditsLeft == 0)
    {
        pConn->TinyTPRecvCreditsLeft = TINY_TP_RECV_CREDITS;

        IrDAMsg.Prim                 = IRLMP_MORECREDIT_REQ;
        IrDAMsg.IRDA_MSG_TtpCredits  = TINY_TP_RECV_CREDITS;

        FREE_CONN_LOCK(pConn);

        rc = IrlmpDown(pConn->pIrLMPContext, &IrDAMsg);
        ASSERT(rc == 0);
        
        GET_CONN_LOCK(pConn);
    }

done:

    if (pConn)
    {
        FREE_CONN_LOCK(pConn);
        REFDELCONN(pConn);
    }

    DEBUGMSG(ZONE_TDI, (TEXT("TdiUp: -IRLMP_DATA_IND\r\n")));
    return;
}

/*++

 Function:       IrlmpAccessmodeConf

 Description:    Processes an IRLMP_ACCESSMODE_CONF.

 Comments:

--*/

__inline VOID
IrlmpAccessmodeConf(void *pConnContext, IRDA_MSG *pIrDAMsg)
{
    IRDA_MSG          IrDAMsg;
    int               rc;
    PIRLMP_CONNECTION pConn;

    DEBUGMSG(ZONE_TDI, (TEXT("TdiUp: +IRLMP_ACCESSMODE_CONF\r\n")));

    pConn = GETCONN(pConnContext);

    if (pConn == NULL)
    {
        // We don't expect to get here, because we will keep an extra
        // ref on the connection if a connection is pending.
        // Normally we don't keep an extra ref on pending requests, but
        // we need to be able to do a connect complete to AFD, other
        // wise, AFD blocks....
        DEBUGMSG(ZONE_WARN,
            (TEXT("IrlmpAccessModeConf - invalid connection handle\r\n")));
        ASSERT(FALSE);
        return;
    }

    VALIDCONN(pConn);
    GET_CONN_LOCK(pConn);

    if (pConn->ConnState == IRLMP_CONN_OPENING)
    {
        if (pIrDAMsg->IRDA_MSG_ModeStatus == IRLMP_ACCESSMODE_SUCCESS)
        {
            // assume IRDA_MSG_AccessMode == IRLMP_EXCLUSIVE
            pConn->ConnState  = IRLMP_CONN_OPEN;

            FREE_CONN_LOCK(pConn);
            if (pConn->pConnectCompleteContext) {
                IrdaAutoSuspend(TRUE);    // new active connection
                pConn->pConnectComplete(
                    pConn->pConnectCompleteContext, 
                    TDI_SUCCESS, 
                    0);
            }
        }
        else
        {
            IrDAMsg.Prim                 = IRLMP_DISCONNECT_REQ;
            IrDAMsg.IRDA_MSG_pDiscData   = NULL;
            IrDAMsg.IRDA_MSG_DiscDataLen = 0;

            FREE_CONN_LOCK(pConn);
            rc = IrlmpDown(pConn->pIrLMPContext, &IrDAMsg);
            ASSERT(rc == 0);

            if (pConn->pConnectCompleteContext) {
                pConn->pConnectComplete(
                    pConn->pConnectCompleteContext, 
                    TDI_LINK_BUSY, 
                    0);
            }
        }
        // IRLMP_Connect left us an extra ref to take care of.
        REFDELCONN(pConn);
    }
    else if (pConn->ConnState == IRLMP_CONN_CLOSING && 
             pConn->fConnPending == TRUE)
    {
        pConn->fConnPending = FALSE;
        FREE_CONN_LOCK(pConn);
        if (pConn->pConnectCompleteContext) {
            pConn->pConnectComplete(
                pConn->pConnectCompleteContext, 
                TDI_CONNECTION_ABORTED, 
                0);
        }
        // IRLMP_Connect left us an extra ref to take care of.
        REFDELCONN(pConn);
    }
    else
    {
        FREE_CONN_LOCK(pConn);
    }

    REFDELCONN(pConn);

    DEBUGMSG(ZONE_TDI, (TEXT("TdiUp: -IRLMP_ACCESSMODE_CONF\r\n")));
    return;
}

/*++

 Function:       TdiUp

 Description:    Indications and confirmations for LMP.

 Comments:

--*/

UINT
TdiUp(void *pConnContext, IRDA_MSG *pIrDAMsg)
{
    switch(pIrDAMsg->Prim)
    {
        case IRLMP_DISCOVERY_CONF:
            IrlmpDiscoveryConf(pIrDAMsg);
            break;

        case IRLMP_DISCOVERY_IND:
            IrlmpDiscoveryInd();
            break;

        case IRLMP_CONNECT_IND:
            IrlmpConnectInd(pIrDAMsg);
            break;

        case IRLMP_DISCONNECT_IND:
            IrlmpDisconnectInd(pConnContext, pIrDAMsg);
            break;

        case IRLMP_CONNECT_CONF:
            IrlmpConnectConf(pConnContext, pIrDAMsg);
            break;

        case IRLMP_GETVALUEBYCLASS_CONF:
            IrlmpGetValueByClassConf(pIrDAMsg);
            break;

        case IRLMP_DATA_CONF:
            IrlmpDataConf(pConnContext, pIrDAMsg);
            break;

        case IRLMP_ACCESSMODE_CONF:
            IrlmpAccessmodeConf(pConnContext, pIrDAMsg);
            break;
        
        case IRLMP_DATA_IND:
            IrlmpDataInd(pConnContext, pIrDAMsg);
            break;
        
        case IRLMP_ACCESSMODE_IND:
            break;

        case IRLAP_STATUS_IND:
            // sh: we now do status indications for different link status'. 
            //     It does not mean that communication has been interrupted.
            // DEBUGMSG(1, (TEXT("WARNING: Ir communication has been interrupted\n")));
            break;

        case IRLAP_DISCONNECT_IND:
            DEBUGMSG(1, (TEXT("WARNING: Ir communication has been terminated\n")));
            break;

        default:
            DEBUGMSG(ZONE_ERROR, (TEXT("TdiUp: Bad primitive %d\r\n"), 
                     pIrDAMsg->Prim) );
            ASSERT(0);
            break;
    }

    return(0);
}

