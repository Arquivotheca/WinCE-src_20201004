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
