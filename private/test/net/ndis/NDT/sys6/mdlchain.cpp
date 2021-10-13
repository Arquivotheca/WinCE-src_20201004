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
#include "Binding.h"
#include "RequestRequest.h"
#include "Log.h"
#include "MDLChain.h"

CMDLChain::CMDLChain(NDIS_HANDLE hNBPool)
{
    m_hNBPool = hNBPool;
    m_pMDLChain = NULL;
}

PMDL CMDLChain::GetMDLChain()
{
    return m_pMDLChain;
}

PMDL CMDLChain::CreateMDL(PVOID pVirtualAddress, ULONG ulLength)
{
    return NdisAllocateMdl(m_hNBPool, pVirtualAddress, ulLength);
}

void CMDLChain::AddMDLAtFront(PMDL pNewMDL)
{
    NDIS_MDL_LINKAGE(pNewMDL) = m_pMDLChain;
    m_pMDLChain = pNewMDL;
}

//TODO: This assumes that the MDL chain is not too large. If it gets large we need to add a tail pointer
//to the class.
void CMDLChain::AddMDLAtBack(PMDL pNewMDL)
{
    PMDL pCurrentMDL;

    NDIS_MDL_LINKAGE(pNewMDL) = NULL;
    if (m_pMDLChain == NULL)
    {
        m_pMDLChain = pNewMDL;
        return;
    }

    pCurrentMDL = m_pMDLChain;

    while (NDIS_MDL_LINKAGE(pCurrentMDL) != NULL)
    {
        pCurrentMDL = NDIS_MDL_LINKAGE(pCurrentMDL);
    }

    NDIS_MDL_LINKAGE(pCurrentMDL) = pNewMDL;
}