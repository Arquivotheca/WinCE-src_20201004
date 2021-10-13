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
// Module Name:    isapistrm.cpp
//
// Contents:
//
//    ISAPI Extension that services SOAP packages.
//
//-----------------------------------------------------------------------------

#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif

#include "isapihdr.h"

#define INIT_SOAP_GLOBALS
#include "soapglo.h"



// HTTP Error codes

const char g_szEmpty[]                          = "";
const char g_szStatusOK[]                       = "200 OK";
const char g_szStatusBadRequest[]               = "400.100 Bad Request";
const char g_szStatusAccessDenied[]             = "401 Access Denied";
const char g_szStatusNotFound[]                 = "404 Not Found";
const char g_szStatusMethodNotAllowed[]         = "405 Method Not Allowed";
const char g_szStatusUnsupportedContentType[]   = "415 Unsupported Media Type";
const char g_szStatusUnprocessableEntity[]      = "422 Unprocessable Entity";
const char g_szStatusNotImplemented[]           = "501 Not Implemented";
const char g_szStatusServiceUnavailable[]       = "503 Service Unavailable";
const char g_szStatusTimeout[]                  = "504 Gateway Timeout";
const char g_szHeaderExMethodNotAllowed[]       = "Allow: GET,HEAD,POST\r\n";
const char g_szHeaderExAccessDenied[]           = "WWW-Authenticate: Basic realm=\"%.900s\"\r\n";
const char g_szHeaderExContentExpires[]         = "Expires: -1;\r\n";

const WCHAR g_pwszStatusInternalError[]         = L"500 Internal Server Error";

// HTTP content types

