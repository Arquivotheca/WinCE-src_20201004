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
#include "StdAfx.h"
#include "NDTLib.h"
#include "NDT.h"
#include "ProtocolHeader.h"
#include "Protocol.h"
#include "Binding.h"
#include "Packet.h"
#include "Medium802_3.h"
#include "Medium802_5.h"
#include "Request.h"
#include "RequestBind.h"
#include "RequestUnbind.h"
#include "RequestReset.h"
#include "RequestRequest.h"
#include "RequestSend.h"
#include "RequestReceive.h"
#include "RequestReceiveEx.h"
#include "Log.h"
#include "MDLChain.h"

#define _MAX_PKTSIZE_TO_SET_NO_OF_PKTS (4000)

//------------------------------------------------------------------------------

LONG CBinding::s_lInstanceCounter = 0;

//------------------------------------------------------------------------------

CBinding::CBinding(CProtocol *pProtocol)
{
    // A magic value for class instance identification
    m_dwMagic = NDT_MAGIC_BINDING;

    // Initialize a string
    NdisInitUnicodeString(&m_nsAdapterName, NULL);
    m_bQueuePackets = FALSE;
    m_cbQueueSrcAddr = 0;
    m_pucQueueSrcAddr = NULL;

    // We need pointer to parent protocol
    m_pProtocol = pProtocol;
    m_pProtocol->AddRef();

    // We don't know medium yet...
    m_pMedium = NULL;

    // Zero unexpected event counters
    m_ulUnexpectedEvents = 0;
    m_ulUnexpectedOpenComplete = 0;
    m_ulUnexpectedCloseComplete = 0;
    m_ulUnexpectedResetComplete = 0;
    m_ulUnexpectedSendComplete = 0;
    m_ulUnexpectedRequestComplete = 0;
    m_ulUnexpectedTransferComplete = 0;
    m_ulUnexpectedReceiveIndicate = 0;
    m_ulUnexpectedStatusIndicate = 0;
    m_ulUnexpectedResetStart = 0;
    m_ulUnexpectedResetEnd = 0;
    m_ulUnexpectedMediaConnect = 0;
    m_ulUnexpectedMediaDisconnect = 0;
    m_ulUnexpectedBreakpoints = 0;

    // Zero status counters
    m_ulTotalStatusIndicate=0;            // Total (Reset Start, Reset stop etc.) status indication
    m_ulStatusResetStart=0;                  // ResetStart indication
    m_ulStatusResetEnd=0;
    m_ulStatusMediaConnect=0;
    m_ulStatusMediaDisconnect=0;

    // Internal command timeouts & other values
    m_dwInternalTimeout = 4000;
    m_uiPacketsSend = 128;
    m_uiPacketsRecv = 128;
    m_uiBuffersPerPacket = 8;
    m_pucStaticBody = NULL;
    m_bPoolsAllocated = FALSE;

    // Protocol related local variables
    m_usLocalId = 0;
    m_usRemoteId = 0;
    m_ulConversationId = 0;    

    // Window with size 1 is no window...
    m_ulWindowSize = 1; 

    // Get instance identification
    m_lInstanceId = InterlockedIncrement(&s_lInstanceCounter);

    NdisInitializeEvent(&m_hUnbindCompleteEvent);
    m_hUnbindContext = NULL;
    m_bRecvCompleteQueued = FALSE;
    m_uiRecvCompleteDelay = 10;
    NdisAllocateSpinLock(&m_spinLockRecvComplete);
    NdisInitializeTimer(&m_timerRecvComplete, CBinding::ProtocolReceiveComplete, this);
}

//------------------------------------------------------------------------------

CBinding::~CBinding()
{
    BOOLEAN bCanceled = FALSE;
    NdisFreeSpinLock(&m_spinLockRecvComplete);
    NdisFreeEvent(&m_hUnbindCompleteEvent);
    NdisCancelTimer(&m_timerRecvComplete, &bCanceled);
    
    // Relase a string
    if (m_nsAdapterName.Buffer != NULL) NdisFreeString(m_nsAdapterName);
    // Delete queue source address if any
    delete m_pucQueueSrcAddr;
    // We don't need static packet body
    delete m_pucStaticBody;
    // Release a protocol reference
    m_pProtocol->Release();
}

//------------------------------------------------------------------------------

VOID CBinding::ProtocolOpenAdapterCompleteEx(
    IN NDIS_HANDLE ProtocolBindingContext, IN NDIS_STATUS Status
    )
{
    Log(
        NDT_DBG_OPEN_ADAPTER_COMPLETE_ENTRY, ProtocolBindingContext, Status
        );
    CBinding *pBinding = (CBinding *)ProtocolBindingContext;
    ASSERT(pBinding->m_dwMagic == NDT_MAGIC_BINDING);
    pBinding->OpenAdapterComplete(Status);
    Log(NDT_DBG_OPEN_ADAPTER_COMPLETE_EXIT);
}

//------------------------------------------------------------------------------

VOID CBinding::ProtocolCloseAdapterCompleteEx(
    IN NDIS_HANDLE ProtocolBindingContext
    )
{
    Log(
        NDT_DBG_CLOSE_ADAPTER_COMPLETE_ENTRY, ProtocolBindingContext
        );
    CBinding *pBinding = (CBinding *)ProtocolBindingContext;
    ASSERT(pBinding->m_dwMagic == NDT_MAGIC_BINDING);
    pBinding->CloseAdapterComplete();
    Log(NDT_DBG_CLOSE_ADAPTER_COMPLETE_EXIT);
}

//------------------------------------------------------------------------------

VOID CBinding::ProtocolOidRequestComplete(
    IN NDIS_HANDLE ProtocolBindingContext, IN PNDIS_OID_REQUEST OidRequest,
    IN NDIS_STATUS Status
    )
{
    Log(
        NDT_DBG_REQUEST_COMPLETE_ENTRY, ProtocolBindingContext, OidRequest, 
        Status
        );
    CBinding *pBinding = (CBinding *)ProtocolBindingContext;
    ASSERT(pBinding->m_dwMagic == NDT_MAGIC_BINDING);
    pBinding->RequestComplete(OidRequest, Status);
    Log(NDT_DBG_REQUEST_COMPLETE_EXIT);
}

//------------------------------------------------------------------------------

VOID CBinding::ProtocolStatusEx(
                                IN NDIS_HANDLE ProtocolBindingContext,
                                IN PNDIS_STATUS_INDICATION StatusIndication
                                )
{
    Log(
        NDT_DBG_STATUS_ENTRY, ProtocolBindingContext, StatusIndication->StatusCode
        );
    CBinding *pBinding = (CBinding *)ProtocolBindingContext;
    ASSERT(pBinding->m_dwMagic == NDT_MAGIC_BINDING);
    pBinding->Status(StatusIndication);
    Log(NDT_DBG_STATUS_EXIT);
}

//------------------------------------------------------------------------------

VOID CBinding::ProtocolStatusComplete(
                                      IN NDIS_HANDLE  ProtocolBindingContext
                                      )
{
    Log(NDT_DBG_STATUS_COMPLETE_ENTRY, ProtocolBindingContext);
    CBinding *pBinding = (CBinding *)ProtocolBindingContext;
    ASSERT(pBinding->m_dwMagic == NDT_MAGIC_BINDING);
    //pBinding->StatusComplete();
    Log(NDT_DBG_STATUS_COMPLETE_EXIT);
}

//------------------------------------------------------------------------------

