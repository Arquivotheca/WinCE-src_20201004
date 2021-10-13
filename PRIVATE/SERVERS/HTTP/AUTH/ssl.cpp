//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: SSL.CPP
Abstract: SSL handling.
--*/

#include "httpd.h"


// This is an undocumented registry setting.  If the user wishes to have HTTPD
// bind to port 443 but wants to use an ISAPI read filter instead of our SSL,
// they should set COMM\HTTP\SSL\Enabled=SKIP_SSL_PROCESSING and HTTPD will
// call recv but will not process the requests.

// This is not for the faint of heart!  This has never received any test and is
// included as a last ditch effort for the desperate.
#define SKIP_SSL_PROCESSING           0x01191977


static HINSTANCE                      hCryptLib;
static HINSTANCE                      hSchannelLib;

PFN_CERTGETNAMESTRINGA                    pCertGetNameStringA;
static PFN_CERTGETNAMESTRINGW             pCertGetNameStringW;
static PFN_CERTFREECERTIFICATECONTEXT     pCertFreeCertificateContext;
static PFN_CERTFINDCERTIFICATEINSTORE     pCertFindCertificateInStore;
static PFN_CERTOPENSTORE                  pCertOpenStore;
static PFN_CERTCLOSESTORE                 pCertCloseStore;
static PFN_SSLCRACKCERTIFICATE            pSSLCrackCert;
static PFN_CERTGETCERTIFICATECHAIN        pCertGetCertificateChain;
static PFN_CERTCERTFREECERTIFICATECHAIN   pCertFreeCertificateChain;

// Modified from wincrypt.h
#define pCertOpenSystemStore(hProv,szSubsystemProtocol) \
    pCertOpenStore(                              \
        CERT_STORE_PROV_SYSTEM_W,               \
        0,                                      \
        (hProv),                                \
        CERT_STORE_NO_CRYPT_RELEASE_FLAG|CERT_SYSTEM_STORE_CURRENT_USER,    \
        (const void *) (szSubsystemProtocol)    \
        )

void ResetSecurityFcnPtrs(void) {
	pCertFreeCertificateContext = NULL;
	pCertGetNameStringA         = NULL;
	pCertGetNameStringW         = NULL;
	pCertFindCertificateInStore = NULL;
	pCertOpenStore              = NULL;
	pCertCloseStore             = NULL;
	pCertGetCertificateChain    = NULL;
	pCertFreeCertificateChain   = NULL;
}

// To allow for devices that don't have CAPI2 but still include HTTPAUTH component,
// load functions at runtime.
BOOL SSLGetFunctionPointers(void) {
	if (pCertCloseStore)
		return TRUE;

	hCryptLib = LoadLibrary(L"crypt32.dll");

	if (hCryptLib == 0) {
		DEBUGMSG(ZONE_INIT,(L"HTTPD: Unable to load crypt32.dll, GLE=0x%08x\r\n",GetLastError()));
		return FALSE;
	}

	pCertFreeCertificateContext = (PFN_CERTFREECERTIFICATECONTEXT)    GetProcAddress(hCryptLib,CE_STRING("CertFreeCertificateContext"));
	pCertGetNameStringA         = (PFN_CERTGETNAMESTRINGA)            GetProcAddress(hCryptLib,CE_STRING("CertGetNameStringA"));
	pCertGetNameStringW         = (PFN_CERTGETNAMESTRINGW)            GetProcAddress(hCryptLib,CE_STRING("CertGetNameStringW"));
	pCertFindCertificateInStore = (PFN_CERTFINDCERTIFICATEINSTORE)    GetProcAddress(hCryptLib,CE_STRING("CertFindCertificateInStore"));
	pCertOpenStore              = (PFN_CERTOPENSTORE)                 GetProcAddress(hCryptLib,CE_STRING("CertOpenStore"));
	pCertCloseStore             = (PFN_CERTCLOSESTORE)                GetProcAddress(hCryptLib,CE_STRING("CertCloseStore"));
	pCertGetCertificateChain    = (PFN_CERTGETCERTIFICATECHAIN)       GetProcAddress(hCryptLib,CE_STRING("CertGetCertificateChain"));
	pCertFreeCertificateChain   = (PFN_CERTCERTFREECERTIFICATECHAIN)  GetProcAddress(hCryptLib,CE_STRING("CertFreeCertificateChain"));
 
	if (!pCertFreeCertificateContext || !pCertGetNameStringA || !pCertFindCertificateInStore ||
	    !pCertOpenStore              || !pCertCloseStore     || !pCertGetNameStringW || !pCertFreeCertificateChain) {
		DEBUGMSG(ZONE_INIT,(L"HTTPD: GetProcAddress fails on Cert functions from crypt32.dll, GLE=0x%08x\r\n",GetLastError()));

		ResetSecurityFcnPtrs();
		return FALSE;
	}

	// Failing this is non-fatal
	hSchannelLib = LoadLibrary(L"schannel.dll");
	if (hSchannelLib) {
		pSSLCrackCert = (PFN_SSLCRACKCERTIFICATE) GetProcAddress(hSchannelLib,SSL_CRACK_CERTIFICATE_NAME);
	}

	return TRUE;
}

