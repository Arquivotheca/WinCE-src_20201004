//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __ADAPTER_H
#define __ADAPTER_H

//------------------------------------------------------------------------------

#include "Object.h"
#include "ObjectList.h"

//------------------------------------------------------------------------------

class CDriver;
class CRequest;

//------------------------------------------------------------------------------

class CAdapter : public CObject
{
private:
   LPTSTR m_szAdapter;              // Adapter name
   LPTSTR m_szSystem;               // System name or a NULL for local system
   HANDLE m_hBinding;               // Adapter binding handler
   CObjectList m_listRequests;      // List of activer requests
   
public:   
   CDriver* m_pDriver;              // Device to communicate with a driver
   DWORD m_dwTimeout;               // Timeout for a sync request
   
public:
   CAdapter();
   ~CAdapter();
   
   HRESULT Open(LPTSTR szAdapter);
   HRESULT Close();

   HRESULT LoadDriver();
   HRESULT UnloadDriver();
 
   HRESULT UnbindProtocol(LPCTSTR szProtocol);
   HRESULT BindProtocol(LPCTSTR szProtocol);

   HRESULT WriteVerifyFlag(DWORD dwFlag);
   HRESULT DeleteVerifyFlag();

   HRESULT SetOptions(DWORD dwZoneMask, DWORD dwBeatDelay);

   HRESULT Bind(
      BOOL bForce40, NDIS_MEDIUM ndisMedium, NDIS_MEDIUM* pNdisMedium = NULL
   );
   HRESULT Unbind();
   HRESULT Reset();

   HRESULT GetCounter(ULONG ulIndex, ULONG *pulValue);

   HRESULT QueryInfo(
      NDIS_OID ndisOid, PVOID pvBuffer, UINT cbBuffer, UINT* puiUsed, 
      UINT* puiRequired
   );

   HRESULT SetInfo(
      NDIS_OID ndisOid, PVOID pvBuffer, UINT cbBuffer, UINT* puiUsed,
      UINT* puiRequired
   );
   
   HRESULT Send(
      ULONG cbAddr, BYTE* pucSrcAddr, ULONG nDestAddrs, UCHAR* apucDestAddrs[], 
      UCHAR ucResponseMode, UCHAR ucPacketSizeMode, ULONG ulPacketSize, 
      ULONG ulPacketCount, UINT uiBeatDelay, UINT uiBeatGroup, 
      CRequest*& pRequest
   );

   HRESULT SendWait(
      CRequest* pRequest, DWORD dwTimeout, ULONG* pulPacketsSent,
      ULONG* pulPacketsCompleted, ULONG* pulPacketsCanceled, 
      ULONG* pulPacketsUncanceled, ULONG* pulPacketsReplied, ULONG* pulTime, 
      ULONG* pulBytesSent, ULONG* pulBytesReceived
   );

   HRESULT Receive(CRequest*& pRequest);

   HRESULT ReceiveStop(
      CRequest* pRequest, ULONG* pulPacketsReceived, ULONG* pulPacketsReplied, 
      ULONG* pulPacketsCompleted, ULONG* pulTime, ULONG* pulBytesSent, 
      ULONG* pulBytesReceived
   );

   HRESULT SetId(USHORT usLocalId, USHORT usRemoteId);
};

//------------------------------------------------------------------------------

#endif
