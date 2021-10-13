//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "StdAfx.h"
#include "NDT.h"
#include "Protocol.h"
#include "Binding.h"
#include "RequestRequest.h"
#include "Marshal.h"
#include "Log.h"

//------------------------------------------------------------------------------

CRequestRequest::CRequestRequest(
   CBinding* pBinding, UINT cbInpBuffer, UINT cbOutBuffer
) : 
   CRequest(NDT_REQUEST_REQUEST, pBinding)
{
   m_dwMagic = NDT_MAGIC_REQUEST_REQUEST;
   m_oid = 0;
   m_pucInpBuffer = NULL;
   m_cbInpBuffer = cbInpBuffer;
   if (cbInpBuffer > 0) m_pucInpBuffer = new BYTE[cbInpBuffer];
   m_pucOutBuffer = NULL;
   m_cbOutBuffer = cbOutBuffer;
   if (cbOutBuffer > 0) m_pucOutBuffer = new BYTE[cbOutBuffer];
   NdisZeroMemory(&m_ndisRequest, sizeof(m_ndisRequest));
}

//------------------------------------------------------------------------------

CRequestRequest::~CRequestRequest()
{
   delete m_pucInpBuffer;
   delete m_pucOutBuffer;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestRequest::Execute()
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   
   // Add request to pending queue
   m_pBinding->AddRequestToList(this);
   
   m_ndisRequest.RequestType = m_requestType;
   switch (m_requestType) {
   case NdisRequestQueryInformation:
   case NdisRequestQueryStatistics:
      m_ndisRequest.DATA.QUERY_INFORMATION.Oid = m_oid;
      m_ndisRequest.DATA.QUERY_INFORMATION.InformationBuffer = m_pucOutBuffer;
      m_ndisRequest.DATA.QUERY_INFORMATION.InformationBufferLength = m_cbOutBuffer;
      m_ndisRequest.DATA.QUERY_INFORMATION.BytesWritten = 0;
      m_ndisRequest.DATA.QUERY_INFORMATION.BytesNeeded = 0;
      break;
   case NdisRequestSetInformation:
      m_ndisRequest.DATA.SET_INFORMATION.Oid = m_oid;
      m_ndisRequest.DATA.SET_INFORMATION.InformationBuffer = m_pucInpBuffer;
      m_ndisRequest.DATA.SET_INFORMATION.InformationBufferLength = m_cbInpBuffer;
      m_ndisRequest.DATA.SET_INFORMATION.BytesRead = 0;
      m_ndisRequest.DATA.SET_INFORMATION.BytesNeeded = 0;
      break;
   }
   
   // Execute command
   NdisRequest(&status, m_pBinding->m_hAdapter, &m_ndisRequest);

   // If request isn't pending remove it and complete
   if (status != NDIS_STATUS_PENDING) {
      m_status = status;
      m_pBinding->RemoveRequestFromList(this);
      Complete();
   }

   // And return
   return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestRequest::UnmarshalInpParams(
   PVOID *ppvBuffer, DWORD *pcbBuffer
)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   
   status = UnmarshalParameters(
      ppvBuffer, pcbBuffer, NDT_MARSHAL_INP_REQUEST, &m_pBinding, 
      &m_requestType, &m_oid, &m_cbInpBuffer, &m_pucInpBuffer, &m_cbOutBuffer
   );
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

   // We save an pointer to object
   m_pBinding->AddRef();

   // There we will allocate buffer for result
   delete m_pucOutBuffer; 
   m_pucOutBuffer = NULL;
   if (m_cbOutBuffer > 0) {
      m_pucOutBuffer = new BYTE[m_cbOutBuffer];
      if (m_pucOutBuffer == NULL) {
         status = NDIS_STATUS_RESOURCES;
         DebugBreak();
         goto cleanUp;
      }
   }

cleanUp:
   return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestRequest::MarshalOutParams(
   PVOID *ppvBuffer, DWORD *pcbBuffer
)
{
   UINT cbRequired = 0;
   UINT cbUsed = 0;
   UINT cbReturn = 0;
   
   switch (m_requestType) {
   case NdisRequestQueryInformation:
   case NdisRequestQueryStatistics:
      cbUsed = m_ndisRequest.DATA.QUERY_INFORMATION.BytesWritten;
      cbRequired = m_ndisRequest.DATA.QUERY_INFORMATION.BytesNeeded;
      cbReturn = cbUsed;
      break;
   case NdisRequestSetInformation:
      cbUsed = m_ndisRequest.DATA.SET_INFORMATION.BytesRead;
      cbRequired = m_ndisRequest.DATA.SET_INFORMATION.BytesNeeded;
      cbReturn = 0;
      break;
   }
   return MarshalParameters(
      ppvBuffer, pcbBuffer, NDT_MARSHAL_OUT_REQUEST, m_status, cbUsed,
      cbRequired, cbReturn, m_pucOutBuffer
   );
}

//------------------------------------------------------------------------------
