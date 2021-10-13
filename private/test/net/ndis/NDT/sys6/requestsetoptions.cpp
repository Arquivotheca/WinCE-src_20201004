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
#include "RequestSetOptions.h"
#include "Marshal.h"
#include "Log.h"

//------------------------------------------------------------------------------

CRequestSetOptions::CRequestSetOptions(
) : 
   CRequest(NDT_REQUEST_SET_OPTIONS)
{
   m_dwMagic = NDT_MAGIC_REQUEST_SET_OPTIONS;
   m_uiLogLevel = 0;
   m_uiHeartBeatDelay = 0;
   m_pProtocol = NULL;
}

//------------------------------------------------------------------------------

CRequestSetOptions::~CRequestSetOptions()
{
   m_pProtocol->Release();
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
      ppvBuffer, pcbBuffer, NDT_MARSHAL_INP_SET_OPTIONS, &m_pProtocol, &m_uiLogLevel,
      &m_uiHeartBeatDelay
   );
   m_pProtocol->AddRef();
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;
   
cleanUp:
   return status;
}

//------------------------------------------------------------------------------
