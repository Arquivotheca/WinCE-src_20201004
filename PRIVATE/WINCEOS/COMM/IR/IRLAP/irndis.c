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
*  Module: irndis.c
*
*  This modules provides the MAC interface of IrLAP (formerly IrMAC
*  of CE). It is now an Ndis protocol interface for communicating
*  with Miniport Ir framers.
*
*                         |---------|                               
*                         |         |                               
*                         |  IrLAP  |                               
*                         |         |                               
*                         |---------|                               
*                           /|\  |                                  
*                            |   |                                  
*      IrlapUp(IrlapContext, |   | IrmacDown(LinkContext,           
*              IrdaMsg)      |   |           IrdaMsg)               
*                            |   |                                  
*                            |  \|/                                 
*                         |----------|                              
*                         |          |                              
*                         |  IrNDIS  |                                
*                         |          |                              
*                         |----------|                              
*                            /|\  |                                 
*                             |   | Ndis Interface for transports   
*                             |   |                                  
*                             |  \|/                                
*                  |---------------------------|                   
*                  |      Ndis Wrapper         |                   
*                  |---------------------------|                   
*                        |------------|                            
*                        |            |
*                        |  Miniport  |                            
*                        |   Framer   |
*                        |            |                           
*                        |------------|                            
*                                                                   
*                                                                   
*
*
*/
#include <irda.h>
#include <ntddndis.h>
#include <ndis.h>
#include <irlap.h>
#include <irlapp.h>
#include <irlmp.h>

#ifndef UNDER_CE
#include <zwapi.h>
#endif // !UNDER_CE

#define WORK_BUF_SIZE   256
NDIS_HANDLE             NdisIrdaHandle; // Handle to identify Irda to Ndis
                                        // when opening adapters 
UINT                    DisconnectTime;
BYTE                    CharSet;
UINT                    Hints;
UINT                    Slots;
UINT                    SlotTimeout;
UINT                    DiscoverTimeout;
BOOL                    AutoSuspend;  // Allow the device to suspend if there are no active connections
UCHAR                   NickName[NICK_NAME_LEN + sizeof(WCHAR)];
UINT                    NickNameLen;

#ifdef DEBUG
int DbgSettings =  0; //DBG_RXFRAME | DBG_TXFRAME;
#endif // DEBUG

VOID
ConfirmDataRequest(
    PIRDA_LINK_CB pIrdaLinkCb,
    PIRDA_MSG     pMsg);

VOID IrdaSendComplete(
    IN  NDIS_HANDLE     Context,
    IN  PNDIS_PACKET    NdisPacket,
    IN  NDIS_STATUS     Status
    );

/***************************************************************************
*
*   Translate the result of an OID query to IrLAP QOS definition
*
*/
VOID
OidToLapQos(
    UINT    ParmTable[],
    UINT    ValArray[],
    UINT    Cnt,
    PUINT   pBitField,
    BOOLEAN MaxVal)
{
    UINT    i, j;

    *pBitField = 0;  
    for (i = 0; i < Cnt; i++)
    {
        for (j = 0; j <= PV_TABLE_MAX_BIT; j++)
        {
            if (ValArray[i] == ParmTable[j])
            {
                *pBitField |= 1<<j;
                if (MaxVal)
                    return;
            }
            else if (MaxVal)
            {
                *pBitField |= 1<<j;
            }
        }
    }
}

//
// Wrap an NDIS_REQUEST and an NDIS_EVENT so we don't free pending requests
//
typedef struct _IRDA_NDIS_REQ {
    NDIS_REQUEST Request;
    NDIS_EVENT   ReqEvent;
    NDIS_STATUS  ReqStatus;
} IRDA_NDIS_REQ, * PIRDA_NDIS_REQ;

#define ZOMBIE_THREAD 0xffffffff
#define WAITING_THREAD 0xfffffffe

/***************************************************************************
*
*   Perform a synchronous request for an OID
*
*/
NDIS_STATUS
IrdaQueryOid(
    IN      PIRDA_LINK_CB   pIrdaLinkCb,
    IN      NDIS_OID        Oid,
    OUT     PUINT           pQBuf,
    IN OUT  PUINT           pQBufLen)
{
    PIRDA_NDIS_REQ  pRequest;
    NDIS_STATUS     Status;

    pRequest = IrdaAlloc(sizeof(IRDA_NDIS_REQ), 0);

    if (pRequest == NULL)
    {
        return (NDIS_STATUS_RESOURCES);
    }

    NdisInitializeEvent(&pRequest->ReqEvent);
    pRequest->ReqStatus = WAITING_THREAD;
    
    ((PNDIS_REQUEST)pRequest)->RequestType = NdisRequestQueryInformation;
    ((PNDIS_REQUEST)pRequest)->DATA.QUERY_INFORMATION.Oid = Oid;
    ((PNDIS_REQUEST)pRequest)->DATA.QUERY_INFORMATION.InformationBuffer = pQBuf;
    ((PNDIS_REQUEST)pRequest)->DATA.QUERY_INFORMATION.InformationBufferLength =
        *pQBufLen * sizeof(UINT);

    DEBUGMSG(DBG_NDIS, (TEXT("IrdaQueryOid submit (0x%x)\n"), pRequest));
    NdisRequest(&Status, pIrdaLinkCb->NdisBindingHandle, (PNDIS_REQUEST)pRequest);

    if (Status == NDIS_STATUS_PENDING)
    {
        DEBUGMSG(DBG_NDIS, (TEXT("IrdaQueryOid wait for (0x%x)\n"), pRequest));
        NdisWaitEvent(&pRequest->ReqEvent, 0);
        Status = pRequest->ReqStatus;

        // If we're on a process thread that's being terminated then we won't wait
        // on the event properly
        if (Status == WAITING_THREAD) {
            Status = pRequest->ReqStatus = ZOMBIE_THREAD;
        }
    }

    if (Status != ZOMBIE_THREAD) {
        *pQBufLen =
            ((PNDIS_REQUEST)pRequest)->DATA.QUERY_INFORMATION.BytesWritten / sizeof(UINT);

        NdisFreeEvent(&pRequest->ReqEvent);
        DEBUGMSG(DBG_NDIS, (TEXT("IrdaQueryOid freeing (0x%x)\n"), pRequest));
        IrdaFree(pRequest);
        return Status;
    } else {
        return NDIS_STATUS_RESOURCES;
    }
}

/***************************************************************************
*
*   Perform a synchronous request to sent an OID
*
*/
NDIS_STATUS
IrdaSetOid(
    IN  PIRDA_LINK_CB   pIrdaLinkCb,
    IN  NDIS_OID        Oid,
    IN  PUINT           pSBuf,
    IN  UINT            SBufLen)
{
    PIRDA_NDIS_REQ  pRequest;
    NDIS_STATUS     Status;

    pRequest = IrdaAlloc(sizeof(IRDA_NDIS_REQ), 0);

    if (pRequest == NULL)
    {
        return (NDIS_STATUS_RESOURCES);
    }

    NdisInitializeEvent(&pRequest->ReqEvent);
    pRequest->ReqStatus = WAITING_THREAD;

    ((PNDIS_REQUEST)pRequest)->RequestType = NdisRequestSetInformation;
    ((PNDIS_REQUEST)pRequest)->DATA.SET_INFORMATION.Oid = Oid;
    ((PNDIS_REQUEST)pRequest)->DATA.SET_INFORMATION.InformationBuffer = pSBuf;
    ((PNDIS_REQUEST)pRequest)->DATA.SET_INFORMATION.InformationBufferLength = SBufLen;

    DEBUGMSG(DBG_NDIS, (TEXT("IrdaSetOid submit (0x%x)\n"), pRequest));
    NdisRequest(&Status, pIrdaLinkCb->NdisBindingHandle, (PNDIS_REQUEST)pRequest);

    if (Status == NDIS_STATUS_PENDING)
    {
        DEBUGMSG(DBG_NDIS, (TEXT("IrdaSetOid wait for (0x%x)\n"), pRequest));
        NdisWaitEvent(&pRequest->ReqEvent, 0);
        Status = pRequest->ReqStatus;
        // If we're on a process thread that's being terminated then we won't wait
        // on the event properly
        if (Status == WAITING_THREAD) {
            Status = pRequest->ReqStatus = ZOMBIE_THREAD;
        }
    }

    if (Status != ZOMBIE_THREAD) {
        NdisFreeEvent(&pRequest->ReqEvent);
        DEBUGMSG(DBG_NDIS, (TEXT("IrdaSetOid freeing (0x%x)\n"), pRequest));
        IrdaFree(pRequest);

        return Status;
    } else {
        return NDIS_STATUS_RESOURCES;
    }
}
        
