//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
