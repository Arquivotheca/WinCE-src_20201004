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
Module Name: LOG.CPP
Abstract: Logging functions
--*/

//  Responsible for log handling functions.  
//  A current-httpd.log and previous-httpd.log keep track of current and the previous
//  changes.  The format for each line is based on IIS's, except that we 
//  include the date in each line; they create a new log for each day.


#include "httpd.h"


const char cszDateOutputFmt[]     = "%3s, %02d %3s %04d %02d:%02d:%02d ";

#define CURRENT_LOG_NAME L"\\current-httpd.log"
#define PREVIOUS_LOG_NAME L"\\previous-httpd.log"
#define LOG_EXTRA_BUFFER_LEN 100

void CLog::WriteEvent(DWORD dwEvent,...) 
{
    CHAR szOutput[MINBUFSIZE];
    WCHAR wszFormat[512];
    WCHAR wszOutput[512];
    PSTR pszTrav = szOutput;
    SYSTEMTIME st;
    va_list ap;

    if (!LoadString(g_hInst,dwEvent,wszFormat,ARRAYSIZEOF(wszFormat)))
    {
        return;
    }

    va_start(ap,dwEvent);
    wvsprintf(wszOutput,wszFormat,ap);
    va_end (ap);

    GetSystemTime(&st);
    pszTrav = szOutput + sprintf(szOutput, cszDateOutputFmt, 
          rgWkday[st.wDayOfWeek], st.wDay, rgMonth[st.wMonth], st.wYear, st.wHour, st.wMinute, st.wSecond);

    pszTrav += MyW2A(wszOutput,pszTrav,MINBUFSIZE - (pszTrav - szOutput)) - 1;
    WriteData(szOutput,pszTrav - szOutput);    
}

CLog::~CLog() 
{
    WriteEvent(IDS_HTTPD_SHUTDOWN_COMPLETE);
    MyCloseHandle(m_hLog);
    DeleteCriticalSection(&m_CritSection);
}

#define MAX_LOG_OPEN_ATTEMPTS  15

CLog::CLog(DWORD dwMaxFileLen, WCHAR * lpszLogDir) 
{
    int i;
    
    memset(this, 0, sizeof(*this));
    m_hLog = INVALID_HANDLE_VALUE;
    InitializeCriticalSection(&m_CritSection);

    DWORD ccLogDir = lpszLogDir ? wcslen(lpszLogDir) : 0;

    if (0 == dwMaxFileLen || 0 == ccLogDir) 
    {
        DEBUGMSG(ZONE_INIT,(L"HTTPD: No logging to be performed, because of on registry settings\r\n"));
        return;
    }

    m_dwMaxFileSize = dwMaxFileLen;  

    DEBUGMSG(ZONE_INIT,(L"HTTPD: Initializing log files\r\n"));

    // remove trailing "\" if log file dir entry ends in it
    if (lpszLogDir[ccLogDir - 1] == L'\\')
    {
        lpszLogDir[ccLogDir - 1] = L'\0';
    }

    if (FAILED(StringCchPrintfW(lpszCurrentLog,MAX_PATH,L"%s%s",lpszLogDir, CURRENT_LOG_NAME)) ||
        FAILED(StringCchPrintfW(lpszPrevLog,MAX_PATH,L"%s%s",lpszLogDir,PREVIOUS_LOG_NAME)))
    {
        DEBUGMSG(ZONE_INIT | ZONE_ERROR,(L"HTTPD: Log directory specified too long, no logging to be performed\r\n"));
        return;        
    }

    // Note:  It's possible that the log file is being written to a flash
    // drive, in which case the web server initialization routines would
    // be called before the drive is ready during boot time, in which
    // case we get an INVALID_HANDLE_VALUE.  To make sure the drive has
    // time to initialize, we go through this loop before failing.
    for (i = 0; i < MAX_LOG_OPEN_ATTEMPTS; i++) 
    {
        if (INVALID_HANDLE_VALUE != (m_hLog = MyOpenAppendFile(lpszCurrentLog))) 
        {
            break;   
        }
        Sleep(1000);
    }

    if (INVALID_HANDLE_VALUE == m_hLog) 
    {
        DEBUGMSG(ZONE_ERROR | ZONE_INIT,(L"HTTPD: CreateFile fails for log <<%s>>, no logging will be done, GLE=0x%08x\r\n",lpszCurrentLog,GetLastError()));
        return;
    }
    
    m_dwFileSize = GetFileSize(m_hLog,NULL);
    if (INVALID_HANDLE_VALUE == ((HANDLE) m_dwFileSize)) 
    {
        DEBUGMSG(ZONE_INIT,(L"HTTPD: Get file size on log <<%s>> failed, err = %d\r\n",lpszCurrentLog,GetLastError()));
        m_hLog = INVALID_HANDLE_VALUE;
        return;
    }

    if (m_dwFileSize != 0)  
    {        // This moves filePtr to append log file
        if (INVALID_HANDLE_VALUE == (HANDLE) SetFilePointer(m_hLog, m_dwFileSize,
                                                      NULL, FILE_BEGIN))
        {
            DEBUGMSG(ZONE_INIT,(L"HTTPD: Get file size on log %s failed, err = %d\r\n",lpszCurrentLog,GetLastError()));
            m_hLog = INVALID_HANDLE_VALUE;   
            return;
        }
    }
    WriteEvent(IDS_HTTPD_STARTUP);    
}