/***************************************************************************
*
*   Allocate an Irda message for IrLap to use for control frames.
*   This modules owns these so IrLAP doesn't have to deal with the
*   Ndis send complete.
*
*/ 
IRDA_MSG *
AllocTxMsg(PIRDA_LINK_CB pIrdaLinkCb)
{
    NDIS_PHYSICAL_ADDRESS	pa = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1);    
    IRDA_MSG                *pMsg;

    pMsg = (IRDA_MSG *) NdisInterlockedRemoveHeadList(
        &pIrdaLinkCb->TxMsgFreeList, &pIrdaLinkCb->SpinLock);
    
    if (pMsg == NULL)
    {
        NdisAllocateMemory(&pMsg, sizeof(IRDA_MSG) + IRDA_MSG_DATA_SIZE,
                           0, pa);
        if (pMsg == NULL)
            return NULL;
        pIrdaLinkCb->TxMsgFreeListLen++;
    }

    // Indicate driver owns message
    REFADD(&pIrdaLinkCb->RefCnt, 'TIMX');
    pMsg->IRDA_MSG_pOwner = &pIrdaLinkCb->TxMsgFreeList;

    // Initialize pointers 
    pMsg->IRDA_MSG_pHdrWrite    = \
    pMsg->IRDA_MSG_pHdrRead     = pMsg->IRDA_MSG_Header + IRDA_HEADER_LEN;
	pMsg->IRDA_MSG_pBase        = \
	pMsg->IRDA_MSG_pRead        = \
	pMsg->IRDA_MSG_pWrite       = (UCHAR *) pMsg + sizeof(IRDA_MSG);
	pMsg->IRDA_MSG_pLimit       = pMsg->IRDA_MSG_pBase + IRDA_MSG_DATA_SIZE-1;

    return pMsg;
}

/***************************************************************************
*
*   Process MAC_CONTROL_REQs from IrLAP
*
*/
VOID
MacControlRequest(
    PIRDA_LINK_CB   pIrdaLinkCb,
    PIRDA_MSG       pMsg)
{
    NDIS_STATUS     Status;
    UINT            MediaBusy;
    UINT            Config = 0;
    UINT            cbConfig = sizeof(UINT);
    
    switch (pMsg->IRDA_MSG_Op)
    {
      case MAC_INITIALIZE_LINK:
      case MAC_RECONFIG_LINK:        
        pIrdaLinkCb->ExtraBofs  = pMsg->IRDA_MSG_NumBOFs;
        pIrdaLinkCb->MinTat     = pMsg->IRDA_MSG_MinTat;
        Status = IrdaSetOid(pIrdaLinkCb,
                          OID_IRDA_LINK_SPEED,
                          (PUINT) &pMsg->IRDA_MSG_Baud,
                          sizeof(UINT));
        return;

      case MAC_MEDIA_SENSE:
        pIrdaLinkCb->MediaBusy = FALSE;
        MediaBusy = 0;
        IrdaSetOid(pIrdaLinkCb, OID_IRDA_MEDIA_BUSY, &MediaBusy, sizeof(UINT)); 
        pIrdaLinkCb->MediaSenseTimer.Timeout = pMsg->IRDA_MSG_SenseTime;
        IrdaTimerStart(&pIrdaLinkCb->MediaSenseTimer);
        return;
        
      case MAC_CLOSE_LINK:
      
        NdisResetEvent(&pIrdaLinkCb->SyncEvent);

        NdisCloseAdapter(&Status, pIrdaLinkCb->NdisBindingHandle);

        if (Status == NDIS_STATUS_PENDING)
        {
            NdisWaitEvent(&pIrdaLinkCb->SyncEvent, 0);
            Status = pIrdaLinkCb->SyncStatus;
        }                            
        
        if (pIrdaLinkCb->UnbindContext != NULL)
        {
        #ifndef UNDER_CE
            NdisCompleteUnbindAdapter(pIrdaLinkCb->UnbindContext, NDIS_STATUS_SUCCESS);
        #endif 
        }
        
        REFDEL(&pIrdaLinkCb->RefCnt, 'DNIB');
        
        return;

      case MAC_ACQUIRE_RESOURCES:
        Status = IrdaSetOid(
            pIrdaLinkCb,
            OID_IRDA_REACQUIRE_HW_RESOURCES,
            &Config, cbConfig);

        ASSERT(Status != NDIS_STATUS_PENDING);

        if (Status == NDIS_STATUS_FAILURE)
        {
            pMsg->IRDA_MSG_MacConfigStatus = IRMAC_ALREADY_INIT;
        }
        else if (Status == NDIS_STATUS_RESOURCES)
        {
            pMsg->IRDA_MSG_MacConfigStatus = IRMAC_MALLOC_FAILED;
        }
        else if (Status != NDIS_STATUS_SUCCESS)
        {
            pMsg->IRDA_MSG_MacConfigStatus = IRMAC_OPEN_PORT_FAILED;
        }
        else
        {
            pMsg->IRDA_MSG_MacConfigStatus = SUCCESS;
        }
        return;

      case MAC_RELEASE_RESOURCES:
        Status = IrdaQueryOid(
            pIrdaLinkCb,
            OID_IRDA_RELEASE_HW_RESOURCES,
            &Config, &cbConfig);

        ASSERT(Status != NDIS_STATUS_PENDING);

        if (Status == NDIS_STATUS_RESOURCES)
        {
            pMsg->IRDA_MSG_MacConfigStatus = IRMAC_MALLOC_FAILED;
        }
        else if (Status != NDIS_STATUS_SUCCESS)
        {
            pMsg->IRDA_MSG_MacConfigStatus = IRMAC_NOT_INITIALIZED;
        }
        else
        {
            pMsg->IRDA_MSG_MacConfigStatus = SUCCESS;
        }
        return;
    }
    ASSERT(0);
}

VOID
ConfirmDataRequest(
    PIRDA_LINK_CB pIrdaLinkCb,
    PIRDA_MSG     pMsg)
{
    if (pMsg->IRDA_MSG_pOwner == &pIrdaLinkCb->TxMsgFreeList)
    {
        // If TxMsgFreeList is the owner, the this is a control
        // frame which isn't confirmed.

        NdisInterlockedInsertTailList(&pIrdaLinkCb->TxMsgFreeList,
                                              &pMsg->Linkage,
                                              &pIrdaLinkCb->SpinLock);
        REFDEL(&pIrdaLinkCb->RefCnt, 'TIMX');
        return;
    }

    // IrNDIS will only confirm if the ref count is 0. this will ensure
    // that we are not messing up message when retranmitting.
    // Proper way may be to incorporate send confirmations into the LAP state
    // machine and not retransmit until the message is confirmed.
    
    // We can't get the LOCK_LINK since it could cause a deadlock if we are
    // holding the NDIS critical section (on an IrdaSendComplete). Therefore,
    // we need to queue and send up on a worker thread.
    if (InterlockedDecrement(&pMsg->IRDA_MSG_SendRefCnt) == 0)
    {
        pMsg->Prim = MAC_DATA_CONF;

        NdisInterlockedInsertTailList(
            &pIrdaLinkCb->RxMsgList,
            &pMsg->Linkage,
            &pIrdaLinkCb->SpinLock);

        IrdaEventSchedule(&pIrdaLinkCb->EvRxMsgReady, pIrdaLinkCb);
    }
}

