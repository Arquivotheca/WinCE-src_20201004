//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __REQUEST_REQUEST_H
#define __REQUEST_REQUEST_H

//------------------------------------------------------------------------------

#include "Request.h"

//------------------------------------------------------------------------------

class CRequestRequest : public CRequest
{
public:
   NDIS_OID m_oid;
   NDIS_REQUEST_TYPE m_requestType;
   UCHAR* m_pucInpBuffer;
   UINT m_cbInpBuffer;
   UCHAR* m_pucOutBuffer;
   UINT m_cbOutBuffer;

private:   
   NDIS_REQUEST m_ndisRequest;
   
public:
   CRequestRequest(
      CBinding* pBinding = NULL, UINT cbInpBuffer = 0, UINT cbOutBuffer = 0
   );
   virtual ~CRequestRequest();
   
   virtual NDIS_STATUS Execute();
   virtual NDIS_STATUS UnmarshalInpParams(PVOID* ppvBuffer, DWORD* pcbBuffer);
   virtual NDIS_STATUS MarshalOutParams(PVOID* ppvBuffer, DWORD* pcbBuffer);

};

//------------------------------------------------------------------------------

#endif

