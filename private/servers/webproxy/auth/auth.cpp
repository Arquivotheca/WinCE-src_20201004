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

#include <authhlp.h>
#include "auth.h"
#include "proxydbg.h"

#define SEC_SUCCESS(Status) ((Status) >= 0)

BOOL NTLMServerContext(PSEC_WINNT_AUTH_IDENTITY pAuthIdentity, PAUTH_NTLM pAS, BYTE *pIn, DWORD cbIn, BYTE *pOut, DWORD *pcbOut, BOOL *pfDone);
DWORD GetUserInfo(PAUTH_NTLM pAuth, char* szUser, int cbUser);


typedef struct _SecurityParams {
    DWORD dwMaxNTLMBuf;
    HINSTANCE hLib;
    SecurityFunctionTable funcs;
} SecurityParams;

SecurityParams params;


DWORD HandleBasicAuth(const char* szBasicAuth, BOOL* pfAuthSuccess, char* szUser, int cbUser)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    CHAR szBuff[MAX_USER_NAME];
    CHAR* pszUserName;

    ASSERT(szBasicAuth);
    ASSERT(pfAuthSuccess);
    ASSERT(szUser);
    ASSERT(cbUser > 0);

    *pfAuthSuccess = FALSE;

    if (! svsutil_Base64Decode((char *) szBasicAuth, szBuff, cbUser, NULL, TRUE)) {
    	dwRetVal = ERROR_INVALID_PARAMETER;
    	goto exit;
    }

    // find the password
    PSTR pszPassword = strchr(szBuff, ':');
    if (! pszPassword) {
    	IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Bad format for basic auth: %hs\n"), szBuff));
    	dwRetVal = ERROR_INVALID_PARAMETER;
    	goto exit;
    }
    *pszPassword++ = 0; // seperate user & pass

    // Some  clients prepend a domain name or machine name in front of user name, preceeding by a '\'.
    // Strip this out.
    if (NULL != (pszUserName = strchr(szBuff,'\\'))) {
    	strcpy(szUser, pszUserName++);
    }
    else {
    	strcpy(szUser, szBuff);
    }

    *pfAuthSuccess = AuthHelpValidateUserA(szUser, pszPassword, NULL, 0);

#ifdef DEBUG
    if (*pfAuthSuccess) {
    	IFDBG(DebugOut(ZONE_AUTH, _T("WebProxy: Successfully authenticated user %hs pass %hs\n"), szUser, pszPassword));
    }
    else {
    	IFDBG(DebugOut(ZONE_AUTH, _T("WebProxy: Failed to authenticate to user %hs pass %hs\n"), szUser, pszPassword));
    }
#endif

exit:
    if (! *pfAuthSuccess) {
    	strcpy(szUser, "");
    }
    
    return dwRetVal;
}


DWORD HandleNTLMAuth(const char* szNTLMAuth, char* szNTLMAuthOut, int cbNTLMAuthOut, char* szUser, int cbUser, PAUTH_NTLM pAuth)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    DWORD dwIn;
    DWORD dwOut;
    BOOL fDone = FALSE;
    PBYTE pOutBuf = NULL;
    PBYTE pInBuf = NULL;   // Base64 decoded data from pszNTLMData

    ASSERT(pAuth);
    ASSERT(szNTLMAuthOut);
    ASSERT(szUser);

    dwIn = strlen(szNTLMAuth)+1;
    pInBuf = (PBYTE) LocalAlloc(0, dwIn);
    if (! pInBuf) {
    	dwRetVal = ERROR_OUTOFMEMORY;
    	goto exit;
    }

    if (! svsutil_Base64Decode((char *) szNTLMAuth, pInBuf, dwIn,NULL, FALSE)) {
    	dwRetVal = ERROR_INVALID_PARAMETER;
    	goto exit;
    }

    dwOut = cbNTLMAuthOut;
    pOutBuf = (PBYTE) LocalAlloc(0,dwOut);
    if (! pOutBuf) {
    	dwRetVal = ERROR_OUTOFMEMORY;
    	goto exit;
    }
    
    if (! NTLMServerContext(NULL, pAuth, pInBuf, dwIn, pOutBuf, &dwOut, &fDone)) {
    	pAuth->m_Conversation = NTLM_DONE;
    	IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: NTLMServerContext failed: %d\n"), dwRetVal));
    	goto exit;
    }

    if (fDone) {
    	IFDBG(DebugOut(ZONE_AUTH, _T("WebProxy: Successfully authenticated user.\n")));
    	dwRetVal = GetUserInfo(pAuth, szUser, cbUser);
    	pAuth->m_Conversation = NTLM_DONE;
    	goto exit;
    }

    pAuth->m_Conversation = NTLM_PROCESSING;
    if (! svsutil_Base64Encode(pOutBuf,dwOut,szNTLMAuthOut,cbNTLMAuthOut,NULL)) {
        pAuth->m_Conversation = NTLM_DONE;
    	dwRetVal = ERROR_INTERNAL_ERROR;
    	goto exit;
    }