/***************************************************************************
*
*   Process control and data requests from IrLAP
*
*/
VOID
IrmacDown(
    IN  PVOID   Context,
    PIRDA_MSG   pMsg)
{
    NDIS_STATUS             Status;
    PNDIS_PACKET            NdisPacket = NULL;
    PNDIS_BUFFER            NdisBuffer = NULL;
    PIRDA_PROTOCOL_RESERVED pReserved;
    PNDIS_IRDA_PACKET_INFO  IrdaPacketInfo;
    PIRDA_LINK_CB           pIrdaLinkCb = (PIRDA_LINK_CB) Context;
    
    DEBUGMSG(DBG_FUNCTION, (TEXT("IrmacDown(0x%.8X, 0x%.8X)\n"),
             Context, pMsg));

    if (NULL == pIrdaLinkCb) {
        DEBUGMSG(DBG_ERROR, (TEXT("IRDASTK:IrmacDown pIrdaLinkCb==NULL!\n")));
        return;
    }

    switch (pMsg->Prim)
    {
      case MAC_CONTROL_REQ:
        MacControlRequest(pIrdaLinkCb, pMsg);
        return;
        
      case MAC_DATA_RESP:
        // A data response from IrLAP is the mechanism used to
        // return ownership of received packets back to Ndis
        if (pMsg->DataContext)
            NdisReturnPackets(&((PNDIS_PACKET)pMsg->DataContext), 1);
            
        NdisInterlockedInsertTailList(&pIrdaLinkCb->RxMsgFreeList,
                                      &pMsg->Linkage,
                                      &pIrdaLinkCb->SpinLock);
        pIrdaLinkCb->RxMsgFreeListLen++;
        
        return;

      case MAC_DATA_REQ:
      
        pReserved = (PIRDA_PROTOCOL_RESERVED) NdisInterlockedRemoveHeadList(
                                &pIrdaLinkCb->PacketList, &pIrdaLinkCb->SpinLock);
                                
        if (pReserved == NULL)
        {
            DEBUGMSG(DBG_ERROR, (TEXT("IRLAP: NdisPacket pool has been depleted\n")));
            ConfirmDataRequest(pIrdaLinkCb, pMsg);
            return;
        }                            

        NdisPacket = CONTAINING_RECORD(pReserved, NDIS_PACKET, ProtocolReserved);
        
        ASSERT(pMsg->IRDA_MSG_pHdrWrite-pMsg->IRDA_MSG_pHdrRead);

        // Allocate buffer for frame header
        NdisAllocateBuffer(&Status, &NdisBuffer, pIrdaLinkCb->BufferPool,
                           pMsg->IRDA_MSG_pHdrRead,
                           pMsg->IRDA_MSG_pHdrWrite-pMsg->IRDA_MSG_pHdrRead);

        if (Status != NDIS_STATUS_SUCCESS)
        {
            DEBUGMSG(DBG_ERROR, (TEXT("IRLAP: NdisAllocateBuffer failed\n")));
            ASSERT(0);
            ConfirmDataRequest(pIrdaLinkCb, pMsg);
            return;
        }
        NdisChainBufferAtFront(NdisPacket, NdisBuffer);

        // if frame contains data, alloc buffer for data
        if (pMsg->IRDA_MSG_pWrite - pMsg->IRDA_MSG_pRead)
        {
            NdisAllocateBuffer(&Status, &NdisBuffer, pIrdaLinkCb->BufferPool,
                               pMsg->IRDA_MSG_pRead,
                               pMsg->IRDA_MSG_pWrite-pMsg->IRDA_MSG_pRead);
            if (Status != NDIS_STATUS_SUCCESS)
            {
                DEBUGMSG(DBG_ERROR, (TEXT("IRLAP: NdisAllocateBuffer failed\n")));
                ASSERT(0);      
                ConfirmDataRequest(pIrdaLinkCb, pMsg);
                return;
            }
            NdisChainBufferAtBack(NdisPacket, NdisBuffer);
        }
    
        pReserved =
            (PIRDA_PROTOCOL_RESERVED)(NdisPacket->ProtocolReserved);

        pReserved->Owner = pMsg->IRDA_MSG_pOwner;
        pReserved->pMsg = pMsg;
    
        IrdaPacketInfo = (PNDIS_IRDA_PACKET_INFO) \
            (pReserved->MediaInfo.ClassInformation);
        
        IrdaPacketInfo->ExtraBOFs           = pIrdaLinkCb->ExtraBofs;
        if (pIrdaLinkCb->WaitMinTat)
        {
            IrdaPacketInfo->MinTurnAroundTime   = pIrdaLinkCb->MinTat;
            pIrdaLinkCb->WaitMinTat = FALSE;
        }
        else
        {
            IrdaPacketInfo->MinTurnAroundTime = 0;
        }

        NDIS_SET_PACKET_MEDIA_SPECIFIC_INFO(
            NdisPacket,
            &pReserved->MediaInfo,
            sizeof(MEDIA_SPECIFIC_INFORMATION) -1 +
            sizeof(NDIS_IRDA_PACKET_INFO));
        
        DBG_FRAME(
            pIrdaLinkCb,
            DBG_TXFRAME,
            pMsg->IRDA_MSG_pHdrRead,
            (UINT) (pMsg->IRDA_MSG_pHdrWrite-pMsg->IRDA_MSG_pHdrRead),
            (UINT) ((pMsg->IRDA_MSG_pHdrWrite-pMsg->IRDA_MSG_pHdrRead) +
            (pMsg->IRDA_MSG_pWrite - pMsg->IRDA_MSG_pRead)) -
            IRLAP_HEADER_LEN);

        NdisSend(&Status, pIrdaLinkCb->NdisBindingHandle, NdisPacket);

        DEBUGMSG(DBG_NDIS, (TEXT("IRNDIS: NdisSend(%x)\n"), NdisPacket));

        if (Status != NDIS_STATUS_PENDING)
        {
            DEBUGMSG(DBG_ERROR, (TEXT("IRLAP: NdisSend returned %x, not STATUS_PENDING\n"),
                     Status));
            IrdaSendComplete(pIrdaLinkCb,
                             NdisPacket,
                             NDIS_STATUS_FAILURE);
            return;
        }
    }
}

/***************************************************************************
*
*   Callback for media sense timer expirations
*
*/
VOID
MediaSenseExp(PVOID Context)
{
    PIRDA_LINK_CB   pIrdaLinkCb = (PIRDA_LINK_CB) Context;
    IRDA_MSG        IMsg;
    UINT            MediaBusy;
    UINT            Cnt = 1;

    IMsg.Prim               = MAC_CONTROL_CONF;   
    IMsg.IRDA_MSG_Op        = MAC_MEDIA_SENSE;
    IMsg.IRDA_MSG_OpStatus  = MAC_MEDIA_BUSY;

    if (pIrdaLinkCb->MediaBusy == FALSE)
    {
        MediaBusy = FALSE;
        IrdaQueryOid(pIrdaLinkCb, OID_IRDA_MEDIA_BUSY, &MediaBusy, &Cnt); 
        
        if (!MediaBusy)
        {
            IMsg.IRDA_MSG_OpStatus = MAC_MEDIA_CLEAR;
        }
    }

    LOCK_LINK(pIrdaLinkCb);
    IrlapUp(pIrdaLinkCb->IrlapContext, &IMsg);
    UNLOCK_LINK(pIrdaLinkCb);
}

/***************************************************************************
*
*   Protocol open adapter complete handler
*
*/
VOID IrdaOpenAdapterComplete(
    IN  NDIS_HANDLE             IrdaBindingContext,
    IN  NDIS_STATUS             Status,
    IN  NDIS_STATUS             OpenErrorStatus
    )
{
    PIRDA_LINK_CB  pIrdaLinkCb = (PIRDA_LINK_CB) IrdaBindingContext;
    
    DEBUGMSG(DBG_NDIS,
             (TEXT("+IrdaOpenAdapterComplete() BindingContext %x, Status %x\n"),
              IrdaBindingContext, Status));

    pIrdaLinkCb->SyncStatus = Status;
    NdisSetEvent(&pIrdaLinkCb->SyncEvent);
    
    DEBUGMSG(DBG_NDIS, (TEXT("-IrdaOpenAdapterComplete()\n")));
              
    return;
}

/***************************************************************************
*
*   Protocol close adapter complete handler
*
*/
VOID IrdaCloseAdapterComplete(
    IN  NDIS_HANDLE             IrdaBindingContext,
    IN  NDIS_STATUS             Status
    )
{
    PIRDA_LINK_CB   pIrdaLinkCb = (PIRDA_LINK_CB) IrdaBindingContext;
    
    DEBUGMSG(DBG_NDIS, (TEXT("IrdaCloseAdapterComplete()\n")));

    pIrdaLinkCb->SyncStatus = Status;
    NdisSetEvent(&pIrdaLinkCb->SyncEvent);
    
    return;
}

/***************************************************************************
*
*   Protocol send complete handler
*
*/
VOID IrdaSendComplete(
    IN  NDIS_HANDLE             Context,
    IN  PNDIS_PACKET            NdisPacket,
    IN  NDIS_STATUS             Status
    )
{
    PIRDA_LINK_CB           pIrdaLinkCb = (PIRDA_LINK_CB) Context;
    PIRDA_PROTOCOL_RESERVED pReserved = \
        (PIRDA_PROTOCOL_RESERVED) NdisPacket->ProtocolReserved;
    PIRDA_MSG               pMsg = pReserved->pMsg;
    PNDIS_BUFFER            NdisBuffer;

    //ASSERT(Status == NDIS_STATUS_SUCCESS);

    ConfirmDataRequest(pIrdaLinkCb, pMsg);
                            
    if (NdisPacket)
    {
        NdisUnchainBufferAtFront(NdisPacket, &NdisBuffer);
        while (NdisBuffer)
        {
            NdisFreeBuffer(NdisBuffer);
            NdisUnchainBufferAtFront(NdisPacket, &NdisBuffer);
        }
    
        NdisReinitializePacket(NdisPacket);
        
        NdisInterlockedInsertTailList(&pIrdaLinkCb->PacketList,
                                      &pReserved->Linkage,
                                      &pIrdaLinkCb->SpinLock);
    }
    
    DEBUGMSG(DBG_NDIS, (TEXT("+IrdaSendComplete()\n")));
    return;
}

