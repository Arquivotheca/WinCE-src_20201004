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
#include "ProtocolHeader.h"
#include "Protocol.h"
#include "Binding.h"
#include "Medium.h"
#include "Packet.h"
#include "RequestSend.h"
#include "Marshal.h"
#include "Log.h"

//------------------------------------------------------------------------------

VOID CRequestSend::SendBeat(
                            IN PVOID SystemSpecific1, IN PVOID FunctionContext,
                            IN PVOID SystemSpecific2, IN PVOID SystemSpecific3
                            )
{
    CRequestSend* pRequest = (CRequestSend*)FunctionContext;
    ASSERT(pRequest->m_dwMagic == NDT_MAGIC_REQUEST_SEND);
    pRequest->Beat();
}

//------------------------------------------------------------------------------

CRequestSend::CRequestSend(CBinding* pBinding) :  
CRequest(NDT_REQUEST_SEND, pBinding)
{
    m_dwMagic = NDT_MAGIC_REQUEST_SEND;

    m_cbSrcAddr = 0;
    m_pucSrcAddr = NULL;
    m_uiDestAddrs = 0;
    m_cbDestAddr = 0;
    m_pucDestAddrs = NULL;
    m_ucResponseMode = NDT_RESPONSE_NONE;
    m_ucPacketSizeMode = NDT_PACKET_TYPE_FIXED;
    m_ulPacketSize = 0;
    m_ulPacketCount = 0;
    m_uiBeatDelay = 0;
    m_uiBeatGroup = 0;

    m_ulTimeout = 16000;

    m_ulPacketsSent = 0;
    m_ulPacketsCompleted = 0;
    m_ulPacketsCanceled = 0;
    m_ulPacketsUncanceled = 0;
    m_ulPacketsReplied = 0; 
    m_ulStartTime.QuadPart = 0;
    m_ulLastTime.QuadPart = 0;
    m_cbSent = 0;
    m_cbReceived = 0;

    m_uiStartBeatDelay = 100;
    m_ulPacketsToSend = 0;
    m_ulCancelId = 0;

    m_apNdisNBs = NULL;
    m_apPackets = NULL;

    AddRef();
    NdisInitializeTimer(&m_timerBeat, CRequestSend::SendBeat, this);
    NdisAllocateSpinLock(&m_spinLock);
};

//------------------------------------------------------------------------------

