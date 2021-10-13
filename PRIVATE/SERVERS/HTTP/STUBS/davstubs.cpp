//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: davstubs.cpp
Abstract: Stubs for WebDAV support
--*/


#include "httpd.h"

const DWORD g_fWebDavModule = FALSE;

BOOL CGlobalVariables::InitWebDav(CReg *pWebsite) {
	m_fWebDav = FALSE;
	return FALSE;
}


void CGlobalVariables::DeInitWebDav(void) {
	DEBUGCHK(0);
	return;
}

BOOL CHttpRequest::HandleWebDav(void) {
	DEBUGCHK(0);
	return FALSE;
}

void CWebDavFileLockManager::RemoveTimedOutLocks(void) {
	DEBUGCHK(0);
}
