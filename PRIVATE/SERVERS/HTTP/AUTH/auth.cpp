//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: AUTH.CPP
Abstract: Authentication
--*/

#include "httpd.h"


const DWORD g_fAuthModule = TRUE;

//**********************************************************************
// Authentication
//**********************************************************************
#define NTLM_DOMAIN TEXT("Domain") 	// dummy data.  
#define SEC_SUCCESS(Status) ((Status) >= 0)

#ifndef OLD_CE_BUILD
BOOL NTLMServerContext(
			PSEC_WINNT_AUTH_IDENTITY pAuthIdentity,
			PAUTH_NTLM pAS,BYTE *pIn, DWORD cbIn, BYTE *pOut, 
			DWORD *pcbOut, BOOL *pfDone
	 );

BOOL NTLMClientContext(
			PSEC_WINNT_AUTH_IDENTITY pAuthIdentity,
			PAUTH_NTLM pAS, BYTE *pIn, DWORD cbIn, 
			BYTE *pOut, DWORD *pcbOut
	 );
#endif	 


void CGlobalVariables::InitAuthentication(CReg *pReg) {
	m_fBasicAuth   =  pReg->ValueDW(RV_BASIC);

#if ! defined (OLD_CE_BUILD)
	// On root site, always load the security library whether we are using NTLM
	// or not.  Do this in the case that one of our sub-websites is using NTLM,
	// so we can point it back to the global function pointer table.
	if (! InitSecurityLib())
		return;

	// NTLM support was added for WinCE 3.0.
	m_fNTLMAuth    = pReg->ValueDW(RV_NTLM);
	m_fBasicToNTLM = pReg->ValueDW(RV_BASICTONTLM,TRUE);
#endif
}


// Called after we've mapped the virtual root.
BOOL CHttpRequest::HandleAuthentication(void) {
	if (m_pszAuthType && m_pszRawRemoteUser) {
		if (AllowBasic() && 0==strcmpi(m_pszAuthType, cszBasic)) {
			FreePersistedAuthInfo();
			HandleBasicAuth(m_pszRawRemoteUser);
		}
		else if (AllowNTLM() && 0==strcmpi(m_pszAuthType, cszNTLM)) {
			FreePersistedAuthInfo();
			HandleNTLMAuth(m_pszRawRemoteUser);
		}
		else {
			DEBUGMSG(ZONE_PARSER,(L"HTTPD: Unknown authorization type requested or requested type not enabled\r\n"));
			m_AuthLevelGranted = DeterminePermissionGranted(GetUserList(),m_AuthLevelGranted);
		}
	}
	else if (IsSecure() && m_SSLInfo.m_pClientCertContext) {
		HandleSSLClientCertCheck();
	}
	else {
		m_AuthLevelGranted = DeterminePermissionGranted(GetUserList(),m_AuthLevelGranted);
	}

	return TRUE;
}


