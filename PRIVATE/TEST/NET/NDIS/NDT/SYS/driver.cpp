//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "StdAfx.h"
#include "NDT_Request.h"
#include "NDT.h"
#include "Driver.h"
#include "Protocol.h"
#include "Binding.h"
#include "Request.h"
#include "RequestBind.h"
#include "RequestUnbind.h"
#include "RequestReset.h"
#include "RequestGetCounter.h"
#include "RequestRequest.h"
#include "RequestSend.h"
#include "RequestReceive.h"
#include "RequestReceiveStop.h"
#include "RequestSetId.h"
#include "RequestSetOptions.h"
#include "Marshal.h"
#include "Log.h"

//------------------------------------------------------------------------------

CDriver::CDriver()
{
   // A magic value for class instance identification
   m_dwMagic = NDT_MAGIC_DRIVER;
   m_pProtocol40 = NULL;
   m_pProtocol = NULL;
}

//------------------------------------------------------------------------------

CDriver::~CDriver()
{
}

//------------------------------------------------------------------------------

BOOL CDriver::Init(DWORD dwContext)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;

   NDT_NdisInitRandom();
   
   // First create object for NDIS 4.0 protocol
   m_pProtocol40 = new CProtocol(this);
   if (m_pProtocol40 == NULL) {
      Log(NDT_ERR_NEW_PROTOCOL_40);
      goto cleanUp;
   }

   // Now register it
   status = m_pProtocol40->RegisterProtocol(
      NDIS40_PROTOCOL_MAJOR_VERSION, NDIS40_PROTOCOL_MINOR_VERSION,
      0x20000000, FALSE, L"NDT40"
   );
   if (status != NDIS_STATUS_SUCCESS) {
      Log(NDT_ERR_REGISTER_PROTOCOL_40, status);
      goto cleanUp;
   }

   // Create object for  NDIS 5.x protocol
   m_pProtocol = new CProtocol(this);
   if (m_pProtocol == NULL) {
      Log(NDT_ERR_NEW_PROTOCOL_5X);
      goto cleanUp;
   }

   // Now register it
   status = m_pProtocol->RegisterProtocol(
      NDIS_PROTOCOL_MAJOR_VERSION, NDIS_PROTOCOL_MINOR_VERSION,
      0x20000000, FALSE, L"NDT51"
   );
   if (status != NDIS_STATUS_SUCCESS) {
      Log(NDT_ERR_REGISTER_PROTOCOL_5X, status);
      goto cleanUp;
   }

   return TRUE;
   
cleanUp:
   Deinit();
   return FALSE;
}

//------------------------------------------------------------------------------

BOOL CDriver::Deinit()
{
   if (m_pProtocol != NULL) {
      m_pProtocol->DeregisterProtocol();
      m_pProtocol->Release();
      m_pProtocol = NULL;
   }
   if (m_pProtocol40 != NULL) {
      m_pProtocol40->DeregisterProtocol();
      m_pProtocol40->Release();
      m_pProtocol40 = NULL;
   }
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
   case NDT_REQUEST_BIND:
      pRequest = new CRequestBind(this);
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
   case NDT_REQUEST_RECEIVE_STOP:
      pRequest = new CRequestReceiveStop;
      break;
   case NDT_REQUEST_SET_ID:
      pRequest = new CRequestSetId;
      break;
   case NDT_REQUEST_SET_OPTIONS:
      pRequest = new CRequestSetOptions(m_pProtocol, m_pProtocol40);
      break;
  default:
      status = NDIS_STATUS_INVALID_DEVICE_REQUEST;
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