exit:
    if (pInBuf) {
    	LocalFree(pInBuf);
    }
    if (pOutBuf) {
    	LocalFree(pOutBuf);
    }
    
    return dwRetVal;
}

// Retrieves the maximum size of a buffer we would need in order
// to hold the maximum size buffer NTLM will give us followed by
// a base64 encode of that buffer (which expands to approx 4/3 size)
DWORD GetMaxNTLMBuffBase64Size(DWORD* pdwMaxSecurityBuffSize)
{
    ASSERT(pdwMaxSecurityBuffSize);
    *pdwMaxSecurityBuffSize = (1 + (params.dwMaxNTLMBuf + 2) / 3 * 4);
    return ERROR_SUCCESS;
}

DWORD InitNTLMSecurityLib(void)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    SECURITY_STATUS ss = 0;
    FARPROC pInit;
    PSecurityFunctionTable pSecFun;
    PSecPkgInfo pkgInfo;

    params.hLib = LoadLibrary(_T("secur32.dll"));
    if (! params.hLib) {
    	dwRetVal = GetLastError();
    	IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Failed to load security library %d\n"), dwRetVal));
    	goto exit;
    }

    pInit = (FARPROC) GetProcAddress (params.hLib, SECURITY_ENTRYPOINT);
    if (! pInit)  {
    	dwRetVal = GetLastError();
    	IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Failed to load security library %d\n"), dwRetVal));
    	goto exit;
    }

    pSecFun = (PSecurityFunctionTable) pInit ();
    if (! pSecFun) {
    	dwRetVal = GetLastError();
    	IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: Failed to load security library %d\n"), dwRetVal));
    	goto exit;
    }

    memcpy(&params.funcs,pSecFun,sizeof(SecurityFunctionTable));

    ss = params.funcs.QuerySecurityPackageInfo (_T("NTLM"), &pkgInfo);
    if (!SEC_SUCCESS(ss)) {
    	dwRetVal = GetLastError();
    	IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: QuerySecurityPackageInfo failed, GLE=0x%08x\n"),dwRetVal));
    	goto exit;
    }

    params.dwMaxNTLMBuf = pkgInfo->cbMaxToken;
    params.funcs.FreeContextBuffer(pkgInfo);

exit:
    if (ERROR_SUCCESS != dwRetVal) {
    	if (params.hLib) {
    		FreeLibrary(params.hLib);
    		params.hLib = NULL;
    	}
    	memset(&params.funcs, 0, sizeof(params.funcs));
    }
    
    return dwRetVal;
}

DWORD DeinitNTLMSecurityLib(void)
{
    DWORD dwRetVal = ERROR_SUCCESS;

    if (params.hLib) {
    	FreeLibrary(params.hLib);
    	params.hLib = NULL;
    }
    
    memset(&params.funcs, 0, sizeof(params.funcs));

    return dwRetVal;
}

DWORD InitBasicAuth(void)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    if (! AuthHelpInitialize()) {
        dwRetVal = GetLastError();    	
    }	

    return dwRetVal;
}

DWORD DeinitBasicAuth(void)
{
	DWORD dwRetVal = ERROR_SUCCESS;
    if (! AuthHelpUnload()) {
        dwRetVal = GetLastError();    	
    }	

    return dwRetVal;
}

