//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*******************************************************************
 *
 * ScrSite.cpp
 *
 *
 *******************************************************************
 *
 * Description: Implementation of IActiveScriptSite
 ******************************************************************/

// #include "script.h"
#include "aspmain.h"

/* Implementation of IUnknown */

CScriptSite *g_pScriptSite;


STDMETHODIMP_(ULONG) CScriptSite::AddRef(void)
{
	DEBUGMSG(ZONE_MEM,(L"ASP: CScriptSite::AddRef, count will be %d\r\n",m_cRef + 1));
	return InterlockedIncrement(&m_cRef);
};

STDMETHODIMP_(ULONG) CScriptSite::Release(void)
{
	long res = InterlockedDecrement(&m_cRef);
	DEBUGMSG(ZONE_MEM,(L"ASP: CScriptSite::Release, count is %d\r\n",res));
	if (res == 0)
		delete this;

	return res;
};

STDMETHODIMP CScriptSite::QueryInterface(REFIID iid, void** ppvObject)
{
	*ppvObject = NULL;

	if ((iid == IID_IUnknown) || (iid == IID_IActiveScriptSite))
	{
		*ppvObject = (IActiveScriptSite *) this;
		AddRef();
		return S_OK;
	} 

	return E_NOINTERFACE;
};

/* Implementation of IActiveScriptSite */
STDMETHODIMP CScriptSite::GetLCID(unsigned long *pVar)
{
	CASPState *pASPState = (CASPState *) TlsGetValue(g_dwTlsSlot);
	*pVar = (long) pASPState->m_lcid;

	DEBUGMSG(ZONE_INIT,(L"ASP: GetLCID called, lcid = %d\r\n",*pVar));
	return S_OK;
}

/* Gets the dipatcdh for the module or for the global functions */
STDMETHODIMP CScriptSite::GetItemInfo(const unsigned short *wszName,unsigned long dwReturnMask,struct IUnknown **ppunkItem ,struct ITypeInfo **pptiItem)
{
	CASPState *pASPState = (CASPState *) TlsGetValue(g_dwTlsSlot);
	
	if (wcscmp(wszName, L"Response") == 0)
	{
		DEBUGMSG(ZONE_INIT,(L"ASP: CScriptSite::GetItemInfo -  Response\r\n"));
		if (dwReturnMask & SCRIPTINFO_IUNKNOWN) 
		{
			if (FAILED (CoCreateInstance(CLSID_Response,NULL,CLSCTX_INPROC_SERVER,IID_IResponse,(void**) ppunkItem)))
				return DISP_E_EXCEPTION;

			//pASPState->m_pResponse = (CResponse *) *ppunkItem;
		}

		if (dwReturnMask & SCRIPTINFO_ITYPEINFO) 
		{
			*pptiItem = NULL;
		}
		return S_OK;
	}

	if (wcscmp(wszName, L"Request") == 0)
	{
		if (dwReturnMask & SCRIPTINFO_IUNKNOWN) 
		{
			DEBUGMSG(ZONE_INIT,(L"ASP: CScriptSite::GetItemInfo -  Request\r\n"));
			if (FAILED (CoCreateInstance(CLSID_Request,NULL,CLSCTX_INPROC_SERVER,IID_IRequest,(void**) ppunkItem)))
				return DISP_E_EXCEPTION;

			//pASPState->m_pRequest = (CRequest *) *ppunkItem;
		}

		if (dwReturnMask & SCRIPTINFO_ITYPEINFO) 
		{
			*pptiItem = NULL;
		}
		return S_OK;
	}

	if (wcscmp(wszName, L"Server") == 0)
	{
		if (dwReturnMask & SCRIPTINFO_IUNKNOWN) 
		{
			DEBUGMSG(ZONE_INIT,(L"ASP: CScriptSite::GetItemInfo -  Server\r\n"));
			if (FAILED (CoCreateInstance(CLSID_Server,NULL,CLSCTX_INPROC_SERVER,IID_IServer,(void**) ppunkItem)))
				return DISP_E_EXCEPTION;

			//pASPState->m_pServer = (CServer *) *ppunkItem;
		}

		if (dwReturnMask & SCRIPTINFO_ITYPEINFO) 
		{
			*pptiItem = NULL;
		}
		return S_OK;
	}

	return TYPE_E_ELEMENTNOTFOUND;
}

STDMETHODIMP CScriptSite::GetDocVersionString(unsigned short ** )
{
	return E_NOTIMPL;
}

STDMETHODIMP CScriptSite::OnScriptTerminate(const struct tagVARIANT *,const struct tagEXCEPINFO *)
{
	return E_NOTIMPL;
}

STDMETHODIMP CScriptSite::OnStateChange(enum tagSCRIPTSTATE tSS)
{
	return E_NOTIMPL;
}

