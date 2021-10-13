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





// wait
DWORD async_internet_handle::wait(int nTimeout)
{
	DWORD dwError = GetLastError();
        
    if(dwError != ERROR_IO_PENDING)
	{
	    ResetEvent(m_hEventComplete);
		m_dwError = dwError;
		goto Done;
	}


#ifdef DEBUG
    WCHAR msg[1024];
    swprintf(msg,L"WARNING: Time out set to: %d", nTimeout); 
    OutputDebugString(msg);
#endif

    if(WAIT_TIMEOUT == WaitForSingleObject(m_hEventComplete, nTimeout))
    {
        close();
	    ResetEvent(m_hEventComplete);
        return m_dwError = ERROR_INTERNET_TIMEOUT;
    }

Done:
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
