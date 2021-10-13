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
#include "RequestKill.h"
#include "Marshal.h"
#include "Log.h"

//------------------------------------------------------------------------------

extern CObjectList g_ProtocolList;

//------------------------------------------------------------------------------

CRequestKill::CRequestKill() : 
CRequest(NDT_REQUEST_KILL)
{
    m_dwMagic = NDT_MAGIC_REQUEST_KILL;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestKill::Execute()
{
    CProtocol *pProtocol = NULL;
    CBinding *pBinding = NULL;
    
    for (;;)
    {
        g_ProtocolList.AcquireSpinLock();
        pProtocol = (CProtocol *) g_ProtocolList.GetHead();
        g_ProtocolList.Remove((CObject *) pProtocol);
        g_ProtocolList.ReleaseSpinLock();

        if (pProtocol == NULL)
        {
            break;
        }
        pBinding = (CBinding *) pProtocol->GetBindingListHead();
        while (pBinding)
        {
            pBinding->CloseAdapter();
            pBinding = (CBinding *) pProtocol->GetBindingListHead();
        }
        pProtocol->DeregisterProtocol();

        pProtocol->Release();
        pProtocol = NULL;
    }
    Complete();
    return NDIS_STATUS_SUCCESS;
}

//------------------------------------------------------------------------------

NDIS_STATUS CRequestKill::UnmarshalInpParams(
    PVOID* ppvBuffer, DWORD* pcbBuffer
    )
{
    return NDIS_STATUS_SUCCESS;
}

//------------------------------------------------------------------------------