// Called when web server is starting up.
void CGlobalVariables::InitSSL(CReg *pWebsite) {
	DEBUG_CODE_INIT;
	DWORD               dwErr = 0;
	WCHAR               wszSubject[MAX_PATH+1];
	PCCERT_CONTEXT      pCertContext = NULL;
	SCHANNEL_CRED       SchannelCred;
	DWORD               dwLen;
	
	DEBUGCHK((m_fSSL == FALSE) && (m_hSSLCertStore == 0) && (m_fHasSSLCreds == 0));
	DEBUGCHK(m_fRootSite); // this should only be called for default site, non-default websites use this still.
	DWORD fEnableSSL;

	CReg reg((HKEY)*pWebsite,RK_SSL);

	m_dwSSLPort = reg.ValueDW(RV_SSL_PORT,IPPORT_SSL);

	if (! (fEnableSSL = reg.ValueDW(RV_SSL_ENABLE))) {
		DEBUGMSG(ZONE_SSL,(L"HTTPD: SSL not enabled via registry settings\r\n"));
		myleave(800);
	}

	if (fEnableSSL == SKIP_SSL_PROCESSING) {
		// HTTPD won't call it's SSL processing, presumably there's an ISAPI filter setup to handle this.
		m_fSSLSkipProcessing = TRUE;
		m_fSSL = TRUE;
		myleave(0);
	}

	if (!SSLGetFunctionPointers())
		myleave(0);

	if (! reg.ValueSZ(RV_SSL_CERT_SUBJECT,wszSubject,ARRAYSIZEOF(wszSubject)))  {
		DEBUGMSG(ZONE_ERROR | ZONE_INIT,(L"HTTPD: SSL has been turned on through registry but key %s wasn't set, required value\r\n",RV_SSL_CERT_SUBJECT));
		myleave(801);
	}

	if (0 == (m_hSSLCertStore = pCertOpenSystemStore(0, L"MY"))) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: CertOpenStore failed, no SSL will be performed.  GLE=0x%08x\r\n",GetLastError()));
		dwErr = GetLastError();
		myleave(802);
	}

	pCertContext = pCertFindCertificateInStore(m_hSSLCertStore, 
	                                          X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
	                                          0,
	                                          CERT_FIND_SUBJECT_STR_W,
	                                          wszSubject,
	                                          NULL);
	if (!pCertContext) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: CertFindCertificateInStore failed, no SSL will be performed.  GLE=0x%08x\r\n",GetLastError()));
		dwErr = GetLastError();
		myleave(803);
	}

	ZeroMemory(&SchannelCred, sizeof(SchannelCred));
	SchannelCred.dwVersion = SCHANNEL_CRED_VERSION;
	SchannelCred.cCreds = 1;
	SchannelCred.paCred = &pCertContext;

	dwErr = m_SecurityInterface.AcquireCredentialsHandle(
	                               	NULL,                 // Name of principal
	                               	UNISP_NAME,           // Name of package
	                               	SECPKG_CRED_INBOUND,  // Flags indicating use
	                               	NULL,                 // Pointer to logon ID
	                               	&SchannelCred,        // Package specific data
	                               	NULL,                 // Pointer to GetKey() func
	                               	NULL,                 // Value to pass to GetKey()
	                               	&m_hSSLCreds,         // (out) Cred Handle
	                               	NULL);                // (out) Lifetime (optional)

	if (dwErr != SEC_E_OK) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: AcquireCredentialsHandle failed, no SSL will be performed.  Error = 0x%08x\r\n",dwErr));
		myleave(804);
	}

	m_fHasSSLCreds = TRUE;

#if defined (UNDER_CE)
	dwLen = pCertGetNameStringA(pCertContext,CERT_NAME_SIMPLE_DISPLAY_TYPE,CERT_NAME_ISSUER_FLAG,NULL,NULL,0);
	if (dwLen != 0 && dwLen != 1)  {
		if (NULL != (m_pszSSLIssuer = MyRgAllocNZ(CHAR,dwLen)))
			pCertGetNameStringA(pCertContext,CERT_NAME_SIMPLE_DISPLAY_TYPE,CERT_NAME_ISSUER_FLAG,NULL,m_pszSSLIssuer,dwLen);
	}

	dwLen = pCertGetNameStringA(pCertContext,CERT_NAME_SIMPLE_DISPLAY_TYPE,0,NULL,NULL,0);
	if (dwLen != 0 && dwLen != 1) {
		if (NULL != (m_pszSSLSubject = MyRgAllocNZ(CHAR,dwLen)))
			pCertGetNameStringA(pCertContext,CERT_NAME_SIMPLE_DISPLAY_TYPE,0,NULL,m_pszSSLSubject,dwLen);
	}

	if (!m_pszSSLIssuer)
		DEBUGMSG(ZONE_SSL,(L"HTTPD: Unable to determine Certificate Issuer\r\n"));

	if (!m_pszSSLSubject)
		DEBUGMSG(ZONE_SSL,(L"HTTPD: Unable to determine Certificate Subject\r\n"));
#endif

	if (NULL == (m_SSLUserMemDescr = svsutil_AllocFixedMemDescr(sizeof(SSLUserMap),10)))
		myleave(809);

	m_SSLUsers.InitializeSSLUsers(&reg,m_SSLUserMemDescr);

	// options to ignore client cert errors.
	m_dwSSLCertTrustOverride  = reg.ValueDW(RV_SSL_CERT_TRUST_OVERRIDE,0);
	m_dwSSLCertTrustOverride  = (~m_dwSSLCertTrustOverride);
	m_fSSL = TRUE;

done:
	if (pCertContext)
		pCertFreeCertificateContext(pCertContext);

	if (!m_fSSL)
		FreeSSLResources();

	if (dwErr) {
		DEBUGCHK(!m_fSSL);
		m_pLog->WriteEvent(IDS_HTTPD_SSL_INIT_ERROR,dwErr);
	}
}

// Called when web server is shutting down.
void CGlobalVariables::FreeSSLResources(void) {
	DEBUGCHK(m_fRootSite);

	if (m_hSSLCertStore) {
		pCertCloseStore(m_hSSLCertStore,0);
		m_hSSLCertStore = 0;
	}

	if (m_fHasSSLCreds) {
		m_SecurityInterface.FreeCredentialHandle(&m_hSSLCreds);
		m_fHasSSLCreds = FALSE;
	}

	if (hCryptLib) {
		FreeLibrary(hCryptLib);
		hCryptLib = 0;
	}

	if (hSchannelLib) {
		FreeLibrary(hSchannelLib);
		hSchannelLib = 0;
	}

	m_SSLUsers.DeInitUsers();

	if (m_SSLUserMemDescr)
		svsutil_ReleaseFixedNonEmpty(m_SSLUserMemDescr);

	ResetSecurityFcnPtrs();
}

