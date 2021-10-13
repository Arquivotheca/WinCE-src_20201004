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
#include "RequestClearCounters.h"
#include "Marshal.h"
#include "Log.h"

//------------------------------------------------------------------------------

CRequestClearCounters::CRequestClearCounters(CBinding *pBinding) : 
   CRequest(NDT_REQUEST_CLEAR_COUNTERS, pBinding)
{
   m_dwMagic = NDT_MAGIC_REQUEST_CLEAR_COUNTERS;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestClearCounters::Execute()
{
      m_pBinding->m_ulUnexpectedEvents = 0;
      m_pBinding->m_ulStatusResetStart = 0;
      m_pBinding->m_ulStatusResetEnd = 0;
      m_pBinding->m_ulStatusMediaConnect = 0;
      m_pBinding->m_ulStatusMediaDisconnect = 0;   
      Complete();
      return NDIS_STATUS_SUCCESS;
}

//------------------------------------------------------------------------------
