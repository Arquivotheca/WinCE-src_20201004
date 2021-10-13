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
Module Name: filtstub.cpp
Abstract: Filter Stub Functions
--*/

#include "httpd.h"

const DWORD g_fIsapiFilterModule = FALSE;

BOOL InitFilters() 
{
    return FALSE;
}

void CleanupFilters() 
{
}

CFilterInfo* CreateCFilterInfo(void) 
{
    return NULL;
}


BOOL CHttpRequest::CallFilter(DWORD dwNotifyType, PSTR *ppszBuf1, 
                              int *pcbBuf, PSTR *ppszBuf2, int *pcbBuf2)
{
    DEBUGCHK(FALSE);
    return TRUE;
}



BOOL CHttpRequest::AuthenticateFilter() 
{
    DEBUGCHK(FALSE);
    return FALSE;
}

BOOL CHttpRequest::SetHeader(LPSTR lpszName, LPSTR lpszValue) 
{
    DEBUGCHK(FALSE);
    return FALSE;
}

BOOL CHttpRequest::FilterMapURL(PSTR pvBuf, WCHAR *wszPath, DWORD *pdwSize, DWORD dwBufNeeded, PSTR pszURLEx) 
{
    DEBUGCHK(FALSE);
    return FALSE;
}

CFilterInfo::~CFilterInfo() 
{
    DEBUGCHK(FALSE);
}

BOOL CFilterInfo::ReInit() 
{
    DEBUGCHK(FALSE);
    return FALSE;
}