// Shuts down SSL handeling when an HTTP session is over.
void CHttpRequest::CloseSSLSession() {
	DEBUG_CODE_INIT;
	DWORD           dwType;

	SecBufferDesc   OutBuffer;
	SecBuffer       OutBuffers[1];
	DWORD           dwSSPIFlags;
	DWORD           dwSSPIOutFlags;

	if (m_SSLInfo.m_pCertChainContext)
		pCertFreeCertificateChain(m_SSLInfo.m_pCertChainContext);

	if (m_SSLInfo.m_pClientCertContext)
		pCertFreeCertificateContext(m_SSLInfo.m_pClientCertContext);

	if (m_SSLInfo.m_fHasCtxt) {
		dwType = SCHANNEL_SHUTDOWN;

		OutBuffers[0].pvBuffer   = &dwType;
		OutBuffers[0].BufferType = SECBUFFER_TOKEN;
		OutBuffers[0].cbBuffer   = sizeof(dwType);

		OutBuffer.cBuffers  = 1;
		OutBuffer.pBuffers  = OutBuffers;
		OutBuffer.ulVersion = SECBUFFER_VERSION;

		if (FAILED(g_pVars->m_SecurityInterface.ApplyControlToken(&m_SSLInfo.m_hcred, &OutBuffer))) {
			DEBUGMSG(ZONE_SSL,(L"HTTPD: ApplyControlToken failed on SCHANNEL_SHUTDOWN."));
			myleave(2000);
		}

		dwSSPIFlags =   ASC_REQ_SEQUENCE_DETECT  | ASC_REQ_REPLAY_DETECT   |
		                ASC_REQ_CONFIDENTIALITY  | ASC_REQ_EXTENDED_ERROR  |
		                ASC_REQ_ALLOCATE_MEMORY  | ASC_REQ_STREAM;

		OutBuffers[0].pvBuffer   = NULL;
		OutBuffers[0].BufferType = SECBUFFER_TOKEN;
		OutBuffers[0].cbBuffer   = 0;

		OutBuffer.cBuffers  = 1;
		OutBuffer.pBuffers  = OutBuffers;
		OutBuffer.ulVersion = SECBUFFER_VERSION;

		if (FAILED(g_pVars->m_SecurityInterface.AcceptSecurityContext(&g_pVars->m_hSSLCreds,&m_SSLInfo.m_hcred,NULL,
		                        dwSSPIFlags,SECURITY_NATIVE_DREP,NULL,&OutBuffer,&dwSSPIOutFlags,NULL))) {

			DEBUGMSG(ZONE_SSL,(L"HTTPD: AcceptSecurity context failed on shutting down SSL connection"));
			myleave (2001);
		}

		if (OutBuffers[0].pvBuffer && OutBuffers[0].cbBuffer) {
			send(m_socket, (PSTR) OutBuffers[0].pvBuffer, OutBuffers[0].cbBuffer, 0);
			g_pVars->m_SecurityInterface.FreeContextBuffer(OutBuffers[0].pvBuffer);
		}
	}

done:
	if (m_SSLInfo.m_fHasCtxt)
		g_pVars->m_SecurityInterface.DeleteSecurityContext(&m_SSLInfo.m_hcred);
}


// from IIS
#define CRED_STATUS_INVALID_TIME    0x00001000
#define CRED_STATUS_REVOKED         0x00002000


BOOL CHttpRequest::CheckClientCert(void)  {
	BOOL                         fRet = FALSE;
	CERT_CHAIN_PARA ChainPara;
	LPSTR  rgpszClientUsage[] = {szOID_PKIX_KP_CLIENT_AUTH,};
	DWORD  dwClientUsageCount = (sizeof(rgpszClientUsage)/sizeof(rgpszClientUsage[0]));
	DWORD  dwErrorStatus;

	if (m_SSLInfo.m_pClientCertContext || m_SSLInfo.m_pCertChainContext) {
		// I don't believe this can happen because web server doesn't accept multiple renegotiates.
		// If it does we'll ignore new client certificate sent across and use existing one.
		DEBUGCHK(0);
		return TRUE;
	}
	DEBUGCHK(m_SSLInfo.m_dwCertFlags == 0);

	// Get Cert Chain information.
	SECURITY_STATUS scRet = g_pVars->m_SecurityInterface.QueryContextAttributes(&m_SSLInfo.m_hcred,
	                                                             SECPKG_ATTR_REMOTE_CERT_CONTEXT,
	                                                             &m_SSLInfo.m_pClientCertContext);

	if (scRet == S_OK && !m_SSLInfo.m_pClientCertContext && ! (GetPerms() & HSE_URL_FLAGS_REQUIRE_CERT)) {
		// If we're only doing HSE_URL_FLAGS_NEGO_CERT and we don't have a certificate
		// then there's no problems.  However if the certificate is garbage (i.e.
		// pCertGetCertificateChain check fails) then we'll fail request in this case.
		fRet = TRUE;
		goto done;
	}

	if (S_OK != scRet || !m_SSLInfo.m_pClientCertContext || !m_SSLInfo.m_pClientCertContext->hCertStore)
		goto done;

	memset(&ChainPara,0,sizeof(ChainPara));

	ChainPara.cbSize = sizeof(CERT_CHAIN_PARA);
	ChainPara.RequestedUsage.dwType = USAGE_MATCH_TYPE_OR;
	ChainPara.RequestedUsage.Usage.cUsageIdentifier = dwClientUsageCount;
	ChainPara.RequestedUsage.Usage.rgpszUsageIdentifier = rgpszClientUsage;

	if (! pCertGetCertificateChain(NULL,m_SSLInfo.m_pClientCertContext,NULL,NULL,
	                                &ChainPara,0,NULL,&m_SSLInfo.m_pCertChainContext)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: CertGetCertificateChain fails, error=0x%08x\r\n",GetLastError()));
		goto done;
	}

	dwErrorStatus = m_SSLInfo.m_pCertChainContext->TrustStatus.dwErrorStatus;
	if (dwErrorStatus) {
		if (dwErrorStatus & g_pVars->m_dwSSLCertTrustOverride) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: SSL Client cert is invalid, failing request.  pCertChainContext->TrustStatus.dwErrorStatus=0x%08x.  Terminating connection.\r\n",dwErrorStatus));
			m_fKeepAlive = FALSE;
			goto done;
		}
		DEBUGMSG(ZONE_SSL,(L"HTTPD: Warning, pCertChainContext->TrustStatus.dwErrorStatus=0x%08x (indicates failure) but was overriden by registry CertTrustOverride setting\r\n",dwErrorStatus));

		if ((dwErrorStatus & CERT_TRUST_IS_NOT_TIME_VALID) || (dwErrorStatus & CERT_TRUST_CTL_IS_NOT_TIME_VALID))
			m_SSLInfo.m_dwCertFlags |= CRED_STATUS_INVALID_TIME;

		if ((dwErrorStatus & CERT_TRUST_IS_UNTRUSTED_ROOT) ||  (dwErrorStatus & CERT_TRUST_IS_PARTIAL_CHAIN) ||
		    (dwErrorStatus & CERT_TRUST_IS_NOT_SIGNATURE_VALID) ||  (dwErrorStatus & CERT_TRUST_CTL_IS_NOT_SIGNATURE_VALID)) {
			m_SSLInfo.m_dwCertFlags |= RCRED_STATUS_UNKNOWN_ISSUER;
		}

		if ((dwErrorStatus & CERT_TRUST_IS_REVOKED) || (dwErrorStatus & CERT_TRUST_REVOCATION_STATUS_UNKNOWN))
			m_SSLInfo.m_dwCertFlags |= CRED_STATUS_REVOKED;
	}
	m_SSLInfo.m_dwCertFlags |= RCRED_CRED_EXISTS;

	fRet = TRUE;
done:
	if (!fRet)
		m_rs = STATUS_FORBIDDEN;

	return fRet;
}

#define SSL_ACCEPT_SECURITY_CONTEXT_FLAGS  (ASC_REQ_SEQUENCE_DETECT |  \
	                                        ASC_REQ_REPLAY_DETECT   |  \
	                                        ASC_REQ_CONFIDENTIALITY |  \
	                                        ASC_REQ_EXTENDED_ERROR  |  \
	                                        ASC_REQ_ALLOCATE_MEMORY |  \
	                                        ASC_REQ_STREAM)