VOID CBinding::ProtocolSendNetBufferListsComplete(
    IN NDIS_HANDLE ProtocolBindingContext, IN PNET_BUFFER_LIST NetBufferLists,
    IN ULONG SendCompleteFlags
    )
{
    Log(NDT_DBG_SEND_COMPLETE_ENTRY, ProtocolBindingContext, NetBufferLists);
    CBinding *pBinding = (CBinding *)ProtocolBindingContext;
    ASSERT(pBinding->m_dwMagic == NDT_MAGIC_BINDING);
    pBinding->SendComplete(NetBufferLists);
    Log(NDT_DBG_SEND_COMPLETE_EXIT);
}

//------------------------------------------------------------------------------

/*
VOID CBinding::ProtocolTransferDataComplete(
IN NDIS_HANDLE ProtocolBindingContext, IN PNDIS_PACKET Packet,
IN NDIS_STATUS Status, IN UINT BytesTransferred
)
{
Log(
NDT_DBG_TRANSFER_DATA_COMPLETE_ENTRY, ProtocolBindingContext, 
Packet, Status, BytesTransferred
);
CBinding *pBinding = (CBinding *)ProtocolBindingContext;
ASSERT(pBinding->m_dwMagic == NDT_MAGIC_BINDING);
pBinding->TransferDataComplete(Packet, Status, BytesTransferred);
Log(NDT_DBG_TRANSFER_DATA_COMPLETE_EXIT);
}
*/
//------------------------------------------------------------------------------

/* 

Verify we dont need this anymore, then remove

NDIS_STATUS CBinding::ProtocolReceive(
IN NDIS_HANDLE ProtocolBindingContext, IN NDIS_HANDLE MacReceiveContext,
IN PVOID HeaderBuffer, IN UINT HeaderBufferSize,
IN PVOID LookAheadBuffer, IN UINT LookaheadBufferSize, IN UINT PacketSize
)
{
Log(
NDT_DBG_RECEIVE_ENTRY, ProtocolBindingContext, MacReceiveContext, 
HeaderBuffer, HeaderBufferSize, LookAheadBuffer, LookaheadBufferSize, 
PacketSize
);
CBinding *pBinding = (CBinding *)ProtocolBindingContext;
ASSERT(pBinding->m_dwMagic == NDT_MAGIC_BINDING);
NDIS_STATUS Status = pBinding->Receive(
MacReceiveContext, HeaderBuffer, HeaderBufferSize,
LookAheadBuffer, LookaheadBufferSize, PacketSize
);
Log(NDT_DBG_RECEIVE_EXIT, Status);
return Status;
}

//------------------------------------------------------------------------------

VOID CBinding::ProtocolReceiveComplete(
IN NDIS_HANDLE ProtocolBindingContext
)
{
Log(NDT_DBG_RECEIVE_COMPLETE_ENTRY, ProtocolBindingContext);
CBinding *pBinding = (CBinding *)ProtocolBindingContext;
ASSERT(pBinding->m_dwMagic == NDT_MAGIC_BINDING);
pBinding->ReceiveComplete();
Log(NDT_DBG_RECEIVE_COMPLETE_EXIT);
}

//------------------------------------------------------------------------------
/*
INT CBinding::ProtocolReceivePacket(
IN NDIS_HANDLE ProtocolBindingContext, IN PNDIS_PACKET Packet
)
{
Log(NDT_DBG_RECEIVE_PACKET_ENTRY, ProtocolBindingContext, Packet);
CBinding *pBinding = (CBinding *)ProtocolBindingContext;
ASSERT(pBinding->m_dwMagic == NDT_MAGIC_BINDING);
INT Code = pBinding->ReceivePacket(Packet);
Log(NDT_DBG_RECEIVE_PACKET_EXIT, Code);
return Code;
}
*/
//------------------------------------------------------------------------------

VOID CBinding::ProtocolReceiveNetBufferLists(
    IN NDIS_HANDLE ProtocolBindingContext, IN PNET_BUFFER_LIST NetBufferLists,
    IN NDIS_PORT_NUMBER PortNumber, IN ULONG NumberOfNetBufferLists,
    IN ULONG ReceiveFlags
    )
{
    Log(NDT_DBG_RECEIVE_NET_BUFFER_LISTS_ENTRY, ProtocolBindingContext, NetBufferLists, PortNumber, NumberOfNetBufferLists, ReceiveFlags);
    CBinding *pBinding = (CBinding *)ProtocolBindingContext;
    ASSERT(pBinding->m_dwMagic == NDT_MAGIC_BINDING);
    pBinding->ReceiveNetBufferLists(NetBufferLists, NumberOfNetBufferLists, ReceiveFlags);
    Log(NDT_DBG_RECEIVE_NET_BUFFER_LISTS_EXIT);
}

//------------------------------------------------------------------------------

NDIS_STATUS CBinding::ProtocolUnbindAdapterEx(
    IN NDIS_HANDLE UnbindContext,
    IN NDIS_HANDLE  ProtocolBindingContext
    )
{
    NDIS_STATUS Status;

    Log(NDT_DBG_UNBIND_ADAPTER_ENTRY, ProtocolBindingContext, UnbindContext);
    CBinding *pBinding = (CBinding *)ProtocolBindingContext;
    ASSERT(pBinding->m_dwMagic == NDT_MAGIC_BINDING);
    Status = pBinding->UnbindAdapter(UnbindContext);
    Log(NDT_DBG_UNBIND_ADAPTER_EXIT, Status);
    return Status;
}

//------------------------------------------------------------------------------
/*
VOID CBinding::ProtocolCoSendComplete(
IN NDIS_STATUS Status, IN NDIS_HANDLE ProtocolVcContext,
IN PNDIS_PACKET Packet
)
{
Log(NDT_DBG_CO_SEND_COMPLETE_ENTRY, Status, ProtocolVcContext, Packet);
Log(NDT_DBG_CO_SEND_COMPLETE_EXIT);
}
*/
//------------------------------------------------------------------------------

VOID CBinding::ProtocolCoStatus(
                                IN NDIS_HANDLE ProtocolBindingContext,
                                IN NDIS_HANDLE ProtocolVcContext OPTIONAL,
                                IN NDIS_STATUS GeneralStatus, IN PVOID StatusBuffer,
                                IN UINT StatusBufferSize
                                )
{
    Log(
        NDT_DBG_CO_STATUS_ENTRY, ProtocolBindingContext, ProtocolVcContext, 
        GeneralStatus, StatusBuffer, StatusBufferSize
        );
    CBinding *pBinding = (CBinding *)ProtocolBindingContext;
    ASSERT(pBinding->m_dwMagic == NDT_MAGIC_BINDING);
    Log(NDT_DBG_CO_STATUS_EXIT);
}

//------------------------------------------------------------------------------
/*
UINT CBinding::ProtocolCoReceivePacket(
IN NDIS_HANDLE ProtocolBindingContext,
IN NDIS_HANDLE ProtocolVcContext, IN PNDIS_PACKET Packet
)
{
Log(
NDT_DBG_CO_RECEIVE_PACKET_ENTRY, ProtocolBindingContext, 
ProtocolVcContext, Packet
);
CBinding *pBinding = (CBinding *)ProtocolBindingContext;
ASSERT(pBinding->m_dwMagic == NDT_MAGIC_BINDING);
UINT Code = 0;
Log(NDT_DBG_CO_RECEIVE_PACKET_EXIT, Code);
return Code;
}
*/
//------------------------------------------------------------------------------

VOID CBinding::ProtocolCoAfRegisterNotify(
    IN NDIS_HANDLE ProtocolBindingContext, IN PCO_ADDRESS_FAMILY AddressFamily
    )
{
    Log(NDT_DBG_CO_AF_REGISTER_ENTRY, ProtocolBindingContext, AddressFamily);
    CBinding *pBinding = (CBinding *)ProtocolBindingContext;
    ASSERT(pBinding->m_dwMagic == NDT_MAGIC_BINDING);
    Log(NDT_DBG_CO_AF_REGISTER_EXIT);
}

