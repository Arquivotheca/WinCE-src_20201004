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
   BYTE  m_baInpBuffer[4096];
   DWORD m_cbInpBuffer;
   BYTE  m_baOutBuffer[12288];
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
