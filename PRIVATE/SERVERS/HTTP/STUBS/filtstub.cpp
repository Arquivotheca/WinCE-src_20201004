//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: filtstub.cpp
Abstract: Filter Stub Functions
--*/

#include "httpd.h"

const DWORD g_fIsapiFilterModule = FALSE;

BOOL InitFilters() {
	return FALSE;
}

void CleanupFilters() {
}

CFilterInfo* CreateCFilterInfo(void) {
	return NULL;
}


BOOL CHttpRequest::CallFilter(DWORD dwNotifyType, PSTR *ppszBuf1, 
							  int *pcbBuf, PSTR *ppszBuf2, int *pcbBuf2)
{
	DEBUGCHK(FALSE);
	return TRUE;
}



BOOL CHttpRequest::AuthenticateFilter() {
	DEBUGCHK(FALSE);
	return FALSE;
}

BOOL CHttpRequest::SetHeader(LPSTR lpszName, LPSTR lpszValue) {
	DEBUGCHK(FALSE);
	return FALSE;
}

BOOL CHttpRequest::FilterMapURL(PSTR pvBuf, WCHAR *wszPath, DWORD *pdwSize, DWORD dwBufNeeded, PSTR pszURLEx) {
	DEBUGCHK(FALSE);
	return FALSE;
}

CFilterInfo::~CFilterInfo() {
	DEBUGCHK(FALSE);
}

BOOL CFilterInfo::ReInit() {
	DEBUGCHK(FALSE);
	return FALSE;
}
