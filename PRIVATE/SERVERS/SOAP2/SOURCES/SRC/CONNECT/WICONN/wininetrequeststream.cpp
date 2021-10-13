//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+----------------------------------------------------------------------------
//
//
// File:
//      WinInetRequestStream.cpp
//
// Contents:
//
//      CWinInetRequestStream object implementation
//
//-----------------------------------------------------------------------------

#include "Headers.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// static member initialization
////////////////////////////////////////////////////////////////////////////////////////////////////

const WCHAR CWinInetRequestStream::s_szContentLength[] = L"Content-Length: ";

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface map
////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_INTERFACE_MAP(CWinInetRequestStream)
    ADD_IUNKNOWN(CWinInetRequestStream, IStream)
    ADD_INTERFACE(CWinInetRequestStream, IStream)
    ADD_INTERFACE(CWinInetRequestStream, ISequentialStream)
END_INTERFACE_MAP(CWinInetRequestStream)

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CWinInetRequestStream::CWinInetRequestStream()
//
//  parameters:
//
//  description:
//          CWinInetRequestStream Consturctor
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CWinInetRequestStream::CWinInetRequestStream()
{
    m_pOwner        = 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CWinInetRequestStream::~CWinInetRequestStream()
//
//  parameters:
//
//  description:
//          CWinInetRequestStream Destructor
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CWinInetRequestStream::~CWinInetRequestStream()
{
    ASSERT(m_pOwner == 0);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWinInetRequestStream::set_Owner(CWinInetConnector *pOwner)
//
//  parameters:
//
//  description:
//          Sets the owner for CSoapOwnedObject
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWinInetRequestStream::set_Owner(CWinInetConnector *pOwner)
{
    ASSERT(m_pOwner == 0);
    ASSERT(pOwner   != 0);

    m_pOwner = pOwner;

    return S_OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWinInetRequestStream::Send(HINTERNET hRequest)
//
//  parameters:
//
//  description:
//          Sends stream contents to the web
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef UNDER_CE
HRESULT CWinInetRequestStream::Send(HINTERNET hRequest)
#else
HRESULT CWinInetRequestStream::Send(async_internet_handle &hRequest)
#endif 
{
    HRESULT                     hr      = S_OK;
    CMemoryStream::StreamPage  *pPage   = 0;
#ifndef UNDER_CE
    INTERNET_BUFFERS            ib;
#endif

    for (int retry = 0; retry < FORCE_RETRY_MAX; retry ++)
    {
        ASSERT(hRequest);

        TRACE(("Sending HttpSendRequestEx\n"));

#ifdef UNDER_CE
     
        //Because httplite doesnt support InternetWriteFile 
        //  we must use HttpSend request -- do so by build a big buffer and
        //  send it out
        
        //allocate a temp buffer for the stream
        UINT uiBufSize = m_MemoryStream.get_TotalSize();
        char *pTempBuf = new char[uiBufSize];
        
        if(!pTempBuf)
        {
            ASSERT(FALSE);
            hr = E_OUTOFMEMORY;
            goto Error;
        }

       
        //concatinate all the buffers together into a big string
        char *pTemp2 = pTempBuf;
        for(pPage = m_MemoryStream.get_Head(); pPage != 0 && pPage->m_dwUsed > 0; pPage = pPage->m_pNextPage)
        {
            memcpy(pTemp2, pPage->m_Data, pPage->m_dwUsed);;
            pTemp2 += pPage->m_dwUsed;
        }
       
        //send off the request 
        if(TRUE != HttpSendRequest(hRequest, 0,0,pTempBuf,uiBufSize))
        {
             VARIANT v;
            VariantInit(&v);
            hr = m_pOwner->get_Property(L"Timeout", &v);
    
            if(SUCCEEDED(hr))
                hr = MAP_WININET_ERROR_(hRequest.wait(v.lVal)); 
           
            VariantClear(&v);

            if(FAILED(hr))
            {                      
                delete [] pTempBuf;
                goto Error;
            }
        }
        
        delete [] pTempBuf;
        hr = S_OK;
        break;
#else   
        memset(&ib, 0, sizeof(INTERNET_BUFFERS));
        ib.dwStructSize  = sizeof(INTERNET_BUFFERS);
        ib.dwBufferTotal = m_MemoryStream.get_TotalSize();


        //send off the request 
        if(TRUE != HttpSendRequestEx(hRequest, &ib, 0, HSR_INITIATE, 0))
        {
            VARIANT v;
            VariantInit(&v);
            hr = m_pOwner->get_Property(L"Timeout", &v);
    
            if(SUCCEEDED(hr))
                hr = MAP_WININET_ERROR_(hRequest.wait(v.lVal)); 
                     
            VariantClear(&v);

            if(FAILED(hr))
            {   
                goto Error;
            }
        }

        TRACE(("InternetWriteFileLoop\n"));

        for(pPage = m_MemoryStream.get_Head(); pPage != 0 && pPage->m_dwUsed > 0; pPage = pPage->m_pNextPage)
        {
            DWORD dwWritten = 0;

#ifndef UNDER_CE
            CHKBE(::InternetWriteFile(hRequest, pPage->m_Data, pPage->m_dwUsed, &dwWritten), MAP_WININET_ERROR());
#else
            if(TRUE != InternetWriteFile(hRequest, pPage->m_Data, pPage->m_dwUsed, &dwWritten))
            {
                VARIANT v;
                VariantInit(&v);
                hr = m_pOwner->get_Property(L"Timeout", &v);
        
                if(SUCCEEDED(hr))
                    hr = MAP_WININET_ERROR_(hRequest.wait(v.lVal));                 
                  
                VariantClear(&v);

                if(FAILED(hr))
                {   
                    goto Error;
                }
            }


#endif /* UNDER_CE */
            TRACE(("InternetWriteFile written == (%i)\n", dwWritten));
        }

        TRACE(("HttpEndRequest\n"));

        if (! ::HttpEndRequest(hRequest, 0, 0, 0))
        {
#ifdef UNDER_CE
			VARIANT v;
            VariantInit(&v);
            hr = m_pOwner->get_Property(L"Timeout", &v);
    
            if(SUCCEEDED(hr))
                hr = MAP_WININET_ERROR_(hRequest.wait(v.lVal)); 
                    
            VariantClear(&v);

			if(SUCCEEDED(hr))
			{
				hr = S_OK;
				break;	
			}
#endif


            switch (hr = HRESULT_FROM_WIN32(GetLastError()))
            {
            case HRESULT_FROM_WIN32(ERROR_INTERNET_FORCE_RETRY):
                TRACE(("Forced data resend # %i", retry + 1));
                continue;
            case HRESULT_FROM_WIN32(ERROR_INTERNET_CLIENT_AUTH_CERT_NEEDED):
                goto Error;
            default:
                hr = MAP_WININET_ERROR_(hr);
                goto Error;
            }
        }
        else
        {
            hr = S_OK;
            break;
        }
#endif
    }

    TRACE(("Send complete\n"));

Error:
    return hr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CWinInetRequestStream::EmptyStream()
//
//  parameters:
//
//  description:
//          Empties the contents of the request memory stream
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CWinInetRequestStream::EmptyStream()
{
    return m_MemoryStream.EmptyStream();
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CWinInetRequestStream::Read(void *pv, ULONG cb, ULONG *pcbRead)
//
//  parameters:
//
//  description:
//          Read from request stream ... does nothing.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CWinInetRequestStream::Read(void *pv, ULONG cb, ULONG *pcbRead)
{
    if (pcbRead)
        *pcbRead = 0;
    return S_FALSE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CWinInetRequestStream::Write(const void *pv, ULONG cb, ULONG *pcbWritten)
//
//  parameters:
//
//  description:
//          ISequentialStream's Write method
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CWinInetRequestStream::Write(const void *pv, ULONG cb, ULONG *pcbWritten)
{
    return m_MemoryStream.Write(pv, cb, pcbWritten);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
