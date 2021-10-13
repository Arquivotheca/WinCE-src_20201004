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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#include "StdAfx.h"
#include "Binding.h"
#include "Packet.h"

//------------------------------------------------------------------------------

CPacket::CPacket(CBinding* pBinding, NDIS_PACKET *pNdisPacket)
{
   m_dwMagic = NDT_MAGIC_PACKET;
   m_pBinding = pBinding; m_pBinding->AddRef();
   m_pNdisPacket = pNdisPacket;
   m_uiSize = 0;
   m_pucMediumHeader = NULL;
   m_pProtocolHeader = NULL;
   m_pucBody = NULL;
   m_bBodyStatic= FALSE;
}

//------------------------------------------------------------------------------

CPacket::~CPacket()
{
   // Standard desctructor should never be called on the CPacket object
   // Reason is that it is allocated as part of NDIS_PACKET
   ASSERT(FALSE);
}

//------------------------------------------------------------------------------

LONG CPacket::ReleaseEx()
{
   LONG nRefCount = NdisInterlockedDecrement(&m_nRefCount);
   // Delete should be done with this special way (see allocation @ CBinding)
   if (nRefCount == 0) {
      FreeAllBuffers();
      m_pBinding->Release();
      NdisFreePacket(m_pNdisPacket);
   }
   return nRefCount;
}

//------------------------------------------------------------------------------

NDIS_STATUS CPacket::ChainBufferAtFront(PVOID pv, UINT cb)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   NDIS_BUFFER* pNdisBuffer = NULL;
   BOOL bZeroLength = (cb == 0);

   // Fix zero length buffer
   if (bZeroLength) cb = 1;
   
   // Allocate a buffer descriptor
   NdisAllocateBuffer(
      &status, &pNdisBuffer, m_pBinding->m_hBufferPool, pv, cb
   );
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

   // Attach it to packet
   NdisChainBufferAtFront(m_pNdisPacket, pNdisBuffer);

   // Fix zero length buffer
   if (bZeroLength) pNdisBuffer->ByteCount = 0;

cleanUp:
   return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CPacket::ChainBufferAtBack(PVOID pv, UINT cb)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   NDIS_BUFFER* pNdisBuffer = NULL;
   BOOL bZeroLength = (cb == 0);

   // Fix zero length buffer
   if (bZeroLength) cb = 1;

   // Allocate a buffer descriptor
   NdisAllocateBuffer(
      &status, &pNdisBuffer, m_pBinding->m_hBufferPool, pv, cb
   );
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

   // Attach it to packet
   NdisChainBufferAtBack(m_pNdisPacket, pNdisBuffer);

   // Fix zero length buffer
   if (bZeroLength) pNdisBuffer->ByteCount = 0;
   
cleanUp:
   return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CPacket::AllocBufferAtFront(UINT cb, PVOID *ppv)
{
   NDIS_STATUS status = NDIS_STATUS_RESOURCES;
   
   *ppv = new BYTE[cb];
   if (*ppv == NULL) { 
      DebugBreak();
      goto cleanUp;
   }
   status = ChainBufferAtFront(*ppv, cb);

cleanUp:
   return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CPacket::AllocBufferAtBack(UINT cb, PVOID *ppv)
{
   NDIS_STATUS status = NDIS_STATUS_RESOURCES;
   
   *ppv = new BYTE[cb];
   if (*ppv == NULL) { 
      DebugBreak();
      goto cleanUp;
   }
   status = ChainBufferAtBack(*ppv, cb);

cleanUp:
   return status;
}

//------------------------------------------------------------------------------

void CPacket::FreeBufferAtFront()
{
   NDIS_BUFFER* pNdisBuffer = NULL;
   PVOID pvBuffer = NULL;

   NdisUnchainBufferAtFront(m_pNdisPacket, &pNdisBuffer);
   if (pNdisBuffer != NULL) {
      pvBuffer = NdisBufferVirtualAddress(pNdisBuffer);
      delete pvBuffer;
      NdisFreeBuffer(pNdisBuffer);
   }      
}

//------------------------------------------------------------------------------

void CPacket::FreeBufferAtBack()
{
   NDIS_BUFFER* pNdisBuffer = NULL;
   PVOID pvBuffer = NULL;

   NdisUnchainBufferAtBack(m_pNdisPacket, &pNdisBuffer);
   if (pNdisBuffer != NULL) {
      pvBuffer = NdisBufferVirtualAddress(pNdisBuffer);
      delete pvBuffer;
      NdisFreeBuffer(pNdisBuffer);
   }      
}

//------------------------------------------------------------------------------

void CPacket::UnchainAllBuffers()
{
   NDIS_PACKET_OOB_DATA* pNdisPacketOobData = NULL;
   NDIS_BUFFER* pNdisBuffer = NULL;
   NDIS_BUFFER* pNdisBufferBody = NULL;   

   NdisUnchainBufferAtBack(m_pNdisPacket, &pNdisBuffer);
   while (pNdisBuffer != NULL) {
      NdisFreeBuffer(pNdisBuffer);
      NdisUnchainBufferAtBack(m_pNdisPacket, &pNdisBuffer);
   }
}

//------------------------------------------------------------------------------

void CPacket::FreeAllBuffers()
{
   NDIS_BUFFER* pNdisBuffer = NULL;
   PVOID pvBuffer = NULL;


   UnchainAllBuffers();
   delete m_pucMediumHeader;
   delete m_pProtocolHeader;
   if (!m_bBodyStatic) delete m_pucBody;
   m_pucMediumHeader = NULL;
   m_pProtocolHeader = NULL;
   m_pucBody = NULL;
   m_uiSize = 0;
}

//------------------------------------------------------------------------------