//------------------------------------------------------------------------------

VOID CBinding::ProtocolReceiveComplete( IN PVOID SystemSpecific1, 
                                       IN PVOID FunctionContext,
                                       IN PVOID SystemSpecific2, 
                                       IN PVOID SystemSpecific3
                                       )
{
    CBinding *pBinding = (CBinding *) FunctionContext;
    pBinding->ReceiveComplete();
}

//------------------------------------------------------------------------------

VOID CBinding::OpenAdapterComplete(
                                   NDIS_STATUS status
                                   )
{
    // Find the request in pending queue and remove it
    CRequestBind* pRequest = FindBindRequest(TRUE);

    // If we get OpenAdapterComplete and there is no request pending
    if (pRequest == NULL) {
        Log(NDT_ERR_UNEXP_OPEN_ADAPTER_COMPLETE);
        NdisInterlockedIncrement((LONG*)&m_ulUnexpectedEvents);
        NdisInterlockedIncrement((LONG*)&m_ulUnexpectedOpenComplete);
        return;
    }

    // Check object signature
    ASSERT(pRequest->m_dwMagic == NDT_MAGIC_REQUEST_BIND);

    // Set results code
    pRequest->m_status = status;

    // Complete request
    pRequest->Complete();

    // We have relase it
    pRequest->Release();
}

//------------------------------------------------------------------------------

VOID CBinding::CloseAdapterComplete()
{
    //Set the event so the request can move on.
    NdisSetEvent(&m_hUnbindCompleteEvent);

    //We must have saved an unbind context
    ASSERT(m_hUnbindContext != NULL);

    NdisCompleteUnbindAdapterEx(m_hUnbindContext);

    // Find the request in pending queue and remove it
    CRequestUnbind* pRequest = FindUnbindRequest(TRUE);

    // If we get CloseAdapterComplete and there is no request pending
    if (pRequest == NULL) {
        Log(NDT_ERR_UNEXP_CLOSE_ADAPTER_COMPLETE);
        NdisInterlockedIncrement((LONG*)&m_ulUnexpectedEvents);
        NdisInterlockedIncrement((LONG*)&m_ulUnexpectedCloseComplete);
        return;
    }

    // Check object signature
    ASSERT(pRequest->m_dwMagic == NDT_MAGIC_REQUEST_UNBIND);

    //TODO: We no longer return a status out of the close, so set status to success
    pRequest->m_status = NDIS_STATUS_SUCCESS;

    // Complete request
    pRequest->Complete();

    // We have relase it
    pRequest->Release();

    // NDIS didn't hold a pointer anymore as context
    Release();
}

//------------------------------------------------------------------------------

VOID CBinding::RequestComplete(NDIS_OID_REQUEST* pOidRequest, NDIS_STATUS status)
{
    // Find the request in pending queue and remove it
    CRequestRequest* pRequest = FindRequestRequest(TRUE);

    // If we get RequestComplete and there is no request pending
    if (pRequest == NULL || pRequest->m_oid != pOidRequest->DATA.QUERY_INFORMATION.Oid) {
        Log(NDT_ERR_UNEXP_REQUEST_COMPLETE);
        NdisInterlockedIncrement((LONG*)&m_ulUnexpectedEvents);
        NdisInterlockedIncrement((LONG*)&m_ulUnexpectedRequestComplete);
        return;
    }

    //Set the request's status
    pRequest->m_status = status;

    // Complete request
    pRequest->Complete();

    pRequest->Release();
}

//------------------------------------------------------------------------------

void CBinding::SendComplete(PNET_BUFFER_LIST pNetBufferList)
{
    CPacket* pPacket = NULL;
    CRequest* pRequest = NULL;
    PNET_BUFFER_LIST currNetBufferList = pNetBufferList, nextNetBufferList = NULL;

    while (currNetBufferList != NULL)
    {
        //walk the NBL and complete the packet for each NB
        PNET_BUFFER pNB = NET_BUFFER_LIST_FIRST_NB(currNetBufferList);

        while (pNB)
        {
            //UNDONE: Cant figure our when pNB->ProtocolReserved was assigned to be a CPacket. Check old code.
            pPacket = (CPacket*)pNB->ProtocolReserved[0];
            if (pPacket == NULL || pPacket->m_dwMagic != NDT_MAGIC_PACKET) {
                Log(NDT_ERR_UNEXP_SEND_COMPLETE_PACKET);
                NdisInterlockedIncrement((LONG*)&m_ulUnexpectedEvents);
                NdisInterlockedIncrement((LONG*)&m_ulUnexpectedSendComplete);
                goto cleanUp;
            }

            if (m_pProtocol->m_bLogPackets) {
                LogX(
                    _T("%08lu Bind %d SendComplete #%04lu/%02u/%02x Status 0x%08x\n"),
                    GetTickCount(), m_lInstanceId,
                    pPacket->m_pProtocolHeader->ulSequenceNumber,
                    pPacket->m_pProtocolHeader->usReplyId,
                    pPacket->m_pProtocolHeader->ucResponseMode, NET_BUFFER_LIST_STATUS(pNetBufferList)
                    );
            }

            // Find the request in pending queue and remove it
            pRequest = pPacket->m_pRequest;
            if (pRequest == NULL) {
                Log(NDT_ERR_UNEXP_SEND_COMPLETE_REQ);
                NdisInterlockedIncrement((LONG*)&m_ulUnexpectedEvents);
                NdisInterlockedIncrement((LONG*)&m_ulUnexpectedSendComplete);
                // Remove it from list and return between free
                RemovePacketFromSent(pPacket);
                AddPacketToFreeSend(pPacket);
                //CHANGED: We no longer are completing only one packet, so we have to continue processing NBs in the NBL
                pNB = NET_BUFFER_NEXT_NB(pNB);
                continue;
            }

            // Call complete routine in a request
            switch (pRequest->m_eType) {
   case NDT_REQUEST_SEND:
       // Check object signature & call send complete
       ASSERT(pRequest->m_dwMagic == NDT_MAGIC_REQUEST_SEND);
       ((CRequestSend*)pRequest)->SendComplete(pPacket, NET_BUFFER_LIST_STATUS(pNetBufferList), (NDIS_GET_NET_BUFFER_LIST_CANCEL_ID(pNetBufferList)? TRUE: FALSE));
       break;
   case NDT_REQUEST_RECEIVE:
       // Check object signature & call send complete
       ASSERT(pRequest->m_dwMagic == NDT_MAGIC_REQUEST_RECEIVE);
       ((CRequestReceive*)pRequest)->SendComplete(pPacket, NET_BUFFER_LIST_STATUS(pNetBufferList));
       break;
   case NDT_REQUEST_RECEIVE_EX:
       // Check object signature & call send complete
       ASSERT(pRequest->m_dwMagic == NDT_MAGIC_REQUEST_RECEIVE_EX);
       ((CRequestReceiveEx*)pRequest)->SendComplete(pPacket, NET_BUFFER_LIST_STATUS(pNetBufferList));
       break;
   default:
       Log(NDT_ERR_UNEXP_SEND_COMPLETE_REQ_TYPE, pRequest->m_eType);
       NdisInterlockedIncrement((LONG*)&m_ulUnexpectedEvents);
       NdisInterlockedIncrement((LONG*)&m_ulUnexpectedSendComplete);
       // Remove it from list and return between free
       RemovePacketFromSent(pPacket);
       AddPacketToFreeSend(pPacket);
       //CHANGED: We no longer are completing only one packet, so we have to continue processing NBs in the NBL
            }

            pNB = NET_BUFFER_NEXT_NB(pNB);
        }

        currNetBufferList = NET_BUFFER_LIST_NEXT_NBL(currNetBufferList);
    }
cleanUp:
    currNetBufferList = pNetBufferList;
    while (currNetBufferList) {
        nextNetBufferList = NET_BUFFER_LIST_NEXT_NBL(currNetBufferList);
        NdisFreeNetBufferList(currNetBufferList);   
        currNetBufferList = nextNetBufferList;
    }
}


