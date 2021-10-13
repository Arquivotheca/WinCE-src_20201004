//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "StdAfx.h"
#include "Binding.h"
#include "Medium802_3.h"
#include "RequestRequest.h"
#include "Packet.h"
#include "Log.h"

//------------------------------------------------------------------------------

CMedium802_3::CMedium802_3(CBinding *pBinding) :
   CMedium(pBinding, NdisMedium802_3)
{
   // A magic value for class instance identification
   m_dwMagic = NDT_MAGIC_MEDIUM_802_3;
   NdisZeroMemory(m_abPermanentAddress, sizeof(m_abPermanentAddress));
   m_uiAddressSize = 6;
   m_uiHeaderSize = 14;
}

//------------------------------------------------------------------------------

CMedium802_3::~CMedium802_3()
{
   m_pBinding->Release();
}

//------------------------------------------------------------------------------

NDIS_STATUS CMedium802_3::Init(UINT uiLookAheadSize)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   CRequestRequest* pRequest = NULL;

   // Just for case when following init will not get all OIDs
   m_uiMaxFrameSize = 1500;
   m_uiMaxTotalSize = 1514;
   m_uiMaxSendPackes = 1;
   m_uiLinkSpeed = 100000;
   
   // Get general information
   CMedium::Init(uiLookAheadSize);
   
   // Get some information related to media type
   pRequest = new CRequestRequest(m_pBinding);
   if (pRequest == NULL) goto cleanUp;
   
   pRequest->m_pucOutBuffer = m_abPermanentAddress;
   pRequest->m_cbOutBuffer = sizeof(m_abPermanentAddress);
   pRequest->m_oid = OID_802_3_PERMANENT_ADDRESS;
   status = pRequest->InternalExecute(m_pBinding->m_dwInternalTimeout);
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

cleanUp:
   if (pRequest != NULL) {
      pRequest->m_pucOutBuffer = NULL;
      pRequest->m_cbOutBuffer = 0;
      pRequest->Release();
   }
   return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CMedium802_3::BuildMediaHeader(
   CPacket* pPacket, PBYTE pbDestAddr, PBYTE pbSrcAddr, UINT uiSize
)
{
   uiSize += m_uiHeaderSize;
   NdisMoveMemory(pPacket->m_pucMediumHeader, pbDestAddr, 6);
   if (pbSrcAddr == NULL) pbSrcAddr = m_abPermanentAddress;
   NdisMoveMemory(pPacket->m_pucMediumHeader + 6, pbSrcAddr, 6);
   pPacket->m_pucMediumHeader[12] = (uiSize >> 8) & 0xFF;
   pPacket->m_pucMediumHeader[13] = uiSize & 0xFF;
   pPacket->m_uiSize = uiSize;
   pPacket->ChainBufferAtFront(pPacket->m_pucMediumHeader, m_uiHeaderSize);
   return NDIS_STATUS_SUCCESS;
}

//------------------------------------------------------------------------------

NDIS_STATUS CMedium802_3::BuildReplyMediaHeader(
   CPacket* pPacket, UINT uiSize, CPacket* pRecvPacket
)
{
   uiSize += m_uiHeaderSize;
   NdisMoveMemory(
      pPacket->m_pucMediumHeader, pRecvPacket->m_pucMediumHeader + 6, 6
   );
   NdisMoveMemory(pPacket->m_pucMediumHeader + 6, m_abPermanentAddress, 6);
   pPacket->m_pucMediumHeader[12] = (uiSize >> 8) & 0xFF;
   pPacket->m_pucMediumHeader[13] = uiSize & 0xFF;
   pPacket->m_uiSize = uiSize;
   pPacket->ChainBufferAtFront(pPacket->m_pucMediumHeader, m_uiHeaderSize);
   return NDIS_STATUS_SUCCESS;
}

//------------------------------------------------------------------------------

UINT CMedium802_3::CheckReceive(PVOID pvPacketHeader, UINT uiPacketSize)
{
   if (uiPacketSize <= (64 - m_uiHeaderSize)) {
      if (((PBYTE)pvPacketHeader)[12] == 0) {
         uiPacketSize = ((PBYTE)pvPacketHeader)[13];
         if (uiPacketSize >= m_uiHeaderSize) uiPacketSize -= m_uiHeaderSize;
         else uiPacketSize = 0;
      }
   }
   return uiPacketSize;
}

//------------------------------------------------------------------------------

INT CMedium802_3::IsSendAddr(PVOID pvPacketHeader, UCHAR* pucAddr)
{
   INT iResult = 0;
   UCHAR* pucSourceAddr = (UCHAR*)pvPacketHeader + 6;

   for (UINT ix = 0; ix < 6; ix++) {
      iResult = (INT)(*pucSourceAddr++ - *pucAddr++);
      if (iResult != NULL) break;
   }
   return iResult;
}

//------------------------------------------------------------------------------
