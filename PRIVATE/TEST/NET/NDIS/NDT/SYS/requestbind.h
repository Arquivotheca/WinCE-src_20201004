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
#ifndef __REQUEST_BIND_H
#define __REQUEST_BIND_H

//------------------------------------------------------------------------------

#include "Request.h"

//------------------------------------------------------------------------------

class CDriver;

//------------------------------------------------------------------------------

class CRequestBind : public CRequest
{
private:
   CDriver* m_pDriver;
   
public:
   // Output params
   NDIS_STATUS m_statusOpenError;
   UINT m_uiSelectedMediaIndex;

   // Input params
   UINT m_uiUseNDIS40;
   UINT m_uiMediumArraySize;
   NDIS_MEDIUM* m_aMedium;
   NDIS_STRING m_nsAdapterName;
   UINT m_uiOpenOptions;
   
public:
   CRequestBind(CDriver* pDriver, CBinding* pBinding = NULL);
   virtual ~CRequestBind();
   
   virtual NDIS_STATUS Execute();
   virtual NDIS_STATUS UnmarshalInpParams(PVOID* ppvBuffer, DWORD* pcbBuffer);
   virtual NDIS_STATUS MarshalOutParams(PVOID* ppvBuffer, DWORD* pcbBuffer);
   virtual void Complete();
};

//------------------------------------------------------------------------------

#endif
