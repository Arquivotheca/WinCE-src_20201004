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
Module Name: asp.cpp
Abstract: ASP Handler
--*/

#include "httpd.h"


//****************************************************************
//  Called by httpd before running ASP
//****************************************************************


BOOL CGlobalVariables::InitASP(CReg *pWebsite) 
{
    CReg reg((HKEY) (*pWebsite), RK_ASP);
    PCWSTR pwszLanguage = NULL;        // don't free this, points to a buffer inside CReg

    if (NULL != (pwszLanguage = reg.ValueSZ(RV_ASP_LANGUAGE)) &&  
        (0 == _wcsicmp(pwszLanguage,L"JSCRIPT") ||
         0 == _wcsicmp(pwszLanguage,L"JAVASCRIPT")))
    {
        m_ASPScriptLang = JSCRIPT;
    }
    else
    {
        m_ASPScriptLang = VBSCRIPT;
    }

    m_lASPCodePage             = (UINT) reg.ValueDW(RV_ASP_CODEPAGE,CP_ACP);
    m_ASPlcid                  = (LCID) reg.ValueDW(RV_ASP_LCID,LOCALE_SYSTEM_DEFAULT);
    m_fASPVerboseErrorMessages = reg.ValueDW(RV_ASP_VERBOSE_ERR,1);
    m_dwAspMaxFormSize         = reg.ValueDW(RV_ASP_MAX_FORM,0);

    DEBUGMSG(ZONE_ASP,(L"HTTPD: ASP Registry defaults -- language = %s, lcid = %d, codepage = %d\r\n",
                         (m_ASPScriptLang == JSCRIPT) ? L"JSCRIPT" : L"VBSCRIPT", m_ASPlcid, m_lASPCodePage));
    return TRUE;
}


// This is the only fcn that httpd calls to get the ASP.dll loaded and running.

// NOTE:  The ASP dll is responsible for reporting all it's own error messages.
// The only time httpd does this is if the ASP crashes or if the file wasn't found.
// ExecuteASP in asp.dll only returns FALSE if there was an exception.

BOOL CHttpRequest::ExecuteASP() 
{
    DEBUG_CODE_INIT;
    DWORD dwRet = HSE_STATUS_ERROR;
    ASP_CONTROL_BLOCK ACB;
    CISAPI *pISAPI = NULL;
    HINSTANCE hInst = 0;
    
    // First make sure file exists.  If it doesn't, don't bother making the call
    // to the dll.
    if (INVALID_HANDLE_VALUE == (HANDLE) GetFileAttributes(m_wszPath)) 
    {
        DEBUGMSG(ZONE_ERROR,(L"HTTPD: GetFileAttributes fails on %s, GLE=0x%08x\r\n",m_wszPath,GetLastError()));
        m_rs = GLEtoStatus();
        myleave(620);
    }

    if (NULL==(hInst=g_pVars->m_pISAPICache->Load(ASP_DLL_NAME,&pISAPI,SCRIPT_TYPE_ASP))) 
    {
        DEBUGMSG(ZONE_ERROR,(L"HTTPD: Unable to load %s, GLE=0x%08x\r\n",ASP_DLL_NAME,GetLastError()));
        m_rs = STATUS_INTERNALERR;
        myleave(621);
    }
    
    FillACB((void *) &ACB,hInst);

    __try 
    {
        dwRet = pISAPI->CallASP(&ACB);
    }
    __except(ReportFault(GetExceptionInformation(),0), EXCEPTION_EXECUTE_HANDLER) 
    {
        m_rs = STATUS_INTERNALERR;
        g_pVars->m_pLog->WriteEvent(IDS_HTTPD_EXT_EXCEPTION,L"asp.dll",GetExceptionCode(),"HttpExtensionProc",GetLastError());
        DEBUGMSG(ZONE_ERROR,(L"HTTPD: exception in asp.dll, exception=0x%08x\r\n",GetExceptionCode()));
        myleave(623);
    }    

   SetConnectionPersist(dwRet==HSE_STATUS_SUCCESS_AND_KEEP_CONN ?
                     CONN_WANTKEEP : CONN_WANTCLOSE);
    

done:
    // Don't free hInsnt.  The cache does this automatically.
    g_pVars->m_pISAPICache->Unload(pISAPI);

    return (dwRet!=HSE_STATUS_ERROR);
}

//**************************************************************
//  Callback fcns for ASP
//**************************************************************