char g_szContentTextXML[]  = "text/xml";
const WCHAR g_pwstrISAPIErrorSource[] = L"Error source: ";


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CIsapiStream::AddRef()
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP_(ULONG) CIsapiStream::AddRef()
{
    return InterlockedIncrement((long*)&m_cRef);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CIsapiStream::Release()
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP_(ULONG) CIsapiStream::Release()
{
    long cRef = InterlockedDecrement((long*)&m_cRef);

    if (!cRef)
    {
        delete this;
        return 0;
    }
    return cRef;

}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CIsapiStream::QueryInterface(REFIID riid, LPVOID *ppv)
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CIsapiStream::QueryInterface(REFIID riid, LPVOID *ppv)
{
    if (NULL == ppv)
        return E_INVALIDARG;

    // Initialize the output param
    *ppv = NULL;

    //    This is the non-delegating IUnknown implementation
    if (riid == IID_IUnknown || riid == IID_ISequentialStream ||
                 riid == IID_IStream )
    {
        *ppv = (LPVOID)this;
    }

    if (NULL == *ppv)
        return E_NOINTERFACE;

    ((LPUNKNOWN)*ppv)->AddRef();
    return S_OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CIsapiStream::Read(void *pv, ULONG cb, ULONG *pcbRead)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CIsapiStream::Read(void *pv,
        ULONG cb, ULONG *pcbRead)
{
    return E_FAIL;

}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CIsapiStream::Write(const void *pv, ULONG cb, ULONG *pcbRead)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CIsapiStream::Write(const void *pv,
        ULONG cb, ULONG *pcbRead)
{
    return E_FAIL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CIsapiStream::Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition)
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CIsapiStream::Seek(LARGE_INTEGER dlibMove,
        DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition)
{
    return E_FAIL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CIsapiStream::SetSize(ULARGE_INTEGER libNewSize)
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CIsapiStream::SetSize(ULARGE_INTEGER libNewSize)
{
    return E_FAIL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CIsapiStream::CopyTo(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead,
//                                 ULARGE_INTEGER *pcbWritten)
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CIsapiStream::CopyTo(IStream *pstm,
    ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead,
    ULARGE_INTEGER *pcbWritten)
{
    return E_FAIL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CIsapiStream::Commit(DWORD grfCommitFlags)
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CIsapiStream::Commit(DWORD grfCommitFlags)
{
    return E_FAIL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CIsapiStream::Revert(void)
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CIsapiStream::Revert(void)
{
    return E_FAIL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CIsapiStream::LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CIsapiStream::LockRegion(
        ULARGE_INTEGER libOffset, ULARGE_INTEGER cb,
        DWORD dwLockType)
{
    return E_FAIL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CIsapiStream::UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CIsapiStream::UnlockRegion(
        ULARGE_INTEGER libOffset, ULARGE_INTEGER cb,
        DWORD dwLockType)
{
    return E_FAIL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CIsapiStream::Stat(STATSTG *pstatstg, DWORD grfStatFlag)
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CIsapiStream::Stat(STATSTG *pstatstg,
        DWORD grfStatFlag)
{
    return E_FAIL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CIsapiStream::Clone(IStream **ppstm)
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CIsapiStream::Clone(IStream **ppstm)
{
    return E_FAIL;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CInputStream::Read(void *pv, ULONG cb, ULONG *pcbRead)
//
//  parameters:
//
//  description:
//      Reads the POST data from iis
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CInputStream::Read(void *pv,
        ULONG cb, ULONG *pcbRead)
{
    if (!m_pECB || (m_pECB->cbTotalBytes <= 0))
        return E_FAIL;

    if (m_dwTotalRead < m_pECB->cbTotalBytes)
    {
        //
        // Read from the data buffer in the ECB
        //
        if (m_dwTotalRead < m_pECB->cbAvailable)
        {
            *pcbRead = min(cb, ULONG(m_pECB->cbAvailable - m_dwTotalRead));
            memcpy(pv, (BYTE *)m_pECB->lpbData + m_dwTotalRead,    *pcbRead);
        }
        else
        {
            // Set the size of our temporary buffer
            *pcbRead = min(cb, ULONG(m_pECB->cbTotalBytes - m_dwTotalRead));

            // Read the data into the temporary buffer
            if (!m_pECB->ReadClient(m_pECB->ConnID, pv, pcbRead))
            {
                return E_FAIL;
            }
        }

        m_dwTotalRead += *pcbRead;
        return m_pECB->cbTotalBytes - m_dwTotalRead ? E_PENDING : S_OK;
    }
    else
        *pcbRead = 0;
    return S_FALSE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: deleteOutputBuffer(void *pObj)
//
//  parameters:
//
//  description: Destructor function for buffers linked list
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void deleteOutputBuffer(void *pObj)
{
    BYTE    * pBuf;

    pBuf = (BYTE *)pObj;
    delete [] pBuf;
    return;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: COutputStream::AddRef()
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP_(ULONG) COutputStream::AddRef()
{
    return InterlockedIncrement((long*)&m_cRef);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: COutputStream::Release()
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP_(ULONG) COutputStream::Release()
{
    long cRef = InterlockedDecrement((long*)&m_cRef);

    if (!cRef)
    {
        delete this;
        return 0;
    }
    return cRef;

}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: COutputStream::QueryInterface(REFIID riid, LPVOID *ppv)
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP COutputStream::QueryInterface(REFIID riid, LPVOID *ppv)
{
    if (NULL == ppv)
        return E_INVALIDARG;

    // Initialize the output param
    *ppv = NULL;

    //    This is the non-delegating IUnknown implementation
    if (riid == IID_IUnknown || riid == IID_ISequentialStream ||
                 riid == IID_IStream)
    {
        *ppv = (LPVOID)this;
    }

    if (riid == IID_ISOAPIsapiResponse)
        *ppv = (LPVOID) &m_SOAPIsapiResponse;

    if (NULL == *ppv)
        return E_NOINTERFACE;

    ((LPUNKNOWN)*ppv)->AddRef();
    return S_OK;

}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: COutputStream::Read(void *pv, ULONG cb, ULONG *pcbRead)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP COutputStream::Read(void *pv,
        ULONG cb, ULONG *pcbRead)
{
    return E_FAIL;

}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: COutputStream::Write(const void *pv, ULONG cb, ULONG *pcbWritten)
//
//  parameters:
//
//  description:
//          Writes to the output Http stream
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP COutputStream::Write(const void *pv,
        ULONG cb, ULONG *pcbWritten)
{
    ULONG cbWritten;
    ULONG cbLeftToWrite;

    // Make sure we didn't send headers yet
    ASSERT(!m_fHeaderSent);
    cbLeftToWrite = cb;

    // Fill out the first buffer if not already FULL
    if (m_cbFirstBuffer < STRM_BUFFER_SIZE)
    {
        // We are still in the first buffer
        cbWritten = STRM_BUFFER_SIZE - m_cbFirstBuffer;
        if (cbWritten > cbLeftToWrite)
            cbWritten = cbLeftToWrite;
        memcpy(m_bFirstBuffer + m_cbFirstBuffer, pv, cbWritten);
        m_cbFirstBuffer += cbWritten;
        cbLeftToWrite -= cbWritten;
    }
    while(cbLeftToWrite)
    {
        // Remainder will be written to linked buffers
        if (!m_pLastBuffer || (m_cbLastBuffer == STRM_BUFFER_SIZE))
        {
            // Need to allocate a new buffer
            m_pLastBuffer = new BYTE[STRM_BUFFER_SIZE];
            if (!m_pLastBuffer)
            {
                m_SOAPIsapiResponse.put_HTTPStatus((BSTR)g_pwszStatusInternalError);
                if (pcbWritten)
                    *pcbWritten = 0;
                return E_OUTOFMEMORY;
            }
            // Add this to the list
            m_pBuffersList.Add((void *) m_pLastBuffer);
            m_cLinkedBuffers++;
            m_cbLastBuffer = 0;
        }

        cbWritten = STRM_BUFFER_SIZE - m_cbLastBuffer;
        if (cbWritten > cbLeftToWrite)
            cbWritten = cbLeftToWrite;
        memcpy(m_pLastBuffer + m_cbLastBuffer, (BYTE *)pv+(cb - cbLeftToWrite), cbWritten);
        m_cbLastBuffer += cbWritten;
        cbLeftToWrite -= cbWritten;
    }

    if (pcbWritten)
        *pcbWritten = cb;

    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: COutputStream::Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition)
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP COutputStream::Seek(LARGE_INTEGER dlibMove,
        DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition)
{
    return E_FAIL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: COutputStream::FlushBuffer()
//
//  parameters:
//
//  description:
//          Flushes to the output Http stream
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP COutputStream::FlushBuffer()
{
    ULONG  cbToWrite;
    DWORD    idx;
    CLinkedList * pLinkIndex = NULL;
    BYTE *    ptempbuf;

    if (!m_pECB)
        return E_FAIL;

    ASSERT(!m_fHeaderSent || (m_cbFirstBuffer == 0));
#ifndef UNDER_CE
    ASSERT(g_dwNaglingFlags == 0 || g_dwNaglingFlags == (HSE_IO_SYNC | HSE_IO_NODELAY));
#endif 

    if(!m_fHeaderSent)
        SendHeader();

    if (m_cbFirstBuffer)
    {
        cbToWrite = m_cbFirstBuffer;
#ifndef UNDER_CE
        m_pECB->WriteClient( m_pECB->ConnID, (void *)m_bFirstBuffer, &cbToWrite, g_dwNaglingFlags);
#else
        m_pECB->WriteClient( m_pECB->ConnID, (void *)m_bFirstBuffer, &cbToWrite, 0);
#endif
        ASSERT(cbToWrite == m_cbFirstBuffer); 
        m_cbFirstBuffer = 0;
    }
    // Write the in between linked buffers
    if (m_cLinkedBuffers)
    {
        if (m_cLinkedBuffers > 1)
        {
            ptempbuf = (BYTE *) m_pBuffersList.First(&pLinkIndex);

            for (idx = 0 ; idx < (m_cLinkedBuffers - 1) ; idx++)
            {
                cbToWrite = STRM_BUFFER_SIZE;
#ifndef UNDER_CE
                m_pECB->WriteClient( m_pECB->ConnID, (void *)ptempbuf, &cbToWrite, g_dwNaglingFlags);
#else
                m_pECB->WriteClient( m_pECB->ConnID, (void *)ptempbuf, &cbToWrite, 0);
#endif 
                ASSERT(cbToWrite == STRM_BUFFER_SIZE);
                ptempbuf = (BYTE *) m_pBuffersList.Next(&pLinkIndex);
            }
        }
        // Write the last buffer
        if (m_cbLastBuffer)
        {
            cbToWrite = m_cbLastBuffer;
#ifndef UNDER_CE
            m_pECB->WriteClient( m_pECB->ConnID, (void *)m_pLastBuffer, &cbToWrite, g_dwNaglingFlags);
#else
            m_pECB->WriteClient( m_pECB->ConnID, (void *)m_pLastBuffer, &cbToWrite, 0);
#endif 
            ASSERT(cbToWrite == m_cbLastBuffer);
            m_cbLastBuffer = 0;
        }

        // Clear all the linked buffers
        m_pBuffersList.DeleteList();
        m_pLastBuffer = NULL;
        m_cLinkedBuffers = 0;
    }

    return (m_errorcode == Error_Success) ? S_OK : E_FAIL ;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: COutputStream::SetSize(ULARGE_INTEGER cbNewSize)
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP COutputStream::SetSize(ULARGE_INTEGER cbNewSize)
{
#if !defined(_W64) && !defined(UNDER_CE)
    m_cbFileSize = (ULONGLONG) cbNewSize;
#else
    m_cbFileSize = (ULONGLONG) cbNewSize.HighPart << 32;
    m_cbFileSize += (ULONGLONG) cbNewSize.LowPart;
#endif
    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: COutputStream::CopyTo(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead,
//                                  ULARGE_INTEGER *pcbWritten)
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP COutputStream::CopyTo(IStream *pstm,
    ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead,
    ULARGE_INTEGER *pcbWritten)
{
    return E_FAIL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: COutputStream::Commit(DWORD grfCommitFlags)
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP COutputStream::Commit(DWORD grfCommitFlags)
{
    return E_FAIL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: COutputStream::Revert(void)
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP COutputStream::Revert(void)
{
    return E_FAIL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: COutputStream::LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP COutputStream::LockRegion(
        ULARGE_INTEGER libOffset, ULARGE_INTEGER cb,
        DWORD dwLockType)
{
    return E_FAIL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: COutputStream::UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP COutputStream::UnlockRegion(
        ULARGE_INTEGER libOffset, ULARGE_INTEGER cb,
        DWORD dwLockType)
{
    return E_FAIL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: COutputStream::Stat(STATSTG *pstatstg, DWORD grfStatFlag)
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP COutputStream::Stat(STATSTG *pstatstg,
        DWORD grfStatFlag)
{
    return E_FAIL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: COutputStream::Clone(IStream **ppstm)
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP COutputStream::Clone(IStream **ppstm)
{
    return E_FAIL;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: COutputStream::Write(IStream *pIStrmIn)
//
//  parameters:
//
//  description:
//          Writes to the output Http stream reading in from the stream given
//            Assumes that the content-length is given in filesize
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP COutputStream::Write(IStream *pIStrmIn)
{
    HRESULT     hr = S_OK;
    BYTE        tempbuf[STRM_BUFFER_SIZE];
    ULONG       cbRead;
    ULONG       cbWritten;

    // We assume that the file size is already given, and use it in the Headers
    ASSERT(m_cbFileSize != 0);
    SendHeader();

    while (TRUE)
    {
        CHK(pIStrmIn->Read(tempbuf, STRM_BUFFER_SIZE, &cbRead));
        if (cbRead == 0)
            break;
        cbWritten = cbRead;
#ifndef UNDER_CE
        m_pECB->WriteClient( m_pECB->ConnID, (void *)tempbuf, &cbWritten, g_dwNaglingFlags);
#else
        m_pECB->WriteClient( m_pECB->ConnID, (void *)tempbuf, &cbWritten, 0);
#endif 
        if (cbWritten != cbRead)
            return E_FAIL;
    }

Cleanup:
    return hr;

}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: COutputStream::SendHeader()
//
//  parameters:
//
//  description:
//          Sends http headers to the output Http stream
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL COutputStream::SendHeader()
{
    HSE_SEND_HEADER_EX_INFO HeaderExInfo;
    char        szHeader[MAX_HEADER_LENGTH];
    char        szTemp[ONEK];
    char* pszStatus = NULL;
    const char* pszHeaderEx = NULL;
    DWORD       cchHeader = 0;
    char        szApplPath[ONEK+1];
    DWORD       cchApplPath = ONEK;
    char        szLength[64];
    DWORD        cchContLength;
    BOOL        KeepAlive = FALSE;

    if (m_fHeaderSent)
        return FALSE;

    if (m_SOAPIsapiResponse.m_pwstrHTTPStatus != NULL)
    {
        DWORD cwchar;
        DWORD dwStringLen;
        // Use the user supplied header
        dwStringLen = wcslen((WCHAR *)(m_SOAPIsapiResponse.m_pwstrHTTPStatus));
#ifndef UNDER_CE
        __try{
#endif 
        pszStatus = (char *) _alloca(dwStringLen * 3 + 1);
#ifndef UNDER_CE
        }
        __except(1){
            return FALSE;
        }
#endif 
        // The buffer is given in Unicode. Convert it to utf-8 before writing
        cwchar = WideCharToMultiByte(CP_UTF8, 0,
                (LPCWSTR)(m_SOAPIsapiResponse.m_pwstrHTTPStatus),
                dwStringLen,
                (LPSTR)pszStatus, dwStringLen * 3 + 1,
                NULL, NULL);
                
#ifdef UNDER_CE
        if(!cwchar)
        {
        cwchar = WideCharToMultiByte(CP_ACP, 0,
                (LPCWSTR)(m_SOAPIsapiResponse.m_pwstrHTTPStatus),
                dwStringLen,
                (LPSTR)pszStatus, dwStringLen * 3 + 1,
                NULL, NULL);
        }
#endif 
        pszStatus[cwchar] = 0;
    }

    switch (m_errorcode)
    {
    case Error_Success:
        if (!pszStatus)
            pszStatus = (char *)g_szStatusOK;
        pszHeaderEx = g_szHeaderExContentExpires;
        KeepAlive   = TRUE;
        break;
    case Error_BadRequest:
        if (!pszStatus)
              pszStatus = (char *)g_szStatusBadRequest;
        break;
    case Error_AccessDenied:                            // permission denied
        if (!pszStatus)
            pszStatus = (char *)g_szStatusAccessDenied;
        szApplPath[0] = NULL;
        m_pECB->GetServerVariable(
            m_pECB->ConnID, "URL", szApplPath, &cchApplPath);
        wsprintfA(szTemp, g_szHeaderExAccessDenied, szApplPath);
        pszHeaderEx = szTemp;
        break;
    case Error_NotFound:                                // bad server or db name or not specified
        if (!pszStatus)
            pszStatus = (char *)g_szStatusNotFound;
        break;
    case Error_MethodNotAllowed:                        // post, get & head are the only methods allowed
        pszHeaderEx = g_szHeaderExMethodNotAllowed;
        if (!pszStatus)
            pszStatus = (char *)g_szStatusMethodNotAllowed;
        break;
    case Error_UnsupportedContentType:                  // content type's other than text/xml,
        if (!pszStatus)
            pszStatus = (char *)g_szStatusUnsupportedContentType;   // or application/x-www-form-urlencoded
        break;
    case Error_UnprocessableEntity:                     // XSL related errors
        if (!pszStatus)
            pszStatus = (char *)g_szStatusUnprocessableEntity;
        break;
    case Error_InternalError:                           // Out of memory etc.
        if (!pszStatus)
            pszStatus = (char *)g_szStatusInternalError;
        break;
    case Error_NotImplemeneted:
        if (!pszStatus)
            pszStatus = (char *)g_szStatusNotImplemented;
        break;
    case Error_Timeout:
        if (!pszStatus)
            pszStatus = (char *)g_szStatusTimeout;
        break;
    case Error_ServiceUnavailable:
        if (!pszStatus)
            pszStatus = (char *)g_szStatusServiceUnavailable;
        break;
    default:
        ASSERT(FALSE);
        if (!pszStatus)
            pszStatus = (char *)g_szStatusInternalError;
        break;
    }

    if (m_fHeadersOnly)
    {
        pszHeaderEx = g_szHeaderExContentExpires;
    }

    // Return the content-length
    if (m_cbFileSize != 0)
    {
        _ui64toa(m_cbFileSize, szLength, 10 );
    }
    else
    {
        cchContLength = m_cbFirstBuffer + m_cbLastBuffer;
        if (m_cLinkedBuffers > 1)
            cchContLength += STRM_BUFFER_SIZE * (m_cLinkedBuffers - 1);
          _itoa(cchContLength, szLength, 10);
    }

    {
        //Assemble the header
        char szHeaderTemp[MAX_HEADER_LENGTH];

        if (m_SOAPIsapiResponse.m_pwstrHTTPCharset)
        {
            cchHeader = wsprintfA(
                            szHeaderTemp,
                            "Content-Type: %s; charset=\"%S\"\r\nContent-Length: %s\r\n",
                            (char *)g_szContentTextXML,
                            m_SOAPIsapiResponse.m_pwstrHTTPCharset,
                            szLength);

        }
        else
        {
            cchHeader = wsprintfA(
                            szHeaderTemp,
                            "Content-Type: %s\r\nContent-Length: %s\r\n",
                            (char *)g_szContentTextXML,
                            szLength);
        }

        if (pszHeaderEx)
        {
            strncat(szHeaderTemp, pszHeaderEx, MAX_HEADER_LENGTH - strlen(szHeaderTemp) - 3);
        }
        
        cchHeader = wsprintfA(szHeader, "%s\r\n", szHeaderTemp);
    }
    
    //ask the server if we are allowed to use keepalives (we might know that
    //  we WANT to -- but the client might not support it
    BOOL fCanKeepAlive = FALSE;
    
    //do a check to see if we CAN do a keepalive
    m_pECB->ServerSupportFunction(m_pECB->ConnID,
        HSE_REQ_IS_KEEP_CONN,
        &fCanKeepAlive, NULL, NULL );                
     
     if(!fCanKeepAlive)
        KeepAlive = FALSE;
    
    
    //fill out the header info
    HeaderExInfo.pszStatus = pszStatus;
    HeaderExInfo.cchStatus = (DWORD) strlen( HeaderExInfo.pszStatus );
    HeaderExInfo.pszHeader = szHeader;
    HeaderExInfo.cchHeader = cchHeader;    
    HeaderExInfo.fKeepConn = KeepAlive;

    m_pECB->ServerSupportFunction(m_pECB->ConnID,
                HSE_REQ_SEND_RESPONSE_HEADER_EX,
                &HeaderExInfo, NULL, NULL );

    m_fHeaderSent = TRUE;
    return TRUE;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: COutputStream::WriteFaultMessage(HRESULT hrResult, BSTR bstrActor)
//
//  parameters:
//
//  description:
//          Writes a SOAP Fault message to the output Http stream
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void  COutputStream::WriteFaultMessage(HRESULT hrResult, BSTR bstrActor)
{

    HRESULT     hr = S_OK;
    VARIANT     vStream;
    CAutoBSTR   bstrFaultString;
    CAutoRefc<ISoapSerializer>  pISoapSerializer;
    CAutoRefc<IStream> pIStrmOut;
    DWORD       hrId;

#ifndef UNDER_CE
    WCHAR        tempbuf[FOURK];
#else  
    WCHAR       *tempbuf = new WCHAR[FOURK];
    CHK_MEM(tempbuf);
#endif 

    VariantInit(&vStream);


    // error code must already be set when we come here
    ASSERT (m_errorcode != Error_Success);

    // Make sure we didn't send headers yet!
    ASSERT(!m_fHeaderSent);

       // Clear all the linked buffers
    m_pBuffersList.DeleteList();
    m_pLastBuffer = NULL;
    m_cLinkedBuffers = 0;
    m_cbFirstBuffer = 0;
    m_cbLastBuffer = 0;


    // Send the headers first, set status code to 500
    m_SOAPIsapiResponse.put_HTTPStatus((BSTR)g_pwszStatusInternalError);

    hr = CoCreateInstance(CLSID_SoapSerializer, NULL,
                          CLSCTX_INPROC_SERVER,
                          IID_ISoapSerializer,
                          (void**)&pISoapSerializer);

    CHK(hr);

    CHK(QueryInterface(IID_IStream, (void **) &pIStrmOut));
    vStream.vt = VT_UNKNOWN;
    V_UNKNOWN(&vStream) = (IUnknown *)pIStrmOut;
    CHK(pISoapSerializer->Init(vStream));

    CHK(pISoapSerializer->startEnvelope((BSTR)g_pwstrEnvPrefix, (BSTR)g_pwstrEncStyleNS, L""));
    CHK(pISoapSerializer->startBody(/* BSTR enc_style_uri */ NULL));

    // Get the faultstring, detail, etc...
    if (m_bstrErrorDescription.Len())
    {
        wcscpy((WCHAR *)tempbuf, g_pwstrISAPIErrorSource);
        wcsncat((WCHAR *)tempbuf, (WCHAR *)m_bstrErrorSource, FOURK-1-wcslen(g_pwstrISAPIErrorSource));
    }
    else
    {
        // No IErrorInfo set, find a generic faultstring
        hrId = HrToMsgId(hrResult);
        GetResourceString(hrId, (WCHAR *)tempbuf, FOURK);
    }

    CHK(bstrFaultString.Assign((BSTR)tempbuf))


      CHK(pISoapSerializer->startFault(L"Server",
                    bstrFaultString,
                    bstrActor));

    // Get the detail info here
    if (m_bstrErrorDescription.Len())
    {
        CHK(pISoapSerializer->startFaultDetail( ));

        // WriteDetailTree
        CHK(pISoapSerializer->startElement(_T("errorInfo"), 0, 0, _T("mserror")));
        CHK(pISoapSerializer->SoapNamespace(_T("mserror"), (BSTR)g_pwstrMSErrorNS));

        CHK(pISoapSerializer->startElement(_T("returnCode"),  0, 0, _T("mserror")));
        swprintf(&(tempbuf[0]), L" HRESULT=0x%lX", hrResult);
        CHK(pISoapSerializer->writeString((WCHAR *)tempbuf));
        CHK(pISoapSerializer->endElement());
        CHK(pISoapSerializer->startElement(_T("callStack"), 0, 0, _T("mserror")));
        CHK(pISoapSerializer->startElement(_T("callElement"), 0, 0, _T("mserror")));
        CHK(pISoapSerializer->startElement(_T("description"), 0, 0, _T("mserror")));
        CHK(pISoapSerializer->writeString((WCHAR *)m_bstrErrorDescription));
        CHK(pISoapSerializer->endElement());
        CHK(pISoapSerializer->endElement());
        // close the callstack
        CHK(pISoapSerializer->endElement());
        // close the errorinfo
        CHK(pISoapSerializer->endElement());

        CHK(pISoapSerializer->endFaultDetail());
    }
    CHK(pISoapSerializer->endFault());
    // End of Soap body element
    CHK(pISoapSerializer->endBody());
    // Finally end the envelope
    CHK(pISoapSerializer->endEnvelope());

    m_bstrErrorDescription.Clear();
    m_bstrErrorSource.Clear();

Cleanup:
#ifdef UNDER_CE
    if(tempbuf)
        delete [] tempbuf;
#endif 
    
    return;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSOAPIsapiResponse::AddRef()
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP_(ULONG) CSOAPIsapiResponse::AddRef()
{
    // Delegate to outer IUnknown
    if (!m_pIUnkOuter)
        return OLE_E_BLANK;

    return m_pIUnkOuter->AddRef();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSOAPIsapiResponse::Release()
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP_(ULONG) CSOAPIsapiResponse::Release()
{
    // Delegate to outer IUnknown
    if (!m_pIUnkOuter)
        return OLE_E_BLANK;

    return m_pIUnkOuter->Release();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSOAPIsapiResponse::QueryInterface(REFIID riid, LPVOID *ppv)
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

STDMETHODIMP CSOAPIsapiResponse::QueryInterface(REFIID riid, LPVOID *ppv)
{
    // Delegate to outer IUnknown
    if (!m_pIUnkOuter)
        return OLE_E_BLANK;

    return m_pIUnkOuter->QueryInterface(riid, ppv);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSOAPIsapiResponse::Init(IUnknown *pIUnkOuter)
//
//  parameters:
//
//  description:
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void CSOAPIsapiResponse::Init(IUnknown *pIUnkOuter)
{
    ASSERT(m_pIUnkOuter == NULL);

    m_pIUnkOuter = pIUnkOuter;

}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSOAPIsapiResponse::get_HTTPStatus(BSTR *pbstrStatus)
//
//  parameters:
//
//  description:
//          Returns the Http stream status
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSOAPIsapiResponse::get_HTTPStatus(BSTR *pbstrStatus)
{
#ifndef UNDER_CE
    try
#else
    __try
#endif 
    {
        if (!m_pwstrHTTPStatus)
            *pbstrStatus = NULL;
        else
            *pbstrStatus = SysAllocString((WCHAR *)m_pwstrHTTPStatus);

        return S_OK;
    }
#ifndef UNDER_CE
    catch (...)
#else
    __except(1)
#endif 
    {
        ASSERTTEXT (FALSE, "CSOAPIsapiResponse::get_HTTPStatus - Unhandled Exception");
        return E_FAIL;
    }

}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSOAPIsapiResponse::put_HTTPStatus(BSTR bstrStatus)
//
//  parameters:
//
//  description:
//          Sets the Http stream status
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSOAPIsapiResponse::put_HTTPStatus(BSTR bstrStatus)
{
    // Called by mssoap1.dll, when a fault message is written to the HTTP body
    WCHAR *pwstrStat;
#ifndef UNDER_CE
    try
#else
    __try
#endif 
    {

        if (!bstrStatus || (*bstrStatus == 0))
            return E_INVALIDARG;

        pwstrStat = new WCHAR[wcslen(bstrStatus)+1];
        if (!pwstrStat)
            return E_OUTOFMEMORY;
        wcscpy (pwstrStat, (WCHAR *) bstrStatus);

        if (m_pwstrHTTPStatus)
            m_pwstrHTTPStatus.Clear();

        m_pwstrHTTPStatus = pwstrStat;
        return S_OK;
    }
#ifndef UNDER_CE
    catch (...)
#else
    __except(1)
#endif 
    {
        ASSERTTEXT (FALSE, "COutputStream::put_HTTPStatus - Unhandled Exception");
        return E_FAIL;
    }

}



////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSOAPIsapiResponse::get_HTTPCharset(BSTR *pbstrCharset)
//
//  parameters:
//
//  description:
//          Returns the Http stream status
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSOAPIsapiResponse::get_HTTPCharset(BSTR *pbstrCharset)
{
#ifndef UNDER_CE
    try
#else
    __try
#endif 
    {
        if (!m_pwstrHTTPCharset)
            *pbstrCharset = NULL;
        else
        {
            *pbstrCharset = SysAllocString((WCHAR *)m_pwstrHTTPCharset);
#ifndef UNDER_CE
            if(NULL == *pbstrCharset)
                return E_OUTOFMEMORY;
#endif 
        }

        return S_OK;
    }
#ifndef UNDER_CE
    catch (...)
#else
    __except(1)
#endif 
    {
        ASSERTTEXT (FALSE, "CSOAPIsapiResponse::get_HTTPCharset - Unhandled Exception");
        return E_FAIL;
    }

}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CSOAPIsapiResponse::put_HTTPCharset(BSTR bstrCharset)
//
//  parameters:
//
//  description:
//          Sets the Http stream status
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CSOAPIsapiResponse::put_HTTPCharset(BSTR bstrCharset)
{
    // Called by mssoap1.dll, when a fault message is written to the HTTP body
    WCHAR *pwstrCharset;
#ifndef UNDER_CE
    try
#else
    __try
#endif 
    {

        if (!bstrCharset || (*bstrCharset == 0))
            return E_INVALIDARG;

        pwstrCharset = new WCHAR[wcslen(bstrCharset)+1];
        if (!pwstrCharset)
            return E_OUTOFMEMORY;
        wcscpy (pwstrCharset, (WCHAR *) bstrCharset);

        if (m_pwstrHTTPCharset)
            m_pwstrHTTPCharset.Clear();

        m_pwstrHTTPCharset = pwstrCharset;
        return S_OK;
    }
#ifndef UNDER_CE
    catch (...)
#else
    __except(1)
#endif 
    {
        ASSERTTEXT (FALSE, "COutputStream::put_HTTPCharset - Unhandled Exception");
        return E_FAIL;
    }

}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: void COutputStream::SetErrorCode(ERROR_CODE errcode)
//
//  parameters:
//
//  description:
//          sets the error code and remembers the relevant error info
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void COutputStream::SetErrorCode(ERROR_CODE errcode)
{
    if (m_errorcode == Error_Success)
    {
        m_errorcode = errcode;
        CAutoRefc<IErrorInfo>  pIErrInfo;
        if (GetErrorInfo(0, &pIErrInfo)==S_OK && pIErrInfo)
        {

            m_bstrErrorDescription.Clear();
            m_bstrErrorSource.Clear();

            // if that fails, we ignore it. send a little generic error
            pIErrInfo->GetSource(&m_bstrErrorSource);
            pIErrInfo->GetDescription(&m_bstrErrorDescription);
        }
    }
};

