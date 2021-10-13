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
//      MemoryStream.cpp
//
// Contents:
//
//      Implementation of the CMemoryStream class.
//
//-----------------------------------------------------------------------------


#include "Headers.h"

#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CMemoryStream::StreamPage *CMemoryStream::AllocPage(DWORD dwSize)
//
//  parameters:
//          dwSize - size of the page to allocate
//  description:
//          Allocates new memory stream page
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
CMemoryStream::StreamPage *CMemoryStream::AllocPage(DWORD dwSize)
{
    CMemoryStream::StreamPage *pPage =
        reinterpret_cast<CMemoryStream::StreamPage *>(new char[offsetof(CMemoryStream::StreamPage, m_Data) + dwSize]);

    if ( pPage )
        pPage->Initialize(dwSize);

    return pPage;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CMemoryStream::FreePage(CMemoryStream::StreamPage *pPage)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void CMemoryStream::FreePage(CMemoryStream::StreamPage *pPage)
{
    delete [] reinterpret_cast<char *>(pPage);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CMemoryStream::CMemoryStream()
//
//  parameters:
//
//  description:
//          CMemoryStream Constructor
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CMemoryStream::CMemoryStream()
  : m_pHead(0),
    m_pTail(0),
    m_dwTotalSize(0)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CMemoryStream::~CMemoryStream()
//
//  parameters:
//
//  description:
//          Destructor
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CMemoryStream::~CMemoryStream()
{
    Initialize();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CMemoryStream::Initialize()
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CMemoryStream::Initialize()
{
    while ( m_pHead )
    {
        StreamPage *pPage = m_pHead;
        m_pHead = m_pHead->m_pNextPage;
        FreePage(pPage);
    }

    m_pHead       = 0;
    m_pTail       = 0;
    m_dwTotalSize = 0;

    m_ReadPosition.Clear();
    m_WritePosition.Clear();

    return S_OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CMemoryStream::Read(void *pv, ULONG cb, ULONG *pcbRead)
//
//  parameters:
//          
//  description:
//          
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CMemoryStream::Read(void *pv, ULONG cb, ULONG *pcbRead)
{
    HRESULT     hr     = S_OK;
    DWORD       dwRead = 0;
    const char *pData  = reinterpret_cast<const char *>(pv);

    if (! cb)
        goto Cleanup;

    if (! m_ReadPosition.m_pPage)
    {
        if (! m_pTail)  // Nothing to read
        {
            ASSERT(hr == S_OK);
            goto Cleanup;
        }
        m_ReadPosition.m_pPage  = m_pHead;
        m_ReadPosition.m_Offset = 0;
    }

    for( ; ; )
    {
        ASSERT(m_ReadPosition.m_pPage);
        DWORD dwR = m_ReadPosition.m_pPage->Read((void*)pData, m_ReadPosition.m_Offset, cb);
        dwRead += dwR;
        cb     -= dwR;
        pData  += dwR;
        m_ReadPosition.m_Offset += dwR;

        if ( cb > 0 )
        {
            if(AdvanceReadPage() != S_OK)
                break;
        }
        else
            break;
    }

Cleanup:
    if (pcbRead)
        *pcbRead = dwRead;

    return hr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CMemoryStream::Write(const void *pv, ULONG cb, ULONG *pcbWritten)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CMemoryStream::Write(const void *pv, ULONG cb, ULONG *pcbWritten)
{
    HRESULT hr        = S_OK;
    DWORD dwWritten   = 0;
    const char *pData = reinterpret_cast<const char *>(pv);

    if (! cb)
        goto Cleanup;

    if ( ! m_WritePosition.m_pPage )
    {
        if ( ! m_pHead )
        {
            ASSERT(m_pTail == 0);
            CHK(AddNewPage(cb));
        }
        m_WritePosition.m_pPage = m_pHead;
    }

    for( ; ; )
    {
        DWORD dwW = m_WritePosition.m_pPage->Write(pData, cb);
        dwWritten += dwW;
        cb        -= dwW;
        pData     += dwW;

        if ( cb > 0 )
        {
            ASSERT(m_WritePosition.m_pPage->Available() == 0);

            AdvanceWritePage(cb);

            ASSERT(m_WritePosition.m_pPage->Available() >= DefaultPageSize);
        }
        else
            break;
    }

Cleanup:
    m_dwTotalSize += dwWritten;
    if (pcbWritten)
        *pcbWritten = dwWritten;
    return hr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CMemoryStream::PreAllocate(DWORD dwSize)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CMemoryStream::PreAllocate(DWORD dwSize)
{
    HRESULT hr = S_OK;

    CHK(Initialize());
    CHK(AddNewPage(dwSize));

    m_WritePosition.m_pPage = m_pHead;
    hr = S_OK;

Cleanup:
    return hr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CMemoryStream::EmptyStream()
//
//  parameters:
//          
//  description:
//          Empties the stream
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CMemoryStream::EmptyStream()
{
    m_WritePosition.m_pPage  = m_pHead;
    m_WritePosition.m_Offset = 0;

    m_ReadPosition.m_Offset  = 0;
    m_ReadPosition.m_pPage   = m_pHead;

	m_dwTotalSize            = 0;

    StreamPage *pPage = m_pHead;

    while (pPage)
    {
        pPage->Empty();
        pPage = pPage->m_pNextPage;
    }

    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CMemoryStream::get_RawData(BYTE **ppData, DWORD *pdwSize)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CMemoryStream::get_RawData(BYTE **ppData, DWORD *pdwSize)
{
    ASSERT(ppData && ! *ppData);
    ASSERT(pdwSize);

    DWORD   dwSize = get_TotalSize();
    BYTE   *pData  = 0;
    HRESULT hr     = S_OK;

    if(dwSize != 0)
    {
        StreamPage *pPage = 0;
        BYTE       *pCopy = 0;

        if ((pData = new BYTE[dwSize]) == 0)
        {
            hr = E_OUTOFMEMORY;
            goto Cleanup;
        }

        pCopy = pData;

        for (pPage = get_Head(); pPage != 0; pPage = pPage->m_pNextPage)
        {
            memcpy(pCopy, pPage->m_Data, pPage->m_dwUsed);
            pCopy += pPage->m_dwUsed;
        }
    }

    *ppData  = pData;
    *pdwSize = dwSize;

Cleanup:
    return hr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CMemoryStream::get_SafeArray(SAFEARRAY **pArray)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CMemoryStream::get_SafeArray(SAFEARRAY **pArray)
{
    ASSERT(pArray);

    DWORD      dwSize = get_TotalSize();
    HRESULT    hr     = S_OK;
    SAFEARRAY *pA     = 0;

    if (dwSize != 0)
    {
        void       *pData = 0;
        BYTE       *pCopy = 0;
        StreamPage *pPage = 0;

        pA = ::SafeArrayCreateVector(VT_UI1, 0, get_TotalSize());
        CHK_MEM(pA);

        CHK(::SafeArrayAccessData(pA, &pData));

        pCopy = reinterpret_cast<BYTE *>(pData);

        for (pPage = get_Head(); pPage != 0; pPage = pPage->m_pNextPage)
        {
            memcpy(pCopy, pPage->m_Data, pPage->m_dwUsed);
            pCopy += pPage->m_dwUsed;
        }

        CHK(::SafeArrayUnaccessData(pA));
    }

    *pArray = pA;
    pA      = 0;

Cleanup:
    if (pA)
        ::SafeArrayDestroy(pA);
    return hr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CMemoryStream::AddNewPage(DWORD dwSize)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CMemoryStream::AddNewPage(DWORD dwSize)
{
    StreamPage *pPage = AllocPage(max(dwSize, DefaultPageSize));
    if (pPage == 0)
        return E_OUTOFMEMORY;

    if ( ! m_pHead )
        m_pHead = pPage;
    if ( m_pTail )
        m_pTail->m_pNextPage = pPage;
    m_pTail = pPage;

    return S_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CMemoryStream::AdvanceWritePage(DWORD dwSize)
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CMemoryStream::AdvanceWritePage(DWORD dwSize)
{
    HRESULT hr = S_OK;

    if ( m_WritePosition.m_pPage == m_pTail )
    {
        ASSERT(m_pTail->m_pNextPage == 0);

        CHK(AddNewPage(dwSize));

        ASSERT(m_pTail->m_pNextPage == 0);
    }

    ASSERT(m_WritePosition.m_pPage->m_pNextPage != 0);
    m_WritePosition.m_pPage = m_WritePosition.m_pPage->m_pNextPage;
    m_WritePosition.m_pPage->Empty();

Cleanup:
    return hr;
}
////////////////////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CMemoryStream::AdvanceReadPage()
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CMemoryStream::AdvanceReadPage()
{
    ASSERT(m_ReadPosition.m_pPage != 0);

    if (m_ReadPosition.m_pPage->m_pNextPage)
    {
        m_ReadPosition.m_pPage   = m_ReadPosition.m_pPage->m_pNextPage;
        m_ReadPosition.m_Offset = 0;

        return S_OK;
    }
    else
    {
        return S_FALSE;
    }
}