// HandleSSLHandShake is called when a request on a secure channel first comes in
// in order to initiante the SSL connection.  It is also called during a 
// renegotiation of an SSL session, which occurs most likely when web server
// requests it in order to retrieve the client certificate.
BOOL CHttpRequest::HandleSSLHandShake(BOOL fRenegotiate, BYTE *pInitialData, DWORD cbInitialData)  {
	if (m_SSLInfo.m_fSSLInitialized && !fRenegotiate)
		return TRUE;

	CBuffer tempBuf;
	CBuffer *pBuf = fRenegotiate ? &tempBuf : &m_bufRequest;

	DEBUGMSG(ZONE_SSL_VERBOSE,(L"HTTPD: call HandleSSLHandShake(fRenegotiate=%d,pInitialData=0x%08x,cbInitialData=%d,m_socket=0x%08x)\r\n",
	                           fRenegotiate, pInitialData,cbInitialData,m_socket));
#if defined (DEBUG)
	// make sure we should we be in here.
	DEBUGCHK(m_fIsSecurePort && g_pVars->m_fSSL && !g_pVars->m_fSSLSkipProcessing); 
	// make sure buffers are clean.
	DEBUGCHK(!pBuf->m_pszBuf && !pBuf->m_iSize && !pBuf->m_iNextIn);
	// if we're not on renogitate, we shouldn't have any VRoot info yet.
	if (!fRenegotiate)
		DEBUGCHK(!GetPerms());
#endif

	SECURITY_STATUS scRet = SEC_I_CONTINUE_NEEDED;
	PCCERT_CONTEXT  pRemoteCertContext = NULL;

	SecBufferDesc   InBuffer;
	SecBufferDesc   OutBuffer;
	SecBuffer       InBuffers[2];
	SecBuffer       OutBuffers[1];
	DWORD           dwRead;

	BOOL   fInitContext = !fRenegotiate; // when renegotiating we have initialized context already.
	DWORD  dwAvailable;    
	DWORD  dwSSPIOutFlags;
	DWORD  dwSSPIFlags  = SSL_ACCEPT_SECURITY_CONTEXT_FLAGS;
	DWORD  dwTotalRecv  = 0;

	// if renegotiating, we start out with data in pInitialData and cbInitialData, so we don't call recv()
	BOOL   fCallRecv    = !fRenegotiate;
	BOOL   fHasClientCert;

	OutBuffer.cBuffers = 1;
	OutBuffer.pBuffers = OutBuffers;
	OutBuffer.ulVersion = SECBUFFER_VERSION;

	// If m_SSLInfo.m_pClientCertContext we set ASC_REQ_MUTUAL_AUTH to avoid renegotiating.  Occurs during a keep-alive.
	// If m_SSLInfo.m_pClientCertContext=NULL and our VRoot permissions require or request a cert, likely we're in renegotiate to get MUTUAL_AUTH.
	if (m_SSLInfo.m_pClientCertContext || (GetPerms() & (HSE_URL_FLAGS_NEGO_CERT | HSE_URL_FLAGS_REQUIRE_CERT)))
		dwSSPIFlags |= ASC_REQ_MUTUAL_AUTH;

	if (fRenegotiate && cbInitialData) {
		// on initial query we have data in m_bufRequest already.  On renegotiate
		// copy initial data into temporary buffer.
		if (! pBuf->AllocMem(cbInitialData))
			return E_FAIL;

		memcpy(pBuf->m_pszBuf,pInitialData,cbInitialData);
		pBuf->m_iNextIn = cbInitialData;
	}

	while (1) {
		if (fCallRecv && (0 == pBuf->m_iNextIn || scRet == SEC_E_INCOMPLETE_MESSAGE)) {
			DEBUGCHK(dwTotalRecv <= g_pVars->m_dwPostReadSize);
			if (dwTotalRecv == g_pVars->m_dwPostReadSize) {
				DEBUGMSG(ZONE_ERROR,(L"HTTPD: Failing SSL Handshake negotiation because > %d bytes have been sent in it\r\n",g_pVars->m_dwPostReadSize));
				return FALSE;
			}

			DEBUGMSG(ZONE_SSL_VERBOSE,(L"HTTPD: HandleSSLHandShake calling select()\r\n"));
			if (! MySelect(m_socket,g_dwServerTimeout))
				return FALSE;

			// check how much input is waiting
			if(ioctlsocket(m_socket, FIONREAD, &dwAvailable))
				return FALSE;

			// We'll read a maximum of g_pVars->m_dwPostReadSize total to prevent DoS attacks of exhausting sys resources.
			if (dwTotalRecv + dwAvailable > g_pVars->m_dwPostReadSize)
				dwAvailable = g_pVars->m_dwPostReadSize - dwTotalRecv;

			if (!pBuf->AllocMem(dwAvailable))
				return FALSE;

			dwRead = recv(m_socket, pBuf->m_pszBuf+pBuf->m_iNextIn,dwAvailable,0);
			if (dwRead == SOCKET_ERROR || dwRead == 0) {
				DEBUGMSG(ZONE_SOCKET,(L"HTTPD: SSL call to recv failed, GLE=0x%08x\r\n",GetLastError()));
				return FALSE;
			}

			pBuf->m_iNextIn += dwRead;
			dwTotalRecv     += dwRead;
			DEBUGMSG(ZONE_SSL_VERBOSE,(L"HTTPD: HandleSSLHandShake read %d bytes\r\n",dwRead));
		}

		//
		// InBuffers[1] is for getting extra data that
		//  SSPI/SCHANNEL doesn't proccess on this
		//  run around the loop.
		//
		InBuffers[0].pvBuffer   = pBuf->m_pszBuf;
		InBuffers[0].cbBuffer   = pBuf->m_iNextIn;
		InBuffers[0].BufferType = SECBUFFER_TOKEN;

		InBuffers[1].pvBuffer   = NULL;
		InBuffers[1].cbBuffer   = 0;
		InBuffers[1].BufferType = SECBUFFER_EMPTY;

		InBuffer.cBuffers       = 2;
		InBuffer.pBuffers       = InBuffers;
		InBuffer.ulVersion      = SECBUFFER_VERSION;

		fCallRecv = TRUE;
		//
		// Initialize these so if we fail, pvBuffer contains NULL,
		// so we don't try to free random garbage at the quit
		//
		OutBuffers[0].pvBuffer   = NULL;
		OutBuffers[0].BufferType = SECBUFFER_TOKEN;
		OutBuffers[0].cbBuffer   = 0;

		scRet = g_pVars->m_SecurityInterface.AcceptSecurityContext(
		                                   &g_pVars->m_hSSLCreds,
		                                   (fInitContext?NULL:&m_SSLInfo.m_hcred),
		                                   &InBuffer,
		                                   dwSSPIFlags,
		                                   SECURITY_NATIVE_DREP,
		                                   (fInitContext?&m_SSLInfo.m_hcred:NULL),
		                                   &OutBuffer,
		                                   &dwSSPIOutFlags,
		                                   NULL);

		DEBUGMSG(ZONE_SSL_VERBOSE,(L"HTTPD: HandleSSLHandShake AcceptSecurityContext() returns 0x%08x\r\n",scRet));

		fInitContext = FALSE;
		fHasClientCert = dwSSPIOutFlags & ASC_RET_MUTUAL_AUTH;

		if ((GetPerms() & HSE_URL_FLAGS_REQUIRE_CERT) && !fHasClientCert) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: Client certificate required for current vroot, not sent by client browser.  Terminating HTTP request.\r\n"));
			m_rs = STATUS_FORBIDDEN;
			return FALSE;
		}

		if (scRet == SEC_E_OK || scRet == SEC_I_CONTINUE_NEEDED ||
		    (FAILED(scRet) && (0 != (dwSSPIOutFlags & ISC_RET_EXTENDED_ERROR))))  {
			if (OutBuffers[0].cbBuffer != 0 && OutBuffers[0].pvBuffer != NULL) {
				DEBUGMSG(ZONE_SSL_VERBOSE,(L"HTTPD: HandleSSLHandShake sending %d bytes to client\r\n",OutBuffers[0].cbBuffer));
				send( m_socket,(PCSTR)OutBuffers[0].pvBuffer,OutBuffers[0].cbBuffer,0);
			}
			m_SSLInfo.m_fHasCtxt = TRUE;
		}

		if (OutBuffers[0].cbBuffer != 0 && OutBuffers[0].pvBuffer != NULL) 
			g_pVars->m_SecurityInterface.FreeContextBuffer( OutBuffers[0].pvBuffer );

		if (InBuffers[1].BufferType == SECBUFFER_EXTRA)  {
			DEBUGMSG(ZONE_SSL_VERBOSE,(L"HTTPD: HandleSSLHandShake returns SECBUFFER_EXTRA, %d extra bytes\r\n",InBuffers[1].cbBuffer));
			memcpy(pBuf->m_pszBuf,
			       (LPBYTE) (pBuf->m_pszBuf + (pBuf->m_iNextIn - InBuffers[1].cbBuffer)),
			       InBuffers[1].cbBuffer);
			pBuf->m_iNextIn= InBuffers[1].cbBuffer;
		}
		else if (scRet != SEC_E_INCOMPLETE_MESSAGE) {
			DEBUGMSG(ZONE_SSL_VERBOSE,(L"HTTPD: HandleSSLHandShake scRet != SEC_E_INCOMPLETE_MESSAGE, resetting read buffer\r\n"));
			pBuf->Reset();
		}

		if (scRet == SEC_E_OK) {
			SecPkgContext_ConnectionInfo ConnectionInfo;
			
			g_pVars->m_SecurityInterface.QueryContextAttributes(&m_SSLInfo.m_hcred,
			                        SECPKG_ATTR_CONNECTION_INFO,
			                        (PVOID)&ConnectionInfo);

			m_SSLInfo.m_dwSessionKeySize = ConnectionInfo.dwCipherStrength;
			m_SSLInfo.m_dwSSLPrivKeySize = ConnectionInfo.dwHashStrength;

			scRet = g_pVars->m_SecurityInterface.QueryContextAttributes(&m_SSLInfo.m_hcred,
			                            SECPKG_ATTR_STREAM_SIZES,&m_SSLInfo.m_Sizes);

			m_SSLInfo.m_fSSLInitialized = TRUE;
			if (scRet != SEC_E_OK)
				return FALSE;

			DEBUGMSG(ZONE_SSL_VERBOSE,(L"HTTPD: HandleSSLHandShake successfully authenticated.  SessionKeySize = %d, PrivKeySize = %d\r\n",
			                           m_SSLInfo.m_dwSessionKeySize,m_SSLInfo.m_dwSSLPrivKeySize));

			return fHasClientCert ? CheckClientCert() : TRUE;
		}
		else if (FAILED(scRet) && (scRet != SEC_E_INCOMPLETE_MESSAGE)) {
			DEBUGMSG(ZONE_SSL_VERBOSE,(L"HTTPD: HandleSSLHandShake fails, scRet = 0x%08x\n",scRet));
			return FALSE;
		}
	}

	DEBUGCHK(FALSE);
	return FALSE;
}