//  The log written has the following format
//  (DATE) (TIME) (IP of requester) (Method) (Request-URI) (Status returned)

//  DATE is in the same format as the recommended one for httpd headers
//  TIME is GMT

void CLog::WriteLog(CHttpRequest* pRequest) 
{
    CHAR szBuffer[MINBUFSIZE];          
    PSTR psz = szBuffer;
    DWORD dwToWrite = 0;       

    if (!pRequest)  
    {
        DEBUGCHK(0);
        return;
    }

    if (INVALID_HANDLE_VALUE == m_hLog)  // no logging setup in reg, or prev failure
    {
        return;  
    }

    // Make sure that buffer is big enough for filter additions, append if not so
    // We add LOG_EXTRA_BUFFER_LEN to account for date, spaces, and HTTP status code. 
    DWORD cbNeeded = LOG_EXTRA_BUFFER_LEN + pRequest->GetLogBufferSize();

    if (cbNeeded > MINBUFSIZE) 
    {
        psz = MyRgAllocNZ(CHAR,cbNeeded);
        if (!psz)
        {
            return;
        }
    }
    
    pRequest->GenerateLog(psz,&dwToWrite);
    WriteData(psz,dwToWrite);

    if (szBuffer != psz)
    {
        MyFree(psz);
    }
}

void CLog::WriteData(PSTR szBuffer, DWORD dwToWrite) 
{
    DWORD dwWritten = 0;

    EnterCriticalSection(&m_CritSection);
    {
        m_dwFileSize += dwToWrite;
        // roll over the logs once the maximum size has been reached.
        if (m_dwFileSize > m_dwMaxFileSize) 
        {
            MyCloseHandle(m_hLog);
            DeleteFile(lpszPrevLog);  
            MoveFile(lpszCurrentLog,lpszPrevLog);
            m_hLog = MyOpenAppendFile(lpszCurrentLog);
            m_dwFileSize = dwToWrite;
        }

        if (m_hLog != INVALID_HANDLE_VALUE) 
        {
            WriteFile(m_hLog,(LPCVOID) szBuffer,dwToWrite,&dwWritten,NULL);
            DEBUGMSG(ZONE_REQUEST,(L"HTTPD: Wrote log out to file\r\n"));
        }
    }
    LeaveCriticalSection(&m_CritSection);
}

