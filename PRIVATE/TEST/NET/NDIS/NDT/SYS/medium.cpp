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
#include "Medium.h"
#include "RequestRequest.h"
#include "Log.h"

//------------------------------------------------------------------------------

CMedium::CMedium(CBinding *pBinding, NDIS_MEDIUM medium)
{
   // Save a media type
   m_medium = medium;
   // We need pointer to parent binding
   m_pBinding = pBinding;
   pBinding->AddRef();
}

//------------------------------------------------------------------------------

CMedium::~CMedium()
{
   m_pBinding->Release();
}

//------------------------------------------------------------------------------

NDIS_STATUS CMedium::Init(UINT uiLookAheadSize)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   CRequestRequest* pRequest = NULL;

   // Get some information about a miniport
   pRequest = new CRequestRequest(m_pBinding, sizeof(ULONG), sizeof(ULONG));
   if (pRequest == NULL) goto cleanUp;
   
   // CURRENT LOOKAHEAD - set
   pRequest->m_requestType = NdisRequestSetInformation;
   pRequest->m_oid = OID_GEN_CURRENT_LOOKAHEAD;
   *(ULONG *)pRequest->m_pucInpBuffer = uiLookAheadSize;
   status = pRequest->InternalExecute(m_pBinding->m_dwInternalTimeout);
  
   // MAC OPTIONS
   pRequest->m_requestType = NdisRequestQueryInformation;
   pRequest->m_oid = OID_GEN_MAC_OPTIONS;
   status = pRequest->InternalExecute(m_pBinding->m_dwInternalTimeout);
   if (status == NDIS_STATUS_SUCCESS) {
      m_uiMacOptions = *(ULONG *)pRequest->m_pucOutBuffer;
   }

   // MAXIMUM FRAME SIZE
   pRequest->m_requestType = NdisRequestQueryInformation;
   pRequest->m_oid = OID_GEN_MAXIMUM_FRAME_SIZE;
   status = pRequest->InternalExecute(m_pBinding->m_dwInternalTimeout);
   if (status == NDIS_STATUS_SUCCESS) {
      m_uiMaxFrameSize = *(ULONG *)pRequest->m_pucOutBuffer;
   }

   // MAXIMUM TOTAL SIZE
   pRequest->m_requestType = NdisRequestQueryInformation;
   pRequest->m_oid = OID_GEN_MAXIMUM_TOTAL_SIZE;
   status = pRequest->InternalExecute(m_pBinding->m_dwInternalTimeout);
   if (status == NDIS_STATUS_SUCCESS) {
      m_uiMaxTotalSize = *(ULONG *)pRequest->m_pucOutBuffer;
   }

   // MAXIMUM SEND PACKETS (if implemented)
   pRequest->m_requestType = NdisRequestQueryInformation;
   pRequest->m_oid = OID_GEN_MAXIMUM_SEND_PACKETS;
   status = pRequest->InternalExecute(m_pBinding->m_dwInternalTimeout);
   if (status == NDIS_STATUS_SUCCESS) {
      m_uiMaxSendPackes = *(ULONG *)pRequest->m_pucOutBuffer;
   }

   // LINK SPEED
   pRequest->m_requestType = NdisRequestQueryInformation;
   pRequest->m_oid = OID_GEN_LINK_SPEED;
   status = pRequest->InternalExecute(m_pBinding->m_dwInternalTimeout);
   if (status == NDIS_STATUS_SUCCESS) {
      m_uiLinkSpeed = *(ULONG *)pRequest->m_pucOutBuffer;
   }
   
cleanUp:
   // We are done with general requests
   if (pRequest != NULL) pRequest->Release(); 
   return NDIS_STATUS_SUCCESS;
}

//------------------------------------------------------------------------------

UINT CMedium::CheckReceive(PVOID pvPacketHeader, UINT uiPacketSize)
{
   return uiPacketSize;
}

//------------------------------------------------------------------------------
