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
#include "NDT_Request6.h"
#include "NDT.h"
#include "Driver.h"
#include "Protocol.h"
#include "Binding.h"
#include "Request.h"
#include "RequestOpen.h"
#include "RequestClose.h"
#include "RequestBind.h"
#include "RequestUnbind.h"
#include "RequestReset.h"
#include "RequestGetCounter.h"
#include "RequestRequest.h"
#include "RequestSend.h"
#include "RequestReceive.h"
#include "RequestReceiveStop.h"
#include "RequestReceiveEx.h"
#include "RequestReceiveStopEx.h"
#include "RequestSetId.h"
#include "RequestSetOptions.h"
#include "RequestStatusStart.h"
#include "RequestClearCounters.h"
#include "RequestHalSetupWake.h"
#include "Marshal.h"
#include "Log.h"

//------------------------------------------------------------------------------

CDriver::CDriver()
{
   // A magic value for class instance identification
   m_dwMagic = NDT_MAGIC_DRIVER;
}

//------------------------------------------------------------------------------

CDriver::~CDriver()
{
}

//------------------------------------------------------------------------------

BOOL CDriver::Init(DWORD dwContext)
{
   NDT_NdisInitRandom();
   //Init the global binds completed event
   NdisInitializeEvent(&g_hBindsCompleteEvent);

   return TRUE;
}

//------------------------------------------------------------------------------

BOOL CDriver::Deinit()
{
   return TRUE;
}

//------------------------------------------------------------------------------

NDIS_STATUS CDriver::IOControl(
   NDT_ENUM_REQUEST_TYPE eRequest, PVOID pvBufferInp, DWORD cbBufferInp, 
   PVOID pvBufferOut, DWORD cbBufferOut, DWORD *pcbActualOut, 
   NDIS_HANDLE hComplete
)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   CRequest *pRequest = NULL;

   switch (eRequest) {
   case NDT_REQUEST_OPEN:
    pRequest = new CRequestOpen();
    break;
   case NDT_REQUEST_CLOSE:
    pRequest = new CRequestClose();
    break;
   case NDT_REQUEST_BIND:
      pRequest = new CRequestBind();
      break;
   case NDT_REQUEST_UNBIND:
      pRequest = new CRequestUnbind;
      break;
   case NDT_REQUEST_RESET:
      pRequest = new CRequestReset;
      break;
   case NDT_REQUEST_GET_COUNTER:
      pRequest = new CRequestGetCounter;
      break;
   case NDT_REQUEST_REQUEST:
      pRequest = new CRequestRequest;
      break;
   case NDT_REQUEST_SEND:
      pRequest = new CRequestSend;
      break;
   case NDT_REQUEST_RECEIVE:
      pRequest = new CRequestReceive;
      break;
   case NDT_REQUEST_RECEIVE_EX:
       pRequest = new CRequestReceiveEx;
       break;
   case NDT_REQUEST_RECEIVE_STOP:
      pRequest = new CRequestReceiveStop;
      break;
   case NDT_REQUEST_RECEIVE_STOP_EX:
       pRequest = new CRequestReceiveStopEx;
       break;
   case NDT_REQUEST_SET_ID:
      pRequest = new CRequestSetId;
      break;
   case NDT_REQUEST_SET_OPTIONS:
      pRequest = new CRequestSetOptions();
      break;
   case NDT_REQUEST_STATUS_START:
      pRequest = new CRequestStatusStart;
      break;
   case NDT_REQUEST_CLEAR_COUNTERS:
      pRequest = new CRequestClearCounters;
      break;   
   case NDT_REQUEST_HAL_EN_WAKE:
      pRequest = new CRequestHalSetupWake;
      break;
  default:
      status = NDIS_STATUS_INVALID_DEVICE_REQUEST;
      goto cleanUp;
   }

   if (!pRequest)
   {
       status = NDIS_STATUS_FAILURE;
       goto cleanUp;
   }

   // Set parameters for the external request
   pRequest->InitExt(hComplete, pvBufferOut, cbBufferOut, pcbActualOut);

   // Parse request input parameters
   status = pRequest->UnmarshalInpParams(&pvBufferInp, &cbBufferInp);
   if (status != NDIS_STATUS_SUCCESS) {
      goto cleanUp;
   }

   // Execute a command
   status = pRequest->Execute();

   // We don't need it anymore...
   pRequest->Release();
   
cleanUp:
   return status;
}

//------------------------------------------------------------------------------
