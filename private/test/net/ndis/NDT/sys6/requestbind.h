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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#ifndef __REQUEST_BIND_H
#define __REQUEST_BIND_H

//------------------------------------------------------------------------------

#include <assert.h>
#include "Request.h"

#define REG_NDIS_KEY            TEXT("\\Comm")

//------------------------------------------------------------------------------

class CProtocol;
extern NDIS_EVENT g_hBindsCompleteEvent;

//------------------------------------------------------------------------------

class CRequestBind : public CRequest
{
private:
   CProtocol* m_pProtocol;

   /**
   * The amount of time we wait for the bind/open to be completed, after we have
   * started binding. Currently we wait for this time, but we would not return
   * until the bind has completed.
   */
   static const ULONG ulBIND_COMPLETE_WAIT  = 60000;

   /**
   * The amount if time in msec we wait for Restart to arrive from NDIS (or
   * internally for 5.x Protocol case) and complete before we fail the bind request.
   */
   static const ULONG ulRESTART_COMPLETE_WAIT = 30000;   
   
public:
   // Output params
   UINT m_uiSelectedMediaIndex;

   // Input params
   UINT m_uiMediumArraySize;
   NDIS_MEDIUM* m_aMedium;
   NDIS_STRING m_nsAdapterName;
   UINT m_uiOpenOptions;
   NDIS_STRING m_nsDriverName;

private:
   NDIS_STATUS CreateRegistryKey();

   LPTSTR GetAdapterName();
BOOL
GetRegMultiSZValue (HKEY hKey, LPTSTR lpValueName, PTSTR pData, LPDWORD pdwSize);
   BOOL
SetRegMultiSZValue (HKEY hKey, LPTSTR lpValueName, PTSTR pData);

     
   
public:
   CRequestBind(CBinding* pBinding = NULL);
   virtual ~CRequestBind();
   
   virtual NDIS_STATUS Execute();
   virtual NDIS_STATUS UnmarshalInpParams(PVOID* ppvBuffer, DWORD* pcbBuffer);
   virtual NDIS_STATUS MarshalOutParams(PVOID* ppvBuffer, DWORD* pcbBuffer);
   virtual void Complete();
 
   NDIS_STATUS CreateBinding(NDIS_HANDLE BindContext);
};

//------------------------------------------------------------------------------

#endif
