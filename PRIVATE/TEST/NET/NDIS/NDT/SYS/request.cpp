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
#include "Binding.h"
#include "Request.h"
#include "Marshal.h"
#include "Log.h"

//------------------------------------------------------------------------------

CRequest::CRequest(NDT_ENUM_REQUEST_TYPE eType, CBinding *pBinding)
{
   m_eType = eType;
   if (pBinding != NULL) {
      m_bInternal = TRUE;
      m_pBinding = pBinding; 
      pBinding->AddRef();
      NdisInitializeEvent(&m_hEvent);
   } else {
      m_bInternal = FALSE;
      m_pBinding = NULL;
   }
   m_pvComplete = NULL;
}

//------------------------------------------------------------------------------

CRequest::~CRequest()
{
   if (m_pBinding != NULL) m_pBinding->Release();
   if (m_bInternal) NdisFreeEvent(&m_hEvent);
}

//------------------------------------------------------------------------------

void CRequest::InitExt(
   PVOID pvComplete, PVOID pvBufferOut, DWORD cbBufferOut, 
   DWORD *pcbActualOut
)
{
   m_pvComplete = pvComplete;
   m_pvBufferOut = pvBufferOut;
   m_cbBufferOut = cbBufferOut;
   m_pcbActualOut = pcbActualOut;

   // Save original buffer size
   *m_pcbActualOut = cbBufferOut;
}

//------------------------------------------------------------------------------

void CRequest::Complete()
{
   // If request is an external marshal output data to buffer
   if (!m_bInternal) {
      NDIS_STATUS status = MarshalOutParams(&m_pvBufferOut, &m_cbBufferOut);
      *m_pcbActualOut -= m_cbBufferOut;
      NDTCompleteRequest(m_pvComplete); m_pvComplete = NULL;
   } else {
      NdisSetEvent(&m_hEvent);
   }
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequest::WaitForComplete(DWORD dwTimeout)
{
   ASSERT(m_bInternal);
   return (
      NdisWaitEvent(&m_hEvent, dwTimeout) ? 
      NDIS_STATUS_SUCCESS : NDIS_STATUS_FAILURE
   );
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequest::InternalExecute(DWORD dwTimeout)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;

   status = Execute();
   if (status == NDIS_STATUS_PENDING) {
      status = WaitForComplete(dwTimeout);
	  if (status == NDIS_STATUS_SUCCESS) status = m_status;
   }
   NdisResetEvent(&m_hEvent);
   return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequest::UnmarshalInpParams(LPVOID *ppvBuffer, DWORD *pcbBuffer)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   
   status = UnmarshalParameters(
      ppvBuffer, pcbBuffer, NDT_MARSHAL_INP_DEFAULT, &m_pBinding
   );
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;
   
   // We save an pointer to object (ok, not to clear but...)
   m_pBinding->AddRef();
   
cleanUp:
   return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequest::MarshalOutParams(LPVOID *ppvBuffer, DWORD *pcbBuffer)
{
   return MarshalParameters(
      ppvBuffer, pcbBuffer, NDT_MARSHAL_OUT_DEFAULT, m_status
   );
}

//------------------------------------------------------------------------------
