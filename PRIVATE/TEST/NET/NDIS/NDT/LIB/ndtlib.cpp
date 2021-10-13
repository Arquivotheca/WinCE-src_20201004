//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "StdAfx.h"
#include "NDTLib.h"
#include "Driver.h"
#include "Adapter.h"
#include "Request.h"
#include "Messages.h"
#include "Utils.h"
#include "NDTError.h"
#include "NDTLog.h"

//------------------------------------------------------------------------------

LPTSTR NDTSystem(LPCTSTR szAdapter)
{
   LPTSTR pcPos = NULL;

   pcPos = lstrchr(szAdapter, _T('@'));
   if (pcPos != NULL) pcPos += 1;
   return pcPos;
}

//------------------------------------------------------------------------------

HRESULT NDTStartup(LPCTSTR szSystem)
{
   return OpenDriver(szSystem);
}

//------------------------------------------------------------------------------

HRESULT NDTCleanup(LPCTSTR szSystem)
{
   return CloseDriver(szSystem);
}

//------------------------------------------------------------------------------

HRESULT NDTQueryAdapters(LPCTSTR szSystem, LPTSTR mszAdapters, DWORD dwSize)
{
   HRESULT hr = S_OK;
   CDriver* pDriver = NULL;

   hr = OpenDriver(szSystem);
   if (FAILED(hr)) goto cleanUp;

   pDriver = FindDriver(szSystem);
   if (pDriver == NULL) {
      hr = E_FAIL;
      goto cleanUp;
   }

   hr = pDriver->QueryAdapters(mszAdapters, dwSize);
   if (FAILED(hr)) goto cleanUp;

cleanUp:
   if (pDriver != NULL) pDriver->Release();
   CloseDriver(szSystem);
   return hr;
}

//------------------------------------------------------------------------------

HRESULT NDTQueryProtocols(LPCTSTR szSystem, LPTSTR mszProtocols, DWORD dwSize)
{
   HRESULT hr = S_OK;
   CDriver* pDriver = NULL;

   hr = OpenDriver(szSystem);
   if (FAILED(hr)) goto cleanUp;

   pDriver = FindDriver(szSystem);
   if (pDriver == NULL) {
      hr = E_FAIL;
      goto cleanUp;
   }

   hr = pDriver->QueryProtocols(mszProtocols, dwSize);
   if (FAILED(hr)) goto cleanUp;

cleanUp:
   if (pDriver != NULL) pDriver->Release();
   CloseDriver(szSystem);
   return hr;
}

//------------------------------------------------------------------------------

HRESULT NDTQueryBindings(
   LPCTSTR szAdapter, LPTSTR mszProtocols, DWORD dwSize
)
{
   HRESULT hr = S_OK;
   CDriver* pDriver = NULL;
   LPTSTR szSystem = NDTSystem(szAdapter);
   
   hr = OpenDriver(szSystem);
   if (FAILED(hr)) goto cleanUp;

   pDriver = FindDriver(szSystem);
   if (pDriver == NULL) {
      hr = E_FAIL;
      goto cleanUp;
   }

   if (szSystem != NULL) *(szSystem - 1) = _T('\0');
   hr = pDriver->QueryBindings(szAdapter, mszProtocols, dwSize);
   if (szSystem != NULL) *(szSystem - 1) = _T('@');
   if (FAILED(hr)) goto cleanUp;
   
cleanUp:
   if (pDriver != NULL) pDriver->Release();
   CloseDriver(szSystem);
   return hr;
}

//------------------------------------------------------------------------------

