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
#include "RequestOpen.h"
#include "Marshal.h"
#include "Log.h"

//------------------------------------------------------------------------------

extern CObjectList g_ProtocolList;

//------------------------------------------------------------------------------

CRequestOpen::CRequestOpen(CBinding* pBinding) :  
CRequest(NDT_REQUEST_OPEN, pBinding)
{
    m_dwMagic = NDT_MAGIC_REQUEST_OPEN;
    m_status = NDIS_STATUS_FAILURE;
    m_pProtocol = NULL;
//    AddRef();
    NdisInitUnicodeString(&m_nsProtocolName, NULL);
};

//------------------------------------------------------------------------------

CRequestOpen::~CRequestOpen()
{
    if (m_nsProtocolName.Buffer != NULL) NdisFreeString(m_nsProtocolName);
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestOpen::Execute()
{
       // Create object for  NDIS 6.0 protocol
       m_pProtocol = new CProtocol();
       if (m_pProtocol == NULL) {
          Log(NDT_ERR_NEW_PROTOCOL_60);
          goto cleanUp;
       }
    
       // Now register it
       m_status = m_pProtocol->RegisterProtocol(
          NDIS_PROTOCOL_MAJOR_VERSION, NDIS_PROTOCOL_MINOR_VERSION,
          0x20000000, FALSE, (LPWSTR) m_nsProtocolName.Buffer
       );
       if (m_status != NDIS_STATUS_SUCCESS) {
          Log(NDT_ERR_REGISTER_PROTOCOL_60, m_status);
          goto cleanUp;
       }
       g_ProtocolList.AcquireSpinLock();
       g_ProtocolList.AddTail((CObject *)m_pProtocol);
       g_ProtocolList.ReleaseSpinLock();
    Complete();

cleanUp:
    return m_status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestOpen::UnmarshalInpParams(
    PVOID* ppvBuffer, DWORD* pcbBuffer
    )
{
    NDIS_STATUS status = NDIS_STATUS_SUCCESS;

    status = UnmarshalParameters(
        ppvBuffer, pcbBuffer, NDT_MARSHAL_INP_OPEN, &m_nsProtocolName
        );
    if (status != NDIS_STATUS_SUCCESS) goto cleanUp;

cleanUp:
    return status;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestOpen::MarshalOutParams(
    PVOID* ppvBuffer, DWORD* pcbBuffer
    )
{

    return MarshalParameters(
        ppvBuffer, pcbBuffer, NDT_MARSHAL_OUT_OPEN, m_status, m_pProtocol
        );
}

//------------------------------------------------------------------------------
