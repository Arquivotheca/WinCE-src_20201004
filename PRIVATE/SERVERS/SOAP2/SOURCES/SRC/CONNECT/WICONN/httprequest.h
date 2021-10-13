//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __HTTP_REQUEST__
#define __HTTP_REQUEST__


#ifdef UTIL_HTTPLITE
#	include "dubinet.h"
#else
#	include "wininet.h"
#endif


#include "auto_xxx.hxx"
#include "string.hxx"

#include "assert.h"

// async_internet_handle
class async_internet_handle
{
public:
	async_internet_handle()
		: m_handle(NULL)
	{
		m_hEventClosed = CreateEvent(NULL, true, false, NULL);
		m_hEventComplete = CreateEvent(NULL, false, false, NULL);
	}

	~async_internet_handle()
		{close(); }

	operator HINTERNET()
		{return m_handle; }

	operator=(const HINTERNET& handle)
	{
		close();

		if(m_handle = handle)
		{
			ResetEvent(m_hEventClosed);

#ifdef UTIL_HTTPLITE
#ifdef DEBUG
			INTERNET_STATUS_CALLBACK callback = 
#endif
			InternetSetStatusCallback(m_handle, &Callback);
#else
#ifdef DEBUG
			INTERNET_STATUS_CALLBACK callback = 
#endif
			InternetSetStatusCallbackA(m_handle, &Callback);
#endif

			ASSERT(callback != INTERNET_INVALID_STATUS_CALLBACK);
		}
	}

	void close()
	{
		if(m_handle)
		{
			InternetCloseHandle(m_handle);
			WaitForSingleObject(m_hEventClosed, INFINITE);
			m_handle = NULL;
		}
	}

	DWORD wait(int nTimeout);

private:
	static void CALLBACK Callback(HINTERNET hInternet,
								  DWORD_PTR dwContext,
								  DWORD dwInternetStatus,
								  LPVOID lpvStatusInformation,
								  DWORD dwStatusInformationLength);
private:
	HINTERNET		m_handle;
	ce::auto_handle	m_hEventClosed;
	ce::auto_handle	m_hEventComplete;
	DWORD			m_dwError;
};



#ifndef HTTP_STATUS_PRECOND_FAILED
#	define HTTP_STATUS_PRECOND_FAILED      412 // precondition given in request failed
#endif

#endif // __HTTP_REQUEST__
