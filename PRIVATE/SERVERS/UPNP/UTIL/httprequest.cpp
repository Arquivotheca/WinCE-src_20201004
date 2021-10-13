//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>

#include "HttpRequest.h"

// static members
ce::string* HttpRequest::m_pstrAgentName = NULL;

// Initialize
void HttpRequest::Initialize(LPCSTR pszAgent)
{
	assert(m_pstrAgentName == NULL);

	m_pstrAgentName = new ce::string(pszAgent);
}

// Uninitialize
void HttpRequest::Uninitialize()
{
	delete m_pstrAgentName;
	m_pstrAgentName = NULL;
}


// HttpRequest
HttpRequest::HttpRequest(DWORD dwTimeout/* = 30 * 1000*/)
    : m_dwTimeout(dwTimeout),
      m_dwError(ERROR_SUCCESS)
{
	assert(HttpRequest::m_pstrAgentName != NULL);

	m_hInternet = InternetOpenA(*HttpRequest::m_pstrAgentName, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, INTERNET_FLAG_ASYNC);
	
	InternetSetOption(m_hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &dwTimeout, sizeof(dwTimeout));
	InternetSetOption(m_hInternet, INTERNET_OPTION_SEND_TIMEOUT, &dwTimeout, sizeof(dwTimeout));
	InternetSetOption(m_hInternet, INTERNET_OPTION_DATA_RECEIVE_TIMEOUT, &dwTimeout, sizeof(dwTimeout));
	InternetSetOption(m_hInternet, INTERNET_OPTION_DATA_SEND_TIMEOUT, &dwTimeout, sizeof(dwTimeout));
	InternetSetOption(m_hInternet, INTERNET_OPTION_CONNECT_TIMEOUT , &dwTimeout, sizeof(dwTimeout));
}


// Open
bool HttpRequest::Open(LPCSTR pszVerb, LPCSTR pszUrl, LPCSTR pszVersion/* = NULL*/)
{
	char pszServer[INTERNET_MAX_HOST_NAME_LENGTH];
	char pszUrlPath[INTERNET_MAX_URL_LENGTH];
	char pszUserName[INTERNET_MAX_USER_NAME_LENGTH];
    char pszPassword[INTERNET_MAX_PASSWORD_LENGTH];
	
	URL_COMPONENTSA urlComp = {0};
	
	urlComp.dwStructSize = sizeof(URL_COMPONENTS);
	
    // server
    urlComp.lpszHostName = pszServer;
	urlComp.dwHostNameLength = INTERNET_MAX_HOST_NAME_LENGTH;
	
    // URL Path
    urlComp.lpszUrlPath = pszUrlPath;
	urlComp.dwUrlPathLength = INTERNET_MAX_URL_LENGTH;

    // user
    urlComp.lpszUserName = pszUserName;
    urlComp.dwUserNameLength = INTERNET_MAX_USER_NAME_LENGTH;

    // password
    urlComp.lpszPassword = pszPassword;
    urlComp.dwPasswordLength = INTERNET_MAX_PASSWORD_LENGTH;
    
	if(!InternetCrackUrlA(pszUrl, 0, ICU_DECODE, &urlComp))
		return false;
    
    if(urlComp.nScheme != INTERNET_SCHEME_HTTP)
		return false;
    
    m_hConnect = InternetConnectA(m_hInternet, urlComp.lpszHostName, urlComp.nPort, 
                                 urlComp.lpszUserName, urlComp.lpszPassword, 
                                 INTERNET_SERVICE_HTTP, 0, reinterpret_cast<DWORD>(&m_hConnect));

	if(m_hConnect == NULL)
		return false;

#ifdef UTIL_HTTPLITE
	m_hRequest = HttpOpenRequestA(m_hConnect, pszVerb, urlComp.lpszUrlPath, pszVersion, NULL, NULL, INTERNET_FLAG_NO_CACHE_WRITE, reinterpret_cast<DWORD>(&m_hRequest));
#else
    m_hRequest = HttpOpenRequestA(m_hConnect, pszVerb, urlComp.lpszUrlPath, pszVersion, NULL, NULL, INTERNET_FLAG_RESYNCHRONIZE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, reinterpret_cast<DWORD>(&m_hRequest));
#endif
    
    return m_hRequest != NULL;
}


// AddHeader
bool HttpRequest::AddHeader(LPCSTR pszHeaderName, int nHeaderValue, DWORD dwModifiers/* = HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD*/)
{
	char pszHeaderValue[11];

	_itoa(nHeaderValue, pszHeaderValue, 10);

	return AddHeader(pszHeaderName, pszHeaderValue, dwModifiers);
}


