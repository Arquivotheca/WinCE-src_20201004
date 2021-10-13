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
Module Name: LOG.H
Abstract: Logging functions
--*/



// If registry key doesn't exist, this will be the largest we'll let log grow to


class CHttpRequest;  // forward declaration


// Right now we assume only one object handles all requests.

class CLog 
{
private:
    HANDLE m_hLog;
    DWORD m_dwMaxFileSize;                // Max log can grow before it's rolled over
    DWORD m_dwFileSize;                   // Current file length
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