/***************************************************************************
*
*   Protocol transfer complete handler
*
*/
VOID IrdaTransferDataComplete(
    IN  NDIS_HANDLE             IrdaBindingContext,
    IN  PNDIS_PACKET            Packet,
    IN  NDIS_STATUS             Status,
    IN  UINT                    BytesTransferred
    )
{
    DEBUGMSG(DBG_NDIS, (TEXT("+IrdaTransferDataComplete()\n")));
    
    ASSERT(0);
    return;
}

/***************************************************************************
*
*   Protocol reset complete handler
*
*/
void IrdaResetComplete(
    IN  NDIS_HANDLE             IrdaBindingContext,
    IN  NDIS_STATUS             Status
    )
{
    DEBUGMSG(DBG_ERROR, (TEXT("+IrdaResetComplete()\n")));

    ASSERT(0);
    
    return;
}

/***************************************************************************
*
*   Protocol request complete handler
*
*/
void IrdaRequestComplete(
    IN  NDIS_HANDLE             IrdaBindingContext,
    IN  PNDIS_REQUEST           NdisRequest,
    IN  NDIS_STATUS             Status
    )
{
    PIRDA_LINK_CB  pIrdaLinkCb = (PIRDA_LINK_CB) IrdaBindingContext;
    PIRDA_NDIS_REQ pRequest = (PIRDA_NDIS_REQ)NdisRequest;
    
    if (pRequest->ReqStatus == ZOMBIE_THREAD) {
        NdisFreeEvent(&pRequest->ReqEvent);
        DEBUGMSG(DBG_NDIS, (TEXT("+IrdaRequestComplete freeing (0x%x)\n"), pRequest));
        IrdaFree(pRequest);
        return;
    }

    DEBUGMSG(DBG_NDIS, (TEXT("+IrdaRequestComplete(0x%x)\n"), NdisRequest));
    
    pRequest->ReqStatus = Status;
    
    NdisSetEvent(&pRequest->ReqEvent);

    return;
}

/***************************************************************************
*
*   Protocol receive handler - This asserts if I don't get all data in the
*   lookahead buffer.
*
*/
NDIS_STATUS IrdaReceive(
    IN  NDIS_HANDLE             IrdaBindingContext,
    IN  NDIS_HANDLE             MacReceiveContext,
    IN  PVOID                   HeaderBuffer,
    IN  UINT                    HeaderBufferSize,
    IN  PVOID                   LookaheadBuffer,
    IN  UINT                    LookaheadBufferSize,
    IN  UINT                    PacketSize
    )
{
    PIRDA_LINK_CB   pIrdaLinkCb = IrdaBindingContext;    
    PIRDA_MSG       pMsg;
    
    DEBUGMSG(DBG_NDIS, (TEXT("+IrdaReceive()\n")));

    pIrdaLinkCb->WaitMinTat = TRUE;

    if (PacketSize + HeaderBufferSize > (UINT) pIrdaLinkCb->RxMsgDataSize)
    {
        DEBUGMSG(1, (TEXT("Packet+Header(%d) > RxMsgDataSize(%d)\n"), 
                PacketSize + HeaderBufferSize, pIrdaLinkCb->RxMsgDataSize));
        
        ASSERT(0);
        return NDIS_STATUS_NOT_ACCEPTED;
    }
    
    // Allocate an IrdaMsg and initialize data pointers
    pMsg = (IRDA_MSG *) NdisInterlockedRemoveHeadList(
        &pIrdaLinkCb->RxMsgFreeList, &pIrdaLinkCb->SpinLock);    

    if (pMsg == NULL)
    {
        DEBUGMSG(DBG_ERROR, (TEXT("RxMsgFreeList has been depleted\n")));
        ASSERT(0);
        return NDIS_STATUS_NOT_ACCEPTED;
    }
    pIrdaLinkCb->RxMsgFreeListLen--;
    
    pMsg->IRDA_MSG_pRead  = \
    pMsg->IRDA_MSG_pWrite = (UCHAR *)pMsg + sizeof(IRDA_MSG);

    // Copy header and data    
    NdisMoveMemory(pMsg->IRDA_MSG_pWrite,
                   HeaderBuffer,
                   HeaderBufferSize);
    
    pMsg->IRDA_MSG_pWrite += HeaderBufferSize;

    NdisMoveMemory(pMsg->IRDA_MSG_pWrite,
                   LookaheadBuffer,
                   LookaheadBufferSize);
    pMsg->IRDA_MSG_pWrite += LookaheadBufferSize;

    if (LookaheadBufferSize == PacketSize)
    {        
        pMsg->Prim        = MAC_DATA_IND;
        pMsg->DataContext = NULL; // i.e. I own this and there is no
                                  // Ndis packet assocaited with it 
                                  // (see MAC_DATA_RESP)
        NdisInterlockedInsertTailList(&pIrdaLinkCb->RxMsgList,
                                      &pMsg->Linkage,
                                      &pIrdaLinkCb->SpinLock);
                                      
                                      
        IrdaEventSchedule(&pIrdaLinkCb->EvRxMsgReady, pIrdaLinkCb);
    }
    else
    {
        DEBUGMSG(DBG_ERROR, (TEXT("LookaheadBufferSize(%d) != PacketSize(%d)\n"),
                 LookaheadBufferSize, PacketSize));
        ASSERT(0);
    }
    
    return NDIS_STATUS_SUCCESS;
}

/***************************************************************************
*
*   Protocol receive complete handler - what is this for?
*
*/
VOID IrdaReceiveComplete(
    IN  NDIS_HANDLE             IrdaBindingContext
    )
{
    DEBUGMSG(DBG_NDIS, (TEXT("+IrdaReceiveComplete()\n")));
    
    return;
}

/***************************************************************************
*
*   Protocol status handler
*
*/
VOID IrdaStatus(
    IN  NDIS_HANDLE             IrdaBindingContext,
    IN  NDIS_STATUS             GeneralStatus,
    IN  PVOID                   StatusBuffer,
    IN  UINT                    StatusBufferSize
    )
{
    PIRDA_LINK_CB   pIrdaLinkCb = (PIRDA_LINK_CB) IrdaBindingContext;
    
    if (GeneralStatus == NDIS_STATUS_MEDIA_BUSY)
    {
        DEBUGMSG(DBG_NDIS, (TEXT("STATUS_MEDIA_BUSY\n")));
        pIrdaLinkCb->MediaBusy = TRUE;
    }
    else
    {
        DEBUGMSG(DBG_NDIS, (TEXT("Unknown Status indication\n")));
    }
    
    return;
}

/***************************************************************************
*
*   Protocol status complete handler
*
*/
VOID IrdaStatusComplete(
    IN  NDIS_HANDLE             IrdaBindingContext
    )
{
    DEBUGMSG(DBG_NDIS, (TEXT("IrdaStatusComplete()\n")));
    
    return;
}

/***************************************************************************
*
*   RxMsgReady - Hands received frames to Irlap for processing. This is
*   the callback of an exec worker thread running at passive level
*   which allows us to get a mutex in order to single thread
*   events through the stack.
*
*/
VOID
RxMsgReady(void *Arg)
{
    PIRDA_LINK_CB   pIrdaLinkCb = (PIRDA_LINK_CB) Arg;
    PIRDA_MSG       pMsg = (PIRDA_MSG) NdisInterlockedRemoveHeadList(
                                        &pIrdaLinkCb->RxMsgList,
                                        &pIrdaLinkCb->SpinLock);

    while (pMsg)
    {
        LOCK_LINK(pIrdaLinkCb);
        
        IrlapUp(pIrdaLinkCb->IrlapContext, pMsg);

        UNLOCK_LINK(pIrdaLinkCb);

        pMsg = (PIRDA_MSG) NdisInterlockedRemoveHeadList(
            &pIrdaLinkCb->RxMsgList, &pIrdaLinkCb->SpinLock);
    }
}