// For calls to Basic Authentication, only called during the parsing stage.
BOOL CHttpRequest::HandleBasicAuth(PSTR pszData) {
	char szUserName[MAXUSERPASS];
	PSTR pszFinalUserName;
	DWORD dwLen = sizeof(szUserName);

	DEBUGCHK((m_pszRemoteUser == NULL) && (m_pszPassword == NULL));

	m_AuthLevelGranted = AUTH_PUBLIC;
	m_pszRemoteUser = NULL;

	if (strlen(pszData) >= MAXUSERPASS) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: Base64 data > 256 bytes on Basic Auth, failing request\r\n"));
		return FALSE;
	}

	// decode the base64
	Base64Decode(pszData, szUserName, &dwLen);

	// find the password
	PSTR pszPassword = strchr(szUserName, ':');
	if(!pszPassword) {
		DEBUGMSG(ZONE_ERROR, (L"HTTPD: Bad Format for Basic userpass(%a)-->(%a)\r\n", pszData, szUserName));
		return FALSE;
	}
	*pszPassword++ = 0; // seperate user & pass

	// Some  clients prepend a domain name or machine name in front of user name, preceeding by a '\'.
	// Strip this out.
	if (NULL != (pszFinalUserName = strchr(szUserName,'\\'))) {
		DEBUGMSG(ZONE_AUTH,(L"HTTPD: UserName <<%s>> has \\ in it, skip preceeding data\r\n",szUserName));
		pszFinalUserName++;
	}
	else
		pszFinalUserName = szUserName;

	//  We save the data no matter what, for logging purposes and for possible
	//  GetServerVariable call.
	m_pszRemoteUser = MySzDupA(pszFinalUserName);
	m_pszPassword   = MySzDupA(pszPassword);

	if ((NULL==m_pszRemoteUser) || (NULL==m_pszPassword))
		return FALSE;

	if (! CallAuthFilterIfNeeded())
		return FALSE;

	WCHAR wszPassword[MAXUSERPASS];
	WCHAR wszRemoteUser[MAXUSERPASS];
	MyA2W(m_pszPassword, wszPassword, CCHSIZEOF(wszPassword));

	MyA2W(m_pszRemoteUser,wszRemoteUser, CCHSIZEOF(wszRemoteUser));


	// If AUTH_USER has been granted, check to see if they're an administrator.
	// In BASIC we can only check against the name (no NTLM group info)
#ifdef UNDER_CE
	if (CheckPassword(wszPassword)) {
		if (NULL == (m_wszRemoteUser = MySzDupW(wszRemoteUser)))
			return FALSE;
	
		m_AuthLevelGranted = DeterminePermissionGranted(GetUserList(),AUTH_USER);
		return TRUE;
	}
#endif

#ifndef OLD_CE_BUILD
	if (AllowBasicToNTLM()) {
		// BasicToNTLM and helpers requires m_wszRemoteUser set.
		if (NULL == (m_wszRemoteUser = MySzDupW(wszRemoteUser)))
			return FALSE;

		if (BasicToNTLM(wszPassword))
			return TRUE;

		// On failure, remove m_wszRemoteUser
		ZeroAndFree(m_wszRemoteUser);	
	}
#endif	
	
	DEBUGMSG(ZONE_ERROR, (L"HTTPD: Failed logon with Basic userpass(%a)-->(%a)(%a)\r\n", pszData, pszFinalUserName, pszPassword));
	return FALSE;
}

#ifdef OLD_CE_BUILD
// The web server beta doesn't have NTLM, but it uses this file so we 
// don't use AuthStub.cpp here
BOOL CGlobalVariables::InitSecurityLib(void) {
	return FALSE;
}

BOOL CHttpRequest::HandleNTLMAuth(PSTR pszNTLMData) {
	return FALSE;
}