// Returns the size of the buffer we'll need.
DWORD CHttpRequest::GetLogBufferSize()
{
    PHTTP_FILTER_LOG pFLog = m_pFInfo ? m_pFInfo->m_pFLog : NULL;        
    DWORD cbNeeded;
    
    // remotehost
    if (pFLog && pFLog->pszClientHostName) 
    {
        cbNeeded = strlen(pFLog->pszClientHostName);
    }
    else
    {
        cbNeeded = LOG_REMOTE_ADDR_SIZE;
    }
    
    if (pFLog && pFLog->pszOperation)
    {
        cbNeeded += strlen(pFLog->pszOperation);
    }
    else if (m_pszMethod)
    {
        cbNeeded += strlen(m_pszMethod);
    }
        
    if (pFLog && pFLog->pszTarget)
    {
        cbNeeded += strlen(pFLog->pszTarget);
    }
    else if (m_pszURL)
    {
        cbNeeded += strlen(m_pszURL);
    }

    if (m_pszLogParam)
    {
        cbNeeded += strlen(m_pszLogParam);
    }

    return cbNeeded;
}


//  Generates the log.  First this fcn sees if a filter has created a new
//  logging structure, in which case valid data in pFLog will override actual
//  server data.  In the typical case we just use CHttpRequest info.

//  Note: We don't use sprintf here in case user has sprintf escape chars they're sending.
void CHttpRequest::GenerateLog(PSTR szBuffer, PDWORD pdwToWrite) 
{
    SYSTEMTIME st;
    PSTR pszTarget = NULL;
    PSTR pszTrav = szBuffer;
    PHTTP_FILTER_LOG pFLog = m_pFInfo ? m_pFInfo->m_pFLog : NULL;

    GetSystemTime(&st);     // Use GMT time for the log, too
    pszTrav += sprintf(szBuffer, cszDateOutputFmt, 
          rgWkday[st.wDayOfWeek], st.wDay, rgMonth[st.wMonth], st.wYear, st.wHour, st.wMinute, st.wSecond);

    // remotehost
    if (pFLog && pFLog->pszClientHostName) 
    {
        pszTrav = strcpyEx(pszTrav,pFLog->pszClientHostName);
    }
    else 
    {
        CHAR szAddress[LOG_REMOTE_ADDR_SIZE];
        if (GetRemoteAddress(m_socket,szAddress,FALSE,LOG_REMOTE_ADDR_SIZE))
        {
            pszTrav = strcpyEx(pszTrav,szAddress);
        }
    }
    *pszTrav++ = ' ';

    //  The method (GET, POST, ...)
    if (pFLog && pFLog->pszOperation)
    {
        pszTrav = strcpyEx(pszTrav,pFLog->pszOperation);
    }
    else if (m_pszMethod)
    {
        pszTrav = strcpyEx(pszTrav,m_pszMethod);
    }
    else
    {
        *pszTrav++ = '\t';    // If we get a bad request from browser, m_pszMethod may be NULL.
    }
        
    *pszTrav++ = ' ';
    
    // target (URI)
    if (pFLog && pFLog->pszTarget)
    {
        pszTarget = (PSTR) pFLog->pszTarget;
    }
    else
    {
        pszTarget = m_pszURL;
    }

            
    // like IIS, we convert any spaces in here into "+" signs.
    // NOTES:   IIS does this for both filter changed targets and URLS.
    // It only does it with spaces, doesn't convert \r\n, tabs, or any other escape
    // characters

    if (pszTarget) 
    { // on a bad request, m_pszURL may = NULL.  Check.
        while ( *pszTarget) 
        {
            if ( *pszTarget == ' ') 
            {
                PREFAST_SUPPRESS(394,"Updating buffer pointer is safe here");
                *pszTrav++ = '+';
            }
            else
            {
                *pszTrav++ = *pszTarget;
            }
            pszTarget++;
        }
    }
    *pszTrav++ = ' ';

    
    // status
    if (pFLog)
    {
        _itoa(pFLog->dwHttpStatus,pszTrav,10);
    }
    else
    {
        _itoa(rgStatus[m_rs].dwStatusNumber,pszTrav,10);
    }
    pszTrav = strchr(pszTrav,'\0');
    *pszTrav++ = ' ';
    
    if (m_pszLogParam)    // ISAPI Extension logging has modified something
    {
        pszTrav = strcpyEx(pszTrav,m_pszLogParam);
    }
    *pszTrav++ = '\r';
    *pszTrav++ = '\n';
    *pszTrav= '\0';

    
    *pdwToWrite = pszTrav - szBuffer;
    return;
}
