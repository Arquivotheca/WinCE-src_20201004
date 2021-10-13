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

#include "Constants.h"
#include "NDT_Request6.h"
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
