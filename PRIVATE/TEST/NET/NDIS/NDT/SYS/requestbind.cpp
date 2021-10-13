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
#include "Driver.h"
#include "Protocol.h"
#include "Binding.h"
#include "RequestBind.h"
#include "Medium802_3.h"
#include "Marshal.h"
#include "Log.h"

//------------------------------------------------------------------------------

CRequestBind::CRequestBind(CDriver *pDriver, CBinding *pBinding) : 
   CRequest(NDT_REQUEST_BIND, pBinding)
{
   m_dwMagic = NDT_MAGIC_REQUEST_BIND;

   if (pDriver != NULL) {
      m_pDriver = pDriver; 
      m_pDriver->AddRef();
   }
   m_statusOpenError = NDIS_STATUS_SUCCESS;
   m_uiSelectedMediaIndex = 0;

   m_uiUseNDIS40 = 0;
   m_uiMediumArraySize = 0;
   m_aMedium = NULL;
   NdisInitUnicodeString(&m_nsAdapterName, NULL);
   m_uiOpenOptions = 0;
}   

//------------------------------------------------------------------------------

CRequestBind::~CRequestBind()
{
   if (m_nsAdapterName.Buffer != NULL) NdisFreeString(m_nsAdapterName);
   delete m_aMedium;
   if (m_pDriver != NULL) m_pDriver->Release();
}

//------------------------------------------------------------------------------

void CRequestBind::Complete()
{
   if (
      m_status == NDIS_STATUS_SUCCESS && 
      m_statusOpenError == NDIS_STATUS_SUCCESS
   ) {
      m_pBinding->OpenAdapter(m_aMedium[m_uiSelectedMediaIndex]);
   }
   CRequest::Complete();   
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestBind::Execute()
{
   CProtocol *pProtocol = NULL;
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;

   // When there is no binding let create it
   if (m_pBinding == NULL) {
      ASSERT(m_pDriver != NULL);
      pProtocol =  m_pDriver->m_pProtocol;
      if (m_uiUseNDIS40) pProtocol = m_pDriver->m_pProtocol40;
      m_pBinding = new CBinding(pProtocol);
   }
   
   // Add request to pending queue
   m_pBinding->AddRequestToList(this);

   // We need to add a reference because NDIS will hold a pointer
   m_pBinding->AddRef();
   
   // Execute command
   NdisOpenAdapter(
      &status, &m_statusOpenError, &m_pBinding->m_hAdapter, 
      &m_uiSelectedMediaIndex, m_aMedium, m_uiMediumArraySize, 
      m_pBinding->m_pProtocol->m_hProtocol, m_pBinding,
      &m_nsAdapterName, m_uiOpenOptions, NULL
   );

   // If open isn't pending remove it and complete
   if (status != NDIS_STATUS_PENDING) {
      m_status = status;
      // If command failed reference isn't hold anymore
      if (status != NDIS_STATUS_SUCCESS) m_pBinding->Release();
      m_pBinding->RemoveRequestFromList(this);
      Complete();
   }
   
   // And return
   return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestBind::UnmarshalInpParams(
   LPVOID *ppvBuffer, DWORD *pcbBuffer
)
{
   return UnmarshalParameters(
      ppvBuffer, pcbBuffer, NDT_MARSHAL_INP_BIND, &m_uiUseNDIS40,
      &m_uiMediumArraySize, &m_aMedium, &m_nsAdapterName, &m_uiOpenOptions
   );
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestBind::MarshalOutParams(
   LPVOID *ppvBuffer, DWORD *pcbBuffer
)
{
   return MarshalParameters(
      ppvBuffer, pcbBuffer, NDT_MARSHAL_OUT_BIND, m_status, m_statusOpenError,
      m_uiSelectedMediaIndex, m_pBinding
   );
}

//------------------------------------------------------------------------------
