//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: authstub.cpp
Abstract: Auth Stub Functions
--*/

#include "httpd.h"

const DWORD g_fAuthModule = FALSE;

void CGlobalVariables::InitAuthentication(CReg *pReg) {
	m_fBasicAuth = FALSE;
	m_fNTLMAuth  = FALSE;
}

BOOL CHttpRequest::HandleAuthentication(void)  {
	DEBUGCHK(FALSE);
	return FALSE;
}

void FreeNTLMHandles(PAUTH_NTLM pNTLMState) {
	;
}
