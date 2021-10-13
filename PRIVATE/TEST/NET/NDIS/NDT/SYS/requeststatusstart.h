//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __REQUEST_STATUS_NOTIFICATION_START_H
#define __REQUEST_STATUS_NOTIFICATION_START_H

//------------------------------------------------------------------------------

#include "Request.h"

//------------------------------------------------------------------------------

class CRequestStatusStart : public CRequest
{
public:
   ULONG m_ulEvent;						  // Event indication from NDIS/miniport.

   CRequestStatusStart(CBinding* pBinding = NULL);
   virtual ~CRequestStatusStart();

   virtual NDIS_STATUS UnmarshalInpParams(PVOID* ppvBuffer, DWORD* pcbBuffer);
   virtual NDIS_STATUS Execute();
};

//------------------------------------------------------------------------------

#endif
