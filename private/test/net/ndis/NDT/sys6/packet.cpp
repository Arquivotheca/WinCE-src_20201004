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
#include "Packet.h"
#include "MDLChain.h"

//------------------------------------------------------------------------------

CPacket::CPacket(CBinding* pBinding, PNET_BUFFER pNB)
{
   m_dwMagic = NDT_MAGIC_PACKET;
   m_pBinding = pBinding; m_pBinding->AddRef();
   m_pNdisNB = pNB;
   m_uiSize = 0;
   m_pucMediumHeader = NULL;
   m_pProtocolHeader = NULL;
   m_pucBody = NULL;
   m_bBodyStatic= FALSE;
}

//------------------------------------------------------------------------------

CPacket::~CPacket()
{
    if (m_pucBody != NULL) delete m_pucBody;
    if (m_pProtocolHeader != NULL) delete m_pProtocolHeader;
    if (m_pucMediumHeader != NULL) delete m_pucMediumHeader;
    NdisFreeNetBuffer(m_pNdisNB);
    m_pBinding->Release();
}

//------------------------------------------------------------------------------

void CPacket::PopulateNetBuffer(CMDLChain *pMDLChain, SIZE_T sDataLength)
{
    NET_BUFFER_FIRST_MDL(m_pNdisNB) = pMDLChain->GetMDLChain();
    NET_BUFFER_DATA_LENGTH(m_pNdisNB) = sDataLength;
    NET_BUFFER_DATA_OFFSET(m_pNdisNB) = 0;
    NET_BUFFER_CURRENT_MDL(m_pNdisNB) = pMDLChain->GetMDLChain();
    NET_BUFFER_CURRENT_MDL_OFFSET(m_pNdisNB) = 0;
}

//------------------------------------------------------------------------------

void CPacket::DestroyNetBufferMDLs()
{
    PMDL pHeadMDL, pNewHeadMDL;

    //Walk the MDL chain and free the members
    pHeadMDL = NET_BUFFER_FIRST_MDL(m_pNdisNB);
    
    while (pHeadMDL)
    {
        pNewHeadMDL = NDIS_MDL_LINKAGE(pHeadMDL);
        NdisFreeMdl(pHeadMDL);
        pHeadMDL = pNewHeadMDL;
    }

    NET_BUFFER_FIRST_MDL(m_pNdisNB) = NULL;
    NET_BUFFER_DATA_LENGTH(m_pNdisNB) = 0;
    NET_BUFFER_DATA_OFFSET(m_pNdisNB) = 0;
    NET_BUFFER_CURRENT_MDL(m_pNdisNB) = NULL;
    NET_BUFFER_CURRENT_MDL_OFFSET(m_pNdisNB) = 0;
}

//------------------------------------------------------------------------------
