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
#include "RequestSetId.h"
#include "Marshal.h"
#include "Log.h"

//------------------------------------------------------------------------------

CRequestSetId::CRequestSetId(CBinding *pBinding) : 
   CRequest(NDT_REQUEST_SET_ID, pBinding)
{
   m_dwMagic = NDT_MAGIC_REQUEST_SET_ID;
   m_usLocalId = 0;
   m_usRemoteId = 0;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestSetId::Execute()
{
   m_pBinding->m_usLocalId = m_usLocalId;
   m_pBinding->m_usRemoteId = m_usRemoteId;
   m_status = NDIS_STATUS_SUCCESS;
   Complete();
   return NDIS_STATUS_SUCCESS;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestSetId::UnmarshalInpParams(
   PVOID *ppvBuffer, DWORD *pcbBuffer
)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   
   status = UnmarshalParameters(
      ppvBuffer, pcbBuffer, NDT_MARSHAL_INP_GET_COUNTER, &m_pBinding, 
      &m_usLocalId, &m_usRemoteId
   );
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;
   
   // We save an pointer to object (ok, not to clear but...)
   m_pBinding->AddRef();
   
cleanUp:
   return status;
}

//------------------------------------------------------------------------------
