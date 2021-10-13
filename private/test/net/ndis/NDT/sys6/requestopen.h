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
#ifndef __REQUEST_OPEN_H
#define __REQUEST_OPEN_H

//------------------------------------------------------------------------------

#include "Request.h"

//------------------------------------------------------------------------------

class CRequestOpen : public CRequest
{
private:
    NDIS_STATUS m_status;
    NDIS_STRING  m_nsProtocolName;
    CProtocol     *m_pProtocol;
    
public:
   CRequestOpen(CBinding* pBinding = NULL);
   virtual ~CRequestOpen();

   virtual NDIS_STATUS UnmarshalInpParams(PVOID* ppvBuffer, DWORD* pcbBuffer);
   virtual NDIS_STATUS MarshalOutParams(PVOID* ppvBuffer, DWORD* pcbBuffer);
   virtual NDIS_STATUS Execute();
};

//------------------------------------------------------------------------------

#endif