/***************************************************************************
*
*   Protocol receive packet handler - Called at DPC, put the message on
*   RxList and have Exec worker thread process it at passive level.
*
*/
INT
IrdaReceivePacket(
    IN  NDIS_HANDLE             IrdaBindingContext,
    IN  PNDIS_PACKET            Packet)
{
    UINT            BufCnt, TotalLen, BufLen;
    PNDIS_BUFFER    pNdisBuf;
    PIRDA_MSG       pMsg;
    UCHAR            *pData;
    PIRDA_LINK_CB   pIrdaLinkCb = IrdaBindingContext;
    
    IRLAP_LOG_NDIS_PACKET(DBG_RXFRAME, Packet);

    DEBUGMSG(DBG_NDIS, (TEXT("IRNDIS: IrdaReceivePacket(%x)\n"), Packet));

    pIrdaLinkCb->WaitMinTat = TRUE;

    pMsg = (IRDA_MSG *) NdisInterlockedRemoveHeadList(
        &pIrdaLinkCb->RxMsgFreeList, &pIrdaLinkCb->SpinLock);    

    if (pMsg == NULL)
    {
        DEBUGMSG(DBG_ERROR, (TEXT("IRNDIS: RxMsgFreeList depleted\n")));
        ASSERT(pIrdaLinkCb->RxMsgFreeListLen == 0);
        return 0;
    }
    pIrdaLinkCb->RxMsgFreeListLen--;

    NdisQueryPacket(Packet, NULL, &BufCnt, &pNdisBuf, &TotalLen);

    ASSERT(BufCnt == 1);
    
    NdisQueryBuffer(pNdisBuf, &pData, &BufLen);

    pMsg->Prim                  = MAC_DATA_IND;
    pMsg->IRDA_MSG_pRead        = pData;
    pMsg->IRDA_MSG_pWrite       = pData + BufLen;
    pMsg->DataContext           = Packet;

    NDIS_SET_PACKET_STATUS(Packet, NDIS_STATUS_PENDING);

    NdisInterlockedInsertTailList(&pIrdaLinkCb->RxMsgList,
                                  &pMsg->Linkage,
                                  &pIrdaLinkCb->SpinLock);

#ifdef DEBUG
    {
        UINT cDbgData = ((UINT) (pMsg->IRDA_MSG_pWrite - pMsg->IRDA_MSG_pRead))
                                 - IRLAP_HEADER_LEN;

        DBG_FRAME(pIrdaLinkCb,
                  DBG_RXFRAME, pMsg->IRDA_MSG_pRead,
                  (UINT) IRLAP_HEADER_LEN,
                  min(cDbgData, 100));
    }
#endif 

#ifdef UNDER_CE
    RxMsgReady(pIrdaLinkCb);
#else
    IrdaEventSchedule(&pIrdaLinkCb->EvRxMsgReady, pIrdaLinkCb);
#endif // !UNDER_CE
    return 1; // Ownership reference count of packet
}

/***************************************************************************
*
*   Delete all control blocks for a given link
*
*/
VOID
DeleteIrdaLink(PVOID Arg)
{
    PIRDA_LINK_CB           pIrdaLinkCb = (PIRDA_LINK_CB) Arg;
    int                     i;
    PIRDA_MSG               pMsg;
        
    DEBUGMSG(1, (TEXT("Deleting IrdaLink instance\n")));

    NdisFreeBufferPool(pIrdaLinkCb->BufferPool);   

    for (i = 0; i < IRDA_NDIS_PACKET_POOL_SIZE; i++)
    {
        PIRDA_PROTOCOL_RESERVED pReserved;
        PNDIS_PACKET            NdisPacket;
    
        pReserved = (PIRDA_PROTOCOL_RESERVED) NdisInterlockedRemoveHeadList(
                                &pIrdaLinkCb->PacketList, &pIrdaLinkCb->SpinLock);
                                
        if (pReserved == NULL)
        {
            DEBUGMSG(DBG_ERROR, (TEXT("Not all NdisPackets were on list when deleting\n")));
            ASSERT(0);
            break;;
        }                            

        NdisPacket = CONTAINING_RECORD(pReserved, NDIS_PACKET, ProtocolReserved);
        
        NdisFreePacket(NdisPacket);
    }
    
    NdisFreePacketPool(pIrdaLinkCb->PacketPool);

    pMsg = (IRDA_MSG *) NdisInterlockedRemoveHeadList(
        &pIrdaLinkCb->TxMsgFreeList, &pIrdaLinkCb->SpinLock);
    while (pMsg != NULL)
    {
        NdisFreeMemory(pMsg, sizeof(IRDA_MSG) + IRDA_MSG_DATA_SIZE, 0);
        pMsg = (IRDA_MSG *) NdisInterlockedRemoveHeadList(
            &pIrdaLinkCb->TxMsgFreeList, &pIrdaLinkCb->SpinLock);        
    }

    pMsg = (IRDA_MSG *) NdisInterlockedRemoveHeadList(
        &pIrdaLinkCb->RxMsgFreeList, &pIrdaLinkCb->SpinLock);
    while (pMsg != NULL)
    {
        NdisFreeMemory(pMsg, sizeof(IRDA_MSG), 0);
        pMsg = (IRDA_MSG *) NdisInterlockedRemoveHeadList(
            &pIrdaLinkCb->RxMsgFreeList, &pIrdaLinkCb->SpinLock);        
    }


    IrlapDeleteInstance(pIrdaLinkCb->IrlapContext);
    
    IrlmpDeleteInstance(pIrdaLinkCb->IrlmpContext);

    DELETE_LINK_LOCK(pIrdaLinkCb);
    
    NdisFreeSpinLock(&pIrdaLinkCb->SpinLock);
    NdisFreeEvent(&pIrdaLinkCb->SyncEvent);
    NdisFreeMemory(pIrdaLinkCb, sizeof(IRDA_LINK_CB), 0);
}    

/***************************************************************************
*
*   Initialize local Qos with info from adapters register and globals
*   initialized at driver entry time (from protocol's registery)
*
*/
VOID InitializeLocalQos(
    IN OUT  IRDA_QOS_PARMS      *pQos,
    IN      PNDIS_STRING        ConfigPath)
{
    NDIS_HANDLE             ConfigHandle;
    NDIS_STRING             DataSizeStr = NDIS_STRING_CONST("DATASIZE");
    NDIS_STRING             WindowSizeStr = NDIS_STRING_CONST("WINDOWSIZE");
    NDIS_STRING             MaxTatStr = NDIS_STRING_CONST("MAXTURNTIME");
    NDIS_STRING             BofsStr = NDIS_STRING_CONST("BOFS");    
    NDIS_STRING             DiscTimeStr = NDIS_STRING_CONST("DISCONNECTTIME");    
	PNDIS_CONFIGURATION_PARAMETER ParmVal;
    NDIS_STATUS             Status;

    pQos->bfDisconnectTime  = DisconnectTime;
    
    pQos->bfDataSize    = IRLAP_DEFAULT_DATASIZE;
    pQos->bfWindowSize  = IRLAP_DEFAULT_WINDOWSIZE;
    pQos->bfMaxTurnTime = IRLAP_DEFAULT_MAXTAT;
    pQos->bfBofs        = BOFS_0;
    
    NdisOpenProtocolConfiguration(&Status,
                                  &ConfigHandle,
                                  ConfigPath);

    if (Status == NDIS_STATUS_SUCCESS)
    {
        NdisReadConfiguration(&Status, 
                              &ParmVal,
                              ConfigHandle, 
                              &DataSizeStr,
                              NdisParameterInteger);

        if (Status == NDIS_STATUS_SUCCESS)
            pQos->bfDataSize = ParmVal->ParameterData.IntegerData;

        NdisReadConfiguration(&Status, 
                              &ParmVal,
                              ConfigHandle, 
                              &WindowSizeStr,
                              NdisParameterInteger);    

        if (Status == NDIS_STATUS_SUCCESS)
            pQos->bfWindowSize = ParmVal->ParameterData.IntegerData;

        NdisReadConfiguration(&Status, 
                              &ParmVal,
                              ConfigHandle, 
                              &MaxTatStr,
                              NdisParameterInteger);    

        if (Status == NDIS_STATUS_SUCCESS)
            pQos->bfMaxTurnTime = ParmVal->ParameterData.IntegerData;

        NdisReadConfiguration(&Status, 
                              &ParmVal,
                              ConfigHandle, 
                              &BofsStr,
                              NdisParameterInteger);    

        if (Status == NDIS_STATUS_SUCCESS)
            pQos->bfBofs = ParmVal->ParameterData.IntegerData;    

#ifdef UNDER_CE
        NdisReadConfiguration(&Status,
            &ParmVal,
            ConfigHandle,
            &DiscTimeStr,
            NdisParameterInteger);

        if (Status == NDIS_STATUS_SUCCESS)
        {
            pQos->bfDisconnectTime = ParmVal->ParameterData.IntegerData;
        }
#endif 

        NdisCloseConfiguration(ConfigHandle);
    }
    
    DEBUGMSG(DBG_NDIS | ZONE_INIT, 
        (TEXT("IrDA Local QOS: DataSize 0x%x, WindowSize 0x%x, MaxTat 0x%x, ")
         TEXT("BOFs 0x%x, DiscTime 0x%x\n"),
         pQos->bfDataSize, pQos->bfWindowSize, pQos->bfMaxTurnTime, 
         pQos->bfBofs, pQos->bfDisconnectTime));
}

