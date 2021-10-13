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
Module Name: sslstub.cpp
Abstract: SSL Stub Functions
--*/

#include "httpd.h"

void CGlobalVariables::InitSSL(CReg *pWebsite) 
{
    ;
}

void CGlobalVariables::FreeSSLResources(void) 
{
    DEBUGCHK(FALSE);
}

BOOL CHttpRequest::HandleSSLHandShake(BOOL fRenegotiate, BYTE *pInitialData, DWORD cbInitialData) 
{
    DEBUGCHK(FALSE);
    return FALSE;
}

CHttpRequest::SSLRenegotiateRequest(void) 
{
    DEBUGCHK(FALSE);
    return FALSE;
}

BOOL CHttpRequest::SendEncryptedData(PSTR pszBuf, DWORD dwLen, BOOL fCopyBuffer) 
{
    DEBUGCHK(FALSE);
    return FALSE;
}

void CHttpRequest::CloseSSLSession() 
{
    DEBUGCHK(FALSE);
}

SECURITY_STATUS CHttpRequest::SSLDecrypt(PSTR pszBuf, DWORD dwLen, DWORD *pdwBytesDecrypted, DWORD *pdwOffset, DWORD *pdwExtraRequired, DWORD *pdwIgnore, BOOL *pfCompletedRenegotiate) 
{
    DEBUGCHK(FALSE);
    return -1;
}

BOOL CHttpRequest::HandleSSLClientCertCheck(void) 
{
    DEBUGCHK(FALSE);
    return FALSE;
}
