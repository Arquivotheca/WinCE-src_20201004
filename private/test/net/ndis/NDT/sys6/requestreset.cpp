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
#include "Binding.h"
#include "RequestReset.h"
#include "Marshal.h"
#include "Log.h"

//------------------------------------------------------------------------------
   
CRequestReset::CRequestReset(CBinding* pBinding) : 
   CRequest(NDT_REQUEST_RESET, pBinding)
{
   m_dwMagic = NDT_MAGIC_REQUEST_RESET;
   NdisInitializeEvent(&m_hResetCompletionEvent);
}

//------------------------------------------------------------------------------

CRequestReset::~CRequestReset()
{
   NdisFreeEvent(&m_hResetCompletionEvent);
}
   
//------------------------------------------------------------------------------

NDIS_STATUS CRequestReset::Execute()
{
    NDIS_STATUS status;
    NDIS_OID_REQUEST oidRequest;

    //in NDIS 6, NdisReset is accomplished using a private OID which returns NDIS_STATUS_SUCCESS. 
    //This is followed by a ProtocolStatusEx call to report NDIS_RESET_STATUS_END at which point we know
    //the miniport actually complied with the request. So for a reset request we should do the following:
    //1. Add request to the pending queue
    m_pBinding->AddRequestToList(this);

    //2. Call NdisOidRequest to set the OID_GEN_RESET_MINIPORT
    oidRequest.Header.Type = NDIS_OBJECT_TYPE_OID_REQUEST;
    oidRequest.Header.Revision = NDIS_OID_REQUEST_REVISION_1;
    oidRequest.Header.Size = sizeof(NDIS_OID_REQUEST);

    oidRequest.RequestType = NdisRequestSetInformation;
    oidRequest.DATA.SET_INFORMATION.Oid = OID_GEN_RESET_MINIPORT;

    status = NdisOidRequest(m_pBinding->m_hAdapter, &oidRequest);
    //3. Verify the call returns NDIS_STATUS_SUCCESS but return NDIS_STATUS_PENDING, to
    //   keep the request alive till the status complete comes in
    ASSERT(status == NDIS_STATUS_SUCCESS);

    if (NdisWaitEvent(&m_hResetCompletionEvent, 30000))
    {
        m_status = NDIS_STATUS_SUCCESS;
    }
    else 
    {
        m_pBinding->RemoveRequestFromList(this);
        m_status = NDIS_STATUS_FAILURE;
    }
    Complete();

    return m_status;
}

//------------------------------------------------------------------------------

VOID CRequestReset::SetRequestCompletionEvent()
{
    NdisSetEvent(&m_hResetCompletionEvent);
}

//------------------------------------------------------------------------------
