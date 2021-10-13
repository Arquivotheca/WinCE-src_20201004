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

   HRESULT ClearCounters();

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

   HRESULT Status(ULONG ulStatus, CRequest*& pRequest);
   HRESULT StatusWait(CRequest* pRequest, DWORD dwTimeout);
};

//------------------------------------------------------------------------------

#endif
