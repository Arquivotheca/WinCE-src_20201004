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
   CPacket* pPacket, CMDLChain* pMDLChain, PBYTE pbDestAddr, PBYTE pbSrcAddr, UINT uiSize
)
{
    PMDL pMediumHeaderMDL = NULL;

   // Fixed Send bug. The size field for a 802.3 header should not include the header size
   // uiSize += m_uiHeaderSize;
   NdisMoveMemory(pPacket->m_pucMediumHeader, pbDestAddr, 6);
   if (pbSrcAddr == NULL) pbSrcAddr = m_abPermanentAddress;
   NdisMoveMemory(pPacket->m_pucMediumHeader + 6, pbSrcAddr, 6);
   pPacket->m_pucMediumHeader[12] = (uiSize >> 8) & 0xFF;
   pPacket->m_pucMediumHeader[13] = uiSize & 0xFF;
   pPacket->m_uiSize = uiSize;

   //TODO: extract into its own function in base class
   pMediumHeaderMDL = pMDLChain->CreateMDL(pPacket->m_pucMediumHeader, m_uiHeaderSize);
   pMDLChain->AddMDLAtFront(pMediumHeaderMDL);

   return NDIS_STATUS_SUCCESS;
}

//------------------------------------------------------------------------------

NDIS_STATUS CMedium802_3::BuildReplyMediaHeader(
   CPacket* pPacket, CMDLChain* pMDLChain, UINT uiSize, CPacket* pRecvPacket
)
{
   PMDL pMediumHeaderMDL = NULL;

   NdisMoveMemory(
      pPacket->m_pucMediumHeader, pRecvPacket->m_pucMediumHeader + 6, 6
   );
   NdisMoveMemory(pPacket->m_pucMediumHeader + 6, m_abPermanentAddress, 6);
   pPacket->m_pucMediumHeader[12] = (uiSize >> 8) & 0xFF;
   pPacket->m_pucMediumHeader[13] = uiSize & 0xFF;
   pPacket->m_uiSize = uiSize;
   
   //TODO: extract into its own function in base class
   pMediumHeaderMDL = pMDLChain->CreateMDL(pPacket->m_pucMediumHeader, m_uiHeaderSize);
   pMDLChain->AddMDLAtFront(pMediumHeaderMDL);

   return NDIS_STATUS_SUCCESS;
}

//------------------------------------------------------------------------------

UINT CMedium802_3::CheckReceive(PVOID pvPacketHeader, UINT uiPacketSize)
{
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