/***************************************************************************
*
*   Protocol bind adapter handler
*
*/
VOID IrdaBindAdapter(
    OUT PNDIS_STATUS            pStatus,
    IN  NDIS_HANDLE             BindContext,
    IN  PNDIS_STRING            AdapterName,
    IN  PVOID                   SystemSpecific1,
    IN  PVOID                   SystemSpecific2
    )
{
    NDIS_STATUS             OpenErrorStatus;
    NDIS_MEDIUM             MediumArray[] = {NdisMediumIrda};
    UINT                    SelectedMediumIndex;
    PIRDA_LINK_CB           pIrdaLinkCb;
    NDIS_PHYSICAL_ADDRESS	pa = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1);
    UINT                    UintArray[8];
    UINT                    UintArrayCnt;
    IRDA_MSG                *pMsg;
    int                     i, WinSize;
    IRDA_QOS_PARMS          LocalQos;    
    UCHAR                   DscvInfoBuf[64];
    int                     DscvInfoLen;
    ULONG                   Val, Mask;    

    NDIS_STATUS             CloseStatus;

    DEBUGMSG(1, (TEXT("+IrdaBindAdapter() \"%ls\", BindContext %x\n"),
                 AdapterName->Buffer, BindContext));
    
    NdisAllocateMemory((PVOID *)&pIrdaLinkCb, sizeof(IRDA_LINK_CB), 0, pa);

    if (!pIrdaLinkCb)
    {
        *pStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto exit10;
    }

    NdisZeroMemory(pIrdaLinkCb, sizeof(IRDA_LINK_CB));
    
    ReferenceInit(&pIrdaLinkCb->RefCnt, pIrdaLinkCb, DeleteIrdaLink);
    REFADD(&pIrdaLinkCb->RefCnt, 'DNIB');

    pIrdaLinkCb->UnbindContext = NULL;
    pIrdaLinkCb->WaitMinTat = FALSE;

    NdisInitializeEvent(&pIrdaLinkCb->SyncEvent);

    NdisResetEvent(&pIrdaLinkCb->SyncEvent);

    NdisAllocateSpinLock(&pIrdaLinkCb->SpinLock);

    INIT_LINK_LOCK(pIrdaLinkCb);

    IrdaEventInitialize(&pIrdaLinkCb->EvRxMsgReady, RxMsgReady);

#if DBG
    pIrdaLinkCb->MediaSenseTimer.pName = "MediaSense";
#endif    
    
    IrdaTimerInitialize(&pIrdaLinkCb->MediaSenseTimer,
                        MediaSenseExp,
                        0,
                        pIrdaLinkCb,
                        pIrdaLinkCb);
    
    NdisAllocateBufferPool(pStatus,
                           &pIrdaLinkCb->BufferPool,
                           IRDA_NDIS_BUFFER_POOL_SIZE);
    
    if (*pStatus != NDIS_STATUS_SUCCESS)
    {
        DEBUGMSG(DBG_ERROR, (TEXT("NdisAllocateBufferPool failed\n")));
        goto error10; 
    }
    
    NdisAllocatePacketPool(pStatus,
                           &pIrdaLinkCb->PacketPool,
                           IRDA_NDIS_PACKET_POOL_SIZE,
                           sizeof(IRDA_PROTOCOL_RESERVED)-1 + \
                           sizeof(NDIS_IRDA_PACKET_INFO));
    if (*pStatus != NDIS_STATUS_SUCCESS)
    {
        DEBUGMSG(DBG_ERROR, (TEXT("NdisAllocatePacketPool failed\n")));
        goto error20; 
    }
    
    NdisInitializeListHead(&pIrdaLinkCb->PacketList);    

    for (i = 0; i < IRDA_NDIS_PACKET_POOL_SIZE; i++)
    {
        PIRDA_PROTOCOL_RESERVED pReserved;
        PNDIS_PACKET            NdisPacket;
        
        NdisAllocatePacket(pStatus, &NdisPacket, pIrdaLinkCb->PacketPool);
        
        if (*pStatus != NDIS_STATUS_SUCCESS)
        {
            ASSERT(0);
            goto error30;
        }    
        
        pReserved =
            (PIRDA_PROTOCOL_RESERVED)(NdisPacket->ProtocolReserved);
        
        NdisInterlockedInsertTailList(&pIrdaLinkCb->PacketList,
                                      &pReserved->Linkage,
                                      &pIrdaLinkCb->SpinLock);
    }

    NdisInitializeListHead(&pIrdaLinkCb->TxMsgFreeList);
    NdisInitializeListHead(&pIrdaLinkCb->RxMsgFreeList);
    NdisInitializeListHead(&pIrdaLinkCb->RxMsgList);

    // Allocate a list of Irda messages w/ data for internally
    // generated LAP messages
    pIrdaLinkCb->TxMsgFreeListLen = 0;
    for (i = 0; i < IRDA_MSG_LIST_LEN; i++)
    {
        NdisAllocateMemory(&pMsg, sizeof(IRDA_MSG) + IRDA_MSG_DATA_SIZE,
                           0, pa);
        if (pMsg == NULL)
        {
            *pStatus = STATUS_INSUFFICIENT_RESOURCES;
            goto error40;
        }
        NdisInterlockedInsertTailList(&pIrdaLinkCb->TxMsgFreeList,
                                      &pMsg->Linkage,
                                      &pIrdaLinkCb->SpinLock);
        pIrdaLinkCb->TxMsgFreeListLen++;
    }

    // Build the discovery info    
    Val = Hints;
    DscvInfoLen = 0;
    for (i = 0, Mask = 0xFF000000; i < 4; i++, Mask >>= 8)
    {
        if (Mask & Val || DscvInfoLen > 0)
        {
            DscvInfoBuf[DscvInfoLen++] = (UCHAR)
                ((Mask & Val) >> (8 * (3-i)));
        }
    }
    DscvInfoBuf[DscvInfoLen++] = CharSet;
    RtlCopyMemory(DscvInfoBuf+DscvInfoLen, NickName, NickNameLen);
    DscvInfoLen += NickNameLen;    

    NdisOpenAdapter(
        pStatus,
        &OpenErrorStatus,
        &pIrdaLinkCb->NdisBindingHandle,
        &SelectedMediumIndex,
        MediumArray,
        1,
        NdisIrdaHandle,
        pIrdaLinkCb,
        AdapterName,
        0,
        NULL);

    DEBUGMSG(DBG_NDIS, (TEXT("NdisOpenAdapter(%x), status %x\n"),
                        pIrdaLinkCb->NdisBindingHandle, *pStatus));

    if (*pStatus == NDIS_STATUS_PENDING)
    {
        NdisWaitEvent(&pIrdaLinkCb->SyncEvent, 0);
        *pStatus = pIrdaLinkCb->SyncStatus;
    }

    if (*pStatus != NDIS_STATUS_SUCCESS)
    { 
        goto error40;
    }

    InitializeLocalQos(&LocalQos, (PNDIS_STRING) SystemSpecific1);

    // Query adapters capabilities and store in LocalQos
    UintArrayCnt = sizeof(UintArray)/sizeof(UINT);
    *pStatus = IrdaQueryOid(pIrdaLinkCb,
                            OID_IRDA_SUPPORTED_SPEEDS,
                            UintArray, &UintArrayCnt);
    if (*pStatus != NDIS_STATUS_SUCCESS)
    {
        DEBUGMSG(DBG_ERROR,
                 (TEXT("Query IRDA_SUPPORTED_SPEEDS failed %x\n"),
                  *pStatus));
        goto error50;
    }

    OidToLapQos(vBaudTable,
                UintArray,
                UintArrayCnt,
                &LocalQos.bfBaud,
                FALSE);

    UintArrayCnt = sizeof(UintArray)/sizeof(UINT);
    *pStatus = IrdaQueryOid(pIrdaLinkCb,
                            OID_IRDA_TURNAROUND_TIME,
                            UintArray, &UintArrayCnt);

    if (*pStatus != NDIS_STATUS_SUCCESS)
    {
        DEBUGMSG(DBG_ERROR,
                 (TEXT("Query IRDA_SUPPORTED_SPEEDS failed %x\n"),
                  *pStatus));
        goto error50;
    }

    OidToLapQos(vMinTATTable,
                UintArray,
                UintArrayCnt,
                &LocalQos.bfMinTurnTime,
                FALSE);

    UintArrayCnt = sizeof(UintArray)/sizeof(UINT);
    *pStatus = IrdaQueryOid(pIrdaLinkCb,
                            OID_IRDA_MAX_RECEIVE_WINDOW_SIZE,
                            UintArray, &UintArrayCnt);
    if (*pStatus != NDIS_STATUS_SUCCESS)
    {
        // Not fatal
        DEBUGMSG(DBG_WARN,
                 (TEXT("Query IRDA_MAX_RECEIVE_WINDOW_SIZE failed %x\n"),
                  *pStatus));
        
    }
    else
    {
        OidToLapQos(vWinSizeTable,
                    UintArray,
                    UintArrayCnt,
                    &LocalQos.bfWindowSize,
                    TRUE);
    }

    // Limit the max window size to 4.     
    LocalQos.bfWindowSize &= (FRAMES_1 | FRAMES_2 | FRAMES_3 | FRAMES_4);

    // Get the window size and data size to determine the number
    // and size of Irda messages to allocate for receiving frames
    WinSize = IrlapGetQosParmVal(vWinSizeTable,
                                 LocalQos.bfWindowSize,
                                 NULL);

    pIrdaLinkCb->RxMsgDataSize = IrlapGetQosParmVal(vDataSizeTable,
                                                    LocalQos.bfDataSize,
                                                    NULL) + IRLAP_HEADER_LEN;

    pIrdaLinkCb->RxMsgFreeListLen = 0;
    for (i = 0; i < WinSize + 1; i++)
    {
        // Allocate room for data in case we get indicated data
        // that must be copied (IrdaReceive vs. IrdaReceivePacket)
        NdisAllocateMemory(&pMsg, sizeof(IRDA_MSG) +
                           pIrdaLinkCb->RxMsgDataSize, 0, pa);
        if (pMsg == NULL)
        {
            *pStatus = STATUS_INSUFFICIENT_RESOURCES;
            goto error50;
        }
        NdisInterlockedInsertTailList(&pIrdaLinkCb->RxMsgFreeList,
                                      &pMsg->Linkage,
                                      &pIrdaLinkCb->SpinLock);
        pIrdaLinkCb->RxMsgFreeListLen++;
    }

    // Create an instance of IrLAP
    IrlapOpenLink(pStatus,
                  pIrdaLinkCb,
                  &LocalQos,
                  DscvInfoBuf,
                  DscvInfoLen,
                  Slots,
                  NickName,
                  NickNameLen,
                  CharSet);

    if (*pStatus != STATUS_SUCCESS)
    {
        goto error50;
    }
    
    goto exit10;