HRESULT NDTBindProtocol(LPCTSTR szAdapter, LPCTSTR szProtocol)
{
   HRESULT hr = S_OK;
   CDriver* pDriver = NULL;
   LPTSTR szSystem = NDTSystem(szAdapter);
   
   hr = OpenDriver(szSystem);
   if (FAILED(hr)) goto cleanUp;

   pDriver = FindDriver(szSystem);
   if (pDriver == NULL) {
      hr = E_FAIL;
      goto cleanUp;
   }

   if (szSystem != NULL) *(szSystem - 1) = _T('\0');
   hr = pDriver->BindProtocol(szAdapter, szProtocol);
   if (szSystem != NULL) *(szSystem - 1) = _T('@');
   if (FAILED(hr)) goto cleanUp;
   
cleanUp:
   if (pDriver != NULL) pDriver->Release();
   CloseDriver(szSystem);
   return hr;
}

//------------------------------------------------------------------------------

HRESULT NDTUnbindProtocol(LPCTSTR szAdapter, LPCTSTR szProtocol)
{
   HRESULT hr = S_OK;
   CDriver* pDriver = NULL;
   LPTSTR szSystem = NDTSystem(szAdapter);
   
   hr = OpenDriver(szSystem);
   if (FAILED(hr)) goto cleanUp;

   pDriver = FindDriver(szSystem);
   if (pDriver == NULL) {
      hr = E_FAIL;
      goto cleanUp;
   }

   if (szSystem != NULL) *(szSystem - 1) = _T('\0');
   hr = pDriver->UnbindProtocol(szAdapter, szProtocol);
   if (szSystem != NULL) *(szSystem - 1) = _T('@');
   if (FAILED(hr)) goto cleanUp;
   
cleanUp:
   if (pDriver != NULL) pDriver->Release();
   CloseDriver(szSystem);
   return hr;
}

//------------------------------------------------------------------------------

HRESULT NDTOpen(LPTSTR szAdapter, HANDLE* phAdapter)
{
   HRESULT hr = S_OK;
   
   CAdapter* pAdapter = new CAdapter;
   if (pAdapter == NULL) {
      hr = E_OUTOFMEMORY;
      goto cleanUp;
   }
   hr = pAdapter->Open(szAdapter);
   
cleanUp:
   *phAdapter = (HANDLE)pAdapter;
   return hr;
}

//------------------------------------------------------------------------------

HRESULT NDTClose(HANDLE *phAdapter)
{
   HRESULT hr = S_OK;
   
   CAdapter* pAdapter = (CAdapter *)*phAdapter;
   ASSERT(pAdapter->m_dwMagic == NDT_MAGIC_ADAPTER);
   hr = pAdapter->Close();
   pAdapter->Release();
   *phAdapter = NULL;
   return hr;
}

//------------------------------------------------------------------------------

HRESULT NDTLoadMiniport(HANDLE hAdapter)
{
   HRESULT hr = S_OK;
   
   CAdapter* pAdapter = (CAdapter *)hAdapter;
   ASSERT(pAdapter->m_dwMagic == NDT_MAGIC_ADAPTER);
   hr = pAdapter->LoadDriver();
   return hr;
}

//------------------------------------------------------------------------------

HRESULT NDTUnloadMiniport(HANDLE hAdapter)
{
   HRESULT hr = S_OK;
   
   CAdapter* pAdapter = (CAdapter *)hAdapter;
   ASSERT(pAdapter->m_dwMagic == NDT_MAGIC_ADAPTER);
   hr = pAdapter->UnloadDriver();
   return hr;
}

//------------------------------------------------------------------------------

HRESULT NDTWriteVerifyFlag(HANDLE hAdapter, DWORD dwFlag)
{
   HRESULT hr = S_OK;
   
   CAdapter* pAdapter = (CAdapter *)hAdapter;
   ASSERT(pAdapter->m_dwMagic == NDT_MAGIC_ADAPTER);
   hr = pAdapter->WriteVerifyFlag(dwFlag);
   return hr;
}

//------------------------------------------------------------------------------

HRESULT NDTDeleteVerifyFlag(HANDLE hAdapter)
{
   HRESULT hr = S_OK;
   
   CAdapter* pAdapter = (CAdapter *)hAdapter;
   ASSERT(pAdapter->m_dwMagic == NDT_MAGIC_ADAPTER);
   hr = pAdapter->DeleteVerifyFlag();
   return hr;
}

