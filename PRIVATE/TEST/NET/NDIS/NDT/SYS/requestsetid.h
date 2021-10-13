//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __REQUEST_SET_ID_H
#define __REQUEST_SET_ID_H

//------------------------------------------------------------------------------

#include "Request.h"

//------------------------------------------------------------------------------

class CRequestSetId : public CRequest
{
public:
   // Input params
   USHORT m_usLocalId;
   USHORT m_usRemoteId;
 
public:
   CRequestSetId(CBinding *pBinding = NULL);
   
   virtual NDIS_STATUS Execute();
   virtual NDIS_STATUS UnmarshalInpParams(LPVOID *ppvBuffer, DWORD *pcbBuffer);
};

//------------------------------------------------------------------------------

#endif