//------------------------------------------------------------------------------

void CBinding::ReceiveComplete()
{
    CPacket* pPacket = NULL;
    NDIS_STATUS status = NDIS_STATUS_SUCCESS;

    // Find requests in pending queue if any but don't remove it
    CRequestSend* pRequestSend = FindSendRequest(FALSE);
    CRequestReceive* pRequestReceive = FindReceiveRequest(FALSE);
    CRequestReceiveEx* pRequestReceiveEx = FindReceiveExRequest(FALSE);

    // Check if we are get correct request objects
    ASSERT(
        pRequestReceive == NULL || 
        pRequestReceive->m_dwMagic == NDT_MAGIC_REQUEST_RECEIVE
        );
    ASSERT(
        pRequestSend == NULL || 
        pRequestSend->m_dwMagic == NDT_MAGIC_REQUEST_SEND
        );
    ASSERT(
        pRequestReceiveEx == NULL ||
        pRequestReceiveEx->m_dwMagic == NDT_MAGIC_REQUEST_RECEIVE_EX
        );
    // Process all transferred packets from list
    m_listReceivedPackets.AcquireSpinLock();
    pPacket = (CPacket*)m_listReceivedPackets.GetHead();
    while (pPacket != NULL) {
        if (pPacket->m_bTransferred) {
            m_listReceivedPackets.Remove(pPacket);
            m_listReceivedPackets.ReleaseSpinLock();
            // Log packet
            if (m_pProtocol->m_bLogPackets) {
                PROTOCOL_HEADER* pHeader = pPacket->m_pProtocolHeader;
                LogX(
                    _T("%08lu Bind %d ReceiveComplete #%04lu/%02u/%02x\n"),
                    GetTickCount(), m_lInstanceId, pHeader->ulSequenceNumber, 
                    pHeader->usReplyId, pHeader->ucResponseMode
                    );
            }
            // Find destination for packet
            status = NDIS_STATUS_NOT_RECOGNIZED;
            if (status != NDIS_STATUS_SUCCESS && pRequestSend != NULL) {
                status = pRequestSend->Receive(pPacket);
            }
            if (status != NDIS_STATUS_SUCCESS && pRequestReceive != NULL) {
                status = pRequestReceive->Receive(pPacket);
            }
            if (status != NDIS_STATUS_SUCCESS && pRequestReceiveEx != NULL) {
                status = pRequestReceiveEx->Receive(pPacket);
            }
            if (status != NDIS_STATUS_SUCCESS) {
                // Else we have to release packet
                AddPacketToFreeRecv(pPacket);
            }
            // Try next packet
            m_listReceivedPackets.AcquireSpinLock();
            pPacket = (CPacket*)m_listReceivedPackets.GetHead();
        } else {
            pPacket = (CPacket*)m_listReceivedPackets.GetNext(pPacket);
        }
    }
    m_listReceivedPackets.ReleaseSpinLock();

    // We have to release a pointer
    if (pRequestReceiveEx != NULL) pRequestReceiveEx->Release();
    if (pRequestReceive != NULL) pRequestReceive->Release();
    if (pRequestSend != NULL) pRequestSend->Release();

    //set the receive complete flag to false so more can be scheduled
    m_bRecvCompleteQueued = FALSE;
}

//------------------------------------------------------------------------------

NDIS_STATUS CBinding::UnbindAdapter(NDIS_HANDLE hUnbindContext)
{
    NDIS_STATUS status = NDIS_STATUS_SUCCESS;

    //Save the unbind context in case the call pends
    m_hUnbindContext = hUnbindContext;

    // Close adapter
    if (m_hAdapter != NULL) {
        status = NdisCloseAdapterEx(m_hAdapter);
    }

    if (status == NDIS_STATUS_SUCCESS)
    {
        //Set the event so the request can move on.
        NdisSetEvent(&m_hUnbindCompleteEvent);

        m_hAdapter = NULL;

        // Remove itself from a list
        m_pProtocol->RemoveBindingFromList(this);

        // And because NDIS will hopefully not use binding context anymore
        Release();

    }

    // Return result
    return status;
}

//------------------------------------------------------------------------------

void CBinding::AddRequestToList(CRequest *pRequest)
{
    // Nobody else should play with a list
    m_listRequest.AcquireSpinLock();
    // Add to queue
    m_listRequest.AddTail(pRequest);
    // We hold a reference
    pRequest->AddRef();
    // We are done
    m_listRequest.ReleaseSpinLock();
}

//------------------------------------------------------------------------------

void CBinding::RemoveRequestFromList(CRequest *pRequest)
{
    // Nobody else should play with a list
    m_listRequest.AcquireSpinLock();
    // Remove it from list
    m_listRequest.Remove(pRequest);
    // We don't hold a reference anymore
    pRequest->Release();
    // We are done
    m_listRequest.ReleaseSpinLock();
}

//------------------------------------------------------------------------------

CRequest* CBinding::FindRequestByType(
                                      NDT_ENUM_REQUEST_TYPE eType, BOOL bRemove
                                      )
{
    CRequest* pRequest = NULL;

    // Nobody else should play with a list
    m_listRequest.AcquireSpinLock();

    // Walk list for request
    pRequest = (CRequest*)m_listRequest.GetHead();
    while (pRequest != NULL) {
        if (pRequest->m_eType == eType) break;
        pRequest = (CRequest*)m_listRequest.GetNext(pRequest);
    }
    // When we find it - return it back (but increase reference count)
    if (pRequest != NULL) {
        if (bRemove) {
            m_listRequest.Remove(pRequest);
        } else {
            pRequest->AddRef();
        }
    }

    // We are done
    m_listRequest.ReleaseSpinLock();
    return pRequest;
}

//------------------------------------------------------------------------------

CRequest* CBinding::GetRequestFromList(BOOL bRemove)
{
    CRequest* pRequest = NULL;

    // Nobody else should play with a list
    m_listRequest.AcquireSpinLock();
    pRequest = (CRequest*)m_listRequest.GetHead();
    if (pRequest != NULL) {
        if (bRemove) {
            m_listRequest.Remove(pRequest);
        } else {
            pRequest->AddRef();
        }
    }

    // We are done
    m_listRequest.ReleaseSpinLock();
    return pRequest;
}

//------------------------------------------------------------------------------

void CBinding::OpenAdapter(NDIS_MEDIUM medium)
{
    NDIS_STATUS status = NDIS_STATUS_SUCCESS;
    UINT ix = 0;

    m_pProtocol->AddBindingToList(this);

    // Set a medium info object
    switch (medium) {
   case NdisMedium802_3:
       m_pMedium = new CMedium802_3(this);
       break;
   case NdisMedium802_5:
       m_pMedium = new CMedium802_5(this);
       break;
    }

    if (m_pMedium != NULL) {

        // Initialize medium related information
        status = m_pMedium->Init(sizeof(PROTOCOL_HEADER));
        ASSERT(status == NDIS_STATUS_SUCCESS);

        LogX(_T("NDT: CBinding: Max Pkt size of Miniport=%d \n"), m_pMedium->m_uiMaxFrameSize);

        if (m_pMedium->m_uiMaxFrameSize > _MAX_PKTSIZE_TO_SET_NO_OF_PKTS)
        {
            //This is to control the number of packets preallocated in this NDT protocol driver to send & recv,
            //when max packet size reported by miniport driver is not 1514 bytes but say 8192 bytes.
            m_uiPacketsSend = 64;
            m_uiPacketsRecv = 64;

            LogX(_T("NDT: CBinding: Adjusting Pkts send =%d, Recv =%d \n"), m_uiPacketsSend, m_uiPacketsRecv);
        }

        // Allocate and initialize structure for static packet body
        delete m_pucStaticBody;
        m_pucStaticBody = new BYTE[m_pMedium->m_uiMaxFrameSize + 256];
        for (ix = 0; ix < m_pMedium->m_uiMaxFrameSize + 256; ix++) {
            m_pucStaticBody[ix] = (BYTE)ix;
        }

    }

}