//------------------------------------------------------------------------------

HRESULT NDTBind(
   HANDLE hAdapter, BOOL bForce30, NDIS_MEDIUM ndisMedium, 
   NDIS_MEDIUM* pNdisMedium
)
{
   HRESULT hr = S_OK;
   
   CAdapter* pAdapter = (CAdapter *)hAdapter;
   ASSERT(pAdapter->m_dwMagic == NDT_MAGIC_ADAPTER);
   hr = pAdapter->Bind(bForce30, ndisMedium, pNdisMedium);
   return hr;
}

//------------------------------------------------------------------------------

HRESULT NDTUnbind(HANDLE hAdapter)
{
   HRESULT hr = S_OK;
   
   CAdapter* pAdapter = (CAdapter *)hAdapter;
   ASSERT(pAdapter->m_dwMagic == NDT_MAGIC_ADAPTER);
   hr = pAdapter->Unbind();
   return hr;
}

//------------------------------------------------------------------------------

HRESULT NDTReset(HANDLE hAdapter)
{
   HRESULT hr = S_OK;
   
   CAdapter* pAdapter = (CAdapter *)hAdapter;
   ASSERT(pAdapter->m_dwMagic == NDT_MAGIC_ADAPTER);
   hr = pAdapter->Reset();
   return hr;
}

//------------------------------------------------------------------------------

HRESULT NDTGetCounter(HANDLE hAdapter, ULONG nIndex, ULONG *pnValue)
{
   HRESULT hr = S_OK;
   
   CAdapter* pAdapter = (CAdapter *)hAdapter;
   ASSERT(pAdapter->m_dwMagic == NDT_MAGIC_ADAPTER);
   hr = pAdapter->GetCounter(nIndex, pnValue);
   return hr;
}

//------------------------------------------------------------------------------

HRESULT NDTSetOptions(HANDLE hAdapter, DWORD dwLogLevel, DWORD dwBeatDelay)
{
   HRESULT hr = S_OK;
   
   CAdapter* pAdapter = (CAdapter *)hAdapter;
   ASSERT(pAdapter->m_dwMagic == NDT_MAGIC_ADAPTER);
   hr = pAdapter->SetOptions(dwLogLevel, dwBeatDelay);
   return hr;
}

//------------------------------------------------------------------------------

HRESULT NDTQueryInfo(
   HANDLE hAdapter, NDIS_OID ndisOid, PVOID pvBuffer, UINT cbBuffer, 
   UINT* pcbUsed, UINT* pcbRequired
)
{
   HRESULT hr = S_OK;
   
   CAdapter* pAdapter = (CAdapter *)hAdapter;
   ASSERT(pAdapter->m_dwMagic == NDT_MAGIC_ADAPTER);
   hr = pAdapter->QueryInfo(ndisOid, pvBuffer, cbBuffer, pcbUsed, pcbRequired);
   return hr;
}

//------------------------------------------------------------------------------

HRESULT NDTSetInfo(
   HANDLE hAdapter, NDIS_OID ndisOid, PVOID pvBuffer, UINT cbBuffer, 
   UINT* pcbUsed, UINT* pcbRequired
)
{
   HRESULT hr = S_OK;
   
   CAdapter* pAdapter = (CAdapter *)hAdapter;
   ASSERT(pAdapter->m_dwMagic == NDT_MAGIC_ADAPTER);
   hr = pAdapter->SetInfo(ndisOid, pvBuffer, cbBuffer, pcbUsed, pcbRequired);
   return hr;
}

//------------------------------------------------------------------------------

