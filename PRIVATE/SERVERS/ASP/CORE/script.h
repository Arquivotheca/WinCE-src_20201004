//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: script.h
Abstract: main interface between httpd and asp.dll
--*/

#ifndef _SCRIPT_H_
#define _SCRIPT_H_


typedef struct
{
	PSTR m_pszFileName;
	int  m_iStartLine;
	int  m_cLines;
	PSTR m_pszStart;		// where to tack it in on file data
	PSTR m_pszEnd;			// where --> ends, where to start copying data after it.
	DWORD m_dwFileSize;
	HANDLE m_hFile;			// Handle to file
} INCLUDE_INFO, *PINCLUDE_INFO;

class CASPState 
{
friend class CRequest;
friend class CResponse;
friend class CServer;
friend class CRequestDictionary;
friend class CScriptSite;
friend class CRequestStrList;

private:
	PASP_CONTROL_BLOCK m_pACB;		// Access control block from httpd


	// ASP Preproc variables
	SCRIPT_LANG  m_scriptLang;		// Jscript or Vbscript
	LCID m_lcid;
	UINT m_lCodePage;

	// Response state variables
	UINT m_aspErr;						// Error code	
	BSTR m_bstrResponseStatus;			// optional user set http response status
	BSTR m_bstrContentType;
	BSTR m_bstrCharSet;
	BOOL m_fSentHeaders;				// Have we sent response headers yet?
	BOOL m_fBufferedResponse;			// Buffering write requests?
	DWORD m_cbBody;						// Number of bytes sent back.
	BOOL  m_fKeepAlive;					// ServerError sets this.
	BOOL  m_fServerUsingKeepAlives;     // TRUE only if client sent keep-alive header
	
	__int64 m_iExpires;						// For expires header
	CRequestDictionary	*m_pResponseCookie;	// Copy of object in CResponse

	// IActiveScript related
	IActiveScript *m_piActiveScript;
	IActiveScriptParse *m_piActiveScriptParse; 
	WCHAR m_wszModName[20];					// For script engine, based on threadID
	CRequestStrList *m_pEmptyString;
	//CRequest *m_pRequest;
	//CResponse *m_pResponse;
	//CServer *m_pServer;

public:
	// PUBLIC FUNCTIONS related to running script
	CASPState(PASP_CONTROL_BLOCK pACB);
	~CASPState();			
	DWORD Main();	
	

	//  These are extended fncs for CResponse.
	STDMETHODIMP Redirect(BSTR pszURL);
	STDMETHODIMP BinaryWrite(VARIANT varData);
	STDMETHODIMP Write(VARIANT varData);
	HRESULT WriteToClient(PSTR pszBody, DWORD dwSpecifiedLength=0);
	
	// Helper functions
	void ServerError(PSTR pszBody = NULL, int iErrLine = -1);
	BOOL SendHeaders(BOOL fSendResponseHeaders=TRUE);

private:
	// These fncs / members are related to parsing, use by parser.cpp but
	// not directly by IACtiveScripting engine

	PSTR m_pszFileData;				// Read from ASP file
	BSTR m_bstrScriptData;		    // Script info passed to script engine
	int m_cbFileData;				// Size of file
	int m_cbScriptData;				// Size of script 
	PSTR m_pszScriptData;			// orig script data, kept around in case of error
	PINCLUDE_INFO m_pIncludeInfo;	// Holds include file names, line # info.

	BOOL ParseProcDirectives();		// Handles <%@
	BOOL ParseProcDirectiveLine(PSTR pszTrav, PSTR pszSubEnd);

	BOOL ParseIncludes();			
	BOOL FindIncludeFiles(int *pnFiles, int *pnNewChars, DWORD *pnMaxFileSize);
	BOOL ReadIncludeFiles(int nFiles, int nNewChars, DWORD dwMaxFileSize);			
	void GetIncErrorInfo(int iScriptErrLine, int *piFileErrLine, PSTR *ppszFileName);

	BOOL GetVirtualIncludeFilePath(PSTR pszFileName, WCHAR wszNewFileName[]);
	BOOL GetIncludeFilePath(PSTR pszFileName, WCHAR wszNewFileName[]);

	BOOL ConvertToScript();			
	void CopyPlainText(PSTR &pszRead, PSTR &pszWrite, PSTR pszSubEnd);
	void CopyScriptText(PSTR &pszRead, PSTR &pszWrite, PSTR pszSubEnd);

	BOOL InitScript();
	BOOL RunScript();
};

#endif
