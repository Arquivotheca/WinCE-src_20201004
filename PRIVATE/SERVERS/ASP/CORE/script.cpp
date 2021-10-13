//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: script.cpp
Abstract: main interface between httpd and asp.dll
--*/


/*
#include "stdafx.h"
#include "Asp.h"
#include "script.h"
#include "dict.h"		
#include "strlist.h"
*/

#include "aspmain.h"

const char* rgWkday[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", 0 };
const char* rgMonth[] = { "", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", 0 };
const char cszDateOutputFmt[]     = "%3s, %02d %3s %04d %02d:%02d:%02d GMT";
const char cszTextHTML[]		= "text/html";
const char cszContentTextHTML[] = "Content-Type: text/html";
const char cszKeepAlive[]  = "Connection: keep-alive\r\nContent-Length: %d\r\n"; // written directly to buf, include CRLF.
const char cszKeepAliveText[]  = "keep-alive\r\nContent-Length: %d"; // called with AddHeader, don't include Connection or CRLF in this string

#define ADDCRLF(szHeader, i)  { (szHeader)[(i++)] = '\r'; (szHeader)[(i++)] = '\n'; }
// 
//------------- Debug data -----------------------
//
#ifdef UNDER_CE
#ifdef DEBUG
  DBGPARAM dpCurSettings = {
    TEXT("ASP"), {
    TEXT("Error"),TEXT("Init"),TEXT("Script"),TEXT("Server"),
    TEXT("Request"),TEXT("Response"),TEXT("RequestDict"),
    TEXT("Mem"),TEXT(""),TEXT(""),TEXT(""),
    TEXT(""),TEXT(""),TEXT(""),TEXT(""),TEXT("") },
    0x0001
  }; 
#endif
#endif


DWORD g_dwTlsSlot;


// #define TEST_PERFORMANCE 
#ifdef TEST_PERFORMANCE
// NOTE: this is NOT thread safe, if TEST_PERFORMANCE is defined (never defined by default) make
// sure only one request comes at a time.  For private dev tests only.
LARGE_INTEGER g_rgliCount[100];
int g_dwPerfCounter;

#define QUERY_COUNT()   QueryPerformanceCounter(&g_rgliCount[g_dwPerfCounter++]);

#else
#define QUERY_COUNT()   
#endif

//  This function is only called by httpd when it's freeing the ISAPI cache
//  for ASP.
void TerminateASP()
{
	DEBUGMSG(ZONE_INIT,(L"ASP:  Calling CoFreeUnusedLibraries()\r\n"));
	CoInitializeEx(NULL,COINIT_MULTITHREADED);
	CoFreeUnusedLibraries();
	CoUninitialize();	
}

//   HTTPD calls this function to handle script processing.
DWORD ExecuteASP(PASP_CONTROL_BLOCK pASP)
{
	DEBUG_CODE_INIT;
	DWORD ret = HSE_STATUS_ERROR;
	CASPState *pASPState = NULL;

	QUERY_COUNT();
	DEBUGMSG(ZONE_INIT,(L"ASP: ExecuteASP successfully called, ASP handler beginning\r\n"));

	DEBUGCHK(pASP != NULL && pASP->wszFileName != NULL);
	DEBUGCHK(pASP->cbSize == sizeof(ASP_CONTROL_BLOCK));

	pASPState = new CASPState(pASP);

	if (NULL == pASPState)
		myleave(1);
	
	//  stores context so callbacks in this thread know where they're from
	if ( !	TlsSetValue(g_dwTlsSlot,(LPVOID) pASPState))
		myleave(2);

	__try
	{
		__try 
		{
			CoInitializeEx(NULL,COINIT_MULTITHREADED);
			ret = pASPState->Main();
		}
		__finally
		{
			TlsSetValue(g_dwTlsSlot,0);
			delete pASPState;
			CoFreeUnusedLibraries();	// 1st call to this function
			CoUninitialize();			
		}
	}
	__except(1)
	{
		RETAILMSG(1,(L"ASP: ASP Script caused an exception during exection\r\n"));
	}

done:
	DEBUGMSG_ERR(ZONE_ERROR,(L"ASP: ExecuteASP failed, err = %d\r\n",err));
	DEBUGMSG(ZONE_INIT,(L"ASP: ExecuteASP completed processing, returning to httpd\r\n"));

	return ret;
}


CASPState::CASPState(PASP_CONTROL_BLOCK pACB)
{
	ZEROMEM(this);
	m_pACB = pACB;
	m_scriptLang = pACB->scriptLang;						
	m_aspErr = IDS_E_DEFAULT;
	m_lcid = pACB->lcid;
	m_lCodePage = pACB->lCodePage;
	m_iExpires = 0;

	m_pACB->ServerSupportFunction(m_pACB->ConnID,HSE_REQ_IS_KEEP_CONN,&m_fServerUsingKeepAlives,0,0);
}


CASPState::~CASPState()			
{
	DEBUGMSG(ZONE_SCRIPT,(L"ASP: In CASPState::~CASPState\r\n"));
	int i = 0;
	
	MyFree(m_pszFileData);
	MyFree(m_pszScriptData);

	if (m_bstrResponseStatus)
		SysFreeString(m_bstrResponseStatus);

	if (m_bstrScriptData)
		SysFreeString(m_bstrScriptData);

	if (m_bstrCharSet)
		SysFreeString(m_bstrCharSet);

	if (m_bstrContentType)
		SysFreeString(m_bstrContentType);
		
	if (m_pIncludeInfo)
	{
		for (; m_pIncludeInfo[i].m_pszFileName != NULL && i < MAX_INCLUDE_FILES; i++)
			MyFree(m_pIncludeInfo[i].m_pszFileName);
		MyFree(m_pIncludeInfo);
	}

	//  Delete the objects used through the script
	//  Later --> move empty string to global?
	if (m_pEmptyString)
		m_pEmptyString->Release();
	
	if (m_pResponseCookie)
		m_pResponseCookie->Release();

	// Note -- we don't delete the Server, Request, or Response objects.
	// They are objects in the scripting engine's namespace, and they are deleted
	// automatically when we closed the script site.
	//if (m_pServer)
	//	m_pServer->Release();

	//if (m_pRequest)
	//	m_pRequest->Release();

	// if (m_pResponse)
	// 	m_pResponse->Release();
	
	if (m_piActiveScriptParse)
		m_piActiveScriptParse->Release();

	if (m_piActiveScript)
		m_piActiveScript->Release();	

#ifdef TEST_PERFORMANCE
	LARGE_INTEGER liFrequency;

	QUERY_COUNT();
	
	if ( !QueryPerformanceFrequency(&liFrequency))
		return; // no CPU Support for this

	for (i = 0; i < g_dwPerfCounter; i++)
		RETAILMSG(1,(L"ASP: Item %i - HighPart = %d, LowPart = 0x%08x\r\n",i,g_rgliCount[i].HighPart,g_rgliCount[i].LowPart));
#endif
}


DWORD CASPState::Main()
{
	DEBUG_CODE_INIT;
	DWORD ret = HSE_STATUS_SUCCESS;		// we only return HSE_STATUS_ERROR if there's an Access Violation in ASP
	DWORD dwFileSize = 0;
	BOOL  fOwnCritSect = FALSE;

	if (! svsutil_OpenAndReadFile(m_pACB->wszFileName,(DWORD *) &m_cbFileData, &m_pszFileData))
		myleave(11);

	//  Read include files and Preproc dirictives if extended parse component included
	if (c_fUseExtendedParse)
	{
		if (! ParseIncludes() )
			myleave(12);
			
		if (! ParseProcDirectives() )
			myleave(13);
	}

	// This is a plain old text file, no ASP commands
	// Send it to client without further processing
	// For ASP pages that have no script commands, this speeds up processing immensly.
	if (NULL == MyStrStr(m_pszFileData,BEGIN_COMMAND))
	{
		CHAR szBuf[256];
		int i = sprintf(szBuf,cszContentTextHTML);
		ADDCRLF(szBuf,i);

		if (m_fServerUsingKeepAlives)
		{
			i += sprintf(szBuf+i,cszKeepAlive,m_cbFileData);
			m_fKeepAlive = TRUE;
		}

		ADDCRLF(szBuf,i);

		szBuf[i] = 0;		

		m_fSentHeaders = TRUE;
		m_pACB->ServerSupportFunction(m_pACB->ConnID,HSE_REQ_SEND_RESPONSE_HEADER, 0, 0, (DWORD*) szBuf);
		m_pACB->WriteClient(m_pACB->ConnID,(LPVOID) m_pszFileData,(LPDWORD) &m_cbFileData, 0);
		goto done;		// we're done processing
	}

	if (! ConvertToScript())
		myleave(14);

	EnterCriticalSection(&g_scriptCS);
	fOwnCritSect = TRUE;

	if (c_fUseCollections)
	{
		m_pEmptyString = CreateCRequestStrList(this,NULL,EMPTY_TYPE);
	}

	// ATL is not thread safe, so protect access to it (which is invoked 
	// via the IActiveScript engine) with a crit section);
	if (! InitScript())
		myleave(15);
		
	if (! RunScript())
		myleave(16);

	// If we haven't sent the headers yet, we can do a keep alive connection
	if (FALSE == m_fSentHeaders)
	{
		m_fKeepAlive = m_fServerUsingKeepAlives;
		SendHeaders();
	}
	m_pACB->Flush(m_pACB->ConnID);

done:
	if (fOwnCritSect)
		LeaveCriticalSection(&g_scriptCS);

	DEBUGMSG_ERR(ZONE_ERROR,(L"ASP: CASPState::Main failed, err = %d\r\n",err));
	if (m_fKeepAlive && m_fServerUsingKeepAlives)
		ret = HSE_STATUS_SUCCESS_AND_KEEP_CONN;
	
	return ret;
}


// Inits IActiveScript related libs/vars
BOOL CASPState::InitScript()
{
	DEBUG_CODE_INIT;
	HRESULT hr;
	CLSID clsid;
	CHAR szErrInfo[128];

	m_aspErr = IDS_E_SCRIPT_INIT;							
	wsprintf(m_wszModName, L"%s-0x%08x", (m_scriptLang == VBSCRIPT) ? L"VBS" : L"JS ",  
						   GetCurrentThreadId());
						   

	hr = CLSIDFromProgID((m_scriptLang == VBSCRIPT) ? L"VBScript" : L"JScript", &clsid);

	if (FAILED(hr)) 
		myleave(40);
	
	hr = CoCreateInstance(clsid, NULL, CLSCTX_INPROC_SERVER, 
					IID_IActiveScript, (void **)&m_piActiveScript);

	if (FAILED(hr)) 
		myleave(41);
	
	hr = m_piActiveScript->SetScriptSite((IActiveScriptSite*) g_pScriptSite);
	if (FAILED(hr))
		myleave(42);
	
	hr = m_piActiveScript->QueryInterface(IID_IActiveScriptParse, (void **)&m_piActiveScriptParse);
	if (FAILED(hr))
		myleave(43);

	hr = m_piActiveScriptParse->InitNew();
	if (FAILED(hr))
		myleave(44);


	hr = m_piActiveScript->AddNamedItem(m_wszModName, SCRIPTITEM_CODEONLY);
	if (FAILED(hr))
		myleave(45);

	// Make the Response, Request, and Server ASP objects visible
	hr = m_piActiveScript->AddNamedItem(L"Response", SCRIPTITEM_ISVISIBLE);
	if (FAILED(hr))
		myleave(46);

	// Make the Response, Request, and Server ASP objects visible
	hr = m_piActiveScript->AddNamedItem(L"Request", SCRIPTITEM_ISVISIBLE);
	if (FAILED(hr))
		myleave(47);

	// Make the Response, Request, and Server ASP objects visible
	hr = m_piActiveScript->AddNamedItem(L"Server", SCRIPTITEM_ISVISIBLE);
	if (FAILED(hr))
		myleave(48);

done:
	DEBUGMSG_ERR(ZONE_ERROR,(L"ASP: InitScript failed, err = %d, hr = 0x%X, GLE = 0x%X\r\n", err, hr, GetLastError() ));

	if (FAILED(hr))
	{
		WCHAR wszFormat[256]; 
		CHAR szFormat[256];

		MyLoadStringW2A(m_pACB->hInst,IDS_ERROR_CODE,wszFormat,szFormat);
		sprintf(szErrInfo,szFormat,"",hr);	
		ServerError(szErrInfo);
	}	
	else
	{
		m_aspErr = IDS_E_DEFAULT;	// set error back to default
	}
	return SUCCEEDED(hr);
}


//  After library initilization, this runs
BOOL CASPState::RunScript()
{
	DEBUG_CODE_INIT;
	HRESULT hr = S_OK;
	EXCEPINFO excep;
	CHAR szErrInfo[128];
	szErrInfo[0] = '\0';

	hr = m_piActiveScriptParse->ParseScriptText(
		m_bstrScriptData, 		// script text
		m_wszModName, 			// module name
		NULL,					// pstrItemName
		NULL, 					// punkContext
		0, 						// End script delemiter
		1, 						// starting line #
		0,
		NULL, 					// pvar Result
		&excep					// error info if there's a failure, must not = NULL
	);

	//  CScriptSite::OnScriptError provides detailed run-time error info, so
	//  don't bother reporting it again if it was called.
  	if (SCRIPT_E_REPORTED == hr)
		hr = S_OK;

	//  Is we didn't report an error already, give them the error code and
	//  a brief description of the problem.
	if (FAILED(hr)) 
	{
		CHAR szFormat[256];
		WCHAR wszFormat[256]; 

		MyLoadStringW2A(m_pACB->hInst,IDS_PARSE_ERROR_FROM_IACTIVE,wszFormat,szFormat);
				
		sprintf(szErrInfo,szFormat, hr);
		myleave(51);
	}

	hr = m_piActiveScript->SetScriptState(SCRIPTSTATE_STARTED);
	if (FAILED(hr))
		myleave(49);
	
done:
	m_piActiveScript->Close();
	
	if (FAILED(hr))
		ServerError(szErrInfo);
	
	DEBUGMSG_ERR(ZONE_ERROR,(L"ASP: RunScript failed, hr = %X, GLE = %X",hr, GetLastError() ));
		
	return SUCCEEDED(hr);
}



//  This (and ServerError) are the only functions that are allowed to directly
//  access the SetHeaders.  Changes ASP script could've made that we handle
//  here and not in httpd are:
//  *  Custom response status
//  *  Cookies
//  *  Expires field
BOOL CASPState::SendHeaders(BOOL fSendResponseHeaders)
{
	BOOL ret = FALSE;
	DEBUG_CODE_INIT;
	PSTR pszResponseStatus = NULL;
	SYSTEMTIME st;
	CHAR szExtraHeaders[4096]; 	
	int i = 0;
	
	//  The user has set a custom m_bstrResponseStatus string.
	//  Codepage note:  Convert to ANSI string since it's an http header, like IIS.
	
	if (m_bstrResponseStatus)
	{
		pszResponseStatus = MySzDupWtoA(m_bstrResponseStatus);

		if (NULL == pszResponseStatus)
		{
			m_aspErr = IDS_E_NOMEM;
			myleave(300);
		}
	}

	// Expires header.  We don't convert the value until now.
	if (m_iExpires != 0)
	{
		FileTimeToSystemTime( (FILETIME*) &m_iExpires, &st);
		
		sprintf(szExtraHeaders, cszDateOutputFmt, rgWkday[st.wDayOfWeek], st.wDay, 
						rgMonth[st.wMonth], st.wYear, st.wHour, st.wMinute, st.wSecond);

		if (! m_pACB->AddHeader(m_pACB->ConnID,"Expires",szExtraHeaders)) 
		{
			m_aspErr = IDS_E_NOMEM;  // only possible reason AddHeader() can fail.
			myleave(304);
		}
	}

	if (m_bstrContentType)
	{
		i = MyW2A(m_bstrContentType,szExtraHeaders,sizeof(szExtraHeaders)) -1;
	}
	else
	{
		strcpy(szExtraHeaders,cszTextHTML);
		i = sizeof(cszTextHTML) - 1;
	}
	// char set follows immediatly content-type in same HTTP line, don't add CRLF here.

	if (m_bstrCharSet)
	{
		szExtraHeaders[i++] = ';';   
		szExtraHeaders[i++] = ' '; 
		strcpy(szExtraHeaders + i,"Charset=");
		i += sizeof("Charset=") - 1;
		i += MyW2A(m_bstrCharSet,szExtraHeaders + i,sizeof(szExtraHeaders) - i) - 1;
	}

	if (! m_pACB->AddHeader(m_pACB->ConnID,"Content-Type",szExtraHeaders))
	{
		m_aspErr = IDS_E_NOMEM;  // only possible reason AddHeader() can fail.
		myleave(306);
	}

	if (m_fKeepAlive) 
	{
		sprintf(szExtraHeaders,cszKeepAliveText,m_cbBody);
		if (! m_pACB->AddHeader(m_pACB->ConnID,"Connection",szExtraHeaders))
		{
			m_aspErr = IDS_E_NOMEM;  // only possible reason AddHeader() can fail.
			myleave(307);
		}
	}

	//  WriteCookies takes care of setting error code.
	if (m_pResponseCookie && FALSE == m_pResponseCookie->WriteCookies())
		myleave(302);

	// fSendResponseHeader = FALSE if calling function has additional work to perform before sending headers.
	if (fSendResponseHeaders) 
	{
		if (FALSE == m_pACB->ServerSupportFunction(m_pACB->ConnID,
						HSE_REQ_SEND_RESPONSE_HEADER,(LPVOID) pszResponseStatus,NULL,NULL))
		{
			m_aspErr = IDS_E_HTTPD;
			myleave(303);
		}
	}

	ret = TRUE;
done:
	DEBUGMSG_ERR(ZONE_ERROR,(L"ASP: CASPState::SendHeaders died, err = %d\r\n",err));

	MyFree(pszResponseStatus);

	m_fSentHeaders = TRUE;
	return ret;
}



//  NOTE:  IE5 does not display the body information on a status code "500 - Internal
//  Error" but instead a local page describing server errs in general, so we
//  send a 200 back no matter what (like IIS ASP).
const char cszInternalErrorHeader[] = "200 - OK";

// To create the body of error message, we nicely format it,
// then we put data in pszBody if any, then we look up code based on
// m_aspErr's status
void CASPState::ServerError(PSTR pszBody, int iErrorLine)
{
	CHAR szErrorString[512];
	CHAR szBodyFormatted[4000]; 
	WCHAR wszErrString[512];
	DWORD cbBodyFormatted = 0;

	// Should always have a valid error code set.  If not, then reset to unknown err.
	DEBUGCHK(m_aspErr <= MAX_ERROR_CODE);
	if (m_aspErr >= MAX_ERROR_CODE)
		m_aspErr = IDS_E_UNKNOWN;

	if (! m_pACB->fASPVerboseErrorMessages) 
	{
		// web server has requesteted that only vague errors be return on script failure.
		pszBody    = NULL;
		iErrorLine = 0;
		m_aspErr   = IDS_E_PARSER;
	}

	MyLoadStringW2A(m_pACB->hInst,m_aspErr,wszErrString,szErrorString);
	cbBodyFormatted += sprintf(szBodyFormatted + cbBodyFormatted, 
						  "<font face=\"Arial\" size=2><p>%s\r\n",szErrorString);

	if (pszBody)
	{
		cbBodyFormatted += sprintf(szBodyFormatted + cbBodyFormatted,"<p>%s\r\n", 
								   pszBody);
	}


	// if iErrorLine = -1 or 0, then we don't have an error line number for it.
	// This happens in cases where server dies on mem failure or initilization and
	// a few other strange cases.
	if (iErrorLine > 0)
	{
		PSTR pszIncFileName = NULL;
		int  iFileErrorLine = 0;

		if ( c_fUseExtendedParse  )
		{
			GetIncErrorInfo(iErrorLine, &iFileErrorLine, &pszIncFileName);
		}
		else
			iFileErrorLine = iErrorLine;
		
		// GetIncErrorInfo sets pszIncFile name =NULL if error occured in main asp page
		if (NULL == pszIncFileName)
			pszIncFileName = m_pACB->pszVirtualFileName;

		WCHAR wszErrFormat[256]; 
		CHAR  szErrFormat[256];


		if (0 != LoadString(m_pACB->hInst,IDS_LINE_FILE_ERROR,wszErrFormat,ARRAYSIZEOF(wszErrFormat)))
		{
			MyW2A(wszErrFormat,szErrFormat,sizeof(szErrFormat));

			cbBodyFormatted += sprintf(szBodyFormatted + cbBodyFormatted, 
									   	szErrFormat,pszIncFileName, iFileErrorLine);						   
		}
	}
	cbBodyFormatted += sprintf(szBodyFormatted + cbBodyFormatted,"</font>\r\n");


	//  If we haven't send the headers yet, do so now.  Don't send the custom
	//  headers / cookies / etc in error case.  
	//  However, we can do a keep-alive in this case.
	
	if (m_fSentHeaders == FALSE)
	{
		CHAR szExtraHeaders[1024];
		int i = 0;

		strcpy(szExtraHeaders ,cszContentTextHTML);
		i = sizeof(cszContentTextHTML) - 1;
		ADDCRLF(szExtraHeaders,i);

		if (m_fServerUsingKeepAlives)
		{
			i += sprintf(szExtraHeaders+i,cszKeepAlive,m_cbBody+cbBodyFormatted);

			// This could be called from IActiveLibs, in which case we have no way
			// to tell CASPState::Main() that we're keeping the session alive.
			m_fKeepAlive = TRUE;			
		}

		ADDCRLF(szExtraHeaders,i);
		szExtraHeaders[i] = 0;

		m_pACB->ServerSupportFunction(m_pACB->ConnID,HSE_REQ_SEND_RESPONSE_HEADER,
						(void *) cszInternalErrorHeader,NULL,(LPDWORD) szExtraHeaders);

		m_fSentHeaders = TRUE;
	}

	// dump out any buffer before error message, like IIS.
	m_pACB->Flush(m_pACB->ConnID);
	m_pACB->WriteClient(m_pACB->ConnID, (LPVOID) szBodyFormatted, &cbBodyFormatted, 0);
	m_pACB->Flush(m_pACB->ConnID);
}
