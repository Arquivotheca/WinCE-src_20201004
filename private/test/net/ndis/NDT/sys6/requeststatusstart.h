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
#ifndef __REQUEST_STATUS_NOTIFICATION_START_H
#define __REQUEST_STATUS_NOTIFICATION_START_H

//------------------------------------------------------------------------------

#include "Request.h"

//------------------------------------------------------------------------------

class CRequestStatusStart : public CRequest
{
public:
   LONG m_ulEvent;                          // Event indication from NDIS/miniport.
   UINT m_cbStatusBufferSize;               // The size of the returned status buffer
   PVOID m_pStatusBuffer;                  // The returned status buffer    

   CRequestStatusStart(CBinding* pBinding = NULL);
   virtual ~CRequestStatusStart();

   virtual NDIS_STATUS UnmarshalInpParams(PVOID* ppvBuffer, DWORD* pcbBuffer);
   virtual NDIS_STATUS MarshalOutParams(PVOID* ppvBuffer, DWORD* pcbBuffer);
   virtual NDIS_STATUS Execute();
};

//------------------------------------------------------------------------------

#endif
