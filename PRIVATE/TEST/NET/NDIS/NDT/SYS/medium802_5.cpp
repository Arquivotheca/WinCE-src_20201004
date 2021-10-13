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
#include "Medium802_5.h"
#include "RequestRequest.h"
#include "Packet.h"
#include "Log.h"

//------------------------------------------------------------------------------

CMedium802_5::CMedium802_5(CBinding *pBinding) :
   CMedium(pBinding, NdisMedium802_3)
{
   // A magic value for class instance identification
   m_dwMagic = NDT_MAGIC_MEDIUM_802_5;
   NdisZeroMemory(m_abPermanentAddress, sizeof(m_abPermanentAddress));
   m_uiAddressSize = 6;
   m_uiHeaderSize = 14;
}

//------------------------------------------------------------------------------

CMedium802_5::~CMedium802_5()
{
   m_pBinding->Release();
}

//------------------------------------------------------------------------------

NDIS_STATUS CMedium802_5::Init(UINT uiLookAheadSize)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   CRequestRequest* pRequest = NULL;

   // Just for case when following init will not get all OIDs
   m_uiMaxFrameSize = 4082;
   m_uiMaxTotalSize = 4096;
   m_uiMaxSendPackes = 1;
   m_uiLinkSpeed = 40000;
   
   // Get general information
   CMedium::Init(uiLookAheadSize);
   
   // Get some information related to media type
   pRequest = new CRequestRequest(m_pBinding);
   if (pRequest == NULL) goto cleanUp;

   pRequest->m_pucOutBuffer = m_abPermanentAddress;
   pRequest->m_cbOutBuffer = sizeof(m_abPermanentAddress);
   pRequest->m_oid = OID_802_5_PERMANENT_ADDRESS;
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

NDIS_STATUS CMedium802_5::BuildMediaHeader(
   CPacket* pPacket, PBYTE pbDestAddr, PBYTE pbSrcAddr, UINT uiSize
)
{
   pPacket->m_pucMediumHeader[0] = 0x10;
   pPacket->m_pucMediumHeader[1] = 0x40;
   NdisMoveMemory(pPacket->m_pucMediumHeader + 2, pbDestAddr, 6);
   if (pbSrcAddr == NULL) pbSrcAddr = m_abPermanentAddress;
   NdisMoveMemory(pPacket->m_pucMediumHeader + 8, pbSrcAddr, 6);
   pPacket->ChainBufferAtFront(pPacket->m_pucMediumHeader, 14);
   return NDIS_STATUS_SUCCESS;
}

//------------------------------------------------------------------------------

NDIS_STATUS CMedium802_5::BuildReplyMediaHeader(
   CPacket* pPacket, UINT uiSize, CPacket* pRecvPacket
)
{
   pPacket->m_pucMediumHeader[0] = 0x10;
   pPacket->m_pucMediumHeader[1] = 0x40;
   NdisMoveMemory(
      pPacket->m_pucMediumHeader + 2, pRecvPacket->m_pucMediumHeader + 8, 6
   );
   NdisMoveMemory(pPacket->m_pucMediumHeader + 8, m_abPermanentAddress, 6);
   pPacket->ChainBufferAtFront(pPacket->m_pucMediumHeader, 14);
   return NDIS_STATUS_SUCCESS;
}

//------------------------------------------------------------------------------

UINT CMedium802_5::CheckReceive(PVOID pvPacketHeader, UINT uiPacketSize)
{
   return uiPacketSize;
}

//------------------------------------------------------------------------------

INT CMedium802_5::IsSendAddr(PVOID pvPacketHeader, UCHAR* pucAddr)
{
   INT iResult = 0;
   UCHAR* pucSourceAddr = (UCHAR*)pvPacketHeader + 8;

   for (UINT ix = 0; ix < 6; ix++) {
      iResult = (INT)(*pucSourceAddr++ - *pucAddr++);
      if (iResult != NULL) break;
   }
   return iResult;
}

//------------------------------------------------------------------------------
