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