BOOL NTLMServerContext(
    		PSEC_WINNT_AUTH_IDENTITY pAuthIdentity,
    		PAUTH_NTLM pAS,		// NTLM state info
    		BYTE *pIn,
    		DWORD cbIn,
    		BYTE *pOut,
    		DWORD *pcbOut,
    		BOOL *pfDone)
{
    SECURITY_STATUS	ss;
    TimeStamp		Lifetime;
    SecBufferDesc	OutBuffDesc;
    SecBuffer		OutSecBuff;
    SecBufferDesc	InBuffDesc;
    SecBuffer		InSecBuff;
    ULONG			ContextAttributes;

    
    if (NTLM_NO_INIT_CONTEXT == pAS->m_Conversation)  {
    	ss = params.funcs.AcquireCredentialsHandle (
    						NULL,	// principal
    						_T("NTLM"),
    						SECPKG_CRED_INBOUND,
    						NULL,	// LOGON id
    					    pAuthIdentity,	
    						NULL,	// get key fn
    						NULL,	// get key arg
    						&pAS->m_hcred,
    						&Lifetime
    						);
    	if (SEC_SUCCESS (ss))
    		pAS->m_fHaveCredHandle = TRUE;
    	else {
    		IFDBG(DebugOut(ZONE_ERROR, L"WebProxy: NTLM AcquireCreds failed: %X\n", ss));
    		return(FALSE);
    	}
    }

    // prepare output buffer
    //
    OutBuffDesc.ulVersion = 0;
    OutBuffDesc.cBuffers = 1;
    OutBuffDesc.pBuffers = &OutSecBuff;

    OutSecBuff.cbBuffer = *pcbOut;
    OutSecBuff.BufferType = SECBUFFER_TOKEN;
    OutSecBuff.pvBuffer = pOut;

    // prepare input buffer
    //
    InBuffDesc.ulVersion = 0;
    InBuffDesc.cBuffers = 1;
    InBuffDesc.pBuffers = &InSecBuff;

    InSecBuff.cbBuffer = cbIn;
    InSecBuff.BufferType = SECBUFFER_TOKEN;
    InSecBuff.pvBuffer = pIn;

    ss = params.funcs.AcceptSecurityContext (
    					&pAS->m_hcred,
    					(pAS->m_Conversation == NTLM_PROCESSING) ?  &pAS->m_hctxt : NULL,
    					&InBuffDesc,
    					0,	// context requirements
    					SECURITY_NATIVE_DREP,
    					&pAS->m_hctxt,
    					&OutBuffDesc,
    					&ContextAttributes,
    					&Lifetime
    					);
    if (!SEC_SUCCESS (ss))  {
    	IFDBG(DebugOut(ZONE_ERROR, L"WebProxy: NTLM init context failed: %X\n", ss));
    	return FALSE;
    }

    pAS->m_fHaveCtxtHandle = TRUE;

    // Complete token -- if applicable
    //
    if ((SEC_I_COMPLETE_NEEDED == ss) || (SEC_I_COMPLETE_AND_CONTINUE == ss))  {
    	if (params.funcs.CompleteAuthToken) {
    		ss = params.funcs.CompleteAuthToken (&pAS->m_hctxt, &OutBuffDesc);
    		if (!SEC_SUCCESS(ss))  {
    			IFDBG(DebugOut(ZONE_ERROR, L"WebProxy:  NTLM complete failed: %X\n", ss));
    			return FALSE;
    		}
    	}
    	else {
    		IFDBG(DebugOut(ZONE_ERROR, L"WebProxy: Complete not supported.\n"));
    		return FALSE;
    	}
    }

    *pcbOut = OutSecBuff.cbBuffer;
    pAS->m_Conversation = NTLM_PROCESSING;

    *pfDone = !((SEC_I_CONTINUE_NEEDED == ss) ||
    			(SEC_I_COMPLETE_AND_CONTINUE == ss));

    return TRUE;
}

void FreeNTLMHandles(PAUTH_NTLM pAuth) {
    ASSERT(pAuth);
    		
    if (pAuth->m_fHaveCtxtHandle)
    	params.funcs.DeleteSecurityContext (&pAuth->m_hctxt);

    if (pAuth->m_fHaveCredHandle)
    	params.funcs.FreeCredentialHandle (&pAuth->m_hcred);

    pAuth->m_fHaveCredHandle = FALSE;
    pAuth->m_fHaveCtxtHandle = FALSE;
}

DWORD GetUserInfo(PAUTH_NTLM pAuth, char* szUser, int cbUser)
{
    DWORD dwRetVal = ERROR_SUCCESS;
    SecPkgContext_Names pkgName;

    pkgName.sUserName = NULL;
    if (! SEC_SUCCESS(params.funcs.QueryContextAttributes(&(pAuth->m_hctxt), SECPKG_ATTR_NAMES, &pkgName))) {
    	dwRetVal = GetLastError();
    	IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: QueryContextAttributes failed.  Cannot determine user name: %d.\n"), dwRetVal));
    	goto exit;
    }

    if (0 == WideCharToMultiByte(CP_ACP, 0, pkgName.sUserName, -1, szUser, cbUser, NULL, NULL)) {
    	dwRetVal = ERROR_INSUFFICIENT_BUFFER;
    	IFDBG(DebugOut(ZONE_ERROR, _T("WebProxy: User buffer is not large enough.\n"), dwRetVal));
    	goto exit;
    }

exit:
    if (pkgName.sUserName) {
    	params.funcs.FreeContextBuffer(pkgName.sUserName);
    }
    
    return dwRetVal;
}


