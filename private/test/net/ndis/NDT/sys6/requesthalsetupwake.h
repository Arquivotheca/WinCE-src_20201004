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
#ifndef __REQUEST_HAL_EN_WAKE_H
#define __REQUEST_HAL_EN_WAKE_H

//------------------------------------------------------------------------------

#include "Request.h"

//------------------------------------------------------------------------------

class CProtocol;

//------------------------------------------------------------------------------

class CRequestHalSetupWake : public CRequest
{
public:
   // Input params
   UINT m_uiWakeSrc;
   UINT m_uiFlags;
   UINT m_uiTimeOut;
 
public:
   CRequestHalSetupWake();
   
   virtual NDIS_STATUS Execute();
   virtual NDIS_STATUS UnmarshalInpParams(LPVOID* ppvBuffer, DWORD* pcbBuffer);
};


//------------------------------------------------------------------------------

#endif