// When we need a client certificate and don't have one, need to renegotiate the request.
CHttpRequest::SSLRenegotiateRequest(void) {
	BOOL            fRet = FALSE;
	SECURITY_STATUS scRet;
	SecBufferDesc   InBuffer; 
	SecBufferDesc   OutBuffer;
	SecBuffer       OutBuffers[1];

	SecBuffer      Buffers[4];
	DWORD          dwSSPIFlags    = SSL_ACCEPT_SECURITY_CONTEXT_FLAGS | ASC_REQ_MUTUAL_AUTH;
	DWORD          dwSSPIOutFlags = 0;

	InBuffer.ulVersion   = SECBUFFER_VERSION;
	InBuffer.cBuffers    = 4;
	InBuffer.pBuffers    = Buffers;

	Buffers[0].pvBuffer   = "";
	Buffers[0].cbBuffer   = 0;
	Buffers[0].BufferType = SECBUFFER_TOKEN;

	Buffers[1].BufferType = SECBUFFER_EMPTY;
	Buffers[2].BufferType = SECBUFFER_EMPTY;
	Buffers[3].BufferType = SECBUFFER_EMPTY;

	OutBuffer.cBuffers = 1;
	OutBuffer.pBuffers = OutBuffers;
	OutBuffer.ulVersion = SECBUFFER_VERSION;

	OutBuffers[0].pvBuffer   = NULL;
	OutBuffers[0].BufferType = SECBUFFER_TOKEN;
	OutBuffers[0].cbBuffer   = 0;

	DEBUGMSG(ZONE_SSL | ZONE_REQUEST,(L"HTTPD: SSL requesting renegotiation\r\n"));

	DEBUGCHK(! m_fPerformedSSLRenegotiateRequest);

	scRet = g_pVars->m_SecurityInterface.AcceptSecurityContext(&g_pVars->m_hSSLCreds,
		                                   &m_SSLInfo.m_hcred,
		                                   &InBuffer,
		                                   dwSSPIFlags,
		                                   SECURITY_NATIVE_DREP,
		                                   &m_SSLInfo.m_hcred,
		                                   &OutBuffer,
		                                   &dwSSPIOutFlags,
		                                   NULL);
	if (FAILED(scRet)) {
		DEBUGMSG(ZONE_SSL,(L"HTTPD: Renegotiation of SSL channel to retrieve client cert failed on AcceptSecurityContext(),error=0x%08x\r\n",scRet));
		goto done;
	}

	if (! (dwSSPIOutFlags & ASC_RET_MUTUAL_AUTH)) {
		DEBUGMSG(ZONE_SSL,(L"HTTPD: Renegotiation of SSL channel to retrieve client cert failed, SSPI doesn't support ASC_REQ_MUTUAL_AUTH flag\r\n"));
		goto done;
	}

	if (SOCKET_ERROR != send(m_socket,(PSTR)OutBuffers[0].pvBuffer,OutBuffers[0].cbBuffer,0))
		fRet = TRUE;

done:
	if (OutBuffers[0].pvBuffer)
		g_pVars->m_SecurityInterface.FreeContextBuffer(OutBuffers[0].pvBuffer);

	m_fPerformedSSLRenegotiateRequest = TRUE;

	return fRet;
}

