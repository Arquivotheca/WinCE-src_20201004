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
#include "Log.h"

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

   // Window with size 1 is no window...
   m_ulWindowSize = 1; 

   // Get instance identification
   m_lInstanceId = InterlockedIncrement(&s_lInstanceCounter);
}

//------------------------------------------------------------------------------

CBinding::~CBinding()
{
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

VOID CBinding::ProtocolOpenAdapterComplete(
   IN NDIS_HANDLE ProtocolBindingContext, IN NDIS_STATUS Status,
   IN NDIS_STATUS OpenErrorStatus
)
{
   Log(
      NDT_DBG_OPEN_ADAPTER_COMPLETE_ENTRY, ProtocolBindingContext, Status, 
      OpenErrorStatus
   );
   CBinding *pBinding = (CBinding *)ProtocolBindingContext;
   ASSERT(pBinding->m_dwMagic == NDT_MAGIC_BINDING);
   pBinding->OpenAdapterComplete(Status, OpenErrorStatus);
   Log(NDT_DBG_OPEN_ADAPTER_COMPLETE_EXIT);
}

//------------------------------------------------------------------------------

VOID CBinding::ProtocolCloseAdapterComplete(
   IN NDIS_HANDLE ProtocolBindingContext, IN NDIS_STATUS Status
)
{
   Log(
      NDT_DBG_CLOSE_ADAPTER_COMPLETE_ENTRY, ProtocolBindingContext, Status
   );
   CBinding *pBinding = (CBinding *)ProtocolBindingContext;
   ASSERT(pBinding->m_dwMagic == NDT_MAGIC_BINDING);
   pBinding->CloseAdapterComplete(Status);
   Log(NDT_DBG_CLOSE_ADAPTER_COMPLETE_EXIT);
}

//------------------------------------------------------------------------------

VOID CBinding::ProtocolResetComplete(
   IN NDIS_HANDLE ProtocolBindingContext, IN NDIS_STATUS Status
)
{
   Log(
      NDT_DBG_RESET_COMPLETE_ENTRY, ProtocolBindingContext, Status
   );
   CBinding *pBinding = (CBinding *)ProtocolBindingContext;
   ASSERT(pBinding->m_dwMagic == NDT_MAGIC_BINDING);
   pBinding->ResetComplete(Status);
   Log(NDT_DBG_RESET_COMPLETE_EXIT);
}

//------------------------------------------------------------------------------

VOID CBinding::ProtocolRequestComplete(
   IN NDIS_HANDLE ProtocolBindingContext, IN PNDIS_REQUEST NdisRequest,
   IN NDIS_STATUS Status
)
{
   Log(
      NDT_DBG_REQUEST_COMPLETE_ENTRY, ProtocolBindingContext, NdisRequest, 
      Status
   );
   CBinding *pBinding = (CBinding *)ProtocolBindingContext;
   ASSERT(pBinding->m_dwMagic == NDT_MAGIC_BINDING);
   pBinding->RequestComplete(NdisRequest, Status);
   Log(NDT_DBG_REQUEST_COMPLETE_EXIT);
}

//------------------------------------------------------------------------------

VOID CBinding::ProtocolStatus(
   IN NDIS_HANDLE ProtocolBindingContext, IN NDIS_STATUS GeneralStatus,
   IN PVOID StatusBuffer, IN UINT StatusBufferSize
)
{
   Log(
      NDT_DBG_STATUS_ENTRY, ProtocolBindingContext, GeneralStatus, StatusBuffer, 
      StatusBufferSize
   );
   CBinding *pBinding = (CBinding *)ProtocolBindingContext;
   ASSERT(pBinding->m_dwMagic == NDT_MAGIC_BINDING);
   //pBinding->Status(GeneralStatus, StatusBuffer, StatusBufferSize);
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

VOID CBinding::ProtocolSendComplete(
   IN NDIS_HANDLE ProtocolBindingContext, IN PNDIS_PACKET Packet,
   IN NDIS_STATUS Status
)
{
   Log(NDT_DBG_SEND_COMPLETE_ENTRY, ProtocolBindingContext, Packet, Status);
   CBinding *pBinding = (CBinding *)ProtocolBindingContext;
   ASSERT(pBinding->m_dwMagic == NDT_MAGIC_BINDING);
   pBinding->SendComplete(Packet, Status);
   Log(NDT_DBG_SEND_COMPLETE_EXIT);
}

//------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------

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

//------------------------------------------------------------------------------

VOID CBinding::ProtocolUnbindAdapter(
   OUT PNDIS_STATUS Status, IN NDIS_HANDLE  ProtocolBindingContext,
   IN NDIS_HANDLE UnbindContext
)
{
   Log(NDT_DBG_UNBIND_ADAPTER_ENTRY, ProtocolBindingContext, UnbindContext);
   CBinding *pBinding = (CBinding *)ProtocolBindingContext;
   ASSERT(pBinding->m_dwMagic == NDT_MAGIC_BINDING);
   *Status = pBinding->UnbindAdapter(UnbindContext);
   Log(NDT_DBG_UNBIND_ADAPTER_EXIT, *Status);
}

//------------------------------------------------------------------------------

VOID CBinding::ProtocolCoSendComplete(
   IN NDIS_STATUS Status, IN NDIS_HANDLE ProtocolVcContext,
   IN PNDIS_PACKET Packet
)
{
   Log(NDT_DBG_CO_SEND_COMPLETE_ENTRY, Status, ProtocolVcContext, Packet);
   Log(NDT_DBG_CO_SEND_COMPLETE_EXIT);
}

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

VOID CBinding::OpenAdapterComplete(
   NDIS_STATUS status, NDIS_STATUS statusOpenError
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
   pRequest->m_statusOpenError = statusOpenError;

   // Complete request
   pRequest->Complete();

   // We have relase it
   pRequest->Release();
}

//------------------------------------------------------------------------------

VOID CBinding::CloseAdapterComplete(NDIS_STATUS status)
{
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

   // Set result code
   pRequest->m_status = status;

   // Complete request
   pRequest->Complete();

   // We have relase it
   pRequest->Release();

   // NDIS didn't hold a pointer anymore as context
   Release();
}

//------------------------------------------------------------------------------

VOID CBinding::ResetComplete(NDIS_STATUS status)
{
   // Find the request in pending queue and remove it
   CRequestReset* pRequest = FindResetRequest(TRUE);

   // If we get ResetComplete and there is no request pending
   if (pRequest == NULL) {
      Log(NDT_ERR_UNEXP_RESET_COMPLETE);
      NdisInterlockedIncrement((LONG*)&m_ulUnexpectedEvents);
      NdisInterlockedIncrement((LONG*)&m_ulUnexpectedResetComplete);
      return;
   }

   // Check object signature
   ASSERT(pRequest->m_dwMagic == NDT_MAGIC_REQUEST_RESET);

   // Set result code
   pRequest->m_status = status;

   // Complete request
   pRequest->Complete();

   // We have relase it
   pRequest->Release();
}

//------------------------------------------------------------------------------

VOID CBinding::RequestComplete(NDIS_REQUEST* pNdisRequest, NDIS_STATUS status)
{
   // Find the request in pending queue and remove it
   CRequestRequest* pRequest = FindRequestRequest(TRUE);

   // If we get RequestComplete and there is no request pending
   if (pRequest == NULL) {
      Log(NDT_ERR_UNEXP_REQUEST_COMPLETE);
      NdisInterlockedIncrement((LONG*)&m_ulUnexpectedEvents);
      NdisInterlockedIncrement((LONG*)&m_ulUnexpectedRequestComplete);
      return;
   }

   // Check object signature
   ASSERT(pRequest->m_dwMagic == NDT_MAGIC_REQUEST_REQUEST);

   // Set result code
   pRequest->m_status = status;

   // Complete request
   pRequest->Complete();

   // We have relase it
   pRequest->Release();
}

//------------------------------------------------------------------------------

void CBinding::SendComplete(NDIS_PACKET* pNdisPacket, NDIS_STATUS status)
{
   CPacket* pPacket = NULL;
   CRequest* pRequest = NULL;
   
   // Get a packet completed
   pPacket = (CPacket*)pNdisPacket->ProtocolReserved;
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
         pPacket->m_pProtocolHeader->ucResponseMode, status
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
      goto cleanUp;
   }

   // This is very specific situation ????
   if (status == NDIS_STATUS_PENDING) goto cleanUp;

   // Call complete routine in a request
   switch (pRequest->m_eType) {
   case NDT_REQUEST_SEND:
      // Check object signature & call send complete
      ASSERT(pRequest->m_dwMagic == NDT_MAGIC_REQUEST_SEND);
      ((CRequestSend*)pRequest)->SendComplete(pPacket, status);
      break;
   case NDT_REQUEST_RECEIVE:
      // Check object signature & call send complete
      ASSERT(pRequest->m_dwMagic == NDT_MAGIC_REQUEST_RECEIVE);
      ((CRequestReceive*)pRequest)->SendComplete(pPacket, status);
      break;
   default:
      Log(NDT_ERR_UNEXP_SEND_COMPLETE_REQ_TYPE, pRequest->m_eType);
      NdisInterlockedIncrement((LONG*)&m_ulUnexpectedEvents);
      NdisInterlockedIncrement((LONG*)&m_ulUnexpectedSendComplete);
      // Remove it from list and return between free
      RemovePacketFromSent(pPacket);
      AddPacketToFreeSend(pPacket);
      goto cleanUp;
   }
   
cleanUp: ;   
}

//------------------------------------------------------------------------------

NDIS_STATUS CBinding::Receive(
   NDIS_HANDLE hMacReceiveContext, PVOID pvHeaderBuffer, 
   UINT uiHeaderBufferSize, PVOID pvLookAheadBuffer, 
   UINT uiLookaheadBufferSize, UINT uiPacketSize
)
{
   NDIS_STATUS status = NDIS_STATUS_NOT_RECOGNIZED;
   CPacket* pPacket = NULL;
   PVOID pvBuffer = NULL;
   UINT uiBytesTransferred = 0;

   // This packet we don't recognize for sure
   if (uiLookaheadBufferSize == 0 && uiPacketSize == 0) {
      Log(NDT_ERR_RECV_PACKET_ZERO_LENGTH);
      NdisInterlockedIncrement((LONG*)&m_ulUnexpectedEvents);
      goto cleanUp;
   }
   
   // Check packet size
   if (uiPacketSize > m_pMedium->m_uiMaxFrameSize) {
      Log(
         NDT_ERR_RECV_PACKET_TOO_LARGE, uiPacketSize, 
         m_pMedium->m_uiMaxFrameSize
      );
      NdisInterlockedIncrement((LONG*)&m_ulUnexpectedEvents);
      goto cleanUp;
   }

   if (uiPacketSize < uiLookaheadBufferSize) {
      Log(
         NDT_ERR_RECV_PACKET_LOOKHEAD_SIZE, uiPacketSize, uiLookaheadBufferSize
      );
      NdisInterlockedIncrement((LONG*)&m_ulUnexpectedEvents);
      goto cleanUp;
   }

   // Fix size of packet if necessary
   uiPacketSize = m_pMedium->CheckReceive(pvHeaderBuffer, uiPacketSize);
   if (uiPacketSize == 0) goto cleanUp;
   
   // Check protocol header to deside if we are interested
   status = CheckProtocolHeader(
      pvLookAheadBuffer, uiLookaheadBufferSize, uiPacketSize
   );
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

   // There we should get a packet
   pPacket = GetPacketFromFreeRecv(TRUE);
   if (pPacket == NULL) {
      Log(NDT_ERR_RECV_PACKET_OUTOFDESCRIPTORS);
      NdisInterlockedIncrement((LONG*)&m_ulUnexpectedEvents);
      status = NDIS_STATUS_NOT_RECOGNIZED;
      goto cleanUp;
   }

   // Copy header to packet
   NdisMoveMemory(
      pPacket->m_pucMediumHeader, pvHeaderBuffer, uiHeaderBufferSize
   );
   pPacket->m_uiSize = uiHeaderBufferSize;

   // Copy protocol header to packet
   NdisCopyLookaheadData(
      pPacket->m_pProtocolHeader, pvLookAheadBuffer, sizeof(PROTOCOL_HEADER), 
      m_pMedium->m_uiMacOptions
   );
   pPacket->m_uiSize += sizeof(PROTOCOL_HEADER);

   // Get packet content
   if (uiLookaheadBufferSize >= uiPacketSize) {

      // Full packet is in a lookahead buffer, so copy body from there
      NdisCopyLookaheadData(
         pPacket->m_pucBody, (PBYTE)pvLookAheadBuffer + sizeof(PROTOCOL_HEADER), 
         uiPacketSize - sizeof(PROTOCOL_HEADER), m_pMedium->m_uiMacOptions
      );
      pPacket->m_uiSize += uiPacketSize - sizeof(PROTOCOL_HEADER);

      // Chain medium header
      pPacket->ChainBufferAtFront(
         pPacket->m_pucMediumHeader, uiHeaderBufferSize
      );

      // Chain protocol header
      pPacket->ChainBufferAtBack(
         pPacket->m_pProtocolHeader, sizeof(PROTOCOL_HEADER)
      );

      // Chain body
      pPacket->ChainBufferAtBack(
         pPacket->m_pucBody, uiPacketSize - sizeof(PROTOCOL_HEADER)
      );

      // Packet is complete (set state & save time)
      pPacket->m_bTransferred = TRUE;
      NdisGetSystemUpTime(&pPacket->m_ulStateChange);
      
      // Move a packet to the queue for postprocessing
      AddPacketToReceived(pPacket);

      if (m_pProtocol->m_bLogPackets) {
         PROTOCOL_HEADER* pHeader = pPacket->m_pProtocolHeader;
         LogX(
            _T("%08lu Bind %d Receive #%04lu/%02u/%02x %d/%d/%d\n"),
            GetTickCount(), m_lInstanceId, pHeader->ulSequenceNumber, 
            pHeader->usReplyId, pHeader->ucResponseMode, uiHeaderBufferSize,
            uiLookaheadBufferSize, uiPacketSize
         );
      }
      
   } else {

      // Packet isn't complete
      pPacket->m_bTransferred = FALSE;
      NdisGetSystemUpTime(&pPacket->m_ulStateChange);

      // Move a packet to the queue where it will wait for data
      AddPacketToReceived(pPacket);

      // Attach body buffer to packet
      pPacket->ChainBufferAtBack(
         pPacket->m_pucBody, uiPacketSize - sizeof(PROTOCOL_HEADER)
      );
      
      // We should ask for a packet body
      NdisTransferData(
         &status, m_hAdapter, hMacReceiveContext, 
         sizeof(PROTOCOL_HEADER), uiPacketSize - sizeof(PROTOCOL_HEADER),
         pPacket->m_pNdisPacket, &uiBytesTransferred
      );

      // Process result
      if (status == NDIS_STATUS_SUCCESS) {
         // Move packet to receive complete queue
         pPacket->m_uiSize += uiBytesTransferred;
         // Chain protocol header
         pPacket->ChainBufferAtFront(
            pPacket->m_pProtocolHeader, sizeof(PROTOCOL_HEADER)
         );
         // Chain medium header
         pPacket->ChainBufferAtFront(
            pPacket->m_pucMediumHeader, uiHeaderBufferSize
         );

         // Update packet state
         m_listReceivedPackets.AcquireSpinLock();
         pPacket->m_bTransferred = TRUE;
	      NdisGetSystemUpTime(&pPacket->m_ulStateChange);
         m_listReceivedPackets.ReleaseSpinLock();
      } else if (status != NDIS_STATUS_PENDING) {
         Log(NDT_ERR_RECV_PACKET_TRANSFER);
         RemovePacketFromReceived(pPacket);
         AddPacketToFreeRecv(pPacket);
         NdisInterlockedIncrement((LONG*)&m_ulUnexpectedEvents);
         status = NDIS_STATUS_NOT_RECOGNIZED;
         goto cleanUp;
      }
   }

cleanUp:
   return status;
}

//------------------------------------------------------------------------------

void CBinding::TransferDataComplete(
   PNDIS_PACKET pNdisPacket, NDIS_STATUS status, UINT uiBytesTransferred
)
{
   // Get a packet object
   CPacket* pPacket = (CPacket*)pNdisPacket->ProtocolReserved;
   if (pPacket == NULL || pPacket->m_dwMagic != NDT_MAGIC_PACKET) {
      Log(NDT_ERR_UNEXP_TRANSFER_DATA_PACKET);
      NdisInterlockedIncrement((LONG*)&m_ulUnexpectedEvents);
      NdisInterlockedIncrement((LONG*)&m_ulUnexpectedTransferComplete);
      goto cleanUp;
   }

   // Based on transfer data result
   if (status != NDIS_STATUS_SUCCESS) {
      Log(NDT_ERR_TRANSFER_DATA_FAILED);
      RemovePacketFromReceived(pPacket);
      AddPacketToFreeRecv(pPacket);
      NdisInterlockedIncrement((LONG*)&m_ulUnexpectedEvents);
   } else {
      // Move packet to receive complete queue
      pPacket->m_uiSize += uiBytesTransferred;
      // Chain protocol header
      pPacket->ChainBufferAtFront(
         pPacket->m_pProtocolHeader, sizeof(PROTOCOL_HEADER)
      );
      // Chain medium header
      pPacket->ChainBufferAtFront(
         pPacket->m_pucMediumHeader, m_pMedium->m_uiHeaderSize
      );
      // Change state & update time
      m_listReceivedPackets.AcquireSpinLock();
      pPacket->m_bTransferred = TRUE;
      NdisGetSystemUpTime(&pPacket->m_ulStateChange);
      m_listReceivedPackets.ReleaseSpinLock();
      // Log packet transfer
      if (m_pProtocol->m_bLogPackets) {
         PROTOCOL_HEADER* pHeader = pPacket->m_pProtocolHeader;
         LogX(
            _T("%08lu Bind %d TransferDataComplete #%04lu/%02u/%02x ")
            _T("Status 0x%08x\n"), GetTickCount(), m_lInstanceId, 
            pHeader->ulSequenceNumber, pHeader->usReplyId, 
            pHeader->ucResponseMode, status
         );
      }
   }
   
cleanUp: ;   
}

//------------------------------------------------------------------------------

INT CBinding::ReceivePacket(PNDIS_PACKET pNdisPacket)
{
   NDIS_STATUS status = NDIS_STATUS_NOT_RECOGNIZED;
   CPacket* pPacket = NULL;
   INT nCount = 0;
   UINT uiSize = 0;
   UINT uiCopied = 0;
   UINT uiPacketSize = 0;

   // Get packet length
   NdisQueryPacket(pNdisPacket, NULL, NULL, NULL, &uiSize);

   // Check packet size
   if (uiSize > m_pMedium->m_uiMaxFrameSize) {
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

   // Chain medium header
   pPacket->ChainBufferAtFront(
      pPacket->m_pucMediumHeader, m_pMedium->m_uiHeaderSize
   );

   // Chain protocol header
   pPacket->ChainBufferAtBack(
      pPacket->m_pProtocolHeader, sizeof(PROTOCOL_HEADER)
   );

   // Chain protocol body
   pPacket->ChainBufferAtBack(
      pPacket->m_pucBody, 
      uiSize - sizeof(PROTOCOL_HEADER) - m_pMedium->m_uiHeaderSize
   );
   
   NdisCopyFromPacketToPacket(
      pPacket->m_pNdisPacket, 0, uiSize, pNdisPacket, 0, &uiCopied
   );

   // We have to copy all packet, if not something bad happen
   if (uiSize != uiCopied) {
      Log(NDT_ERR_RECV_PACKET_COPY, uiSize, uiCopied);
      AddPacketToFreeRecv(pPacket);
      goto cleanUp;      
   }
   
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
   NdisGetSystemUpTime(&pPacket->m_ulStateChange);

   // Add packet to queue
   AddPacketToReceived(pPacket);

   // And process it
   ReceiveComplete();
   
cleanUp:   
   return nCount;
}

//------------------------------------------------------------------------------

void CBinding::ReceiveComplete()
{
   CPacket* pPacket = NULL;
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;

   // Find requests in pending queue if any but don't remove it
   CRequestSend* pRequestSend = FindSendRequest(FALSE);
   CRequestReceive* pRequestReceive = FindReceiveRequest(FALSE);
   
   // Check if we are get correct request objects
   ASSERT(
      pRequestReceive == NULL || 
      pRequestReceive->m_dwMagic == NDT_MAGIC_REQUEST_RECEIVE
   );
   ASSERT(
      pRequestSend == NULL || 
      pRequestSend->m_dwMagic == NDT_MAGIC_REQUEST_SEND
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
   
   // We have to release a pointert
   if (pRequestReceive != NULL) pRequestReceive->Release();
   if (pRequestSend != NULL) pRequestSend->Release();
}

//------------------------------------------------------------------------------

NDIS_STATUS CBinding::UnbindAdapter(NDIS_HANDLE hUnbindContext)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;

   // Close adapter
   if (m_hAdapter != NULL) {
      NdisCloseAdapter(&status, m_hAdapter);
      m_hAdapter = NULL;
   }

   // Remove itself from a list
   m_pProtocol->RemoveBindingFromList(this);

   // And because NDIS will hopefully not use binding context anymore
   Release();
   
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
   NDIS_PACKET* pNdisPacket = NULL;
   UINT uiPackets = m_uiPacketsSend + m_uiPacketsRecv;
   UINT uiBuffers = uiPackets * (2 + m_uiBuffersPerPacket);
   CPacket* pPacket = NULL;
   CPacket* pNextPacket = NULL;
   UINT i = 0;

   // Allocate a packet pool
   NdisAllocatePacketPool(
      &status, &m_hPacketPool, uiPackets, sizeof(CPacket)
   );
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

   // Allocate a buffer pool
   NdisAllocateBufferPool(&status, &m_hBufferPool, uiBuffers);
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

   // Now create an internal pool
   for (i = 0; i < uiPackets; i++) {
      
      // Allocate a packet from the pool
      NdisAllocatePacket(&status, &pNdisPacket, m_hPacketPool);
      if (status != NDIS_STATUS_SUCCESS) goto cleanUp;
      // Get a pointer to CPacket
      pPacket = (CPacket*)pNdisPacket->ProtocolReserved;
      // Call constructor for it
      pPacket = new((void *)pPacket) CPacket(this, pNdisPacket);

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

   // Remove & destroy packets from all queues
   while ((pPacket = GetPacketFromReceived(TRUE)) != NULL) pPacket->ReleaseEx();
   while ((pPacket = GetPacketFromSent(TRUE)) != NULL) pPacket->ReleaseEx();
   while ((pPacket = GetPacketFromFreeSend(TRUE)) != NULL) pPacket->ReleaseEx();
   while ((pPacket = GetPacketFromFreeRecv(TRUE)) != NULL) pPacket->ReleaseEx();
   // Release buffer pool
   if (m_hBufferPool != NULL) NdisFreeBufferPool(m_hBufferPool);
   m_hBufferPool = NULL;
   // Release packet pool
   if (m_hPacketPool != NULL) NdisFreePacketPool(m_hPacketPool);
   m_hPacketPool = NULL;
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
   CPacket* pPacket, UINT uiSize, BYTE ucResponseMode, BYTE ucFirstByte, 
   ULONG ulSequenceNumber, USHORT usReplyId
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
   pHeader->ucResponseMode = ucResponseMode;
   pHeader->ucFirstByte = ucFirstByte;
   pHeader->usReplyId = usReplyId;

   pHeader->uiSize = uiSize;

   // TODO: Compute checksum
   pHeader->ulCheckSum = 0;

   pPacket->ChainBufferAtBack(
      pPacket->m_pProtocolHeader, sizeof(PROTOCOL_HEADER)
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

   if (pHeader->uiSize != uiPacketSize) {
      ASSERT(0);
      goto cleanUp;
   }
   
   // When we get there so packet is ours....
   status = NDIS_STATUS_SUCCESS;
   
cleanUp:
   return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CBinding::BuildPacketBody(
   CPacket* pPacket, BYTE ucSizeMode, UINT uiSize, BYTE ucFirstByte
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

      pPacket->ChainBufferAtBack(m_pucStaticBody + ucFirstByte, uiSize);

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
         pPacket->ChainBufferAtBack(pucWorkPos, uiBufferSize);
         pucWorkPos += uiBufferSize;
         uiWorkSize += uiBufferSize;
      }

      // We should chain full packet
      if (uiWorkSize < uiSize) {
         uiBufferSize = uiSize - uiWorkSize;
         pPacket->ChainBufferAtBack(pucWorkPos, uiBufferSize);
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
   BYTE ucSizeMode, UINT uiSize, ULONG ulSequenceNumber, USHORT usReplyId
)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   BYTE ucFirstByte = (BYTE)NDT_NdisGetRandom(0, 255);
   
   // Get media header
   status = m_pMedium->BuildMediaHeader(
      pPacket, pucDestAddr, pucSrcAddr, uiSize
   );
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

   // Get protocol header
   status = BuildProtocolHeader(
      pPacket, uiSize, ucResponseMode, ucFirstByte, ulSequenceNumber, usReplyId
   );
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

   // Fix a data size
   uiSize -= sizeof(PROTOCOL_HEADER);

   // And packet body
   status = BuildPacketBody(pPacket, ucSizeMode, uiSize, ucFirstByte);
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

cleanUp:   
   return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CBinding::BuildPacketForResponse(
   CPacket* pPacket, CPacket* pRecvPacket
)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   PROTOCOL_HEADER* pHeader = pPacket->m_pProtocolHeader;
   PROTOCOL_HEADER* pRecvHeader = pRecvPacket->m_pProtocolHeader;

   // Copy protocol header
   NdisMoveMemory(pHeader, pRecvHeader, sizeof(PROTOCOL_HEADER));
   pHeader->ucResponseMode |= NDT_RESPONSE_FLAG_RESPONSE;
   pHeader->usSourcePortId = pRecvHeader->usTargetPortId;
   pHeader->usTargetPortId = pRecvHeader->usSourcePortId;
   pHeader->uiSize = sizeof(PROTOCOL_HEADER);
   pPacket->ChainBufferAtBack(pHeader, sizeof(PROTOCOL_HEADER));

   if ((pHeader->ucResponseMode & NDT_RESPONSE_MASK) == NDT_RESPONSE_FULL) {
      pHeader->uiSize = pRecvHeader->uiSize;
      status = BuildPacketBody(
         pPacket, NDT_PACKET_BUFFERS_NORMAL, 
         pHeader->uiSize - sizeof(PROTOCOL_HEADER), pHeader->ucFirstByte
      );
      if (status != NDIS_STATUS_SUCCESS) goto cleanUp;
   }

   // First build reply medium header
   status = m_pMedium->BuildReplyMediaHeader(
      pPacket, pHeader->uiSize, pRecvPacket
   );
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

cleanUp:   
   return status;
}   

//------------------------------------------------------------------------------