void FreeNTLMHandles(PAUTH_NTLM pNTLMState) {
	;
}
#else
BOOL CGlobalVariables::InitSecurityLib(void) {
	DEBUG_CODE_INIT;
	FARPROC pInit;
	SECURITY_STATUS ss = 0;
	BOOL ret = FALSE;
	PSecurityFunctionTable pSecFun;

	if (!m_fRootSite)  {
		// No need to reload DLL and get new interface ptrs for each website.
		if (g_pVars->m_hSecurityLib) {
			memcpy(&m_SecurityInterface,&g_pVars->m_SecurityInterface,sizeof(m_SecurityInterface));
			m_cbNTLMMax = g_pVars->m_cbNTLMMax;
			return TRUE;
		}
		return FALSE;
	}
	DEBUGCHK(!m_hSecurityLib);

	m_hSecurityLib = LoadLibrary (NTLM_DLL_NAME);
	if (NULL == m_hSecurityLib)  {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: Unable to load %s, GLE=0x%08x\r\n",NTLM_DLL_NAME,GetLastError()));
		myleave(700);
	}

	pInit = (FARPROC) GetProcAddress (m_hSecurityLib, SECURITY_ENTRYPOINT_CE);
	if (NULL == pInit)  {
		DEBUGCHK(0);
		myleave(701);
	}

	pSecFun = (PSecurityFunctionTable) pInit ();
	if (NULL == pSecFun) {
		DEBUGCHK(0);
		myleave(702);
	}

	memcpy(&m_SecurityInterface,pSecFun,sizeof(SecurityFunctionTable));

	PSecPkgInfo pkgInfo;
	ss = m_SecurityInterface.QuerySecurityPackageInfo (NTLM_PACKAGE_NAME, &pkgInfo);
	if (!SEC_SUCCESS(ss))  {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: QuerySecurityPackageInfo failed, GLE=0x%08x\r\n",GetLastError()));
		myleave(703);
	}

	m_cbNTLMMax = pkgInfo->cbMaxToken;
	m_SecurityInterface.FreeContextBuffer (pkgInfo);

	m_SecurityInterface.EncryptMessage = (ENCRYPT_MESSAGE_FN) m_SecurityInterface.Reserved3;
	m_SecurityInterface.DecryptMessage = (DECRYPT_MESSAGE_FN) m_SecurityInterface.Reserved4;

	DEBUGMSG(ZONE_AUTH,(L"HTTPD: Security Library successfully initialized\r\n"));

	ret = TRUE;
done:
	if (FALSE == ret) {
		CReg reg(HKEY_LOCAL_MACHINE,RK_HTTPD);
		MyFreeLib (m_hSecurityLib);
		m_hSecurityLib = 0;

		// only complain if the user requested NTLM.
		if (reg.ValueDW(RV_NTLM)) {
			if (ss == 0) // if unset above, then API that failed uses SetLastError().
				ss = GetLastError();

			m_pLog->WriteEvent(IDS_HTTPD_AUTH_INIT_ERROR,ss);
		}
	}

	return ret;
}


// This function is called 2 times during an NTLM auth session.  The first
// time it has the Client user name, domain,... which it forwards to DC or
// looks in registry for (this NTLM detail is transparent to httpd)
// On 2nd time it has the client's response, which either is or is not 
// enough to grant access to page.  On 2nd pass, free up all NTLM context data.

// FILTER NOTES:  On IIS no user name or password info is given to the filter
// on NTLM calls, so neither do we.  (WinCE does give BASIC data, though).

BOOL CHttpRequest::HandleNTLMAuth(PSTR pszNTLMData) {
	DEBUG_CODE_INIT;
	BOOL ret = FALSE;
	DWORD dwIn;
	DWORD dwOut;
	BOOL fDone = FALSE;
	PBYTE pOutBuf = NULL;
	PBYTE pInBuf = NULL;   // Base64 decoded data from pszNTLMData

	DEBUGCHK(AllowNTLM());
	
	if (!g_pVars->m_hSecurityLib)
		myretleave(FALSE,94);

	dwOut = g_pVars->m_cbNTLMMax;
	if (NULL == (pOutBuf = MyRgAllocNZ(BYTE,dwOut)))
		myleave(360);

	if (NULL == m_pszNTLMOutBuf) {
		// We will later Base64Encode pOutBuf later, encoding writes 4 outbut bytes
		// for every 3 input bytes
		if (NULL == (m_pszNTLMOutBuf = MyRgAllocNZ(CHAR,dwOut*(4/3) + 1)))
			myleave(361);
	}

	dwIn = strlen(pszNTLMData) + 1;
	if (NULL == (pInBuf = MyRgAllocNZ(BYTE,dwIn)))
		myleave(363);
		
	Base64Decode(pszNTLMData,(PSTR) pInBuf,&dwIn);

	//  On the 1st pass this gets a data blob to be sent back to the client
	//  broweser in pOutBuf, which is encoded to m_pszNTLMOutBuf.  On the 2nd
	//  pass it either authenticates or fails.


	if (! NTLMServerContext(NULL, &m_NTLMState, pInBuf,
							dwIn,pOutBuf,&dwOut,&fDone))
	{
		// Note:  We MUST free the m_pszNTMLOutBuf on 2nd pass failure.  If the 
		// client recieves the blob on a failure
		// it will consider the web server to be malfunctioning and will not send
		// another set of data, and will not prompt the user for a password.
		MyFree(m_pszNTLMOutBuf);


		// Setting to DONE will cause the local structs to be freed; they must
		// be fresh in case browser attempts to do NTLM again with new user name/
		// password on same session.  Don't bother unloading the lib.
		m_NTLMState.m_Conversation = NTLM_DONE;
		myleave(362);
	}
	
	if (fDone) {
		DEBUGMSG(ZONE_AUTH,(L"HTTPD: NTLM Successfully authenticated user\r\n"));
		GetUserAndGroupInfo(&m_NTLMState);
		m_AuthLevelGranted = DeterminePermissionGranted(GetUserList(),AUTH_USER);
		
		m_dwAuthFlags |= m_AuthLevelGranted;
		m_NTLMState.m_Conversation = NTLM_DONE;
		MyFree(m_pszNTLMOutBuf);
		
		myretleave(TRUE,0);
	}
	Base64Encode(pOutBuf,dwOut,m_pszNTLMOutBuf);

	ret = TRUE;
done:
	DEBUGMSG_ERR(ZONE_ERROR,(L"HTTPD: HandleNTLMAuthent died, err = %d, gle = %d\r\n",err,GetLastError()));
	
	MyFree(pOutBuf);
	MyFree(pInBuf);
	return ret;
}



