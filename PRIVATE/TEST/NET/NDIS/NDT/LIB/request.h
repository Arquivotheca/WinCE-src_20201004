//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __REQUEST_H
#define __REQUEST_H

//------------------------------------------------------------------------------

#include "NDT_Request.h"
#include "Object.h"

//------------------------------------------------------------------------------

class CDriver;

//------------------------------------------------------------------------------

class CRequest : public CObject
{
private:
   CDriver* m_pDriver;

public:
   BYTE  m_baInpBuffer[2048];
   DWORD m_cbInpBuffer;
   BYTE  m_baOutBuffer[2048];
   DWORD m_cbOutBuffer;
   PVOID m_pvOverlapped;
   
public:
   CRequest(CDriver* pDriver);
   virtual ~CRequest();

   HRESULT MarshalInpParameters(LPCTSTR szFormat, ...);
   HRESULT UnmarshalOutParameters(LPCTSTR szFormat, ...);

   HRESULT Execute(DWORD dwTimeout, NDT_ENUM_REQUEST_TYPE eRequest);
   HRESULT WaitForComplete(DWORD dwTimeout);
   HRESULT Stop();
};

//------------------------------------------------------------------------------

#endif
