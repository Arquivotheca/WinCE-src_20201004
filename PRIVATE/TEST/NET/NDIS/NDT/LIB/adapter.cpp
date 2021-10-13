//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "StdAfx.h"
#include "NDT_Request.h"
#include "ndterror.h"
#include "Driver.h"
#include "Adapter.h"
#include "Request.h"
#include "Marshal.h"
#include "Utils.h"

//------------------------------------------------------------------------------

#define NDT_SYNC_REQUEST_TIMEOUT    30000

//------------------------------------------------------------------------------

CAdapter::CAdapter()
{
   m_dwMagic = NDT_MAGIC_ADAPTER;
   m_pDriver = NULL;
   m_hBinding = NULL;
   m_szAdapter = NULL;
   m_szSystem = NULL;
}

//------------------------------------------------------------------------------

CAdapter::~CAdapter()
{
   // Make sure that all will be closed
   Unbind();
   delete m_szAdapter;
   delete m_szSystem;
}

//------------------------------------------------------------------------------

HRESULT CAdapter::Open(LPTSTR szAdapter)
{
   HRESULT hr = S_OK;
   LPTSTR pcPos = NULL;

   // Divide adapter name on a local name and a system name
   pcPos = lstrchr(szAdapter, _T('@'));
   if (pcPos != NULL) {
      *pcPos = _T('\0');
      m_szAdapter = lstrdup(szAdapter);
      m_szSystem = lstrdup(pcPos + 1);
      *pcPos = _T('@');
   } else {
      m_szAdapter = lstrdup(szAdapter);
   }
   
   // Find an existing or open a new connection
   m_pDriver = FindDriver(m_szSystem);
   if (m_pDriver == NULL) {
      hr = E_FAIL;
      goto cleanUp;
   }

   // TODO: Check if adapter really exists on target system

cleanUp:
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CAdapter::Close()
{
   HRESULT hr = S_OK;

   if (m_pDriver != NULL) {
      // If there is an open adapter close it
      if (m_hBinding != NULL) Unbind();
      // Release a device 
      m_pDriver->Release();
      m_pDriver = NULL;
   }
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CAdapter::LoadDriver()
{
   return m_pDriver->LoadAdapter(m_szAdapter);
}

//------------------------------------------------------------------------------

HRESULT CAdapter::UnloadDriver()
{
   return m_pDriver->UnloadAdapter(m_szAdapter);
}

//------------------------------------------------------------------------------

HRESULT CAdapter::BindProtocol(LPCTSTR szProtocol)
{
   return m_pDriver->BindProtocol(m_szAdapter, szProtocol);
}

//------------------------------------------------------------------------------

HRESULT CAdapter::UnbindProtocol(LPCTSTR szProtocol)
{
   return m_pDriver->UnbindProtocol(m_szAdapter, szProtocol);
}

//------------------------------------------------------------------------------

HRESULT CAdapter::WriteVerifyFlag(DWORD dwFlag)
{
   return m_pDriver->WriteVerifyFlag(m_szAdapter, dwFlag);
}

//------------------------------------------------------------------------------

HRESULT CAdapter::DeleteVerifyFlag()
{
   return m_pDriver->DeleteVerifyFlag(m_szAdapter);
}

//------------------------------------------------------------------------------

HRESULT CAdapter::SetOptions(DWORD dwZoneMask, DWORD dwBeatDelay)
{
   HRESULT hr = S_OK;
   HRESULT hrRequest = S_OK;
   CRequest* pRequest = NULL;
   
   // Check a binding status
   if (m_pDriver == NULL) {
      hr = NDT_STATUS_INVALID_STATE;
      goto cleanUp;
   }

   pRequest = new CRequest(m_pDriver);
   if (pRequest == NULL) {
      hr = E_OUTOFMEMORY;
      goto cleanUp;
   }
   
   // Marshal input parameters
   hr = pRequest->MarshalInpParameters(
      NDT_MARSHAL_INP_SET_OPTIONS, dwZoneMask, dwBeatDelay
   );
   if (FAILED(hr)) goto cleanUp;

   // Execute request
   hr = pRequest->Execute(NDT_SYNC_REQUEST_TIMEOUT, NDT_REQUEST_SET_OPTIONS);
   if (FAILED(hr)) goto cleanUp;

   // Unmarshal output parameters
   hr = pRequest->UnmarshalOutParameters(
      NDT_MARSHAL_OUT_SET_OPTIONS, &hrRequest
   );
   if (FAILED(hr)) goto cleanUp;

   // So final return code is open error status
   hr = hrRequest;

cleanUp:
   if (pRequest != NULL) pRequest->Release();
   return hr;
   
}

//------------------------------------------------------------------------------

HRESULT CAdapter::Bind(
   BOOL bForce40, NDIS_MEDIUM ndisMedium, NDIS_MEDIUM* pNdisMedium
)
{
   HRESULT hr = S_OK;
   HRESULT hrRequest = S_OK;
   HRESULT hrOpenError = S_OK;
   CRequest* pRequest = NULL;
   UINT uiIndex = 0;
   NDIS_MEDIUM aNdisMedium[] = {
      NdisMedium802_3, NdisMedium802_5, NdisMediumFddi, NdisMediumWan,
      NdisMediumLocalTalk, NdisMediumDix, NdisMediumArcnetRaw,
      NdisMediumArcnet878_2, NdisMediumAtm, NdisMediumWirelessWan,
      NdisMediumIrda, NdisMediumBpc, NdisMediumCoWan, NdisMedium1394,
   };
   NDIS_MEDIUM* pMedium = NULL;
   UINT uiMedium = 0;
   
   // Check a binding status
   if (m_pDriver == NULL || m_hBinding != NULL) {
      hr = NDT_STATUS_INVALID_STATE;
      goto cleanUp;
   }

   pRequest = new CRequest(m_pDriver);
   if (pRequest == NULL) {
      hr = E_OUTOFMEMORY;
      goto cleanUp;
   }
   
   if (ndisMedium != -1) {
      pMedium = &ndisMedium;
      uiMedium = 1;
   } else {
      pMedium = aNdisMedium;
      uiMedium = sizeof(aNdisMedium)/sizeof(NDIS_MEDIUM);
   }
      
   // Marshal input parameters
   hr = pRequest->MarshalInpParameters(
      NDT_MARSHAL_INP_BIND, bForce40 ? 1 : 0, uiMedium, pMedium, 
      m_szAdapter, 0
   );
   if (FAILED(hr)) goto cleanUp;

   // Execute request
   hr = pRequest->Execute(NDT_SYNC_REQUEST_TIMEOUT, NDT_REQUEST_BIND);
   if (FAILED(hr)) goto cleanUp;

   // Unmarshal output parameters
   hr = pRequest->UnmarshalOutParameters(
      NDT_MARSHAL_OUT_BIND, &hrRequest, &hrOpenError, &uiIndex, &m_hBinding
   );
   if (FAILED(hr)) goto cleanUp;

   // When we get an error on command use it as the return value
   hr = hrRequest;
   if (FAILED(hr)) goto cleanUp;

   // So final return code is open error status
   hr = hrOpenError;

   // Return prefered medium
   if (pNdisMedium != NULL) *pNdisMedium = pMedium[uiIndex];
   
cleanUp:
   if (pRequest != NULL) pRequest->Release();
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CAdapter::Unbind()
{
   HRESULT hr = S_OK;
   HRESULT hrRequest = S_OK;
   CRequest* pRequest = NULL;
   CObject* pObject = NULL;

   // Check a binding status
   if (m_pDriver == NULL || m_hBinding == NULL) {
      hr = NDT_STATUS_INVALID_STATE;
      goto cleanUp;
   }


   // We have to close all pending requests
   while ((pRequest = (CRequest*)m_listRequests.GetHead()) != NULL) {
      pRequest->Stop();
      m_listRequests.Remove(pRequest);
      pRequest->Release();
   }
   
   pRequest = new CRequest(m_pDriver);
   if (pRequest == NULL) {
      hr = E_OUTOFMEMORY;
      goto cleanUp;
   }

   // Marshal input parameters
   hr = pRequest->MarshalInpParameters(NDT_MARSHAL_INP_UNBIND, m_hBinding);
   if (FAILED(hr)) goto cleanUp;

   // Execute request
   hr = pRequest->Execute(NDT_SYNC_REQUEST_TIMEOUT, NDT_REQUEST_UNBIND);
   if (FAILED(hr)) goto cleanUp;

   // Unmarshal output parameters
   hr = pRequest->UnmarshalOutParameters(NDT_MARSHAL_OUT_UNBIND, &hrRequest);
   if (FAILED(hr)) goto cleanUp;

   // When we get an error on command use it as the return value
   hr = hrRequest;
     
cleanUp:
   // But we close a local handler even close fails...
   m_hBinding = NULL;
   if (pRequest != NULL) pRequest->Release();
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CAdapter::Reset()
{
   HRESULT hr = S_OK;
   HRESULT hrRequest = S_OK;
   CRequest* pRequest = NULL;
   
   // Check a binding status
   if (m_pDriver == NULL || m_hBinding == NULL) {
      hr = NDT_STATUS_INVALID_STATE;
      goto cleanUp;
   }
   
   pRequest = new CRequest(m_pDriver);
   if (pRequest == NULL) {
      hr = E_OUTOFMEMORY;
      goto cleanUp;
   }

   // Marshal input parameters
   hr = pRequest->MarshalInpParameters(NDT_MARSHAL_INP_RESET, m_hBinding);
   if (FAILED(hr)) goto cleanUp;

   // Execute request
   hr = pRequest->Execute(NDT_SYNC_REQUEST_TIMEOUT, NDT_REQUEST_RESET);
   if (FAILED(hr)) goto cleanUp;

   // Unmarshal output parameters
   hr = pRequest->UnmarshalOutParameters(NDT_MARSHAL_OUT_RESET, &hrRequest);
   if (FAILED(hr)) goto cleanUp;

   // When we get an error on command use it as the return value
   hr = hrRequest;
   
cleanUp:
   if (pRequest != NULL) pRequest->Release();
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CAdapter::GetCounter(ULONG nIndex, ULONG* pnValue)
{
   HRESULT hr = S_OK;
   HRESULT hrRequest = S_OK;
   CRequest* pRequest = NULL;
   
   // Check a binding status
   if (m_pDriver == NULL || m_hBinding == NULL) {
      hr = NDT_STATUS_INVALID_STATE;
      goto cleanUp;
   }

   pRequest = new CRequest(m_pDriver);
   if (pRequest == NULL) {
      hr = E_OUTOFMEMORY;
      goto cleanUp;
   }
   
   // Marshal input parameters
   hr = pRequest->MarshalInpParameters(
      NDT_MARSHAL_INP_GET_COUNTER, m_hBinding, nIndex
   );
   if (FAILED(hr)) goto cleanUp;

   // Execute request
   hr = pRequest->Execute(NDT_SYNC_REQUEST_TIMEOUT, NDT_REQUEST_GET_COUNTER);
   if (FAILED(hr)) goto cleanUp;

   // Unmarshal output parameters
   hr = pRequest->UnmarshalOutParameters(NDT_MARSHAL_OUT_GET_COUNTER, pnValue);
   if (FAILED(hr)) goto cleanUp;

cleanUp:
   if (pRequest != NULL) pRequest->Release();
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CAdapter::QueryInfo(
   NDIS_OID ndisOid, PVOID pvBuffer, UINT cbBuffer, UINT* pcbUsed, 
   UINT* pcbRequired
)
{
   HRESULT hr = S_OK;
   HRESULT hrRequest = S_OK;
   CRequest* pRequest = NULL;
   UINT cbReturned = 0;
   UINT cbUsed = 0;
   UINT cbRequired = 0;
   UCHAR* pucBuffer = NULL;
   
   
   // Check a binding status
   if (m_pDriver == NULL || m_hBinding == NULL) {
      hr = NDT_STATUS_INVALID_STATE;
      goto cleanUp;
   }

   pRequest = new CRequest(m_pDriver);
   if (pRequest == NULL) {
      hr = E_OUTOFMEMORY;
      goto cleanUp;
   }
   
   // Marshal input parameters
   hr = pRequest->MarshalInpParameters(
      NDT_MARSHAL_INP_REQUEST, m_hBinding, NdisRequestQueryInformation,
      ndisOid, 0, NULL, cbBuffer
   );
   if (FAILED(hr)) goto cleanUp;

   // Execute request
   hr = pRequest->Execute(NDT_SYNC_REQUEST_TIMEOUT, NDT_REQUEST_REQUEST);
   if (FAILED(hr)) goto cleanUp;

   // Unmarshal output parameters
   hr = pRequest->UnmarshalOutParameters(
      NDT_MARSHAL_OUT_REQUEST, &hrRequest, &cbUsed, &cbRequired, &cbReturned, 
      &pucBuffer
   );
   if (FAILED(hr)) goto cleanUp;
   
   if (cbReturned > cbBuffer) {
      if (SUCCEEDED(hrRequest)) {
         hr = NDT_STATUS_BUFFER_OVERFLOW;
         goto cleanUp;
      }
      cbReturned = 0;
   }
   
   if (cbReturned > 0) memcpy(pvBuffer, pucBuffer, cbReturned);
   
   // When we get an error on command use it as the return value
   hr = hrRequest;
   
cleanUp:
   if (pcbUsed != NULL) *pcbUsed = cbUsed;
   if (pcbRequired != NULL) *pcbRequired = cbRequired;
   if (pRequest != NULL) pRequest->Release();
   delete pucBuffer;
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CAdapter::SetInfo(
   NDIS_OID ndisOid, PVOID pvBuffer, UINT cbBuffer, UINT* pcbUsed, 
   UINT* pcbRequired
)
{
   HRESULT hr = S_OK;
   HRESULT hrRequest = S_OK;
   CRequest* pRequest = NULL;
   UINT cbReturned = 0;
   UINT cbUsed = 0;
   UINT cbRequired = 0;
   UCHAR* pucBuffer = NULL;
   
   
   // Check a binding status
   if (m_pDriver == NULL || m_hBinding == NULL) {
      hr = NDT_STATUS_INVALID_STATE;
      goto cleanUp;
   }

   pRequest = new CRequest(m_pDriver);
   if (pRequest == NULL) {
      hr = E_OUTOFMEMORY;
      goto cleanUp;
   }
   
   // Marshal input parameters
   hr = pRequest->MarshalInpParameters(
      NDT_MARSHAL_INP_REQUEST, m_hBinding, NdisRequestSetInformation,
      ndisOid, cbBuffer, pvBuffer, 0
   );
   if (FAILED(hr)) goto cleanUp;

   // Execute request
   hr = pRequest->Execute(NDT_SYNC_REQUEST_TIMEOUT, NDT_REQUEST_REQUEST);
   if (FAILED(hr)) goto cleanUp;

   // Unmarshal output parameters
   hr = pRequest->UnmarshalOutParameters(
      NDT_MARSHAL_OUT_REQUEST, &hrRequest, &cbUsed, &cbRequired, &cbReturned, 
      &pucBuffer
   );
   if (FAILED(hr)) goto cleanUp;
   
   ASSERT(cbReturned == 0);

   // When we get an error on command use it as the return value
   hr = hrRequest;
   
cleanUp:
   if (pcbUsed != NULL) *pcbUsed = cbUsed;
   if (pcbRequired != NULL) *pcbRequired = cbRequired;
   if (pRequest != NULL) pRequest->Release();
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CAdapter::Send(
   ULONG cbAddr, BYTE* pucSrcAddr, ULONG nDestAddrs, BYTE* apucDestAddrs[], 
   BYTE ucResponseMode, BYTE ucPacketSizeMode, ULONG ulPacketSize, 
   ULONG ulPacketCount, UINT uiBeatDelay, UINT uiBeatGroup, 
   CRequest*& pRequest
)
{
   HRESULT hr = S_OK;
   HRESULT hrRequest = S_OK;
   
   // Check a binding status
   if (m_pDriver == NULL || m_hBinding == NULL) {
      hr = NDT_STATUS_INVALID_STATE;
      goto cleanUp;
   }
   
   pRequest = new CRequest(m_pDriver);
   if (pRequest == NULL) {
      hr = E_OUTOFMEMORY;
      goto cleanUp;
   }

   // Marshal input parameters
   hr = pRequest->MarshalInpParameters(
      NDT_MARSHAL_INP_SEND, m_hBinding, cbAddr, pucSrcAddr, nDestAddrs, 
      cbAddr, apucDestAddrs, ucResponseMode, ucPacketSizeMode, ulPacketSize, 
      ulPacketCount, uiBeatDelay, uiBeatGroup
   );
   if (FAILED(hr)) goto cleanUp;

   // Execute request (async)
   hr = pRequest->Execute(0, NDT_REQUEST_SEND);
   if (FAILED(hr)) goto cleanUp;

   // Add request to list of pendings
   m_listRequests.AddTail(pRequest);

   return hr;
   
cleanUp:
   if (pRequest != NULL) pRequest->Release();
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CAdapter::SendWait(
   CRequest* pRequest, DWORD dwTimeout, ULONG* pulPacketsSent,
   ULONG* pulPacketsCompleted, ULONG* pulPacketsCanceled, 
   ULONG* pulPacketsUncanceled, ULONG* pulPacketsReplied, ULONG *pulTime, 
   ULONG *pulSend, ULONG *pulReceive
)
{
   HRESULT hr = S_OK;
   HRESULT hrRequest = S_OK;
   ULONG ulPacketsSent = 0;
   ULONG ulPacketsCompleted = 0;
   ULONG ulPacketsCanceled = 0;
   ULONG ulPacketsUncanceled = 0;
   ULONG ulPacketsReplied = 0;
   ULONG ulTime = 0;
   ULONG ulSend = 0;
   ULONG ulReceive = 0;

   // Check parameters
   if (pRequest == NULL || pRequest->m_dwMagic != NDT_MAGIC_REQUEST) {
      hr = NDT_STATUS_INVALID_STATE;
      goto cleanUp;
   }

   // Wait for complete
   hr = pRequest->WaitForComplete(dwTimeout);
   if (FAILED(hr)) goto cleanUp;
   
   // Unmarshal output parameters
   hr = pRequest->UnmarshalOutParameters(
      NDT_MARSHAL_OUT_SEND, &hrRequest, &ulPacketsSent, &ulPacketsCompleted, 
      &ulPacketsCanceled, &ulPacketsUncanceled, &ulPacketsReplied, &ulTime, 
      &ulSend, &ulReceive
   );
   if (FAILED(hr)) goto cleanUp;

   // Remove request from list of pendings
   m_listRequests.Remove(pRequest);
   
   // We don't need request anymore
   pRequest->Release();

   // When we get an error on command use it as the return value
   hr = hrRequest;
   
cleanUp:
   if (pRequest != NULL) pRequest->Release();
   if (pulPacketsSent != NULL) *pulPacketsSent = ulPacketsSent;
   if (pulPacketsCompleted != NULL) *pulPacketsCompleted = ulPacketsCompleted;
   if (pulPacketsCanceled != NULL) *pulPacketsCanceled = ulPacketsCanceled;
   if (pulPacketsUncanceled != NULL) *pulPacketsUncanceled = ulPacketsUncanceled;
   if (ulPacketsReplied != NULL) *pulPacketsReplied = ulPacketsReplied;
   if (ulTime != NULL) *pulTime = ulTime;
   if (ulSend != NULL) *pulSend = ulSend;
   if (ulReceive != NULL) *pulReceive = ulReceive;
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CAdapter::Receive(CRequest*& pRequest)
{
   HRESULT hr = S_OK;
   
   // Check a binding status
   if (m_pDriver == NULL || m_hBinding == NULL) {
      hr = NDT_STATUS_INVALID_STATE;
      goto cleanUp;
   }
   
   pRequest = new CRequest(m_pDriver);
   if (pRequest == NULL) {
      hr = E_OUTOFMEMORY;
      goto cleanUp;
   }

   // Marshal input parameters
   hr = pRequest->MarshalInpParameters(NDT_MARSHAL_INP_RECEIVE, m_hBinding);
   if (FAILED(hr)) goto cleanUp;

   // Execute request (async)
   hr = pRequest->Execute(0, NDT_REQUEST_RECEIVE);
   if (FAILED(hr)) goto cleanUp;

   // Add request to list of pendings
   m_listRequests.AddTail(pRequest);

   return hr;
   
cleanUp:
   if (pRequest != NULL) pRequest->Release();
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CAdapter::ReceiveStop(
   CRequest* pRequest, ULONG* pulPacketsReceived, ULONG* pulPacketsReplied, 
   ULONG* pulPacketsCompleted, ULONG* pulTime, ULONG *pulSent, 
   ULONG *pulReceived
)
{
   HRESULT hr = S_OK;
   HRESULT hrRequest = S_OK;
   CRequest* pRequestStop = NULL;
   ULONG ulPacketsReceived = 0;
   ULONG ulPacketsReplied = 0;
   ULONG ulPacketsCompleted = 0;
   ULONG ulTime = 0;
   ULONG ulSent = 0;
   ULONG ulReceived = 0;
   
   // Check a binding status
   if (m_pDriver == NULL || m_hBinding == NULL) {
      hr = NDT_STATUS_INVALID_STATE;
      goto cleanUp;
   }
   
   pRequestStop = new CRequest(m_pDriver);
   if (pRequestStop == NULL) {
      hr = E_OUTOFMEMORY;
      goto cleanUp;
   }

   // Marshal input parameters
   hr = pRequestStop->MarshalInpParameters(
      NDT_MARSHAL_INP_RECEIVE_STOP, m_hBinding
   );
   if (FAILED(hr)) goto cleanUp;

   // Execute request (async)
   hr = pRequestStop->Execute(
      NDT_SYNC_REQUEST_TIMEOUT, NDT_REQUEST_RECEIVE_STOP
   );
   if (FAILED(hr)) goto cleanUp;

   // Wait for complete
   hr = pRequest->WaitForComplete(NDT_SYNC_REQUEST_TIMEOUT);
   if (FAILED(hr)) goto cleanUp;
   
   // Unmarshal output parameters
   hr = pRequest->UnmarshalOutParameters(
      NDT_MARSHAL_OUT_RECEIVE, &hrRequest, &ulPacketsReceived, 
      &ulPacketsReplied, &ulPacketsCompleted, &ulTime, &ulSent, &ulReceived
   );
   if (FAILED(hr)) goto cleanUp;

   // When we get an error on command use it as the return value
   hr = hrRequest;
   
cleanUp:
   if (pRequestStop != NULL) pRequestStop->Release();
   if (pRequest != NULL) {
      // Remove request from list of pendings
      m_listRequests.Remove(pRequest);
      pRequest->Release();
   }
   if (pulPacketsReceived != NULL) *pulPacketsReceived = ulPacketsReceived;
   if (pulPacketsReplied != NULL) *pulPacketsReplied = ulPacketsReplied;
   if (pulPacketsCompleted != NULL) *pulPacketsCompleted = ulPacketsCompleted;
   if (pulTime != NULL) *pulTime = ulTime;
   if (pulSent != NULL) *pulSent = ulSent;
   if (pulReceived != NULL) *pulReceived = ulReceived;
   return hr;
}

//------------------------------------------------------------------------------

HRESULT CAdapter::SetId(USHORT usLocalId, USHORT usRemoteId)
{
   HRESULT hr = S_OK;
   HRESULT hrRequest = S_OK;
   CRequest* pRequest = NULL;
   
   // Check a binding status
   if (m_pDriver == NULL || m_hBinding == NULL) {
      hr = NDT_STATUS_INVALID_STATE;
      goto cleanUp;
   }

   pRequest = new CRequest(m_pDriver);
   if (pRequest == NULL) {
      hr = E_OUTOFMEMORY;
      goto cleanUp;
   }
   
   // Marshal input parameters
   hr = pRequest->MarshalInpParameters(
      NDT_MARSHAL_INP_SET_ID, m_hBinding, usLocalId, usRemoteId
   );
   if (FAILED(hr)) goto cleanUp;

   // Execute request
   hr = pRequest->Execute(NDT_SYNC_REQUEST_TIMEOUT, NDT_REQUEST_SET_ID);
   if (FAILED(hr)) goto cleanUp;

   // Unmarshal output parameters
   hr = pRequest->UnmarshalOutParameters(NDT_MARSHAL_OUT_SET_ID, &hrRequest);
   if (FAILED(hr)) goto cleanUp;

   // So final return code is open error status
   hr = hrRequest;
     
cleanUp:
   if (pRequest != NULL) pRequest->Release();
   return hr;
}

//------------------------------------------------------------------------------
