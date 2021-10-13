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
#include "RequestStatusStart.h"
#include "Marshal.h"
#include "Log.h"

/*
When caller request for STATUS notification we just hook up this request into binding's request link list.
Whenever ProtocolStatus() is called Binding object goes through this list & find out the request object of type
NDT_REQUEST_STATUS_START & matches the STATUS being indicated to the one requested & mentioned in the request 
object. If there is match then that request is completed & freed during which the event object for that 
request is set.

Suppose if the status which is requested by the request is never indicated then the request will be completed
when CBinding::CloseAdapter is called.
*/

//------------------------------------------------------------------------------

CRequestStatusStart::CRequestStatusStart(CBinding* pBinding) :  
   CRequest(NDT_REQUEST_STATUS_START, pBinding)
{
       m_ulEvent = 0;
}

//------------------------------------------------------------------------------

CRequestStatusStart::~CRequestStatusStart()
{
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestStatusStart::Execute()
{
    NDIS_STATUS status = NDIS_STATUS_FAILURE;
    
    if (!m_pBinding)
        goto cleanUp;
    // Add request to pending queue
    m_pBinding->AddRequestToList(this);
    
    // We return pending state by default.
    status = NDIS_STATUS_PENDING;
    
cleanUp:
    return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestStatusStart::UnmarshalInpParams(
   PVOID *ppvBuffer, DWORD *pcbBuffer
)
{
   NDIS_STATUS status = NDIS_STATUS_SUCCESS;
   
   status = UnmarshalParameters(
      ppvBuffer, pcbBuffer, NDT_MARSHAL_INP_STATUS_START, &m_pBinding, &m_ulEvent
   );
   if (status != NDIS_STATUS_SUCCESS) goto cleanUp;
   
   // We save an pointer to object
   if (m_pBinding)
       m_pBinding->AddRef();

cleanUp:
   return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestStatusStart::MarshalOutParams(
    PVOID* ppvBuffer, DWORD* pcbBuffer
    )
{
    return MarshalParameters(
        ppvBuffer, pcbBuffer, NDT_MARSHAL_OUT_STATUS_START, m_cbStatusBufferSize, m_pStatusBuffer
        );
}

//------------------------------------------------------------------------------