error50:

    NdisCloseAdapter(&CloseStatus, pIrdaLinkCb->NdisBindingHandle);

error40:

    pMsg = (IRDA_MSG *) NdisInterlockedRemoveHeadList(
        &pIrdaLinkCb->TxMsgFreeList, &pIrdaLinkCb->SpinLock);
    while (pMsg != NULL)
    {
        NdisFreeMemory(pMsg, sizeof(IRDA_MSG) + IRDA_MSG_DATA_SIZE, 0);
        pMsg = (IRDA_MSG *) NdisInterlockedRemoveHeadList(
            &pIrdaLinkCb->TxMsgFreeList, &pIrdaLinkCb->SpinLock);        
    }

    pMsg = (IRDA_MSG *) NdisInterlockedRemoveHeadList(
        &pIrdaLinkCb->RxMsgFreeList, &pIrdaLinkCb->SpinLock);
    while (pMsg != NULL)
    {
        NdisFreeMemory(pMsg, sizeof(IRDA_MSG), 0);
        pMsg = (IRDA_MSG *) NdisInterlockedRemoveHeadList(
            &pIrdaLinkCb->RxMsgFreeList, &pIrdaLinkCb->SpinLock);        
    }

error30:
    for (i = 0; i < IRDA_NDIS_PACKET_POOL_SIZE; i++)
    {
        PIRDA_PROTOCOL_RESERVED pReserved;
        PNDIS_PACKET            NdisPacket;
    
        pReserved = (PIRDA_PROTOCOL_RESERVED) NdisInterlockedRemoveHeadList(
                                &pIrdaLinkCb->PacketList, &pIrdaLinkCb->SpinLock);
                                
        if (pReserved == NULL)
        {
            DEBUGMSG(DBG_ERROR, (TEXT("Not all NdisPackets were on list when deleting\n")));
            ASSERT(0);
            break;;
        }                            

        NdisPacket = CONTAINING_RECORD(pReserved, NDIS_PACKET, ProtocolReserved);
        
        NdisFreePacket(NdisPacket);
    }

    NdisFreePacketPool(pIrdaLinkCb->PacketPool);
    
error20:
    NdisFreeBufferPool(pIrdaLinkCb->BufferPool);
    
error10:

    DELETE_LINK_LOCK(pIrdaLinkCb);
    NdisFreeSpinLock(&pIrdaLinkCb->SpinLock);
    NdisFreeEvent(&pIrdaLinkCb->SyncEvent);
    NdisFreeMemory(pIrdaLinkCb, sizeof(IRDA_LINK_CB), 0);
    
exit10:
    DEBUGMSG(DBG_NDIS, (TEXT("-IrdaBindAdapter() status %x\n"),
                        *pStatus));
    
    return;
}


/***************************************************************************
*
*   Protocol unbind adapter handler
*
*/
VOID IrdaUnbindAdapter(
    OUT PNDIS_STATUS            pStatus,
    IN  NDIS_HANDLE             IrdaBindingContext,
    IN  NDIS_HANDLE             UnbindContext
    )
{
    PIRDA_LINK_CB   pIrdaLinkCb = (PIRDA_LINK_CB) IrdaBindingContext;
    
    DEBUGMSG(1, (TEXT("+IrdaUnbindAdapter()\n")));
    
    pIrdaLinkCb->UnbindContext = UnbindContext;
    
    LOCK_LINK(pIrdaLinkCb);
    
    IrlmpCloseLink(pIrdaLinkCb);
    
    UNLOCK_LINK(pIrdaLinkCb);
    
    *pStatus = NDIS_STATUS_PENDING;

    DEBUGMSG(DBG_NDIS, (TEXT("-IrdaUnbindAdapter() Status %x\n"),
                        *pStatus));

    return;
}

#ifdef UNDER_CE
//
// Get the device name from the registry and store in global NickName string
// and adjust NickNameLen such that "memcpy(buf, NickName, NickNameLen)" will
// include the null terminator.
//
void UpdateNickName(void)
{
    NDIS_STATUS Status;
    WCHAR lpszRegKey[128];
    HKEY  hKey;

    // Get discovery name from registry.
    wcscpy(lpszRegKey, TEXT("Ident"));

    Status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, lpszRegKey, 0, 0, &hKey);

    if (Status == ERROR_SUCCESS)
    {
        BYTE TempBuf[NICK_NAME_LEN + sizeof(WCHAR)];

        memset(TempBuf, 0, sizeof(TempBuf));

        if (GetRegSZValue(hKey, TEXT("Name"), (LPWSTR)TempBuf, NICK_NAME_LEN))
        {
            if (LmCharSetASCII == CharSet) {
                // Convert from unicode to ANSI
                NickNameLen = wcstombs(NickName, (LPWSTR)TempBuf, sizeof(TempBuf)) + 1;
                if (NickNameLen) {
                    NickName[NickNameLen - 1] = 0;
                }
            } else {
                NickNameLen = (wcslen((LPWSTR)TempBuf) + 1) * sizeof(WCHAR);
                memcpy(NickName, TempBuf, NickNameLen);
            }
        }
        RegCloseKey(hKey);        
    }
}
#endif

/***************************************************************************
*
*   IrdaInitialize - register Irda with Ndis, get Irlap parms from registry
*
*/
NTSTATUS IrdaInitialize(
    PNDIS_STRING    ProtocolName,
    PUNICODE_STRING RegistryPath,
    PUINT           pLazyDscvInterval)
{
    NDIS_STATUS Status;
    NDIS40_PROTOCOL_CHARACTERISTICS pc;
#ifdef UNDER_CE
    HKEY  hKey;

    DEBUGMSG(DBG_NDIS | ZONE_INIT,(TEXT("+IrdaInitialize(%s, %s)\n"), 
             ProtocolName->Buffer, RegistryPath->Buffer));


    // Set defaults.
    Slots           = IRLAP_DEFAULT_SLOTS;
    CharSet         = IRLAP_DEFAULT_CHARSET;
    Hints           = IRLAP_DEFAULT_HINTS;
    DisconnectTime  = IRLAP_DEFAULT_DISCONNECTTIME;
    SlotTimeout     = IRLAP_SLOT_TIMEOUT;
    DiscoverTimeout = IRLAP_DSCV_SENSE_TIME;
    AutoSuspend     = TRUE;
    NickName[0]     = 0;
    NickNameLen     = 0;

    *pLazyDscvInterval = 0;

    Status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, RegistryPath->Buffer, 0, 0, &hKey);

    if (Status == ERROR_SUCCESS)
    {
        UINT dwTemp;

        if (GetRegDWORDValue(hKey, TEXT("Slots"), &dwTemp) == TRUE)
            Slots = dwTemp;

        if (GetRegDWORDValue(hKey, TEXT("Hints"), &dwTemp) == TRUE)
            Hints = dwTemp;

        if (GetRegDWORDValue(hKey, TEXT("Unicode"), &dwTemp) == TRUE)
            CharSet = dwTemp ? LmCharSetUNICODE : LmCharSetASCII;

        if (GetRegDWORDValue(hKey, TEXT("SlotTimeout"), &dwTemp) == TRUE)
            SlotTimeout = dwTemp;
        
        if (GetRegDWORDValue(hKey, TEXT("DscvTimeout"), &dwTemp) == TRUE)
            DiscoverTimeout = dwTemp;

        if (GetRegDWORDValue(hKey, TEXT("AutoSuspend"), &dwTemp) == TRUE)
            AutoSuspend = (dwTemp) ? TRUE : FALSE;

        RegCloseKey(hKey);
    }

    DEBUGMSG(DBG_NDIS | ZONE_INIT, 
        (TEXT("IrDA: Slots %d, Slot timeout = %d, Discover timeout = %d, ")
         TEXT("Hints = %#x, Charset = %#x\n"),
         Slots, SlotTimeout, DiscoverTimeout, Hints, CharSet));
    
    UpdateNickName();

