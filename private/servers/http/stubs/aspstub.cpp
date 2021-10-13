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
Module Name: aspstub.cpp
Abstract: ASP Stub Functions
--*/

#include "httpd.h"

//
// ASP stubs
//
BOOL CHttpRequest::ExecuteASP() 
{
    DEBUGMSG(ZONE_ERROR, (L"HTTPD: ASP caleld, but ASP component not included!!\r\n"));
    m_rs = STATUS_NOTIMPLEM;
    return FALSE;
}

BOOL CGlobalVariables::InitASP(CReg *pWebsite) 
{
    return FALSE;
}