// SSLDecrypt Decrypts m_bufRequest buffer during RecvToBuf() call.
//
// Paramaters:
//
//   pszBuf -  pointer to the buffer to decrypt, which in this implementation 
//             will always be a pointer to the first unencrypted byte of m_bufRequest.m_pszBuf.
//             This data is modified in place.
//
//   dwLen  -  number of bytes from pszBuf to the end of the buffer that we've received off the wire.
//
//   pdwBytesDecrypted - returns the number of bytes decrypted during the function.
// 
//   pdwExtraRequired  - If DecryptMessage returns SEC_E_INCOMPLETE_MESSAGE, it also specifies
//                       the number of bytes that need to be read for it to have a complete buffer to unencrypt with.
//
//   pdwIgnore         - Each block of data (there are most likely multiple blocks in a signle HTTP request) has
//                       a header and trailer that are security related only and aren't part of 
//                       the HTTP message as far as the protocol is concerned.  For instance, when HandleRequest()
//                       requests to read 48KB of POST data it wants 48KB of actual POST, any
//                       header and trailer data sizes will be factored out of calculation by RecvToBuf().
//
//  pfCompletedRenegotiate - Set TRUE if we successfully complete a SSL renegotiation, most
//                           likely with the intention of retrieving a client certificate.
//

SECURITY_STATUS CHttpRequest::SSLDecrypt(PSTR pszBuf, DWORD dwLen, DWORD *pdwBytesDecrypted, DWORD *pdwOffset, DWORD *pdwExtraRequired, DWORD *pdwIgnore, BOOL *pfCompletedRenegotiate) {
	SECURITY_STATUS scRet;
	SecBuffer      Buffers[4];
	SecBufferDesc  Message;

	DWORD cbBuffer    = dwLen;
	DWORD dwRemaining = dwLen;
	PVOID pvBuffer    = (PVOID) pszBuf;
	int   iExtra;

	DEBUGCHK((*pdwBytesDecrypted == 0) && (*pdwIgnore==0) && (*pfCompletedRenegotiate==FALSE));

	Message.ulVersion = SECBUFFER_VERSION;
	Message.cBuffers = 4;
	Message.pBuffers = Buffers;

	DEBUGMSG(ZONE_SSL_VERBOSE,(L"HTTPD: SSLDecrypt(pszBuf=0x%08x,dwLen=%d)\r\n",pszBuf,dwLen));

	while (1) {
		Buffers[0].pvBuffer = (PVOID) pvBuffer;
		Buffers[0].cbBuffer = cbBuffer;
		Buffers[0].BufferType = SECBUFFER_DATA;

		Buffers[1].BufferType = SECBUFFER_EMPTY;
		Buffers[2].BufferType = SECBUFFER_EMPTY;
		Buffers[3].BufferType = SECBUFFER_EMPTY;

		scRet = g_pVars->m_SecurityInterface.DecryptMessage(&m_SSLInfo.m_hcred, &Message, 0, NULL);
		if (scRet != SEC_E_OK && scRet != SEC_E_INCOMPLETE_MESSAGE && scRet != SEC_I_RENEGOTIATE) {
			DEBUGMSG(ZONE_ERROR,(L"HTTPD: failed on DecryptMessage, err code = 0x%08x\r\n",scRet));
			return scRet;
		}

		iExtra = (Buffers[2].BufferType == SECBUFFER_EXTRA) ? 2 : 0;
		iExtra = (!iExtra && Buffers[3].BufferType == SECBUFFER_EXTRA) ? 3 : 0;

		if (scRet == SEC_E_OK)  {
			if (m_SSLInfo.m_Sizes.cbHeader)
				memmove(pvBuffer,(PSTR)pvBuffer+m_SSLInfo.m_Sizes.cbHeader,dwRemaining);

			*pdwBytesDecrypted += Buffers[1].cbBuffer;
			*pdwIgnore += m_SSLInfo.m_Sizes.cbHeader+m_SSLInfo.m_Sizes.cbTrailer;
			dwRemaining -= (Buffers[1].cbBuffer+m_SSLInfo.m_Sizes.cbHeader+m_SSLInfo.m_Sizes.cbTrailer);

			DEBUGCHK((int)dwRemaining >= 0);
			DEBUGMSG(ZONE_SSL_VERBOSE,(L"HTTPD: DecryptMessage SEC_E_OK, bytes decrypted=%d, dwRemaining=%d,iExtra=%d\r\n",
			                             Buffers[1].cbBuffer,dwRemaining,iExtra));

			if (iExtra) {
				PSTR pszEnd = (PSTR)pvBuffer+Buffers[1].cbBuffer;
				if (m_SSLInfo.m_Sizes.cbTrailer)
					memmove(pszEnd,pszEnd+m_SSLInfo.m_Sizes.cbTrailer,dwRemaining);

				pvBuffer   = (PVOID) pszEnd;
				*pdwOffset = cbBuffer = Buffers[iExtra].cbBuffer;
				continue;
			}
		}
		else if (scRet == SEC_I_RENEGOTIATE) {
			DEBUGMSG(ZONE_SSL_VERBOSE,(L"HTTPD: DecryptMessage SEC_I_RENEGOTIATE\r\n"));

			if (m_fPerformedClientInitiatedRenegotiate) {
				// We only allow client to request one renegotiate per session.  We
				// do this to prevent denial of service attacks where client kept sending
				// a renegotiate every few seconds.
				DEBUGMSG(ZONE_ERROR,(L"HTTPD: Client has requested more than one SSL renegotiation, web server does not support this, failing request\r\n"));
				return E_FAIL;
			}

			if (! HandleSSLHandShake(TRUE))
				return E_FAIL;

			m_fPerformedClientInitiatedRenegotiate = TRUE;
			*pfCompletedRenegotiate = TRUE;

//			*pdwBytesDecrypted += Buffers[1].cbBuffer;
//			dwRemaining -= (Buffers[1].cbBuffer+m_SSLInfo.m_Sizes.cbHeader+m_SSLInfo.m_Sizes.cbTrailer);
			*pdwIgnore  += m_SSLInfo.m_Sizes.cbHeader+m_SSLInfo.m_Sizes.cbTrailer;
			dwRemaining -= (m_SSLInfo.m_Sizes.cbHeader+m_SSLInfo.m_Sizes.cbTrailer);

			if (iExtra) {
				PSTR pszEnd = (PSTR)pvBuffer+Buffers[1].cbBuffer;
				if (m_SSLInfo.m_Sizes.cbTrailer)
					memmove(pszEnd,pszEnd+m_SSLInfo.m_Sizes.cbTrailer,dwRemaining);

				pvBuffer   = (PVOID) pszEnd;
				*pdwOffset = cbBuffer = Buffers[iExtra].cbBuffer;
				continue;
			}
			scRet = SEC_E_OK;
		}
		else  { // SEC_E_INCOMPLETE_MESSAGE
			DEBUGCHK(Buffers[1].BufferType == SECBUFFER_MISSING);
			*pdwExtraRequired = Buffers[1].cbBuffer;
			DEBUGMSG(ZONE_SSL_VERBOSE,(L"HTTPD: DecryptMessage SEC_E_INCOMPLETE_MESSAGE, *pdwExtraRequired=%d\r\n",*pdwExtraRequired));
		}
		
		*pdwOffset = dwLen - *pdwBytesDecrypted - *pdwIgnore;
		
		DEBUGCHK(Buffers[2].BufferType == SECBUFFER_STREAM_TRAILER || Buffers[2].BufferType == SECBUFFER_EMPTY);
		break;
	}
	return scRet;
}