//------------------------------------------------------------------------------

void CBinding::CloseAdapter()
{
    CRequest* pRequest = NULL;

    // Remove all possible pending requests
    while ((pRequest = GetRequestFromList(TRUE)) != NULL) {
        switch (pRequest->m_eType) {
      case NDT_REQUEST_SEND:
          ((CRequestSend*)pRequest)->StopBeat();
          break;
        }
        pRequest->Complete();
        pRequest->Release();
    }

    // Release pointer to medium specific info
    if (m_pMedium != NULL) {
        m_pMedium->Release();
        m_pMedium = NULL;
    }

    // Deallocate a pool
    DeallocatePools();

    // We aren't binded to adapter anymore
    m_pProtocol->RemoveBindingFromList(this);
}

//------------------------------------------------------------------------------

NDIS_STATUS CBinding::AllocatePools()
{
    NDIS_STATUS status = NDIS_STATUS_SUCCESS;
    NET_BUFFER_LIST_POOL_PARAMETERS NBLPoolParameters;
    NET_BUFFER_POOL_PARAMETERS NBPoolParameters; 
    PNET_BUFFER pNB;
    UINT uiPackets = m_uiPacketsSend + m_uiPacketsRecv;
    CPacket* pPacket = NULL;
    UINT i = 0;

    LogX(_T("NDT:CBinding:AllocatePools: Tot Pkts=%d"), uiPackets);
    LogX(_T("NDT:CBinding:AllocatePools: size in bytes Buf1=%d, Buf2=%d, Buf3=%d\n"), m_pMedium->m_uiHeaderSize, 
        sizeof(PROTOCOL_HEADER),
        m_pMedium->m_uiMaxFrameSize - sizeof(PROTOCOL_HEADER));

    NBLPoolParameters.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    NBLPoolParameters.Header.Revision = NET_BUFFER_LIST_POOL_PARAMETERS_REVISION_1;
    NBLPoolParameters.Header.Size = sizeof(NET_BUFFER_LIST_POOL_PARAMETERS);

    NBLPoolParameters.ProtocolId = NDIS_PROTOCOL_ID_DEFAULT;
    NBLPoolParameters.fAllocateNetBuffer = FALSE;
    NBLPoolParameters.ContextSize = 0;
    NBLPoolParameters.PoolTag = NDT6_TAG; //TODO: AllocatePools: make NDT6 the pool tag
    NBLPoolParameters.DataSize = 0;


    m_hNBLPool = NdisAllocateNetBufferListPool(
        m_pProtocol->m_hProtocol,
        &NBLPoolParameters
        );

    if (m_hNBLPool == NULL) goto cleanUp;

    // Allocate a net buffer list pool
    NBPoolParameters.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
    NBPoolParameters.Header.Revision = NET_BUFFER_LIST_POOL_PARAMETERS_REVISION_1;
    NBPoolParameters.Header.Size = sizeof(NET_BUFFER_POOL_PARAMETERS);

    NBPoolParameters.PoolTag = NDT6_TAG; //TODO: AllocatePools: make NDT6 the pool tag
    NBPoolParameters.DataSize = 0;

    m_hNBPool = NdisAllocateNetBufferPool(
        m_pProtocol->m_hProtocol,
        &NBPoolParameters
        );

    if (m_hNBPool == NULL) goto cleanUp;

    // Now create an internal pool
    for (i = 0; i < uiPackets; i++) {

        //Create a net buffer for the packet
        pNB = NdisAllocateNetBuffer(m_hNBPool, NULL, 0, 0);

        // Create a new CPacket
        //        pPacket = new((void *)pPacket) CPacket(this, pNB);
        pPacket = new CPacket(this, pNB);
        pNB->ProtocolReserved[0] = (PVOID) pPacket;

        pPacket->m_pucMediumHeader = new BYTE[m_pMedium->m_uiHeaderSize];
        ASSERT(pPacket->m_pucMediumHeader != NULL);
        pPacket->m_pProtocolHeader = new PROTOCOL_HEADER;
        ASSERT(pPacket->m_pProtocolHeader != NULL);

        // Add a packet to free packet list
        if (i < m_uiPacketsSend) {
            AddPacketToFreeSend(pPacket);
        } else {
            pPacket->m_pucBody = new BYTE[
                m_pMedium->m_uiMaxFrameSize - sizeof(PROTOCOL_HEADER)
            ];
            ASSERT(pPacket->m_pucBody != NULL);
            AddPacketToFreeRecv(pPacket);
        }
    }

    m_bPoolsAllocated = TRUE;
    return NDIS_STATUS_SUCCESS;

cleanUp:
    DeallocatePools();
    return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CBinding::DeallocatePools()
{
    CPacket* pPacket = NULL;

    LogX(_T("NDT:CBinding:DeallocatePools \n"));

    // Remove & destroy packets from all queues
    while ((pPacket = GetPacketFromReceived(TRUE)) != NULL) pPacket->Release();
    while ((pPacket = GetPacketFromSent(TRUE)) != NULL) pPacket->Release();
    while ((pPacket = GetPacketFromFreeSend(TRUE)) != NULL) pPacket->Release();
    while ((pPacket = GetPacketFromFreeRecv(TRUE)) != NULL) pPacket->Release();
    // Release netbuffer pool
    if (m_hNBPool != NULL) NdisFreeNetBufferPool(m_hNBPool);
    m_hNBPool = NULL;

    // release netbufferlist pool
    if (m_hNBLPool != NULL) NdisFreeNetBufferListPool(m_hNBLPool);
    m_hNBLPool = NULL;

    // Set status flag to binding
    m_bPoolsAllocated = FALSE;
    return NDIS_STATUS_SUCCESS;
}

//------------------------------------------------------------------------------

void CBinding::AddPacketToList(CObjectList* pPacketList, CPacket* pPacket)
{
    pPacketList->AcquireSpinLock();
    pPacketList->AddTail(pPacket);
    pPacketList->ReleaseSpinLock();
}

//------------------------------------------------------------------------------

void CBinding::RemovePacketFromList(CObjectList* pPacketList, CPacket* pPacket)
{
    pPacketList->AcquireSpinLock();
    pPacketList->Remove(pPacket);
    pPacketList->ReleaseSpinLock();
}

//------------------------------------------------------------------------------

CPacket* CBinding::GetPacketFromList(CObjectList* pPacketList, BOOL bRemove)
{
    CPacket *pPacket = NULL;

    pPacketList->AcquireSpinLock();
    pPacket = (CPacket*)pPacketList->GetHead();
    if (pPacket != NULL) {
        if (bRemove) pPacketList->Remove(pPacket);
        else pPacket->AddRef();
    }
    pPacketList->ReleaseSpinLock();
    return pPacket;
}

//------------------------------------------------------------------------------

BOOLEAN CBinding::GetPacketsFromList(
                                     CObjectList* pPacketList, CPacket** apPackets, ULONG ulCount
                                     )
{
    BOOLEAN bOk;

    pPacketList->AcquireSpinLock();
    bOk = pPacketList->m_uiItems >= ulCount;
    if (bOk) {
        while (ulCount > 0) {
            *apPackets = (CPacket*)pPacketList->GetHead();
            pPacketList->Remove(*apPackets);
            apPackets++;
            ulCount--;
        }
    }
    pPacketList->ReleaseSpinLock();
    return bOk;
}

//------------------------------------------------------------------------------

BOOLEAN CBinding::IsPacketListEmpty(CObjectList* pPacketList)
{
    BOOLEAN bEmpty;

    pPacketList->AcquireSpinLock();
    bEmpty = pPacketList->m_uiItems == 0;
    pPacketList->ReleaseSpinLock();
    return bEmpty;
}

//------------------------------------------------------------------------------

NDIS_STATUS CBinding::BuildProtocolHeader(
    CPacket* pPacket, CMDLChain* pMDLChain, UINT uiSize, BYTE ucResponseMode, BYTE ucFirstByte, 
    ULONG ulSequenceNumber, ULONG ulConversationId, USHORT usReplyId
    )
{
    NDIS_STATUS status = NDIS_STATUS_SUCCESS;
    PROTOCOL_HEADER* pHeader = pPacket->m_pProtocolHeader;

    pHeader->ucDSAP = 0xAA;
    pHeader->ucSSAP = 0xAA;
    pHeader->ucControl = 0x03;
    pHeader->ucPID0 = 0x00;
    pHeader->ucPID1 = 0x00;
    pHeader->ucPID2 = 0x00;
    pHeader->usDIX = 0x3781;

    pHeader->ulSignature = 0x5349444E;
    pHeader->usTargetPortId = m_usRemoteId;
    pHeader->usSourcePortId = m_usLocalId;
    pHeader->ulSequenceNumber = ulSequenceNumber;
    pHeader->ulConversationId = ulConversationId;
    pHeader->ucResponseMode = ucResponseMode;
    pHeader->ucFirstByte = ucFirstByte;
    pHeader->usReplyId = usReplyId;

    pHeader->uiSize = uiSize;

    // TODO: Compute checksum
    pHeader->ulCheckSum = 0;
    pMDLChain->AddMDLAtBack(
        pMDLChain->CreateMDL(
        pPacket->m_pProtocolHeader,
        sizeof(PROTOCOL_HEADER)
        )
        );
    return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CBinding::CheckProtocolHeader(
    PVOID pvLookAheadBuffer, UINT uiLookaheadBufferSize, UINT uiPacketSize
    )
{
    NDIS_STATUS status = NDIS_STATUS_NOT_RECOGNIZED;
    PROTOCOL_HEADER* pHeader = (PROTOCOL_HEADER*)pvLookAheadBuffer;

    // This should never happen
    if (uiLookaheadBufferSize < sizeof(PROTOCOL_HEADER)) goto cleanUp;

    // Signature and other constants in header should be ours
    if (
        pHeader->ucDSAP != 0xAA || pHeader->ucSSAP != 0xAA ||
        pHeader->usDIX != 0x3781 || pHeader->ulSignature != 0x5349444E
        ) goto cleanUp;

    // Target port id should be ours
    if (pHeader->usTargetPortId != m_usLocalId) goto cleanUp;

    // And remote should fit also
    if (pHeader->usSourcePortId != m_usRemoteId) goto cleanUp;

   // Conversation id should be correct
   if (pHeader->ulConversationId != m_ulConversationId) goto cleanUp;    

    // If data size of wired ethernet frame is 46 then it may indicate that the packet was padded with 0 at the sender's end.
    // In that case uiPacketSize would be more than the actual size pHeader->uiSize. We do want to consider such packet.
    // Also in case of wireless, for packets of smaller sizes theere is diff between pHeader->uiSize & uiPacketSize
    // & the diff was always 14 bytes, but uiPacketSize was not constant (like 46 bytes for wired ethernet).
    if (pHeader->uiSize != uiPacketSize){
        // The actual packet comparision is not possible due to padding done for small size packets.
        // Also there is not common law which is same for all medias, eg. wired ethernet padds til the data+padding is 46, but it
        // does not seem true for wireless.

        //So the right way would be not to make above comparision & to use checksum to verify the packet integrity.

        //ASSERT(0);
        //goto cleanUp;
    }

    // When we get there so packet is ours....
    status = NDIS_STATUS_SUCCESS;

cleanUp:
    return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CBinding::BuildPacketBody(
                                      CPacket* pPacket, CMDLChain* pMDLChain, BYTE ucSizeMode, UINT uiSize, BYTE ucFirstByte
                                      )
{
    NDIS_STATUS status = NDIS_STATUS_SUCCESS;
    UINT uiBuffers = 0;
    UINT uiMaxRandSize = 0;
    UINT uiWorkSize = 0;
    BYTE* pucWorkPos = NULL;
    UINT uiBufferSize = 0;

    // Special situation when size is zero
    if (uiSize == 0) return status;

    pPacket->m_bBodyStatic = TRUE;
    ucSizeMode &= NDT_PACKET_BUFFERS_MASK;

    if (ucSizeMode == NDT_PACKET_BUFFERS_NORMAL) {
        pMDLChain->AddMDLAtBack(pMDLChain->CreateMDL(m_pucStaticBody + ucFirstByte, uiSize));

    } else {

        uiMaxRandSize = 4 * m_pMedium->m_uiMaxFrameSize/m_uiBuffersPerPacket;
        if (ucSizeMode == NDT_PACKET_BUFFERS_SMALL) {
            uiMaxRandSize = 1 + uiMaxRandSize/8;
        }

        pucWorkPos = m_pucStaticBody + ucFirstByte;
        uiWorkSize = 0;
        uiBuffers = 0;

        while (uiWorkSize < uiSize && uiBuffers < m_uiBuffersPerPacket - 1) {
            // Get next buffer size
            switch (ucSizeMode) {
         case NDT_PACKET_BUFFERS_ZEROS:
             uiBufferSize = NDT_NdisGetRandom(0, 2);
             if (uiBufferSize != 0) {
                 uiBufferSize = NDT_NdisGetRandom(0, uiMaxRandSize);
             }
             break;
         case NDT_PACKET_BUFFERS_ONES:
             uiBufferSize = NDT_NdisGetRandom(0, 2);
             if (uiBufferSize != 1) {
                 uiBufferSize = NDT_NdisGetRandom(0, uiMaxRandSize);
             }
             break;
         case NDT_PACKET_BUFFERS_RANDOM:
         case NDT_PACKET_BUFFERS_SMALL:
         default:
             uiBufferSize = NDT_NdisGetRandom(0, uiMaxRandSize);
             break;
            }
            // We don't need more that uiSize bytes in packet
            if (uiBufferSize > uiSize - uiWorkSize) {
                uiBufferSize = uiSize - uiWorkSize;
            }
            // Attach buffer
            pMDLChain->AddMDLAtBack(pMDLChain->CreateMDL(pucWorkPos, uiBufferSize));
            pucWorkPos += uiBufferSize;
            uiWorkSize += uiBufferSize;
        }

        // We should chain full packet
        if (uiWorkSize < uiSize) {
            uiBufferSize = uiSize - uiWorkSize;
            pMDLChain->AddMDLAtBack(pMDLChain->CreateMDL(pucWorkPos, uiBufferSize));
        }
    }

    return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CBinding::CheckPacketBody(
                                      PVOID pvBody, BYTE ucSizeMode, UINT uiSize, BYTE ucFirstByte
                                      )
{
    NDIS_STATUS status = NDIS_STATUS_SUCCESS;

    if (uiSize == 0) return status;
    if (NdisEqualMemory(m_pucStaticBody + ucFirstByte, pvBody, uiSize) != 0) {
        status = NDIS_STATUS_FAILURE;
    }
    return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CBinding::BuildPacketToSend(
                                        CPacket* pPacket, BYTE* pucSrcAddr, BYTE* pucDestAddr, UCHAR ucResponseMode, 
                                        BYTE ucSizeMode, UINT uiSize, ULONG ulSequenceNumber, ULONG ulConversationId, USHORT usReplyId
                                        )
{
    NDIS_STATUS status = NDIS_STATUS_SUCCESS;
    CMDLChain *pMDLChain = new CMDLChain(m_hNBPool);
    BYTE ucFirstByte = (BYTE)NDT_NdisGetRandom(0, 255);
    UINT uiTotalSize = uiSize;

    // Get media header
    status = m_pMedium->BuildMediaHeader(
        pPacket, pMDLChain, pucDestAddr, pucSrcAddr, uiSize
        );

    if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

    // Get protocol header
    status = BuildProtocolHeader(
        pPacket, pMDLChain, uiSize, ucResponseMode, ucFirstByte, ulSequenceNumber, ulConversationId, usReplyId
        );
    if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

    // Fix a data size
    uiSize -= sizeof(PROTOCOL_HEADER);

    // And packet body
    status = BuildPacketBody(pPacket, pMDLChain, ucSizeMode, uiSize, ucFirstByte);
    if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

    pPacket->PopulateNetBuffer(pMDLChain, uiTotalSize + m_pMedium->m_uiHeaderSize);
cleanUp:   
    pMDLChain->Release();
    return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CBinding::BuildPacketForResponse(
    CPacket* pPacket, CPacket* pRecvPacket
    )
{
    NDIS_STATUS status = NDIS_STATUS_SUCCESS;
    CMDLChain *pMDLChain = new CMDLChain(m_hNBPool);
    PMDL pCurrentMDL = NULL;
    PROTOCOL_HEADER* pHeader = pPacket->m_pProtocolHeader;
    PROTOCOL_HEADER* pRecvHeader = pRecvPacket->m_pProtocolHeader;

    // Copy protocol header
    NdisMoveMemory(pHeader, pRecvHeader, sizeof(PROTOCOL_HEADER));
    pHeader->ucResponseMode |= NDT_RESPONSE_FLAG_RESPONSE;
    pHeader->usSourcePortId = pRecvHeader->usTargetPortId;
    pHeader->usTargetPortId = pRecvHeader->usSourcePortId;
    pHeader->uiSize = sizeof(PROTOCOL_HEADER);

    pCurrentMDL = pMDLChain->CreateMDL(pHeader, sizeof(PROTOCOL_HEADER));
    pMDLChain->AddMDLAtBack(pCurrentMDL);

    if ((pHeader->ucResponseMode & NDT_RESPONSE_MASK) == NDT_RESPONSE_FULL) {
        pHeader->uiSize = pRecvHeader->uiSize;
    }
    else {
        //pad it so as to make it at least 64 bytes
        pHeader->uiSize = 50;
    }

    status = BuildPacketBody(
        pPacket, pMDLChain, NDT_PACKET_BUFFERS_NORMAL, 
        pHeader->uiSize - sizeof(PROTOCOL_HEADER), pHeader->ucFirstByte
        );
    if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

    // First build reply medium header
    status = m_pMedium->BuildReplyMediaHeader(
        pPacket, pMDLChain, pHeader->uiSize, pRecvPacket
        );
    pPacket->PopulateNetBuffer(pMDLChain, pHeader->uiSize + m_pMedium->m_uiHeaderSize);
    if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

cleanUp:   
    pMDLChain->Release();
    return status;
}   

//------------------------------------------------------------------------------
#include "RequestStatusStart.h"

void CBinding::Status(
                      PNDIS_STATUS_INDICATION pStatusIndication
                      )
{
    BOOL bCompleted = FALSE;

    // Nobody else should play with a list
    m_listRequest.AcquireSpinLock();

    // Walk list for request
    CRequest* pRequest = (CRequest*)m_listRequest.GetHead();
    while (pRequest != NULL) {
        CRequest* pTempRequest = pRequest;
        pRequest = (CRequest*)m_listRequest.GetNext(pRequest);

        if ((pTempRequest->m_eType == NDT_REQUEST_STATUS_START) && (((CRequestStatusStart *)pTempRequest)->m_ulEvent == pStatusIndication->StatusCode))
        {
            ((CRequestStatusStart *)pTempRequest)->m_cbStatusBufferSize = (UINT) pStatusIndication->StatusBufferSize;
            ((CRequestStatusStart *)pTempRequest)->m_pStatusBuffer = pStatusIndication->StatusBuffer;

            // Complete request
            pTempRequest->Complete();
            // Remove request from queue
            RemoveRequestFromList(pTempRequest);

            bCompleted = TRUE;
            break;
        }        
    }

    // We are done
    m_listRequest.ReleaseSpinLock();

    //These counters used to be updated on ProtocolResetComplete. That handler no longer 
    //exists for NDIS 6, so these counters must be updated here.
    if (bCompleted == FALSE && pStatusIndication->StatusCode == NDIS_STATUS_RESET_END) {
        Log(NDT_ERR_UNEXP_RESET_COMPLETE);
        NdisInterlockedIncrement((LONG*)&m_ulUnexpectedEvents);
        NdisInterlockedIncrement((LONG*)&m_ulUnexpectedResetComplete);
    }

    // Now update the status counters
    NdisInterlockedIncrement((PLONG)&m_ulTotalStatusIndicate);
    switch (pStatusIndication->StatusCode)
    {
    case NDIS_STATUS_RESET_START :
        Log(NDT_INF_STATUS, _T("NDIS_STATUS_RESET_START"),NDIS_STATUS_RESET_START);
        NdisInterlockedIncrement((PLONG)&m_ulStatusResetStart);
        break;

    case NDIS_STATUS_RESET_END :
        Log(NDT_INF_STATUS, _T("NDIS_STATUS_RESET_END"),NDIS_STATUS_RESET_END);
        NdisInterlockedIncrement((PLONG)&m_ulStatusResetEnd);
        ResetComplete();
        break;

    case NDIS_STATUS_OPER_STATUS:
        break;
    case NDIS_STATUS_LINK_STATE:
        if ( MediaConnectStateConnected == ((PNDIS_LINK_STATE)pStatusIndication->StatusBuffer)->MediaConnectState )
        {
            Log(NDT_INF_STATUS, _T("NDIS_STATUS_MEDIA_CONNECT"),NDIS_STATUS_MEDIA_CONNECT);
            NdisInterlockedIncrement((PLONG)&m_ulStatusMediaConnect);
        }
        else
        {
            Log(NDT_INF_STATUS, _T("NDIS_STATUS_MEDIA_DISCONNECT"),NDIS_STATUS_MEDIA_DISCONNECT);
            NdisInterlockedIncrement((PLONG)&m_ulStatusMediaDisconnect);
        }
        break;

    default :
        break;
    }
}

//------------------------------------------------------------------------------

void CBinding::BuildNBLs(PNET_BUFFER_LIST *ppNBLList, UINT uiNumDestinations, HANDLE hSource)
{
    PNET_BUFFER_LIST pCurrent=NULL;
    UINT ix;

    if (uiNumDestinations > 0)
    {
        pCurrent = *ppNBLList = NdisAllocateNetBufferList(m_hNBLPool, 0, 0);
        pCurrent->SourceHandle = hSource;
        NET_BUFFER_LIST_NEXT_NBL(pCurrent) = NULL;
    }

    for (ix = 1; ix < uiNumDestinations; ix++)
    {
        NET_BUFFER_LIST_NEXT_NBL(pCurrent) = NdisAllocateNetBufferList(m_hNBLPool, 0, 0);
        pCurrent = NET_BUFFER_LIST_NEXT_NBL(pCurrent);
        pCurrent->SourceHandle = hSource;
        NET_BUFFER_LIST_NEXT_NBL(pCurrent)  = NULL;
    }
}

//------------------------------------------------------------------------------

void CBinding::FreeNBLs(PNET_BUFFER_LIST nblList)
{
    PNET_BUFFER_LIST pNewHead;

    while (nblList)
    {
        pNewHead = NET_BUFFER_LIST_NEXT_NBL(nblList);
        NdisFreeNetBufferList(nblList);
        nblList = pNewHead;
    }
}

//------------------------------------------------------------------------------

PNET_BUFFER_LIST CBinding::BuildNBL(HANDLE hSource)
{
    PNET_BUFFER_LIST pNewNBL;
    pNewNBL = NdisAllocateNetBufferList(m_hNBLPool, 0, 0);
    pNewNBL->SourceHandle = hSource;
    NET_BUFFER_LIST_NEXT_NBL(pNewNBL) = NULL;
    return pNewNBL;
}
//------------------------------------------------------------------------------

void CBinding::ReceiveNetBufferLists(PNET_BUFFER_LIST pNBLs, ULONG ulNumNBLs,
                                     ULONG ulRecvFlags)
{
    NDIS_STATUS status = NDIS_STATUS_NOT_RECOGNIZED;
    CPacket *pPacket = NULL;
    PNET_BUFFER_LIST pCurrentNBL = pNBLs;
    PNET_BUFFER pNB = NULL;
    CMDLChain *pMDLChain = NULL;
    PMDL pMDL = NULL;
    UINT ix = 0;
    ULONG uiSize = 0;
    ULONG uiCopied = 0;
    UINT uiPacketSize = 0;

    //we always copy into our own net buffers and return the passed in net buffers
    //so we dont need to check for the NDIS_RECEIVE_FLAGS_RESOURCES flag.
    //TODO: Can we assume that the MDL offset within NB will be 0?

    if (m_bPoolsAllocated == FALSE)
    {
        //we arent ready for packets yet
        if ((ulRecvFlags & NDIS_RECEIVE_FLAGS_RESOURCES) == 0)
            NdisReturnNetBufferLists(m_hAdapter, pNBLs, 0);

        return;
    }

    for (ix = 0; ix < ulNumNBLs; ix++, pCurrentNBL = NET_BUFFER_LIST_NEXT_NBL(pCurrentNBL))
    {
        pNB = NET_BUFFER_LIST_FIRST_NB(pCurrentNBL);
        while (pNB)
        {
            // No need to get packet length anymore, NB has NET_BUFFER_DATA_LENGTH macro
            uiSize = NET_BUFFER_DATA_LENGTH(pNB);
            // Check packet size: data length will give the actual length starting from data offset
            if (uiSize > m_pMedium->m_uiMaxTotalSize) {
                Log(
                    NDT_ERR_RECV_PACKET_TOO_LARGE, uiSize, m_pMedium->m_uiMaxFrameSize
                    );
                NdisInterlockedIncrement((LONG*)&m_ulUnexpectedEvents);
                ASSERT(0);
                goto cleanUp;
            }

            // If packet is shorter it isn't for us
            if (uiSize < m_pMedium->m_uiHeaderSize + sizeof(PROTOCOL_HEADER)) {
                goto cleanUp;
            }

            // There we should get a packet
            pPacket = GetPacketFromFreeRecv(TRUE);
            if (pPacket == NULL) {
                Log(NDT_ERR_RECV_PACKET_OUTOFDESCRIPTORS2);
                goto cleanUp;
            }

            //reconstruct the MDL chain
            pMDLChain = new CMDLChain(m_hNBPool);

            // Chain medium header
            pMDL = pMDLChain->CreateMDL(pPacket->m_pucMediumHeader, m_pMedium->m_uiHeaderSize);
            pMDLChain->AddMDLAtFront(pMDL);

            // Chain protocol header
            pMDL = pMDLChain->CreateMDL(pPacket->m_pProtocolHeader, sizeof(PROTOCOL_HEADER));
            pMDLChain->AddMDLAtBack(pMDL);

            // Chain protocol body
            pMDL = pMDLChain->CreateMDL(pPacket->m_pucBody, uiSize - sizeof(PROTOCOL_HEADER) - m_pMedium->m_uiHeaderSize);
            pMDLChain->AddMDLAtBack(pMDL);

            pPacket->PopulateNetBuffer(pMDLChain, uiSize);

            NdisCopyFromNetBufferToNetBuffer(
                pPacket->m_pNdisNB, 0, uiSize, pNB, 0, &uiCopied
                );

            // We have to copy all packet, if not something bad happen
            if (uiSize != uiCopied) {
                Log(NDT_ERR_RECV_PACKET_COPY, uiSize, uiCopied);
                AddPacketToFreeRecv(pPacket);
                goto cleanUp;      
            }

            pPacket->m_uiSize = uiCopied;
            uiPacketSize = uiSize;
            /*TODO: Figure out what this code segment is for, since CheckReceive only returns header length
            // Fix size of packet if necessary
            uiPacketSize = m_pMedium->CheckReceive(
            pPacket->m_pucMediumHeader, m_pMedium->m_uiHeaderSize
            );
            if (uiPacketSize == 0) {
            AddPacketToFreeRecv(pPacket);
            goto cleanUp;
            }

            // Check if packet contain all packet
            if (uiPacketSize != uiSize) {
            Log(NDT_ERR_RECV_PACKET_SIZE, uiSize, uiPacketSize);
            AddPacketToFreeRecv(pPacket);
            goto cleanUp;      
            }
            */

            // Check protocol header to deside if we are interested
            status = CheckProtocolHeader(
                pPacket->m_pProtocolHeader, sizeof(PROTOCOL_HEADER), 
                uiPacketSize - sizeof(PROTOCOL_HEADER)
                );
            if (status != NDIS_STATUS_SUCCESS) {
                AddPacketToFreeRecv(pPacket);
                goto cleanUp;
            }

            // Change state & update time
            pPacket->m_bTransferred = TRUE;
            NdisGetSystemUpTimeEx(&pPacket->m_ulStateChange);

            // Add packet to queue
            AddPacketToReceived(pPacket);

cleanUp: 
            if (pMDLChain != NULL)
            {
                pMDLChain->Release();
                pMDLChain = NULL;
            }

            pNB = NET_BUFFER_NEXT_NB(pNB);
        }//end while

    } //end for

    if ((ulRecvFlags & NDIS_RECEIVE_FLAGS_RESOURCES) == 0)
        NdisReturnNetBufferLists(m_hAdapter, pNBLs, 0);

    //Now queue the processing of received packets
    QueueReceiveComplete();

}

//------------------------------------------------------------------------------

void CBinding::ResetComplete()
{
    // Find the request in pending queue and remove it
    CRequestReset* pRequest = FindResetRequest(TRUE);

    // If we get RequestComplete and there is no request pending
    if (pRequest == NULL) {
        Log(NDT_ERR_UNEXP_REQUEST_COMPLETE);
        NdisInterlockedIncrement((LONG*)&m_ulUnexpectedEvents);
        NdisInterlockedIncrement((LONG*)&m_ulUnexpectedRequestComplete);
        return;
    }

    // Release it before setting the event so the object can be destroyed before another reset comes in
    pRequest->Release();

    pRequest->SetRequestCompletionEvent();
}

//------------------------------------------------------------------------------

void CBinding::QueueReceiveComplete()
{
    NdisAcquireSpinLock(&m_spinLockRecvComplete);
    if (m_bRecvCompleteQueued)
    {
        NdisReleaseSpinLock(&m_spinLockRecvComplete);
        return;
    }

    m_bRecvCompleteQueued = TRUE;
    NdisSetTimer(&m_timerRecvComplete, m_uiRecvCompleteDelay);
    NdisReleaseSpinLock(&m_spinLockRecvComplete);
}

//------------------------------------------------------------------------------