HRESULT NDTSend(
   HANDLE hAdapter, ULONG cbAddr, BYTE* pucSrcAddr, ULONG nDestAddr, 
   BYTE* apucDestAddrs[], BYTE ucResponseMode, BYTE ucPacketSizeMode, 
   ULONG ulPacketSize, ULONG ulPacketCount, UINT uiBeatDelay, UINT uiBeatGroup,
   HANDLE *phSend
)
{
   HRESULT hr = S_OK;
   CRequest* pRequest = NULL;
   CAdapter* pAdapter = (CAdapter *)hAdapter;
   ASSERT(pAdapter->m_dwMagic == NDT_MAGIC_ADAPTER);

   hr = pAdapter->Send(
      cbAddr, pucSrcAddr, nDestAddr, apucDestAddrs, ucResponseMode, 
      ucPacketSizeMode, ulPacketSize, ulPacketCount, uiBeatDelay, uiBeatGroup, 
      pRequest
   );
   *phSend = (HANDLE)pRequest;
   return hr;
}

//------------------------------------------------------------------------------

HRESULT NDTSendWait(
   HANDLE hAdapter, HANDLE hRequest, DWORD dwTimeout, ULONG* pulPacketsSent,
   ULONG* pulPacketsCompleted, ULONG* pulPacketsCanceled, 
   ULONG* pulPacketsUncanceled, ULONG* pulPacketsReplied, ULONG *pulTime, 
   ULONG *pulSend, ULONG *pulReceive
)
{
   HRESULT hr = S_OK;
   
   CAdapter* pAdapter = (CAdapter *)hAdapter;
   ASSERT(pAdapter->m_dwMagic == NDT_MAGIC_ADAPTER);
   CRequest* pRequest = (CRequest *)hRequest;
   ASSERT(pRequest->m_dwMagic == NDT_MAGIC_REQUEST);
   
   hr = pAdapter->SendWait(
      pRequest, dwTimeout, pulPacketsSent, pulPacketsCompleted, 
      pulPacketsCanceled, pulPacketsUncanceled, pulPacketsReplied, pulTime,
      pulSend, pulReceive
   );
   return hr;
}

//------------------------------------------------------------------------------

HRESULT NDTReceive(HANDLE hAdapter, HANDLE *phRequest)
{
   HRESULT hr = S_OK;
   CRequest* pRequest = NULL;
   CAdapter* pAdapter = (CAdapter *)hAdapter;
   ASSERT(pAdapter->m_dwMagic == NDT_MAGIC_ADAPTER);

   hr = pAdapter->Receive(pRequest);
   *phRequest = (HANDLE)pRequest;
   return hr;
}

//------------------------------------------------------------------------------

HRESULT NDTReceiveStop(
   HANDLE hAdapter, HANDLE hRequest, ULONG* pulPacketsReceived, 
   ULONG* pulPacketsReplied, ULONG* pulPacketsCompleted, ULONG* pulTime,
   ULONG *pulBytesSent, ULONG *pulBytesReceived
)
{
   HRESULT hr = S_OK;
   
   CAdapter* pAdapter = (CAdapter *)hAdapter;
   ASSERT(pAdapter->m_dwMagic == NDT_MAGIC_ADAPTER);
   CRequest* pRequest = (CRequest *)hRequest;
   ASSERT(pRequest->m_dwMagic == NDT_MAGIC_REQUEST);
   
   hr = pAdapter->ReceiveStop(
      pRequest, pulPacketsReceived, pulPacketsReplied, pulPacketsCompleted,
      pulTime, pulBytesSent, pulBytesReceived
   );
   return hr;
}

//------------------------------------------------------------------------------

HRESULT NDTSetId(HANDLE hAdapter, USHORT usLocalId, USHORT usRemoteId)
{
   HRESULT hr = S_OK;
   
   CAdapter* pAdapter = (CAdapter *)hAdapter;
   ASSERT(pAdapter->m_dwMagic == NDT_MAGIC_ADAPTER);
   hr = pAdapter->SetId(usLocalId, usRemoteId);
   return hr;
}

//------------------------------------------------------------------------------
