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
/*--
Module Name: authstub.cpp
Abstract: Auth Stub Functions
--*/

#include "httpd.h"

const DWORD g_fAuthModule = FALSE;

void CGlobalVariables::InitAuthentication(CReg *pReg) 
{
    m_fBasicAuth        = FALSE;
    m_fNTLMAuth         = FALSE;
    m_fNegotiateAuth    = FALSE;
}

BOOL CHttpRequest::HandleAuthentication(void)  
{
    DEBUGCHK(FALSE);
    return FALSE;
}

void FreeSecContextHandles(PAUTH_STATE pAuthState) 
{
    ;
}
