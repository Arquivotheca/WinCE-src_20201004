//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __REQUEST_GET_COUNTER_H
#define __REQUEST_GET_COUNTER_H

//------------------------------------------------------------------------------

#include "Request.h"

//------------------------------------------------------------------------------

class CRequestGetCounter : public CRequest
{
public:
   // Output params
   ULONG m_nValue;

   // Input params
   ULONG m_nIndex;
 
public:
   CRequestGetCounter(CBinding* pBinding = NULL);
   
   virtual NDIS_STATUS Execute();
   virtual NDIS_STATUS UnmarshalInpParams(PVOID* ppvBuffer, DWORD* pcbBuffer);
   virtual NDIS_STATUS MarshalOutParams(PVOID* ppvBuffer, DWORD* pcbBuffer);
};

//------------------------------------------------------------------------------

#endif