#else // UNDER_CE
    OBJECT_ATTRIBUTES               ObjectAttribs;
    HANDLE                          KeyHandle;
    UNICODE_STRING                  ValStr;
    PKEY_VALUE_FULL_INFORMATION     FullInfo;
    ULONG                           Result;
    UCHAR                           Buf[WORK_BUF_SIZE];
    WCHAR                           StrBuf[WORK_BUF_SIZE];
    UNICODE_STRING                  Path;
    ULONG                           i, Multiplier;

    DEBUGMSG(DBG_NDIS,("+IrdaInitialize()\n"));

    // Get protocol configuration from registry
    Path.Buffer         = StrBuf;
    Path.MaximumLength  = WORK_BUF_SIZE;
    Path.Length         = 0;

    RtlAppendUnicodeStringToString(&Path, RegistryPath);

    RtlAppendUnicodeToString(&Path, L"\\Parameters");
    
    InitializeObjectAttributes(&ObjectAttribs,
                               &Path,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);
    
    Status = ZwOpenKey(&KeyHandle, KEY_READ, &ObjectAttribs);

    Slots           = IRLAP_DEFAULT_SLOTS;
    HintCharSet     = IRLAP_DEFAULT_HINTCHARSET;
    DisconnectTime  = IRLAP_DEFAULT_DISCONNECTTIME;

    *pLazyDscvInterval = 0;
    
    if (Status == STATUS_SUCCESS)
    {
        RtlInitUnicodeString(&ValStr, L"DiscoveryRate");
        FullInfo = (PKEY_VALUE_FULL_INFORMATION) Buf;
        Status = ZwQueryValueKey(KeyHandle,
                                 &ValStr,
                                 KeyValueFullInformation,
                                 FullInfo,
                                 WORK_BUF_SIZE,
                                 &Result);
        if (Status == STATUS_SUCCESS && FullInfo->Type == REG_DWORD)
            *pLazyDscvInterval = *((ULONG UNALIGNED *) ((PCHAR)FullInfo +
                                           FullInfo->DataOffset));

        RtlInitUnicodeString(&ValStr, L"Slots");
        FullInfo = (PKEY_VALUE_FULL_INFORMATION) Buf;
        Status = ZwQueryValueKey(KeyHandle,
                                 &ValStr,
                                 KeyValueFullInformation,
                                 FullInfo,
                                 WORK_BUF_SIZE,
                                 &Result);
        if (Status == STATUS_SUCCESS && FullInfo->Type == REG_DWORD)
            Slots = *((ULONG UNALIGNED *) ((PCHAR)FullInfo +
                                           FullInfo->DataOffset));

        RtlInitUnicodeString(&ValStr, L"HINTCHARSET");
        FullInfo = (PKEY_VALUE_FULL_INFORMATION) Buf;        
        Status = ZwQueryValueKey(KeyHandle,
                                 &ValStr,
                                 KeyValueFullInformation,
                                 FullInfo,
                                 WORK_BUF_SIZE,
                                 &Result);
        if (Status == STATUS_SUCCESS && FullInfo->Type == REG_DWORD)
            HintCharSet = *((ULONG UNALIGNED *) ((PCHAR)FullInfo +
                                                 FullInfo->DataOffset));
        
        RtlInitUnicodeString(&ValStr, L"DISCONNECTTIME");
        FullInfo = (PKEY_VALUE_FULL_INFORMATION) Buf;        
        Status = ZwQueryValueKey(KeyHandle,
                                 &ValStr,
                                 KeyValueFullInformation,
                                 FullInfo,
                                 WORK_BUF_SIZE,
                                 &Result);
        
        if (Status == STATUS_SUCCESS && FullInfo->Type == REG_DWORD)
            DisconnectTime = *((ULONG UNALIGNED *) ((PCHAR)FullInfo +
                                                    FullInfo->DataOffset));
        ZwClose(KeyHandle);
    }
    else
        DEBUGMSG(1, ("Failed to open key %x\n", Status));
    DEBUGMSG(DBG_NDIS, ("Slots %x, HintCharSet %x, Disconnect %x\n",
                        Slots, HintCharSet, DisconnectTime));
    // Use the ComputerName as the discovery nickname
    NickName[0]     = 0;
    NickNameLen     = 0;
    RtlInitUnicodeString(
        &Path,
        L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\ComputerName\\ComputerName");
    
    InitializeObjectAttributes(&ObjectAttribs,
                               &Path,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);
    
    Status = ZwOpenKey(&KeyHandle, KEY_READ, &ObjectAttribs);
    
    if (Status == STATUS_SUCCESS)
    {
        RtlInitUnicodeString(&ValStr, L"ComputerName");
        FullInfo = (PKEY_VALUE_FULL_INFORMATION) Buf;
        Status = ZwQueryValueKey(KeyHandle,
                                 &ValStr,
                                 KeyValueFullInformation,
                                 FullInfo,
                                 WORK_BUF_SIZE,
                                 &Result);
        NickNameLen = 0;

        if ((HintCharSet & 0XFF) == UNICODE_CHAR_SET)
            Multiplier = 1; // As is
        else
            Multiplier = 2; // remove extra
        
        if (Status == STATUS_SUCCESS && FullInfo->Type == REG_SZ)
        {
            for (i=0; i< FullInfo->DataLength/Multiplier && i < NICK_NAME_LEN; i++)
            {
                NickName[i] = *((PUCHAR)FullInfo + FullInfo->DataOffset + Multiplier*i);
                NickNameLen++;
            }
            NickName[NICK_NAME_LEN-1] = 0;
        }
        ZwClose(KeyHandle);        
    }
    
#endif // !UNDER_CE

    // Register protocol with Ndis
    NdisZeroMemory((PVOID)&pc, sizeof(NDIS40_PROTOCOL_CHARACTERISTICS));
    pc.MajorNdisVersion             = 0x04;
    pc.MinorNdisVersion             = 0x00;
    pc.OpenAdapterCompleteHandler   = IrdaOpenAdapterComplete;
    pc.CloseAdapterCompleteHandler  = IrdaCloseAdapterComplete;
    pc.SendCompleteHandler          = IrdaSendComplete;
    pc.TransferDataCompleteHandler  = IrdaTransferDataComplete;
    pc.ResetCompleteHandler         = IrdaResetComplete;
    pc.RequestCompleteHandler       = IrdaRequestComplete;
    pc.ReceiveHandler               = IrdaReceive;
    pc.ReceiveCompleteHandler       = IrdaReceiveComplete;
    pc.StatusHandler                = IrdaStatus;
    pc.StatusCompleteHandler        = IrdaStatusComplete;
    pc.BindAdapterHandler           = IrdaBindAdapter;
    pc.UnbindAdapterHandler         = IrdaUnbindAdapter;
    pc.UnloadHandler                = NULL;
    pc.Name                         = *ProtocolName;
    pc.ReceivePacketHandler         = IrdaReceivePacket;
	
    IrlmpInitialize();

    NdisRegisterProtocol(&Status,
                         &NdisIrdaHandle,
                         (PNDIS_PROTOCOL_CHARACTERISTICS)&pc,
                         sizeof(NDIS40_PROTOCOL_CHARACTERISTICS));

    DBG_FRAME_LOG_INIT();

    DEBUGMSG(DBG_NDIS, (TEXT("-IrdaInitialize(), rc %x\n"), Status));

    return Status;
}
