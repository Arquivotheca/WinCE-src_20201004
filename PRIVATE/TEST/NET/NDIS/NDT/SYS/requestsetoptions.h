//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
   CProtocol* m_pProtocol40;

public:
   // Input params
   UINT m_uiLogLevel;
   UINT m_uiHeartBeatDelay;
 
public:
   CRequestSetOptions(CProtocol* pProtocol, CProtocol* pProtocol40);
   virtual ~CRequestSetOptions();
   
   virtual NDIS_STATUS Execute();
   virtual NDIS_STATUS UnmarshalInpParams(LPVOID* ppvBuffer, DWORD* pcbBuffer);
};

//------------------------------------------------------------------------------

#endif

