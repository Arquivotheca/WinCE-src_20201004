//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: aspstub.cpp
Abstract: ASP Stub Functions
--*/

#include "httpd.h"

//
// ASP stubs
//
BOOL CHttpRequest::ExecuteASP() {
	DEBUGMSG(ZONE_ERROR, (L"HTTPD: ASP caleld, but ASP component not included!!\r\n"));
	m_rs = STATUS_NOTIMPLEM;
	return FALSE;
}

BOOL CGlobalVariables::InitASP(CReg *pWebsite) {
	return FALSE;
}

