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
#include "StdAfx.h"
#include "NDT.h"
#include "Protocol.h"
#include "Binding.h"
#include "RequestReceive.h"
#include "RequestReceiveStop.h"
#include "Marshal.h"
#include "Log.h"

//------------------------------------------------------------------------------

CRequestReceiveStop::CRequestReceiveStop(CBinding* pBinding) : 
   CRequest(NDT_REQUEST_RECEIVE_STOP, pBinding)
{
   m_dwMagic = NDT_MAGIC_REQUEST_RECEIVE_STOP;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestReceiveStop::Execute()
{
   CRequestReceive* pRequest = m_pBinding->FindReceiveRequest(FALSE);

   // If there isn't such request fail
   if (pRequest == NULL) {
      m_status = NDIS_STATUS_INVALID_DEVICE_REQUEST;
      goto cleanUp;
   }
   
   // Just make sure that we get a proper object
   ASSERT(pRequest->m_dwMagic == NDT_MAGIC_REQUEST_RECEIVE);

   // Let it deside what to do...
   pRequest->Stop();

   // And release pointer
   pRequest->Release();

   // Complete our request
   Complete();
   
cleanUp:   
   return NDIS_STATUS_SUCCESS;
}

//------------------------------------------------------------------------------
