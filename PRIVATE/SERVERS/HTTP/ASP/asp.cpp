//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: asp.cpp
Abstract: ASP Handler
--*/

#include "httpd.h"


//****************************************************************
//  Called by httpd before running ASP
//****************************************************************


BOOL CGlobalVariables::InitASP(CReg *pWebsite) {
	CReg reg((HKEY) (*pWebsite), RK_ASP);
	PCWSTR pwszLanguage = NULL;		// don't free this, points to a buffer inside CReg

	if (NULL != (pwszLanguage = reg.ValueSZ(RV_ASP_LANGUAGE)) &&  
		(0 == _wcsicmp(pwszLanguage,L"JSCRIPT") ||
		 0 == _wcsicmp(pwszLanguage,L"JAVASCRIPT")))
	{
		m_ASPScriptLang = JSCRIPT;
	}
	else
		m_ASPScriptLang = VBSCRIPT;

	m_lASPCodePage             = (UINT) reg.ValueDW(RV_ASP_CODEPAGE,CP_ACP);
	m_ASPlcid                  = (LCID) reg.ValueDW(RV_ASP_LCID,LOCALE_SYSTEM_DEFAULT);
	m_fASPVerboseErrorMessages = reg.ValueDW(RV_ASP_VERBOSE_ERR,0);

	DEBUGMSG(ZONE_ASP,(L"HTTPD: ASP Registry defaults -- language = %s, lcid = %d, codepage = %d\r\n",
						 (m_ASPScriptLang == JSCRIPT) ? L"JSCRIPT" : L"VBSCRIPT", m_ASPlcid, m_lASPCodePage));
	return TRUE;
}


// This is the only fcn that httpd calls to get the ASP.dll loaded and running.

// NOTE:  The ASP dll is responsible for reporting all it's own error messages.
// The only time httpd does this is if the ASP crashes or if the file wasn't found.
// ExecuteASP in asp.dll only returns FALSE if there was an exception.

BOOL CHttpRequest::ExecuteASP() {
	DEBUG_CODE_INIT;
	DWORD dwRet = HSE_STATUS_ERROR;
	ASP_CONTROL_BLOCK ACB;
	CISAPI *pISAPI = NULL;
	HINSTANCE hInst = 0;
	
	// First make sure file exists.  If it doesn't, don't bother making the call
	// to the dll.
	if (INVALID_HANDLE_VALUE == (HANDLE) GetFileAttributes(m_wszPath)) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: GetFileAttributes fails on %s, GLE=0x%08x\r\n",m_wszPath,GetLastError()));
		m_rs = GLEtoStatus();
		myleave(620);
	}

	if (NULL==(hInst=g_pVars->m_pISAPICache->Load(ASP_DLL_NAME,&pISAPI,SCRIPT_TYPE_ASP))) {
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: Unable to load %s, GLE=0x%08x\r\n",ASP_DLL_NAME,GetLastError()));
		m_rs = STATUS_INTERNALERR;
		myleave(621);
	}
	
	FillACB((void *) &ACB,hInst);

	__try {
		dwRet = pISAPI->CallASP(&ACB);
	}
	__except(1) {
		m_rs = STATUS_INTERNALERR;
		g_pVars->m_pLog->WriteEvent(IDS_HTTPD_EXT_EXCEPTION,L"asp.dll",GetExceptionCode(),"HttpExtensionProc",GetLastError());
		DEBUGMSG(ZONE_ERROR,(L"HTTPD: exception in asp.dll, exception=0x%08x\r\n",GetExceptionCode()));
		myleave(623);
	}	

	m_fKeepAlive = (dwRet==HSE_STATUS_SUCCESS_AND_KEEP_CONN);
	

done:
	// Don't free hInsnt.  The cache does this automatically.
	g_pVars->m_pISAPICache->Unload(pISAPI);

	return (dwRet!=HSE_STATUS_ERROR);
}

//**************************************************************
//  Callback fcns for ASP
//**************************************************************

//  Modifies headers in our extended header information for Response Put Methods.
BOOL WINAPI AddHeader (HCONN hConn, LPSTR lpszName, LPSTR lpszValue) {
	DEBUGCHK(CHECKSIG(hConn));
	return ((CHttpRequest*)hConn)->m_bufRespHeaders.AddHeader(lpszName, lpszValue,TRUE);
}