BOOL CHttpRequest::SendEncryptedData(PSTR pszBuf, DWORD dwLen, BOOL fCopyBuffer) {
	DEBUG_CODE_INIT;
	BOOL            fRet = FALSE;
	SecBuffer       Buffers[4];
	SecBufferDesc   Message;
	CHAR            szHeader[64];
	CHAR            szTrailer[64];
	SECURITY_STATUS scRet;
	DWORD           dwRemaining;
	DWORD           dwOffset = 0;
	CHAR            szStaticBuf[2048];
	PSTR            pszSendBuf = pszBuf;

	DEBUGCHK(sizeof(szHeader) >= m_SSLInfo.m_Sizes.cbHeader);
	DEBUGCHK(sizeof(szTrailer) >= m_SSLInfo.m_Sizes.cbTrailer);
	DEBUGCHK(m_fIsSecurePort && g_pVars->m_fSSL);

	Message.ulVersion = SECBUFFER_VERSION;
	Message.cBuffers = 4;
	Message.pBuffers = Buffers;

	Buffers[0].pvBuffer = szHeader;
	Buffers[0].cbBuffer = m_SSLInfo.m_Sizes.cbHeader;
	Buffers[0].BufferType = SECBUFFER_STREAM_HEADER;

	Buffers[2].pvBuffer = szTrailer;
	Buffers[2].cbBuffer = m_SSLInfo.m_Sizes.cbTrailer;
	Buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;

	Buffers[3].BufferType = SECBUFFER_EMPTY;

	dwRemaining = dwLen;

	if (fCopyBuffer) {
		if (dwLen <= sizeof(szStaticBuf)) {
			pszSendBuf = szStaticBuf;
		}
		else {
			if (! (pszSendBuf = MyRgAllocNZ(CHAR,dwLen)))
				myleave(1908);
		}
		memcpy(pszSendBuf,pszBuf,dwLen);
	}

	while (dwRemaining) {
		DWORD dwSend = min(dwRemaining,m_SSLInfo.m_Sizes.cbMaximumMessage);

		Buffers[1].pvBuffer   = pszSendBuf+dwOffset;
		Buffers[1].cbBuffer   = dwSend;
		Buffers[1].BufferType = SECBUFFER_DATA;

		// Note: Assuming nagling is turned on the header+trailer should NOT be sent
		// across in separate packets but along with body.
		scRet = g_pVars->m_SecurityInterface.EncryptMessage(&m_SSLInfo.m_hcred,0,&Message,0);
		if (scRet != SEC_E_OK && scRet != SEC_E_INCOMPLETE_MESSAGE)
			myleave(1909);

		if (SOCKET_ERROR == send(m_socket,(PSTR)Buffers[0].pvBuffer,Buffers[0].cbBuffer,0))
			myleave(1910);

		if (SOCKET_ERROR == send(m_socket,(PSTR)Buffers[1].pvBuffer,Buffers[1].cbBuffer,0))
			myleave(1911);

		if (SOCKET_ERROR == send(m_socket,(PSTR)Buffers[2].pvBuffer,Buffers[2].cbBuffer,0))
			myleave(1912);		

		dwOffset += dwSend;
		dwRemaining -= dwSend;
	}

	fRet = TRUE;
done:
	DEBUGMSG_ERR(ZONE_RESPONSE | ZONE_SSL,(L"HTTPD: SendEncryptedData failed, err = %d, GLE=0x%08x\r\n",err,GetLastError()));
	if (pszSendBuf != szStaticBuf && pszSendBuf != pszBuf) {
		DEBUGCHK(fCopyBuffer);
		MyFree(pszSendBuf);
	}
	return fRet;
}

//
//  CSSLUsers implementation functions
//
VOID
ReverseMemCopy(PUCHAR Dest, PUCHAR Source, ULONG Size) {
	PUCHAR  p;

	p = Dest + Size - 1;
	do {
		*p-- = *Source++;
	} while (p >= Dest);
}