//  Unload the contexts.  The library is NOT freed in this call, only freed
//  in CHttpRequest destructor.
void FreeNTLMHandles(PAUTH_NTLM pNTLMState) {
	if (NULL == pNTLMState || NULL == g_pVars->m_hSecurityLib)
		return;
		
	if (pNTLMState->m_fHaveCtxtHandle)
		g_pVars->m_SecurityInterface.DeleteSecurityContext (&pNTLMState->m_hctxt);

	if (pNTLMState->m_fHaveCredHandle)
		g_pVars->m_SecurityInterface.FreeCredentialHandle (&pNTLMState->m_hcred);

	pNTLMState->m_fHaveCredHandle = FALSE;
	pNTLMState->m_fHaveCtxtHandle = FALSE;
}




//  Given Basic authentication data, we try to "forge" and NTLM request
//  This fcn simulates a client+server talking to each other, though it's in the
//  same proc.  The client is "virtual," doesn't refer to the http client

//  pNTLMState is CHttpRequest::m_NTLMState
BOOL CHttpRequest::BasicToNTLM(WCHAR * wszPassword) {
	DEBUG_CODE_INIT;

	AUTH_NTLM  ClientState;			// forges the client role
	AUTH_NTLM  ServerState;			// forges the server role
	BOOL fDone = FALSE;
	PBYTE pClientOutBuf = NULL;
	PBYTE pServerOutBuf = NULL;
	DWORD cbServerBuf;
	DWORD cbClientBuf;
	
	DEBUGCHK(wszPassword != NULL && m_wszRemoteUser != NULL);
		
	SEC_WINNT_AUTH_IDENTITY AuthIdentityClient = {
					    m_wszRemoteUser, wcslen(m_wszRemoteUser), 
						NTLM_DOMAIN,sizeof(NTLM_DOMAIN)/sizeof(TCHAR) - 1,		
						wszPassword, wcslen(wszPassword), 
						0};		// dummy domain needed

	memset(&ServerState,0,sizeof(AUTH_NTLM));
	memset(&ClientState,0,sizeof(AUTH_NTLM));

	if (!g_pVars->m_hSecurityLib)
		myleave(368);

	// NTLM auth functions seem to expect that these buffer will be zeroed.
	pClientOutBuf = MyRgAllocZ(BYTE,g_pVars->m_cbNTLMMax);		
	if (NULL == pClientOutBuf)
		myleave(370);

	pServerOutBuf = MyRgAllocZ(BYTE,g_pVars->m_cbNTLMMax);		
	if (NULL == pServerOutBuf)
		myleave(371);

	ServerState.m_Conversation = NTLM_NO_INIT_CONTEXT;
	ClientState.m_Conversation = NTLM_NO_INIT_CONTEXT;

	cbClientBuf = cbServerBuf = g_pVars->m_cbNTLMMax;

	//  Main loop that forges client and server talking.
	while (!fDone) {
		cbClientBuf = g_pVars->m_cbNTLMMax;
		if (! NTLMClientContext(&AuthIdentityClient,&ClientState,pServerOutBuf,
								cbServerBuf, pClientOutBuf, &cbClientBuf))
		{
			myleave(372);
		}

		cbServerBuf = g_pVars->m_cbNTLMMax;
		if (! NTLMServerContext(&AuthIdentityClient,&ServerState, pClientOutBuf,
							  cbClientBuf, pServerOutBuf, &cbServerBuf, &fDone))
		{
			myleave(373);
		}
	}

done:
	DEBUGMSG_ERR(ZONE_ERROR,(L"HTTPD: Unable to convert Basic Auth to NTLM Auth, err = %d\r\n",err));
	
	if (fDone) {
		GetUserAndGroupInfo(&ServerState);
		m_AuthLevelGranted = DeterminePermissionGranted(GetUserList(),AUTH_USER);
	}
	
	MyFree(pClientOutBuf);
	MyFree(pServerOutBuf);
	
	FreeNTLMHandles(&ServerState);
	FreeNTLMHandles(&ClientState);

	return fDone;
}

