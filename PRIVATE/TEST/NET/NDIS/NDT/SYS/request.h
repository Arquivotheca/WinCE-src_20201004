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

class CBinding;

//------------------------------------------------------------------------------

class CRequest : public CObject
{
   friend class CBinding;

private:
   BOOL m_bInternal;                   // Is a request internal or extern?
   NDIS_EVENT m_hEvent;                // Internal event for req complete
   PVOID  m_pvComplete;                // Cookie for NDTCompleteRequest
   PVOID  m_pvBufferOut;               // Where to move date for extern req
   DWORD  m_cbBufferOut;               // Size of output buffer for extern req
   DWORD* m_pcbActualOut;              // Where write how many bytes returned
   
protected:
   NDT_ENUM_REQUEST_TYPE m_eType;      // Request type
   CBinding* m_pBinding;               // Request binding context
   
public:
   NDIS_STATUS m_status;               // Request result code

public:
   CRequest(NDT_ENUM_REQUEST_TYPE eType, CBinding *pBinding = NULL);
   virtual ~CRequest();
   
   void InitExt(
      PVOID pvComplete, PVOID pvBufferOut, DWORD cbBufferOut, 
      DWORD* pcbActualOut
   );
   NDIS_STATUS WaitForComplete(DWORD dwTimeout);
   NDIS_STATUS InternalExecute(DWORD dwTimeout);
   
   virtual NDIS_STATUS UnmarshalInpParams(PVOID* ppvBuffer, DWORD* pcbBuffer);
   virtual NDIS_STATUS MarshalOutParams(PVOID* ppvBuffer, DWORD* pcbBuffer);
   virtual NDIS_STATUS Execute() = 0;

   virtual void Complete();            // Complete request
};

//------------------------------------------------------------------------------

#endif