//  If there's a buffer, flush it to sock.
BOOL WINAPI Flush(HCONN hConn) {
	DEBUGCHK(CHECKSIG(hConn));
	CHttpRequest *pRequest = (CHttpRequest*)hConn;

	// all these rules straight from docs
	if (FALSE == pRequest->m_fBufferedResponse)
		return FALSE;

	return pRequest->m_bufRespBody.SendBuffer(pRequest);
}

//  If there's a buffer, clear it.
BOOL WINAPI Clear(HCONN hConn) {
	DEBUGCHK(CHECKSIG(hConn));
	CHttpRequest *pRequest = (CHttpRequest*)hConn;

	// all these rules straight from docs
	if (FALSE == pRequest->m_fBufferedResponse)
		return FALSE;

	pRequest->m_bufRespBody.Reset();		
	return TRUE;
}

//  Toggles buffer.  Error checking on this done in ASP calling fnc.
BOOL WINAPI SetBuffer(HCONN hConn, BOOL fBuffer) {
	DEBUGCHK(CHECKSIG(hConn));
	((CHttpRequest*)hConn)->m_fBufferedResponse = fBuffer;
	
	return TRUE;
}

// Reads any remaining bytes off the wire into the request buffer. 
// May need to update some pointers in 
BOOL WINAPI ReceiveCompleteRequest(struct _ASP_CONTROL_BLOCK *pcb) {
	DEBUGCHK(CHECKSIG(pcb->ConnID));
	CHttpRequest *pRequest = (CHttpRequest*)pcb->ConnID;

	pRequest->ReceiveCompleteRequest(pcb);
	return TRUE;
}

// Refill buffer...
BOOL CHttpRequest::ReceiveCompleteRequest(PASP_CONTROL_BLOCK pcb) {
	DEBUGCHK(m_bufRequest.Count() < m_dwContentLength); // only call if we have extra remaining.

	HRINPUT hi = m_bufRequest.RecvBody(m_socket, m_dwContentLength - m_bufRequest.Count(),TRUE, FALSE, this,FALSE);

	if(hi != INPUT_OK && hi != INPUT_NOCHANGE) {
		// only change status if it's unset by lower layer.
		m_rs = (m_rs == STATUS_OK) ? STATUS_BADREQ : m_rs;
		return FALSE;
	}	

	if (g_pVars->m_fFilters  && hi != INPUT_NOCHANGE && 
		! CallFilter(SF_NOTIFY_READ_RAW_DATA)) 	{
		return FALSE;
	}	

	// We need to reset the read pointer because we may have had to reallocate the buffer.
	pcb->pszForm = (PSTR) m_bufRequest.Data();
	return TRUE;
}


//  Setup struct for ASP dll
void CHttpRequest::FillACB(void *p, HINSTANCE hInst) {
	PASP_CONTROL_BLOCK pcb = (PASP_CONTROL_BLOCK) p;
	pcb->cbSize = sizeof(ASP_CONTROL_BLOCK);
	pcb->ConnID = (HCONN) this;

	pcb->cbTotalBytes = m_dwContentLength;
	pcb->cbAvailable  = m_bufRequest.Count();
	pcb->wszFileName = m_wszPath;
	pcb->pszVirtualFileName = m_pszURL;

	if (m_dwContentLength)
		pcb->pszForm = (PSTR) m_bufRequest.Data();
	else
		pcb->pszForm = NULL;

	pcb->pszQueryString            = m_pszQueryString;
	pcb->pszCookie                 = m_pszCookie;
	pcb->scriptLang                = m_pWebsite->m_ASPScriptLang;
	pcb->lcid                      = m_pWebsite->m_ASPlcid;
	pcb->lCodePage                 = m_pWebsite->m_lASPCodePage;
	pcb->fASPVerboseErrorMessages  = m_pWebsite->m_fASPVerboseErrorMessages;
	pcb->hInst                     = hInst;

	pcb->GetServerVariable         = ::GetServerVariable;
	pcb->WriteClient               = ::WriteClient;
	pcb->ServerSupportFunction     = ::ServerSupportFunction;
	pcb->AddHeader                 = ::AddHeader;
	pcb->Flush                     = ::Flush;
	pcb->Clear                     = ::Clear;
	pcb->SetBuffer                 = ::SetBuffer;
	pcb->ReceiveCompleteRequest    = ::ReceiveCompleteRequest;
	pcb->ReadClient                = ::ReadClient;
}