//  This calls the DC (or goes to registry in local case), either getting a
//  data blob to return to client or granting auth or denying.

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
		ss = g_pVars->m_SecurityInterface.AcquireCredentialsHandle (
							NULL,	// principal
							NTLM_PACKAGE_NAME,
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
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: NTLM AcquireCreds failed: %X\n", ss));
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

	ss = g_pVars->m_SecurityInterface.AcceptSecurityContext (
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
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: NTLM init context failed: %X\n", ss));
		return FALSE;
	}

	pAS->m_fHaveCtxtHandle = TRUE;

	// Complete token -- if applicable
	//
	if ((SEC_I_COMPLETE_NEEDED == ss) || (SEC_I_COMPLETE_AND_CONTINUE == ss))  {
		if (g_pVars->m_SecurityInterface.CompleteAuthToken) {
			ss = g_pVars->m_SecurityInterface.CompleteAuthToken (&pAS->m_hctxt, &OutBuffDesc);
			if (!SEC_SUCCESS(ss))  {
				DEBUGMSG(ZONE_ERROR,(L"HTTPD:  NTLM complete failed: %X\n", ss));
				return FALSE;
			}
		}
		else {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: Complete not supported.\n"));
			return FALSE;
		}
	}

	*pcbOut = OutSecBuff.cbBuffer;
	pAS->m_Conversation = NTLM_PROCESSING;

	*pfDone = !((SEC_I_CONTINUE_NEEDED == ss) ||
				(SEC_I_COMPLETE_AND_CONTINUE == ss));

	return TRUE;
}


