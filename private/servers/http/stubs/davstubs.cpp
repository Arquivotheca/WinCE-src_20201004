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
Module Name: davstubs.cpp
Abstract: Stubs for WebDAV support
--*/


#include "httpd.h"

const DWORD g_fWebDavModule = FALSE;

BOOL CGlobalVariables::InitWebDav(CReg *pWebsite) 
{
    m_fWebDav = FALSE;
    return FALSE;
}


void CGlobalVariables::DeInitWebDav(void) 
{
    DEBUGCHK(0);
    return;
}

BOOL CHttpRequest::HandleWebDav(void) 
{
    DEBUGCHK(0);
    return FALSE;
}

void CWebDavFileLockManager::RemoveTimedOutLocks(void) 
{
    DEBUGCHK(0);
}