CRequestSend::~CRequestSend()
{
    delete m_apNdisNBs;
    delete m_apPackets;
    delete m_pucSrcAddr;
    delete m_pucDestAddrs;
    NdisFreeSpinLock(&m_spinLock);
      m_pBinding->m_pProtocol->m_pPauseRestartLock->ReadComplete();
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestSend::Execute()
{
    NDIS_STATUS status = NDIS_STATUS_SUCCESS;
    CMedium* pMedium = m_pBinding->m_pMedium;
    m_pBinding->m_pProtocol->m_pPauseRestartLock->Read();

    // There should be at least one destination address
    if (m_uiDestAddrs == 0) {
        status = NDIS_STATUS_FAILURE;
        goto cleanUp;
    }

    if (!m_pBinding->m_bPoolsAllocated) {
        status = m_pBinding->AllocatePools();
        if (status != NDIS_STATUS_SUCCESS) goto cleanUp;
    }

    // Add request to pending queue
    m_pBinding->AddRequestToList(this);

    // Check & fix a packet size
    if (m_ulPacketSize > pMedium->m_uiMaxFrameSize) {
        m_ulPacketSize = pMedium->m_uiMaxFrameSize;
    }
    if (m_ulPacketSize < sizeof(PROTOCOL_HEADER)) {
        m_ulPacketSize = sizeof(PROTOCOL_HEADER);
    }
    if ((m_ucPacketSizeMode & NDT_PACKET_FLAG_MASK) == NDT_PACKET_TYPE_CYCLICAL) {
        m_ulPacketSize = pMedium->m_uiMaxFrameSize;
    }      

    // Calculate parameters when not defined
    if (m_uiBeatDelay == 0) {
        // Calculate a recomended delay between beats based on speed
        m_uiBeatDelay = (20000 / pMedium->m_uiLinkSpeed) * 10 + 40;
    }
    // If number of packet to be send in one beat is zero it should be one
    if (m_uiBeatGroup == 0) m_uiBeatGroup = 1;

    // We need this array for packet descriptors
    m_apNdisNBs = new PNET_BUFFER[m_uiDestAddrs * m_uiBeatGroup];
    m_apPackets = new PCPacket[m_uiDestAddrs * m_uiBeatGroup];

    // We need send this number of packets
    m_ulPacketsToSend = m_uiDestAddrs * m_ulPacketCount;

    // Get cancel id 
    if (m_ucPacketSizeMode & NDT_PACKET_FLAG_CANCEL) {
        m_ulCancelId = (ULONG)NdisGeneratePartialCancelId() << 24;
    } else {
        m_ulCancelId = 0;
    }


    // Initialize counters for beats procedure
    m_ulPacketsSent = 0;
    m_ulPacketsCompleted = 0;
    m_ulPacketsCanceled = 0;
    m_ulPacketsUncanceled = 0;
    m_ulPacketsReplied = 0; 
    m_ulStartTime.QuadPart = 0;
    m_ulLastTime.QuadPart = 0;
    m_cbSent = 0;
    m_cbReceived = 0;

    // Plan a first beat pulse
    NdisSetTimer(&m_timerBeat, m_uiStartBeatDelay);

    // We have return pending state
    status = NDIS_STATUS_PENDING;

cleanUp:
    return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestSend::UnmarshalInpParams(
    PVOID* ppvBuffer, DWORD* pcbBuffer
    )
{
    NDIS_STATUS status = NDIS_STATUS_SUCCESS;

    status = UnmarshalParameters(
        ppvBuffer, pcbBuffer, NDT_MARSHAL_INP_SEND, &m_pBinding, &m_cbSrcAddr,
        &m_pucSrcAddr, &m_uiDestAddrs, &m_cbDestAddr, &m_pucDestAddrs, 
        &m_ucResponseMode, &m_ucPacketSizeMode, &m_ulPacketSize, 
        &m_ulPacketCount, &m_ulConversationId, &m_uiBeatDelay, &m_uiBeatGroup
        );
    if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

       m_pBinding->m_ulConversationId = m_ulConversationId;

    // We save an pointer to object
    m_pBinding->AddRef();

cleanUp:
    return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestSend::MarshalOutParams(
    PVOID* ppvBuffer, DWORD* pcbBuffer
    )
{
    //BUGBUG: we're in trouble if the difference is greater than max posssible ULONG
    //not changing it to ULONGLONG now because test library assumes its getting back a ULONG
    ULONGLONG ulTime = 0;
    ulTime = m_ulLastTime.QuadPart - m_ulStartTime.QuadPart;

    return MarshalParameters(
        ppvBuffer, pcbBuffer, NDT_MARSHAL_OUT_SEND, m_status, m_ulPacketsSent,
        m_ulPacketsCompleted, m_ulPacketsCanceled, m_ulPacketsUncanceled,
        m_ulPacketsReplied, ulTime, m_cbSent, m_cbReceived
        );
}

//------------------------------------------------------------------------------

void CRequestSend::Beat()
{
    NDIS_STATUS status = NDIS_STATUS_SUCCESS;
    CMedium* pMedium = m_pBinding->m_pMedium;
    ULONG ulPackets = m_uiDestAddrs * m_uiBeatGroup;
    CPacket* pPacket;
    CPacket* pPacketForRelease;
    ULONG ulSequenceId;
    USHORT usReplyId;
    ULONG ix, iy, ulSize, ulSize2;
    LARGE_INTEGER ulTime;
    BOOL bCancel = FALSE;
    NET_BUFFER_LIST * pNBL;

    // If we are called first time remember when and get start counter values
    if (m_ulStartTime.QuadPart == 0) NdisGetSystemUpTimeEx(&m_ulStartTime);

    // Some packets should be sent
    if (m_ulPacketsToSend > 0) {

        // If only a few packets remain
        if (ulPackets > m_ulPacketsToSend) ulPackets = m_ulPacketsToSend;

        // We don't have enought packets so don't send
        if (m_pBinding->GetPacketsFromFreeSend(m_apPackets, ulPackets)) {

            // Create packets to send
            for (ix = 0; ix < ulPackets; ix++) {

                // Make sure that we getting "sense" frame size
                ASSERT(pMedium->m_uiMaxFrameSize != 0);
                ASSERT(pMedium->m_uiMaxFrameSize < 0x4000);

                ulSequenceId = (m_ulPacketsSent + ix) / m_uiDestAddrs;
                usReplyId = (USHORT)((m_ulPacketsSent + ix) % m_uiDestAddrs);

                // Deside if this and later packets will be canceled
                if (m_ucPacketSizeMode & NDT_PACKET_FLAG_CANCEL) {
                    if (ulSequenceId >= 100) bCancel = TRUE;
                }

                // Get packet size
                switch (m_ucPacketSizeMode & NDT_PACKET_TYPE_MASK) {
            case NDT_PACKET_TYPE_CYCLICAL:
                m_ulPacketSize++;
                if (m_ulPacketSize > pMedium->m_uiMaxFrameSize) {
                    m_ulPacketSize = sizeof(PROTOCOL_HEADER);
                }
                ulSize = m_ulPacketSize;
                break;
            case NDT_PACKET_TYPE_RANDOM:
                ulSize = NDT_NdisGetRandom(
                    sizeof(PROTOCOL_HEADER), pMedium->m_uiMaxFrameSize
                    );
                break;
            case NDT_PACKET_TYPE_RAND_SMALL:
                ulSize = NDT_NdisGetRandom(
                    sizeof(PROTOCOL_HEADER), pMedium->m_uiMaxFrameSize
                    );
                ulSize2 = NDT_NdisGetRandom(
                    sizeof(PROTOCOL_HEADER), pMedium->m_uiMaxFrameSize
                    );
                if (ulSize2 < ulSize) ulSize = ulSize2;
                break;
            case NDT_PACKET_TYPE_FIXED:
            default:
                ulSize = m_ulPacketSize;
                break;
                }

                // And fill it with data
                status = m_pBinding->BuildPacketToSend(
                    m_apPackets[ix], m_pucSrcAddr, 
                    m_pucDestAddrs + usReplyId * m_cbDestAddr, m_ucResponseMode, 
                    m_ucPacketSizeMode, ulSize, ulSequenceId, m_ulConversationId, usReplyId
                    );
                ASSERT(status == NDIS_STATUS_SUCCESS);

                m_apNdisNBs[ix] = m_apPackets[ix]->m_pNdisNB;

                ASSERT(m_apPackets[ix]->m_pRequest == NULL);

                // Associate packet with this request
                m_apPackets[ix]->m_pRequest = this;
                AddRef();

                // Set packet state
                m_apPackets[ix]->m_bSendCompleted = FALSE;
                m_apPackets[ix]->m_bReplyReceived = FALSE;
                NdisGetSystemUpTimeEx(&m_apPackets[ix]->m_ulStateChange);

                // Move it to sent queue
                m_pBinding->AddPacketToSent(m_apPackets[ix]);
            }

            // Flag define which function be used
            if (m_ucPacketSizeMode & NDT_PACKET_FLAG_GROUP) {

                // Update counters & send packets
                m_ulPacketsSent += ulPackets;
                ASSERT(m_ulPacketsToSend >= ulPackets);
                m_ulPacketsToSend -= ulPackets;

                //init the NBL list for all the destinations
                m_pBinding->BuildNBLs(&m_pNdisNBLs, m_uiDestAddrs, m_pBinding->m_hAdapter);

                // Set cancel id for later usage
                if (bCancel) 
                {
                    //walk the NBL list and set the cancel ids
                    pNBL = m_pNdisNBLs;
                    for(ix = 0; ix < m_uiDestAddrs; ix++)
                    {
                        NDIS_SET_NET_BUFFER_LIST_CANCEL_ID(
                            pNBL, &m_ulCancelId
                            );
                        pNBL = NET_BUFFER_LIST_NEXT_NBL(pNBL);
                    }
                }

                    pNBL = m_pNdisNBLs;

                    //Now add the NBs in the NB array to the respective NBLs
                    for (ix = 0; ix < m_uiDestAddrs; ix++)
                    {
                        PNET_BUFFER pLastNB = NULL;

                        for (iy = ix; iy < ulPackets; iy += m_uiDestAddrs)
                        {
                            if (pLastNB == NULL)
                            {
                                //first time,
                                NET_BUFFER_LIST_FIRST_NB(pNBL) = m_apNdisNBs[iy];
                                pLastNB = NET_BUFFER_LIST_FIRST_NB(pNBL);
                            }
                            else
                            {
                                NET_BUFFER_NEXT_NB(pLastNB) = m_apNdisNBs[iy];
                                pLastNB = NET_BUFFER_NEXT_NB(pLastNB);
                            }
                        }

                        //NULL terminate the NB list
                        NET_BUFFER_NEXT_NB(pLastNB) = NULL;
                        
                        pNBL = NET_BUFFER_LIST_NEXT_NBL(pNBL);
                    }

                    //TODO: Setting NDIS_SEND_FLAGS_CHECK_FOR_LOOPBACK to 0.
                    NdisSendNetBufferLists(
                        m_pBinding->m_hAdapter, this->m_pNdisNBLs, 0, NDIS_SEND_FLAGS_CHECK_FOR_LOOPBACK 
                        );

                    // Cancel packets if we have do so
                    if (bCancel) NdisCancelSendNetBufferLists(
                        m_pBinding->m_hAdapter, &m_ulCancelId
                        );

                    if (m_pBinding->m_pProtocol->m_bLogPackets) {
                        for (ix = 0; ix < ulPackets; ix++) {
                            PROTOCOL_HEADER* pHeader = m_apPackets[ix]->m_pProtocolHeader;
                            LogX(
                                _T("%08lu Bind %d SendPackets #%04lu/%02u/%02x\n"), 
                                GetTickCount(), m_pBinding->m_lInstanceId,
                                pHeader->ulSequenceNumber, pHeader->usReplyId,
                                pHeader->ucResponseMode
                                );      
                        }
                    }

                    //TODO: We're assuming that these NBLs and NB will be returned in the SendComplete path
                    //where we will free them, so we dont need these references.
                    for (ix = 0; ix < ulPackets; ix++) {
                        m_apPackets[ix] = NULL;
                    }

                    for (ix = 0; ix < m_uiDestAddrs; ix++){
                        m_apNdisNBs[ix] = NULL;
                    }

                    m_pNdisNBLs = NULL;

                } else {

                    for (ix = 0; ix < ulPackets; ix++) {
                        m_ulPacketsSent++;
                        ASSERT(m_ulPacketsToSend > 0);
                        m_ulPacketsToSend--;

                        //we should build NBLs and NBs in CBinding since it has the pool handles
                        pNBL = m_pBinding->BuildNBL(m_pBinding->m_hAdapter);
                        NET_BUFFER_LIST_FIRST_NB(pNBL) = m_apNdisNBs[ix];
                        NET_BUFFER_NEXT_NB(m_apNdisNBs[ix]) = NULL;

                        NdisSendNetBufferLists(m_pBinding->m_hAdapter, pNBL, 0, NDIS_SEND_FLAGS_CHECK_FOR_LOOPBACK);

                        if (m_pBinding->m_pProtocol->m_bLogPackets) {
                            PROTOCOL_HEADER* pHeader = m_apPackets[ix]->m_pProtocolHeader;
                            LogX(
                                _T("%08lu Bind %d Send #%04lu/%02u/%02x Status 0x%08x\n"),
                                GetTickCount(), m_pBinding->m_lInstanceId,
                                pHeader->ulSequenceNumber, pHeader->usReplyId,
                                pHeader->ucResponseMode, status
                                );      
                        }

                        //In NDIS 6.0 send is always asynchronous. So we will always get only pending status.

                        m_apPackets[ix] = NULL;
                        m_apNdisNBs[ix] = NULL;
                        pNBL = NULL;
                    }

                }

            }
        }

        // Find timeout packets
        NdisGetSystemUpTimeEx(&ulTime);
        m_pBinding->m_listSentPackets.AcquireSpinLock();
        pPacket = (CPacket*)m_pBinding->m_listSentPackets.GetHead();
        while (pPacket != NULL) {
            // We can timout only packets waiting for reply. There isn't good way
            // how to manage situation when miniport holds packet
            if (
                pPacket->m_bSendCompleted &&
                ((ULONGLONG) (ulTime.QuadPart - pPacket->m_ulStateChange.QuadPart)) > m_ulTimeout
                ) {
                    // We need first move to next packet
                    pPacketForRelease = pPacket;
                    pPacket = (CPacket*)m_pBinding->m_listSentPackets.GetNext(pPacket);
                    // Remove packet from queue
                    m_pBinding->m_listSentPackets.Remove(pPacketForRelease);
                    // Remove association with this request
                    pPacketForRelease->m_pRequest->Release();
                    pPacketForRelease->m_pRequest = NULL;
                    // Return it back between free
                    m_pBinding->AddPacketToFreeSend(pPacketForRelease);
            } else {
                pPacket = (CPacket*)m_pBinding->m_listSentPackets.GetNext(pPacket);
            }
        }
        m_pBinding->m_listSentPackets.ReleaseSpinLock();

        // Reschedule beat until request is done
        if (m_ulPacketsToSend > 0 || !m_pBinding->IsEmptyPacketSent()) {
            NdisSetTimer(&m_timerBeat, m_uiBeatDelay);
        } else {
            // Set return code
            m_status = NDIS_STATUS_SUCCESS;
            // Complete request
            Complete();
            // Remove request from queue
            m_pBinding->RemoveRequestFromList(this);
            // Release
            Release();
        }
    }


//------------------------------------------------------------------------------

void CRequestSend::StopBeat()
{
    BOOLEAN bCanceled = FALSE;
    UINT uiDelay = m_uiBeatDelay;

    while (uiDelay > 0) {
        NdisCancelTimer(&m_timerBeat, &bCanceled);
        if (bCanceled) break;
        NdisMSleep(1);
        uiDelay -= 1;
    }
    Release();
}

//------------------------------------------------------------------------------

void CRequestSend::SendComplete(CPacket* pPacket, NDIS_STATUS status, BOOL bCancel)
{
    //Artificially bump up the refcount to prevent the object from getting garbage collected
    AddRef();
    
    // Update last time when we was called
    NdisGetSystemUpTimeEx(&m_ulLastTime);

    // If we set a cancel id a packet may be canceled 
    if (bCancel) {
        switch (status) {
      case NDIS_STATUS_REQUEST_ABORTED:
          m_ulPacketsCanceled++;
          break;
      case NDIS_STATUS_SUCCESS:
          m_ulPacketsUncanceled++;
          break;
        }
    }

    // When send was succesfull
    if (status == NDIS_STATUS_SUCCESS) {
        // The packet was completed successfuly
        m_ulPacketsCompleted++;
        // Update counter
        m_cbSent += pPacket->m_uiSize;
    }

    // Update packet state, we need get send packet queue lock
    m_pBinding->m_listSentPackets.AcquireSpinLock();

    // Update flag and time stamp
    pPacket->m_bSendCompleted = TRUE;
    if (m_ucResponseMode == NDT_RESPONSE_NONE) pPacket->m_bReplyReceived = TRUE;

    //Note that at the time when Miniport indicates NO NDIS_STATUS_SUCCESS, 
    //we should move the packets back to free list
    if (status == NDIS_STATUS_PENDING)
    {
        //I guess this should not happen.
        LogX(_T("CRequestSend::SendComplete:Status = NDIS_STATUS_PENDING\n"));
        ASSERT(FALSE);
    }
    else
    {
        if (status != NDIS_STATUS_SUCCESS)
        {
            //LogX(_T("CRequestSend::SendComplete:Status = %x\n"),status);       
            pPacket->m_bReplyReceived = TRUE;
        }
    }

    NdisGetSystemUpTimeEx(&pPacket->m_ulStateChange);

    // If packet is done move it
    if (pPacket->m_bReplyReceived) {
        m_pBinding->m_listSentPackets.Remove(pPacket);
        // Remove association with this request
        pPacket->m_pRequest->Release();
        pPacket->m_pRequest = NULL;
        // Release a packet and return it back between free
        m_pBinding->AddPacketToFreeSend(pPacket);
    }
    // We are done
    m_pBinding->m_listSentPackets.ReleaseSpinLock();

    //Lower the artificially incremented ref count.
    Release();
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestSend::Receive(CPacket* pRecvPacket)
{
    NDIS_STATUS status = NDIS_STATUS_NOT_RECOGNIZED;
    PROTOCOL_HEADER* pHeader = pRecvPacket->m_pProtocolHeader;
    USHORT usReplyId = pHeader->usReplyId;
    ULONG ulSequence = pHeader->ulSequenceNumber;
    CPacket* pPacket;
    CPacket* pPacketForRelease;
    LONG lDistance;

    // Is this a copy of packet we send?
    if (!(pHeader->ucResponseMode & NDT_RESPONSE_FLAG_RESPONSE)) goto cleanUp;

    // Update counter
    m_cbReceived += pRecvPacket->m_uiSize;

    // Update packet state
    m_pBinding->m_listSentPackets.AcquireSpinLock();
    pPacket = (CPacket*)m_pBinding->m_listSentPackets.GetHead();
    while (pPacket != NULL) {
        lDistance = pPacket->m_pProtocolHeader->ulSequenceNumber - ulSequence;
        if (
            !pPacket->m_bReplyReceived && lDistance <= 0 &&
            usReplyId == pPacket->m_pProtocolHeader->usReplyId
            ) {
                // Packet was replied
                m_ulPacketsReplied++;
                // Update flag & time stamp
                pPacket->m_bReplyReceived = TRUE;
                NdisGetSystemUpTimeEx(&pPacket->m_ulStateChange);
                // If packet is done move it
                if (pPacket->m_bSendCompleted) {
                    // We need first move to next packet
                    pPacketForRelease = pPacket;
                    pPacket = (CPacket*)m_pBinding->m_listSentPackets.GetNext(pPacket);
                    // Remove packet from queue
                    m_pBinding->m_listSentPackets.Remove(pPacketForRelease);
                    // Remove association with this request
                    pPacketForRelease->m_pRequest->Release();
                    pPacketForRelease->m_pRequest = NULL;
                    // Return it back between free
                    m_pBinding->AddPacketToFreeSend(pPacketForRelease);
                } else {
                    pPacket = (CPacket*)m_pBinding->m_listSentPackets.GetNext(pPacket);
                }
        } else {
            pPacket = (CPacket*)m_pBinding->m_listSentPackets.GetNext(pPacket);
        }
        // Make it faster
        if (lDistance >= 0) break;
    }
    // We are done with queue walk
    m_pBinding->m_listSentPackets.ReleaseSpinLock();
    // And with packet
    m_pBinding->AddPacketToFreeRecv(pRecvPacket);
    // The packet was processed
    status = NDIS_STATUS_SUCCESS;

cleanUp:
    return status;
}

//------------------------------------------------------------------------------
