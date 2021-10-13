//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: extnstub.cpp
Abstract: Extension Stub Functions
--*/

#include "httpd.h"


const DWORD g_fIsapiExtensionModule = FALSE;

BOOL CHttpRequest::ExecuteISAPI(WCHAR *wszExecutePath) {
	DEBUGMSG(ZONE_ERROR, (L"HTTPD: ISAPI dll caleld, but ISAPI component not included!!\r\n"));
	DEBUGCHK(FALSE);
	return FALSE;
}

BOOL CHttpRequest::ServerSupportFunction(DWORD dwReq, PVOID pvBuf, PDWORD pdwSize, PDWORD pdwType) {
	DEBUGCHK(FALSE);
	return FALSE;
}

BOOL CGlobalVariables::InitExtensions(CReg *pWebsite) {
	return FALSE;
}

void CGlobalVariables::DeInitExtensions(void) {
	DEBUGCHK(FALSE);
}

BOOL MapScriptExtension(WCHAR *wszFileExt, WCHAR **pwszISAPIPath, BOOL *pfIsASP)  {
	DEBUGCHK(FALSE);
	return FALSE;
}

BOOL CHttpRequest::SetPathTranslated() {
	DEBUGCHK(FALSE);
	return FALSE;
}

void CISAPICache::RemoveUnusedISAPIs(BOOL fRemoveAll) {
	DEBUGCHK(FALSE);
}
