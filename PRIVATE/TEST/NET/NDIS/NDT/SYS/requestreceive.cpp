//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "StdAfx.h"
#include "NDTLib.h"
#include "ProtocolHeader.h"
#include "Protocol.h"
#include "Binding.h"
#include "Medium.h"
#include "Packet.h"
#include "RequestReceive.h"
#include "Marshal.h"
#include "Log.h"

//------------------------------------------------------------------------------

CRequestReceive::CRequestReceive(CBinding* pBinding) :  
   CRequest(NDT_REQUEST_RECEIVE, pBinding)
{
   m_dwMagic = NDT_MAGIC_REQUEST_RECEIVE;
   m_ulPacketsReceived = 0;
   m_ulPacketsReplied = 0;
   m_ulPacketsCompleted = 0;
   m_ulStartTime = 0;
   m_ulLastTime = 0;
   m_cbSent = 0;
   m_cbReceived = 0;
};

//------------------------------------------------------------------------------

NDIS_STATUS CRequestReceive::Execute()
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   
   if (!m_pBinding->m_bPoolsAllocated) {
      status = m_pBinding->AllocatePools();
      if (status != NDIS_STATUS_SUCCESS) goto cleanUp;
   }
   
   // Add request to pending queue
   m_pBinding->AddRequestToList(this);

   // Set initial values
   m_ulPacketsReceived = 0;
   m_ulPacketsReplied = 0;
   m_ulPacketsCompleted = 0;
   m_ulStartTime = 0;
   m_ulLastTime = 0;
   m_cbSent = 0;
   m_cbReceived = 0;

   // We have return pending state
   status = NDIS_STATUS_PENDING;

cleanUp:
   return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestReceive::MarshalOutParams(
   PVOID* ppvBuffer, DWORD* pcbBuffer
)
{
   return MarshalParameters(
      ppvBuffer, pcbBuffer, NDT_MARSHAL_OUT_RECEIVE, m_status, 
      m_ulPacketsReceived, m_ulPacketsReplied, m_ulPacketsCompleted, 
      m_ulLastTime - m_ulStartTime, m_cbSent, m_cbReceived
   );
}

//------------------------------------------------------------------------------

void CRequestReceive::Stop()
{
   // Set return code
   m_status = NDIS_STATUS_SUCCESS;
   // And remove it from the list
   m_pBinding->RemoveRequestFromList(this);
   // We are done with a request, so complete it
   Complete();
}

//------------------------------------------------------------------------------

void CRequestReceive::SendComplete(CPacket* pPacket, NDIS_STATUS status)
{
   ULONG ulIndex = 0;

   // Update last time when we was called
   NdisGetSystemUpTime(&m_ulLastTime);

   // Remove packet from wait to send complete list
   m_pBinding->RemovePacketFromSent(pPacket);

   // First check a status
   if (status == NDIS_STATUS_SUCCESS) {
      // Update counter
      m_ulPacketsCompleted++;
      // Update data amount sent
      m_cbSent += pPacket->m_uiSize;
      // This can be last time also
      NdisGetSystemUpTime(&m_ulLastTime);
   }

   // Remove association with this request
   pPacket->m_pRequest->Release();
   pPacket->m_pRequest = NULL;

   // Release a packet and return it back between free
   m_pBinding->AddPacketToFreeSend(pPacket);
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestReceive::Receive(CPacket* pPacket)
{
   PROTOCOL_HEADER* pHeader = pPacket->m_pProtocolHeader;
   NDIS_STATUS status = NDIS_STATUS_NOT_RECOGNIZED;
   CPacket* pResponsePacket = NULL;

   // Avoid to process packet echo
   if (pHeader->ucResponseMode & NDT_RESPONSE_FLAG_RESPONSE) goto cleanUp;

   // If this is first time save system time
   if (m_ulStartTime == 0) NdisGetSystemUpTime(&m_ulStartTime);

   // Update number of packets received
   m_ulPacketsReceived++;
   
   // Update counter
   m_cbReceived += pPacket->m_uiSize;
   
   // Send response if we have to do so
   if (pHeader->ucResponseMode != NDT_RESPONSE_NONE) {

      // Get packet for response
      pResponsePacket = m_pBinding->GetPacketFromFreeSend(TRUE);

      // If we don't have a packet let skip response
      if (pResponsePacket != NULL) {
      
         // Modify packet to reply
         m_pBinding->BuildPacketForResponse(pResponsePacket, pPacket);

         // Associate packet with this request
         pResponsePacket->m_pRequest = this; 
         AddRef();

         // Set packet state
         pResponsePacket->m_bSendCompleted = FALSE;
         pResponsePacket->m_bReplyReceived = FALSE;
         NdisGetSystemUpTime(&pPacket->m_ulStateChange);

         // And move it to a list of packet in sending
         m_pBinding->AddPacketToSent(pResponsePacket);

         // Update counter
         m_ulPacketsReplied++;
      
         // Send it back
         NdisSend(
            &status, m_pBinding->m_hAdapter, pResponsePacket->m_pNdisPacket
         );
         if (m_pBinding->m_pProtocol->m_bLogPackets) {
            PROTOCOL_HEADER* pRespHeader = pResponsePacket->m_pProtocolHeader;
            LogX(
               _T("%08lu Bind %d Send #%04lu/%02u/%02x Status 0x%08x\n"), 
               GetTickCount(), m_pBinding->m_lInstanceId,
               pRespHeader->ulSequenceNumber, pRespHeader->usReplyId, 
               pRespHeader->ucResponseMode, status
            );      
         }
         if (status != NDIS_STATUS_PENDING) {
            SendComplete(pResponsePacket, status);
         }

      }    

   }

   // This can be last time also
   NdisGetSystemUpTime(&m_ulLastTime);

   // We can release received packet
   m_pBinding->AddPacketToFreeRecv(pPacket);

   status = NDIS_STATUS_SUCCESS;

cleanUp:
   return status;
}

//------------------------------------------------------------------------------
