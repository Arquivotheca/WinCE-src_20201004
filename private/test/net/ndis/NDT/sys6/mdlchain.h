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

#ifndef __MDLCHAIN_H
#define __MDLCHAIN_H

//------------------------------------------------------------------------------

#include "Object.h"
#include "ObjectList.h"

//------------------------------------------------------------------------------

class CMDLChain : public CObject
{
private:
    NDIS_HANDLE m_hNBPool;
    PMDL m_pMDLChain;                //The MDL chain this object maintains

public:
    CMDLChain(NDIS_HANDLE hNBPool);
    PMDL GetMDLChain();                //Gets the MDL chain member
    PMDL CreateMDL(PVOID pVirtualAddress, ULONG ulLength);
    void AddMDLAtFront(PMDL newMDL);
    void AddMDLAtBack(PMDL newMDL);
};

#endif