// Called on web server initialization, reads SSL users into table from registry.
void CSSLUsers::InitializeSSLUsers(CReg *pSSLReg, FixedMemDescr *pUserMem) {
#if defined (DEBUG) || defined (_DEBUG)
	BYTE b[sizeof(CSSLUsers)] = {0};
	DEBUGCHK(0 == memcmp(&b,this,sizeof(CSSLUsers)));
#endif
	CReg  rootReg((HKEY) (*pSSLReg),RK_USERS);

	if (! rootReg.IsOK())
		return;

	DWORD cUsers = rootReg.NumSubkeys();
	if (0 == cUsers)	
		return;

	for (DWORD i = 0; i < cUsers; i++) {
		CReg  userReg;
		WCHAR szUserName[MAX_PATH];
		DWORD cbUserName;  // size in bytes, not WCHARs

		if (! rootReg.EnumKey(szUserName,SVSUTIL_ARRLEN(szUserName)))
			continue;

		if (! userReg.Open(rootReg,szUserName)) {
			DEBUGCHK(0); // EnumKey shouldn't have suceeded if Opening can fail.
			continue;
		}

		DWORD cMappings = userReg.NumSubkeys();
		if (0 == cMappings)
			continue;

		cbUserName = (wcslen(szUserName)+1)*sizeof(WCHAR);

		for (DWORD j = 0; j < cMappings; j++) {
			CReg mapReg;
			WCHAR szUserMap[MAX_PATH];

			// open mapping registry key and read in its parameters
			if (! userReg.EnumKey(szUserMap,SVSUTIL_ARRLEN(szUserMap)))
				continue;

			if (! mapReg.Open(userReg,szUserMap)) {
				DEBUGCHK(0);
				continue;
			}

			WCHAR szIssuerCN[MAX_ISSUER_LEN];
			BYTE  bSerialNumber[MAX_SERIAL_NUMBER];
			DWORD cbIssuerCN     = sizeof(szIssuerCN);
			DWORD cbSerialNumber = sizeof(bSerialNumber);;

			if (! mapReg.IsOK()) {
				DEBUGCHK(0);
				continue;
			}

			if (ERROR_SUCCESS != RegQueryValueEx(mapReg, RV_SSL_ISSUER_CN, NULL, NULL, (LPBYTE)szIssuerCN, &cbIssuerCN))
				cbIssuerCN = 0;

			if (ERROR_SUCCESS != RegQueryValueEx(mapReg, RV_SSL_SERIAL_NUMBER, NULL, NULL, (LPBYTE)bSerialNumber, &cbSerialNumber))
				cbSerialNumber = 0;
			
			if ((cbIssuerCN == 0) && (cbSerialNumber == 0)) {
				DEBUGMSG(ZONE_SSL,(L"HTTPD: Initialize SSL skipping user %s's path %s, either issuer or serial number must be set\r\n",szUserName,szUserMap));
				continue;
			}

			SSLUserMap *pNew = (SSLUserMap *)svsutil_GetFixed(pUserMem);
			if (! pNew)
				return;

			pNew->pNext  = pUserMapHead;
			pUserMapHead = pNew;

			memset(pNew,0,sizeof(SSLUserMap));
			DWORD cbAlloc = cbUserName+cbIssuerCN+cbSerialNumber;

			if (NULL == (pNew->pBuffer = MyRgAllocNZ(BYTE,cbAlloc)))
				return;

			// write this mapping information to the buffer
			BYTE *pWrite = pNew->pBuffer;

			memcpy(pWrite,szUserName,cbUserName);
			pNew->szUserName = (WCHAR*)pWrite;
			pWrite += cbUserName;

			if (cbIssuerCN) {
				memcpy(pWrite,szIssuerCN,cbIssuerCN);
				pNew->szIssuerCN = (WCHAR*)pWrite;
				pWrite += cbIssuerCN;
			}

			pNew->cbSerialNumber = cbSerialNumber;
			if (cbSerialNumber) {
				// Serial number is in reverse byte order in cert.  Reverse it 
				// now so we can do memcmp's per request.
				ReverseMemCopy(pWrite,bSerialNumber,cbSerialNumber);
				pNew->pSerialNumber = pWrite;
				pWrite += cbSerialNumber;
			}
			DEBUGCHK(pWrite == (pNew->pBuffer + cbAlloc));
		}
	}
}

// Called once an SSL client certificate has been received, uses the cert's serial number 
// and/or issuer to see if it maps to a web server "pseudo-user".
WCHAR * CSSLUsers::MapUser(BYTE *pSerialNumber, DWORD cbSerialNumber, WCHAR *szIssuerCN) {
	SSLUserMap *pTrav = pUserMapHead;

	while (pTrav) {
		// For the SSLUserMap, the serial number and/or issuer is present.  If 
		// both members are set the certificate needs to match both of them, otherwise
		// we're fine if we only match the one present field.
		DEBUGCHK(pTrav->szUserName && ((pTrav->cbSerialNumber && pTrav->pSerialNumber) || pTrav->szIssuerCN));
		
		if (pTrav->cbSerialNumber && ((pTrav->cbSerialNumber != cbSerialNumber) || 
		   (0 != memcmp(pTrav->pSerialNumber,pSerialNumber,cbSerialNumber)))) 
		{
			// User mapping had a serial number but it didn't match passed in certificate.
			pTrav = pTrav->pNext;
		  	continue; 
		}

		if (pTrav->szIssuerCN && (0 != wcscmp(szIssuerCN,pTrav->szIssuerCN))) {
			// User mapping had IssuerCN but it didn't match passed in certificate.
			pTrav = pTrav->pNext;
			continue;
		}

		return pTrav->szUserName;
	}
	return NULL;
}

// Called once an SSL client certificate has been received, tries to map
// certificate to user.
BOOL CHttpRequest::HandleSSLClientCertCheck(void) {
	WCHAR szIssuer[2048];
	WCHAR *szUser;

	// the virtual root must request a mapping explicitly.
	if (! (GetPerms() & HSE_URL_FLAGS_MAP_CERT))
		return FALSE;

	// don't have SSL info.
	if (!m_fHandleSSL || !m_SSLInfo.m_pClientCertContext || 
	    !m_SSLInfo.m_pClientCertContext->pCertInfo       || 
	    !m_SSLInfo.m_pClientCertContext->pCertInfo->SerialNumber.pbData)
		return FALSE;

	int  cbSerialNum    = m_SSLInfo.m_pClientCertContext->pCertInfo->SerialNumber.cbData;
	BYTE *pSerialNumber = m_SSLInfo.m_pClientCertContext->pCertInfo->SerialNumber.pbData;

	if (m_wszRemoteUser == NULL) {
		if (! pCertGetNameStringW(m_SSLInfo.m_pClientCertContext,CERT_NAME_SIMPLE_DISPLAY_TYPE,CERT_NAME_ISSUER_FLAG,0,szIssuer,sizeof(szIssuer)))
			return FALSE;

		if (NULL == (szUser = g_pVars->m_SSLUsers.MapUser(pSerialNumber,cbSerialNum,szIssuer)))
			return FALSE;

		m_wszRemoteUser = MySzDupW(szUser);
	}
	DeterminePermissionGranted(GetUserList(),m_AuthLevelGranted);

	DEBUGMSG(ZONE_SSL_VERBOSE,(L"HTTPD: SSL Client cert mapping retrieves user=<<%s>>, auth granted = %d\r\n",szUser,m_AuthLevelGranted));
	return TRUE;
}
