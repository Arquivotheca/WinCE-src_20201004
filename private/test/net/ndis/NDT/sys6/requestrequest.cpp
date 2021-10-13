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
   NdisZeroMemory(&m_ndisOidRequest, sizeof(m_ndisOidRequest));
}

//------------------------------------------------------------------------------

void CRequestRequest::Complete()
{
   if ((m_status == NDIS_STATUS_SUCCESS) && (m_requestType == NdisRequestMethod))
   {
       //For method type, the input bufffer is the output buffer
       //Therefore copy the input buffer's contents into the output buffer
    NdisMoveMemory(m_pucOutBuffer, m_pucInpBuffer, m_ndisOidRequest.DATA.METHOD_INFORMATION.BytesWritten);
   }

   CRequest::Complete();   
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
      
   m_ndisOidRequest.Header.Type = NDIS_OBJECT_TYPE_OID_REQUEST;
   m_ndisOidRequest.Header.Revision = NDIS_OID_REQUEST_REVISION_1;
   m_ndisOidRequest.Header.Size = sizeof(NDIS_OID_REQUEST);

   m_ndisOidRequest.RequestType = m_requestType;

   m_ndisOidRequest.Timeout = 0;
   //TODO: Setting null values for request and requesthandle since we dont need delayed indications
   //revisit if we need them: We dont need them for a 5.1 miniport since it will never return
   //NDIS_STATUS_INDICATION
   m_ndisOidRequest.RequestId = 0;
   m_ndisOidRequest.RequestHandle = NULL;

   switch (m_requestType) {
   case NdisRequestQueryInformation:
   case NdisRequestQueryStatistics:
      m_ndisOidRequest.DATA.QUERY_INFORMATION.Oid = m_oid;
      m_ndisOidRequest.DATA.QUERY_INFORMATION.InformationBuffer = m_pucOutBuffer;
      m_ndisOidRequest.DATA.QUERY_INFORMATION.InformationBufferLength = m_cbOutBuffer;
      m_ndisOidRequest.DATA.QUERY_INFORMATION.BytesWritten = 0;
      m_ndisOidRequest.DATA.QUERY_INFORMATION.BytesNeeded = 0;
      break;
   case NdisRequestSetInformation:
      m_ndisOidRequest.DATA.SET_INFORMATION.Oid = m_oid;
      m_ndisOidRequest.DATA.SET_INFORMATION.InformationBuffer = m_pucInpBuffer;
      m_ndisOidRequest.DATA.SET_INFORMATION.InformationBufferLength = m_cbInpBuffer;
      m_ndisOidRequest.DATA.SET_INFORMATION.BytesRead = 0;
      m_ndisOidRequest.DATA.SET_INFORMATION.BytesNeeded = 0;
      break;
  case NdisRequestMethod:
      m_ndisOidRequest.DATA.METHOD_INFORMATION.Oid = m_oid;
    m_ndisOidRequest.DATA.METHOD_INFORMATION.InformationBuffer = m_pucInpBuffer;
    m_ndisOidRequest.DATA.METHOD_INFORMATION.InputBufferLength = m_cbMethodInpBuffer;
    m_ndisOidRequest.DATA.METHOD_INFORMATION.OutputBufferLength = m_cbMethodOutBuffer;
    m_ndisOidRequest.DATA.METHOD_INFORMATION.MethodId = m_ulMethodId;
    m_ndisOidRequest.DATA.METHOD_INFORMATION.BytesWritten = 0;
       m_ndisOidRequest.DATA.METHOD_INFORMATION.BytesRead = 0;
       m_ndisOidRequest.DATA.METHOD_INFORMATION.BytesNeeded = 0;
   }

   // Add request to pending queue
   m_pBinding->AddRequestToList(this);

   // Execute command
   status = NdisOidRequest(m_pBinding->m_hAdapter, &m_ndisOidRequest);

   // If request isn't pending remove it and complete
   //TODO: If we ever run against a 6.0 miniport we must consider NDIS_STATUS_INDICATION
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

VOID CRequestRequest::SetRequestCompletionEvent()
{
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestRequest::UnmarshalInpParams(
   PVOID *ppvBuffer, DWORD *pcbBuffer
)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   
   status = UnmarshalParameters(
      ppvBuffer, pcbBuffer, NDT_MARSHAL_INP_REQUEST, &m_pBinding, 
      &m_requestType, &m_oid, &m_cbInpBuffer, &m_pucInpBuffer, &m_cbOutBuffer, 
      &m_cbMethodInpBuffer, &m_cbMethodOutBuffer, &m_ulMethodId
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
      cbUsed = m_ndisOidRequest.DATA.QUERY_INFORMATION.BytesWritten;
      cbRequired = m_ndisOidRequest.DATA.QUERY_INFORMATION.BytesNeeded;
      cbReturn = cbUsed;
      break;
   case NdisRequestSetInformation:
      cbUsed = m_ndisOidRequest.DATA.SET_INFORMATION.BytesRead;
      cbRequired = m_ndisOidRequest.DATA.SET_INFORMATION.BytesNeeded;
      cbReturn = 0;
      break;
   case NdisRequestMethod:
      cbUsed = m_ndisOidRequest.DATA.METHOD_INFORMATION.BytesRead;
      cbRequired = 0;
      cbReturn = m_ndisOidRequest.DATA.METHOD_INFORMATION.BytesWritten;
      break;
       
   }
   return MarshalParameters(
      ppvBuffer, pcbBuffer, NDT_MARSHAL_OUT_REQUEST, m_status, cbUsed,
      cbRequired, cbReturn, m_pucOutBuffer
   );
}

//------------------------------------------------------------------------------