//  Modifies headers in our extended header information for Response Put Methods.
BOOL WINAPI AddHeader (HCONN hConn, LPSTR lpszName, LPSTR lpszValue) 
{
    DEBUGCHK(CHECKSIG(hConn));
    return ((CHttpRequest*)hConn)->m_bufRespHeaders.AddHeader(lpszName, lpszValue,TRUE);
}

//  If there's a buffer, flush it to sock.
BOOL WINAPI Flush(HCONN hConn) 
{
    DEBUGCHK(CHECKSIG(hConn));
    CHttpRequest *pRequest = (CHttpRequest*)hConn;

    // all these rules straight from docs
    if (FALSE == pRequest->m_fBufferedResponse)
    {
        return FALSE;
    }

    return pRequest->m_bufRespBody.SendBuffer(pRequest);
}

//  If there's a buffer, clear it.
BOOL WINAPI Clear(HCONN hConn) 
{
    DEBUGCHK(CHECKSIG(hConn));
    CHttpRequest *pRequest = (CHttpRequest*)hConn;

    // all these rules straight from docs
    if (FALSE == pRequest->m_fBufferedResponse)
    {
        return FALSE;
    }

    pRequest->m_bufRespBody.Reset();        
    return TRUE;
}

//  Toggles buffer.  Error checking on this done in ASP calling fnc.
BOOL WINAPI SetBuffer(HCONN hConn, BOOL fBuffer) 
{
    DEBUGCHK(CHECKSIG(hConn));
    ((CHttpRequest*)hConn)->m_fBufferedResponse = fBuffer;
    
    return TRUE;
}

// Reads any remaining bytes off the wire into the request buffer. 
// May need to update some pointers in 
BOOL WINAPI ReceiveCompleteRequest(struct _ASP_CONTROL_BLOCK *pcb) 
{
    DEBUGCHK(CHECKSIG(pcb->ConnID));
    CHttpRequest *pRequest = (CHttpRequest*)pcb->ConnID;

    return pRequest->ReceiveCompleteRequest(pcb);
}

// Refill buffer...
BOOL CHttpRequest::ReceiveCompleteRequest(PASP_CONTROL_BLOCK pcb) 
{
    DEBUGCHK(m_bufRequest.Count() < m_dwContentLength); // only call if we have extra remaining.

    if (m_pWebsite->m_dwAspMaxFormSize < m_dwContentLength) 
    {
        // Check to make sure there's not a huge HTTP POST body, as if we tried
        // to read that in we would exhaust system memory.  Because ASP Request.Form
        // is a random access data structure, we need to have all the data read it 
        // in one block so hence if we're sent a huge block, must reject it.

        // Prior to this call we have read in m_dwPostReadSize (default 48KB) bytes
        DEBUGMSG(ZONE_ERROR,(L"HTTPD: ERROR: ASP requested ALL HTTP body, maxFormSize (%d) is < content length (%d)\r\n",
                              m_pWebsite->m_dwAspMaxFormSize,m_dwContentLength));
        return FALSE;
    }

    HRINPUT hi = m_bufRequest.RecvBody(m_socket, m_dwContentLength - m_bufRequest.Count(),TRUE, FALSE, this,FALSE);

    if(hi != INPUT_OK && hi != INPUT_NOCHANGE) 
    {
        // only change status if it's unset by lower layer.
        m_rs = (m_rs == STATUS_OK) ? STATUS_BADREQ : m_rs;
        return FALSE;
    }    

    if (g_pVars->m_fFilters  && hi != INPUT_NOCHANGE && 
        ! CallFilter(SF_NOTIFY_READ_RAW_DATA))     
    {
        return FALSE;
    }    

    // We need to reset the read pointer because we may have had to reallocate the buffer.
    pcb->pszForm = (PSTR) m_bufRequest.Data();
    return TRUE;
}


//  Setup struct for ASP dll
void CHttpRequest::FillACB(void *p, HINSTANCE hInst) 
{
    PASP_CONTROL_BLOCK pcb = (PASP_CONTROL_BLOCK) p;
    pcb->cbSize = sizeof(ASP_CONTROL_BLOCK);
    pcb->ConnID = (HCONN) this;

    pcb->cbTotalBytes = m_dwContentLength;
    pcb->cbAvailable  = m_bufRequest.Count();
    pcb->wszFileName = m_wszPath;
    pcb->pszVirtualFileName = m_pszURL;

    if (m_dwContentLength)
    {
        pcb->pszForm = (PSTR) m_bufRequest.Data();
    }
    else
    {
        pcb->pszForm = NULL;
    }

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
