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

