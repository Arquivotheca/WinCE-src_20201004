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
Module Name: extnstub.cpp
Abstract: Extension Stub Functions
--*/

#include "httpd.h"


const DWORD g_fIsapiExtensionModule = FALSE;

BOOL CHttpRequest::ExecuteISAPI(WCHAR *wszExecutePath) 
{
    DEBUGMSG(ZONE_ERROR, (L"HTTPD: ISAPI dll caleld, but ISAPI component not included!!\r\n"));
    DEBUGCHK(FALSE);
    return FALSE;
}

BOOL CHttpRequest::ServerSupportFunction(DWORD dwReq, PVOID pvBuf, PDWORD pdwSize, PDWORD pdwType) 
{
    DEBUGCHK(FALSE);
    return FALSE;
}

BOOL CGlobalVariables::InitExtensions(CReg *pWebsite) 
{
    return FALSE;
}

void CGlobalVariables::DeInitExtensions(void) 
{
    DEBUGCHK(FALSE);
}

BOOL MapScriptExtension(WCHAR *wszFileExt, WCHAR **pwszISAPIPath, BOOL *pfIsASP)  
{
    DEBUGCHK(FALSE);
    return FALSE;
}

BOOL CHttpRequest::SetPathTranslated() 
{
    DEBUGCHK(FALSE);
    return FALSE;
}

void CISAPICache::RemoveUnusedISAPIs(BOOL fRemoveAll) 
{
    DEBUGCHK(FALSE);
}