// AddHeader
bool HttpRequest::AddHeader(LPCSTR pszHeaderName, LPCSTR pszHeaderValue, DWORD dwModifiers/* = HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD*/)
{
    ce::string strHeader;

    strHeader.reserve(strlen(pszHeaderName) + strlen(pszHeaderValue) + 3);
    
    strHeader = pszHeaderName;
    strHeader += ": ";
    strHeader += pszHeaderValue;

    return TRUE == HttpAddRequestHeadersA(m_hRequest, strHeader, -1, dwModifiers);
}


// Write
void HttpRequest::Write(LPCSTR pszData)
{
    m_strMessage += pszData;
}


// Write
void HttpRequest::Write(LPCWSTR pwszData, UINT CodePage/* = CP_UTF8*/)
{
    int nSizeCurrent = m_strMessage.size();
	int nSizeAdd = WideCharToMultiByte(CodePage, 0, pwszData, -1, NULL, 0, NULL, NULL);

    if(!nSizeAdd)
        if(nSizeAdd = WideCharToMultiByte(CP_ACP, 0, pwszData, -1, NULL, 0, NULL, NULL))
            CodePage = CP_ACP;

	m_strMessage.reserve(nSizeCurrent + nSizeAdd);
	
	WideCharToMultiByte(CodePage, 0, pwszData, -1, m_strMessage.get_buffer() + nSizeCurrent, m_strMessage.capacity() - nSizeCurrent, NULL, NULL);
}


// Send
bool HttpRequest::Send()
{
    LPCSTR	pszMessage = NULL;
	DWORD	dwLength = 0;

	if(m_strMessage.size())
	{
		pszMessage = m_strMessage;
		dwLength = m_strMessage.size();
	}

	if(!HttpSendRequestA(m_hRequest, NULL, 0, const_cast<LPSTR>(pszMessage), dwLength))
        m_dwError = m_hRequest.wait(m_dwTimeout);
	else
		m_dwError = ERROR_SUCCESS;

    m_strMessage.resize(0);
    
    return m_dwError == ERROR_SUCCESS;
}


// Read
bool HttpRequest::Read(LPVOID pBuffer, DWORD cbBytesToRead, DWORD* pcbBytesRead)
{
    if(!InternetReadFile(m_hRequest, pBuffer, cbBytesToRead, pcbBytesRead))
        m_dwError = m_hRequest.wait(m_dwTimeout);
	else
		m_dwError = ERROR_SUCCESS;

    return m_dwError == ERROR_SUCCESS;
}


// GetStatus
DWORD HttpRequest::GetStatus()
{
    DWORD dwStatus, cb = sizeof(dwStatus);
    
    if(HttpQueryInfo(m_hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &dwStatus, &cb, NULL))
        return dwStatus;
    else
        return 0;
}


// GetHeader
bool HttpRequest::GetHeader(LPCSTR pszHeaderName, LPSTR pszBuffer, LPDWORD lpdwBufferLength)
{
	DWORD nLen = strlen(pszHeaderName) + 1;

	if(nLen > *lpdwBufferLength)
		*lpdwBufferLength = nLen;
	
	strcpy(pszBuffer, pszHeaderName);
	
	return TRUE == HttpQueryInfoA(m_hRequest, HTTP_QUERY_CUSTOM, pszBuffer, lpdwBufferLength, NULL);
}


// wait
DWORD async_internet_handle::wait(int nTimeout)
{
    DWORD dwError = GetLastError();
        
    if(dwError != ERROR_IO_PENDING)
    {
        ResetEvent(m_hEventComplete);
        
        return m_dwError = dwError;
    }

    if(WAIT_TIMEOUT == WaitForSingleObject(m_hEventComplete, nTimeout))
    {
        close();

	    ResetEvent(m_hEventComplete);

        return m_dwError = ERROR_INTERNET_TIMEOUT;
    }

    return m_dwError;
}



// Callback
// Note, this ANSI callback function (Unicode callbacks don't work).
// Status callbacks that contain strings will have ANSI strings

void CALLBACK async_internet_handle::Callback(HINTERNET hInternet,
											  DWORD_PTR dwContext,
											  DWORD dwInternetStatus,
											  LPVOID lpvStatusInformation,
											  DWORD dwStatusInformationLength)
{
    async_internet_handle* pThis = reinterpret_cast<async_internet_handle*>(dwContext);

    switch(dwInternetStatus)
    {
        case INTERNET_STATUS_REQUEST_COMPLETE:
                {
                    INTERNET_ASYNC_RESULT* pAsyncResult = reinterpret_cast<INTERNET_ASYNC_RESULT*>(lpvStatusInformation);

                    pThis->m_dwError = pAsyncResult->dwError;
                    SetEvent(pThis->m_hEventComplete);
                }
                break;

		case INTERNET_STATUS_HANDLE_CLOSING:
				
				SetEvent(pThis->m_hEventClosed);
				break;
    }
}
