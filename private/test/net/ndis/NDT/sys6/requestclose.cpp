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
#include "NDTLib.h"
#include "ProtocolHeader.h"
#include "Protocol.h"
#include "Binding.h"
#include "Medium.h"
#include "Packet.h"
#include "RequestClose.h"
#include "Marshal.h"
#include "Log.h"

//------------------------------------------------------------------------------

extern CObjectList g_ProtocolList;

//------------------------------------------------------------------------------

CRequestClose::CRequestClose(CBinding* pBinding) :  
CRequest(NDT_REQUEST_CLOSE, pBinding)
{
    m_pProtocol = NULL;
    m_dwMagic = NDT_MAGIC_REQUEST_CLOSE;
//    AddRef();
};

//------------------------------------------------------------------------------

CRequestClose::~CRequestClose()
{
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestClose::Execute()
{
       // Create object for  NDIS 6.0 protocol
    if (m_pProtocol != NULL) {
        g_ProtocolList.AcquireSpinLock();
        g_ProtocolList.Remove((CObject *)m_pProtocol);
        g_ProtocolList.ReleaseSpinLock();
        m_pProtocol->DeregisterProtocol();
        m_pProtocol->Release();
        m_pProtocol = NULL;
    }

      Complete();
    
      return NDIS_STATUS_SUCCESS;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestClose::UnmarshalInpParams(
    PVOID* ppvBuffer, DWORD* pcbBuffer
    )
{
    NDIS_STATUS status = NDIS_STATUS_SUCCESS;

    status = UnmarshalParameters(
        ppvBuffer, pcbBuffer, NDT_MARSHAL_INP_CLOSE, &m_pProtocol
        );
    if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

cleanUp:
    return status;
}

//------------------------------------------------------------------------------

