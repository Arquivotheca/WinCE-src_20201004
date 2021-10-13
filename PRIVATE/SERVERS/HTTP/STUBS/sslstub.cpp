//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: sslstub.cpp
Abstract: SSL Stub Functions
--*/

#include "httpd.h"

void CGlobalVariables::InitSSL(CReg *pWebsite) {
	;
}

void CGlobalVariables::FreeSSLResources(void) {
	DEBUGCHK(FALSE);
}

BOOL CHttpRequest::HandleSSLHandShake(BOOL fRenegotiate, BYTE *pInitialData, DWORD cbInitialData) {
	DEBUGCHK(FALSE);
	return FALSE;
}

CHttpRequest::SSLRenegotiateRequest(void) {
	DEBUGCHK(FALSE);
	return FALSE;
}

BOOL CHttpRequest::SendEncryptedData(PSTR pszBuf, DWORD dwLen, BOOL fCopyBuffer) {
	DEBUGCHK(FALSE);
	return FALSE;
}

void CHttpRequest::CloseSSLSession() {
	DEBUGCHK(FALSE);
}

SECURITY_STATUS CHttpRequest::SSLDecrypt(PSTR pszBuf, DWORD dwLen, DWORD *pdwBytesDecrypted, DWORD *pdwOffset, DWORD *pdwExtraRequired, DWORD *pdwIgnore, BOOL *pfCompletedRenegotiate) {
	DEBUGCHK(FALSE);
	return -1;
}

BOOL CHttpRequest::HandleSSLClientCertCheck(void) {
	DEBUGCHK(FALSE);
	return FALSE;
}
