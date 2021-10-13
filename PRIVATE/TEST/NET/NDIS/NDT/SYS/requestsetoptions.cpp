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
#include "RequestSetOptions.h"
#include "Marshal.h"
#include "Log.h"

//------------------------------------------------------------------------------

CRequestSetOptions::CRequestSetOptions(
   CProtocol* pProtocol, CProtocol* pProtocol40
) : 
   CRequest(NDT_REQUEST_SET_OPTIONS)
{
   m_dwMagic = NDT_MAGIC_REQUEST_SET_OPTIONS;
   m_uiLogLevel = 0;
   m_uiHeartBeatDelay = 0;
   m_pProtocol = pProtocol; m_pProtocol->AddRef();
   m_pProtocol40 = pProtocol40; m_pProtocol40->AddRef();
}

//------------------------------------------------------------------------------

CRequestSetOptions::~CRequestSetOptions()
{
   m_pProtocol->Release();
   m_pProtocol40->Release();
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestSetOptions::Execute()
{
   LogSetLevel(m_uiLogLevel & NDT_LOG_MASK);
   m_pProtocol->m_bLogPackets = (m_uiLogLevel & NDT_LOG_PACKETS) != 0;
   m_pProtocol->SetHeartBeat(m_uiHeartBeatDelay);
   Complete();
   return NDIS_STATUS_SUCCESS;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestSetOptions::UnmarshalInpParams(
   PVOID *ppvBuffer, DWORD *pcbBuffer
)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   
   status = UnmarshalParameters(
      ppvBuffer, pcbBuffer, NDT_MARSHAL_INP_SET_OPTIONS, &m_uiLogLevel,
      &m_uiHeartBeatDelay
   );
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;
   
cleanUp:
   return status;
}

//------------------------------------------------------------------------------
