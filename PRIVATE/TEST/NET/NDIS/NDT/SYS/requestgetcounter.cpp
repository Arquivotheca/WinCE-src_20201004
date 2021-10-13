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
#include "RequestGetCounter.h"
#include "Marshal.h"
#include "Log.h"

//------------------------------------------------------------------------------
// Ensure that following values matche to those defined in \NDT\inc\ndtlib.h
#define NDT_COUNTER_UNEXPECTED_EVENTS        (1)
#define NDT_COUNTER_STATUS_RESET_START		 (2)
#define NDT_COUNTER_STATUS_RESET_END		 (3)
#define NDT_COUNTER_STATUS_MEDIA_CONNECT	 (4)
#define NDT_COUNTER_STATUS_MEDIA_DISCONNECT	 (5)

//------------------------------------------------------------------------------

CRequestGetCounter::CRequestGetCounter(CBinding *pBinding) : 
   CRequest(NDT_REQUEST_GET_COUNTER, pBinding)
{
   m_dwMagic = NDT_MAGIC_REQUEST_GET_COUNTER;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestGetCounter::Execute()
{
   switch (m_nIndex) {
	  case NDT_COUNTER_UNEXPECTED_EVENTS:
		  m_nValue = m_pBinding->m_ulUnexpectedEvents;
		  break;
	  case NDT_COUNTER_STATUS_RESET_START:
		  m_nValue = m_pBinding->m_ulStatusResetStart;
		  break;
	  case NDT_COUNTER_STATUS_RESET_END:
		  m_nValue = m_pBinding->m_ulStatusResetEnd;
		  break;
	  case NDT_COUNTER_STATUS_MEDIA_CONNECT:
		  m_nValue = m_pBinding->m_ulStatusMediaConnect;
		  break;
	  case NDT_COUNTER_STATUS_MEDIA_DISCONNECT:
		  m_nValue = m_pBinding->m_ulStatusMediaDisconnect;
		  break;
	  default:
		  m_nValue = 0;
   }
   Complete();
   return NDIS_STATUS_SUCCESS;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestGetCounter::UnmarshalInpParams(
   PVOID *ppvBuffer, DWORD *pcbBuffer
)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   
   status = UnmarshalParameters(
      ppvBuffer, pcbBuffer, NDT_MARSHAL_INP_GET_COUNTER, &m_pBinding, &m_nIndex
   );
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;
   
   // We save an pointer to object (ok, not to clear but...)
   m_pBinding->AddRef();
   
cleanUp:
   return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestGetCounter::MarshalOutParams(
   PVOID *ppvBuffer, DWORD *pcbBuffer
)
{
   return MarshalParameters(
      ppvBuffer, pcbBuffer, NDT_MARSHAL_OUT_GET_COUNTER, m_nValue
   );
}

//------------------------------------------------------------------------------
