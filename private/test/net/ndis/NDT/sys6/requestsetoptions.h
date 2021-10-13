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
#ifndef __REQUEST_SET_OPTIONS_H
#define __REQUEST_SET_OPTIONS_H

//------------------------------------------------------------------------------

#include "Request.h"

//------------------------------------------------------------------------------

class CProtocol;

//------------------------------------------------------------------------------

class CRequestSetOptions : public CRequest
{
private:
   CProtocol* m_pProtocol;

public:
   // Input params
   UINT m_uiLogLevel;
   UINT m_uiHeartBeatDelay;
 
public:
   CRequestSetOptions();
   virtual ~CRequestSetOptions();
   
   virtual NDIS_STATUS Execute();
   virtual NDIS_STATUS UnmarshalInpParams(LPVOID* ppvBuffer, DWORD* pcbBuffer);
};

//------------------------------------------------------------------------------

#endif

