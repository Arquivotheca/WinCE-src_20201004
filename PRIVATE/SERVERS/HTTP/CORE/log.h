//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: LOG.H
Abstract: Logging functions
--*/



// If registry key doesn't exist, this will be the largest we'll let log grow to


class CHttpRequest;  // forward declaration


// Right now we assume only one object handles all requests.

class CLog {
private:
	HANDLE m_hLog;
	DWORD m_dwMaxFileSize;				// Max log can grow before it's rolled over
	DWORD m_dwFileSize;		   			// Current file lenght
	CRITICAL_SECTION m_CritSection;
	WCHAR lpszCurrentLog[MAX_PATH+1];
	WCHAR lpszPrevLog[MAX_PATH+1];
	
public:
	CLog(DWORD dwMaxFileLen, WCHAR * lpszLogDir);
	~CLog();

	void WriteData(PSTR wszData, DWORD dwToWrite);
	void WriteLog(CHttpRequest* pThis);
	void WriteEvent(DWORD dwEvent,...);
};

