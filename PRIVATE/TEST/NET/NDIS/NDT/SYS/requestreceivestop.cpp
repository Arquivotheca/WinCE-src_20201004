//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