//  Forges the client browser's part in NTLM communication if the browser
//  sent a Basic request.
BOOL NTLMClientContext(
			PSEC_WINNT_AUTH_IDENTITY pAuthIdentity,
			PAUTH_NTLM pAS,		// NTLM state info
			BYTE *pIn,
			DWORD cbIn,
			BYTE *pOut,
			DWORD *pcbOut) 
{
	SECURITY_STATUS	ss;
	TimeStamp		Lifetime;
	SecBufferDesc	OutBuffDesc;
	SecBuffer		OutSecBuff;
	SecBufferDesc	InBuffDesc;
	SecBuffer		InSecBuff;
	ULONG			ContextAttributes;

	if (NTLM_NO_INIT_CONTEXT == pAS->m_Conversation)  {
		ss = g_pVars->m_SecurityInterface.AcquireCredentialsHandle (
							NULL,	// principal
							NTLM_PACKAGE_NAME,
							SECPKG_CRED_OUTBOUND,
							NULL,	// LOGON id
							pAuthIdentity,	// auth data
							NULL,	// get key fn
							NULL,	// get key arg
							&pAS->m_hcred,
							&Lifetime
							);
		if (SEC_SUCCESS (ss))
			pAS->m_fHaveCredHandle = TRUE;
		else {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: AcquireCreds failed: %X\n", ss));
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
	
	if (NTLM_NO_INIT_CONTEXT != pAS->m_Conversation)  {
		InBuffDesc.ulVersion = 0;
		InBuffDesc.cBuffers = 1;
		InBuffDesc.pBuffers = &InSecBuff;

		InSecBuff.cbBuffer = cbIn;
		InSecBuff.BufferType = SECBUFFER_TOKEN;
		InSecBuff.pvBuffer = pIn;
	}

	ss = g_pVars->m_SecurityInterface.InitializeSecurityContext (
						&pAS->m_hcred,
						(pAS->m_Conversation == NTLM_PROCESSING) ? &pAS->m_hctxt : NULL,
						NULL,  
						0,	// context requirements
						0,	// reserved1
						SECURITY_NATIVE_DREP,
						(pAS->m_Conversation == NTLM_PROCESSING) ?  &InBuffDesc : NULL,
						0,	// reserved2
						&pAS->m_hctxt,
						&OutBuffDesc,
						&ContextAttributes,
						&Lifetime
						);
	if (!SEC_SUCCESS (ss))  {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: init context failed: %X\n", ss));
		return FALSE;
	}

	pAS->m_fHaveCtxtHandle = TRUE;

	// Complete token -- if applicable
	//
	if ((SEC_I_COMPLETE_NEEDED == ss) || (SEC_I_COMPLETE_AND_CONTINUE == ss))  {
		if (g_pVars->m_SecurityInterface.CompleteAuthToken) {
			ss = g_pVars->m_SecurityInterface.CompleteAuthToken (&pAS->m_hctxt, &OutBuffDesc);
			if (!SEC_SUCCESS(ss))  {
				DEBUGMSG(ZONE_ERROR,(L"HTTPD: complete failed: %X\n", ss));
				return FALSE;
			}
		}
		else {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: Complete not supported.\n"));
			return FALSE;
		}
	}

	*pcbOut = OutSecBuff.cbBuffer;
	pAS->m_Conversation = NTLM_PROCESSING;

	return TRUE;
}
#endif // OLD_CE_BUILD


// Gets group and user info from NTLM structure after a user has retrieved info.
// Called after a user has been successfully authenticated with either BASIC or NTLM.
// See \winceos\comm\security\authhlp to see ACL algorithm function/description.
void CHttpRequest::GetUserAndGroupInfo(PAUTH_NTLM pAuth)
{
	PWSTR wsz = NULL;
	SecPkgContext_Names pkgName;
	pkgName.sUserName = NULL;

#if defined(UNDER_CE) && !defined (OLD_CE_BUILD)
	SecPkgContext_GroupNames ContextGroups;	
	ContextGroups.msGroupNames = NULL;
	int ccGroupNamesLen = 0; // null delimited string; wcslen won't work!
#endif	

	// If we're called from NTLM request (pAuth != NULL) then we need 
	// to get the user name if we don't have it.  On BASIC request, we
	// have m_wszRemoteUser name already.
	if ((NULL==m_wszRemoteUser) && ! SEC_SUCCESS(g_pVars->m_SecurityInterface.QueryContextAttributes(&(pAuth->m_hctxt),
	                   SECPKG_ATTR_NAMES, &pkgName)))
	{
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: QueryContextAttributes failed, cannot determine user name!\r\n"));
		goto done; 	// If we can't get user name, don't bother continuing
	}

#if defined(UNDER_CE) && !defined (OLD_CE_BUILD)
	g_pVars->m_SecurityInterface.QueryContextAttributes(&(pAuth->m_hctxt),SECPKG_ATTR_GROUP_NAMES,&ContextGroups);
	// Group list is returned to us a NULL delimited list.  Need to calculate len of this multi-string, wcslen() is out.
	if (ContextGroups.msGroupNames) {
		PWSTR szTrav = ContextGroups.msGroupNames;

		while (*szTrav) {
			szTrav += wcslen(szTrav)+1;
		}
		ccGroupNamesLen = szTrav - ContextGroups.msGroupNames + 1;
	}
#endif
		
	DEBUGMSG(ZONE_AUTH,(L"HTTPD: Admin Users = %s, pkgName.sUserName = %s, group name = %s\r\n",
	         m_pWebsite->m_wszAdminUsers,pkgName.sUserName,ContextGroups.msGroupNames));

done:
	if (pkgName.sUserName) {
		DEBUGCHK(m_wszRemoteUser == NULL);
		m_wszRemoteUser = MySzDupW(pkgName.sUserName);
		g_pVars->m_SecurityInterface.FreeContextBuffer(pkgName.sUserName);
	}
#if defined(UNDER_CE) && !defined (OLD_CE_BUILD)
	DEBUGCHK(m_wszMemberOf == NULL);

	if (ContextGroups.msGroupNames && (ccGroupNamesLen >1)) {
		m_wszMemberOf = MySzAllocW(ccGroupNamesLen);
		if (m_wszMemberOf) {
			memcpy(m_wszMemberOf,ContextGroups.msGroupNames,ccGroupNamesLen*sizeof(WCHAR));
			DEBUGCHK(m_wszMemberOf[ccGroupNamesLen-1] == 0 && m_wszMemberOf[ccGroupNamesLen-2] == 0); // last 2 bytes should = 0.
		}
	}
	if (ContextGroups.msGroupNames)
		g_pVars->m_SecurityInterface.FreeContextBuffer(ContextGroups.msGroupNames);
#endif
}


AUTHLEVEL CHttpRequest::DeterminePermissionGranted(const WCHAR *wszUserList, AUTHLEVEL authDefault) {
	if (!m_wszRemoteUser) {
		DEBUGMSG(ZONE_AUTH,(L"HTTPD: DeterminePermissionGranted returns default level %d because there's no user\r\n",authDefault));
		return authDefault;
	}

	// Administrators always get admin access and access to the page, even if
	// they're barred in the VRoot list.  If there is a vroot list and we fail
	// the IsAccessAllowed test, we set the auth granted to 0 - this
	// will deny access.  If no VRoot user list is set, keep us at AUTH_USER.
	if (m_pWebsite->m_wszAdminUsers && IsAccessAllowed(m_wszRemoteUser,m_wszMemberOf,m_pWebsite->m_wszAdminUsers,FALSE)) {
		DEBUGMSG(ZONE_AUTH,(L"HTTPD: DeterminePermissionGranted returns AUTH_ADMIN because %s is in admin user list\r\n",m_wszRemoteUser));
		return AUTH_ADMIN;
	}
	else if (wszUserList && IsAccessAllowed(m_wszRemoteUser,m_wszMemberOf,(WCHAR*)wszUserList,FALSE)) {
		DEBUGMSG(ZONE_AUTH,(L"HTTPD: DeterminePermissionGranted returns AUTH_USER because %s is in user list\r\n",m_wszRemoteUser));
		return AUTH_USER;
	}
	else if (wszUserList) {
		DEBUGMSG(ZONE_AUTH,(L"HTTPD: DeterminePermissionGranted returns AUTH_PUBLIC because %s was not in user list\r\n",m_wszRemoteUser));
		return AUTH_PUBLIC;
	}
	else {
		DEBUGMSG(ZONE_AUTH,(L"HTTPD: DeterminePermissionGranted returns default level permission %d because not in Adminlist/userlist\r\n",authDefault));
		return authDefault;
	}
}