STDMETHODIMP CScriptSite::OnScriptError(struct IActiveScriptError *pASE)
{
	EXCEPINFO ei;
	DWORD dwCookie;
	ULONG ulLineNum;
	LONG lCharPos;
	HRESULT hr = S_OK;
	CASPState *pASPState = (CASPState *) TlsGetValue(g_dwTlsSlot);

	CHAR szErr[4096];
	CHAR szDescription[128];
	CHAR szSource[128];
	
	DEBUGCHK(pASE != NULL);

	hr = pASE->GetSourcePosition(&dwCookie, &ulLineNum, &lCharPos);

	if (SUCCEEDED(hr))
	{
		hr = pASE->GetExceptionInfo(&ei);
		DEBUGMSG(ZONE_ERROR,(L"ASP: Parse error in script caught by IActiveScript\r\n"));
		DEBUGMSG(ZONE_ERROR,(L"ASP: Error string is <<%s>>, err code = %X\r\n",ei.bstrDescription,ei.scode));

		// We called interupt with Request.End(), no error reporting
		if (ei.scode == E_THREAD_INTERRUPT)
			return S_OK;
			
		if (ei.bstrDescription)
			MyW2A(ei.bstrDescription, szDescription, sizeof(szDescription));
		else
			szDescription[0] = '\0';

		// ei.bstrSource will usually say "Jscript" or "Vbscript" caused runtime error, like IIS.
		// If we don't get this string, send one of our own.  
		if (ei.bstrSource)
		{
			MyW2A(ei.bstrSource, szSource, sizeof(szSource));
		}
		else
		{
			WCHAR wszSource[128];
			MyLoadStringW2A(pASPState->m_pACB->hInst,IDS_ASP_ERROR_FROM_IACTIVE,wszSource,szSource);
		}


		// NOTE:  IIS prints out not only the line number but also the statement
		// that caused the error, underlining the exact spot where it happened.
		// Our script engine only prints out the line number.
#if defined (DEBUG) && defined (DEBUG_ASP_LINE_CODES)
		PSTR pszTrav = NULL;
		CHAR szBadLine[256];		// tries to find offending line
		PSTR pszSubEnd = NULL;
		int i;


		szBadLine[0] = '\0';
		pszTrav = pASPState->m_pszScriptData;

		// We're going to let the strchr's slide in this case, even though it may
		// not be Multinational safe, because it's wrapped in the #ifdef DEBUG.
		// If this code is ever put into the retail then we'd need to fix the strchr.
		
		// find line in script body (always matches original), based on \n count...
		for (i = 0; i < (int) ulLineNum - 1; i++)
		{
			pszTrav = strchr(pszTrav,'\n');	
			if (NULL == pszTrav)
				break;

			pszTrav++;
		}

		if (0 == ulLineNum)
		{
			szBadLine[0] = '\0';
		}
		else if (NULL != pszTrav)
		{
			// it's OK to modify m_pszScriptData info, script uses BSTR
			// and we're done processing, anyway
			if (NULL != (pszSubEnd = strchr(pszTrav,'\n')))
			{	
				*pszSubEnd = '\0';
			}
			else  //  we're at end of string..., last line
			{
				pszSubEnd = strchr(pszTrav,'\0');
				DEBUGCHK(pszSubEnd != NULL);
			}
			DEBUGCHK(pszTrav != pszSubEnd);
			memcpy(szBadLine,  pszTrav, pszSubEnd - pszTrav);
			szBadLine[pszSubEnd - pszTrav] = '\0';
		}

		sprintf(szErr, "%s:  '%08x'\r\n"
					   "<p>Description:  %s\r\n"
					   "<p>Line that caused error:  %s\r\n",
					   szSource, ei.scode, szDescription, szBadLine);  		

#else

		CHAR szFormat[128];
		WCHAR wszFormat[128];

		MyLoadStringW2A(pASPState->m_pACB->hInst,IDS_DESCRIPTION,wszFormat,szFormat);	
		sprintf(szErr, szFormat,szSource, ei.scode, szDescription);  		
#endif


		pASPState->ServerError(szErr,ulLineNum);
	}
	else  
	{
		// should never happen!  Otherwise script writer is left without error info
		DEBUGCHK(FALSE);
		pASPState->ServerError(NULL);
	}
	

	if (SUCCEEDED(hr)) {
		if (E_THREAD_INTERRUPT == ei.scode)	 // we interrupted the thread
			return S_OK; 					 // so don't return an error
	} 
	return hr;
}

STDMETHODIMP CScriptSite::OnEnterScript(void)
{
	return E_NOTIMPL;
}

STDMETHODIMP CScriptSite::OnLeaveScript(void)
{
	return E_NOTIMPL;
}
