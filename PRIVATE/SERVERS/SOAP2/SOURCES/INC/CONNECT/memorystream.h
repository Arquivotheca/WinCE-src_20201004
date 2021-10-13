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
//      MemoryStream.h
//
// Contents:
//
//      CMemoryStream class declaration
//
//-----------------------------------------------------------------------------


#ifndef __MEMORYSTREAM_H_INCLUDED__
#define __MEMORYSTREAM_H_INCLUDED__

class CMemoryStream  
{
public:
    struct StreamPage
    {
        StreamPage *m_pNextPage;
        DWORD       m_dwSize;
        DWORD       m_dwUsed;
        BYTE        m_Data[1];

        void Initialize(DWORD dwSize);
        void Empty();
        DWORD Available();
        DWORD Read(void *pv, DWORD dwOffset, ULONG cb);
        DWORD Write(const void *pv, ULONG cb);
    };

    struct StreamPosition
    {
        StreamPage *m_pPage;
        DWORD       m_Offset;

        StreamPosition();
        void Clear();
    };

    enum
    {
        DefaultPageSize = 4096
    };

private:
    static StreamPage *AllocPage(DWORD dwSize);
    static void FreePage(StreamPage *pPage);

    StreamPage      *m_pHead;
    StreamPage      *m_pTail;

    StreamPosition   m_ReadPosition;
    StreamPosition   m_WritePosition;

    DWORD            m_dwTotalSize;

public:
	CMemoryStream();
	~CMemoryStream();

    DWORD get_TotalSize();
    StreamPage *get_Head();

public:
    HRESULT Initialize();

    HRESULT Read(void *pv, ULONG cb, ULONG *pcbRead);
    HRESULT Write(const void *pv, ULONG cb, ULONG *pcbWritten);

    HRESULT PreAllocate(DWORD dwSize);
    HRESULT EmptyStream();

    HRESULT get_RawData(BYTE **ppData, DWORD *pdwSize);
    HRESULT get_SafeArray(SAFEARRAY **pArray);

private:
    HRESULT AdvanceWritePage(DWORD dwSize);
    HRESULT AdvanceReadPage();
    HRESULT AddNewPage(DWORD dwSize);
};

//////////////////////////////////////////////////////////////////////
// CMemoryStream::StreamPage inline methods
//////////////////////////////////////////////////////////////////////

inline void CMemoryStream::StreamPage::Initialize(DWORD dwSize)
{
    m_dwSize     = dwSize;
    m_dwUsed     = 0;
    m_pNextPage  = 0;
}

inline void CMemoryStream::StreamPage::Empty()
{
    m_dwUsed     = 0;
}

inline DWORD CMemoryStream::StreamPage::Available()
{
    return m_dwSize - m_dwUsed;
}

inline DWORD CMemoryStream::StreamPage::Write(const void *pv, ULONG cb)
{
    DWORD dwWritten = min(cb, Available());
    memcpy(m_Data + m_dwUsed, pv, dwWritten);
    m_dwUsed += dwWritten;
    return dwWritten;
}

inline DWORD CMemoryStream::StreamPage::Read(void *pv, DWORD dwOffset, ULONG cb)
{
    if (dwOffset > m_dwUsed)
        return 0;

    DWORD dwRead = min(m_dwUsed - dwOffset, cb);
    memcpy(pv, m_Data + dwOffset, dwRead);
    return dwRead;
}

//////////////////////////////////////////////////////////////////////
// CMemoryStream::StreamPosition inline methods
//////////////////////////////////////////////////////////////////////

inline CMemoryStream::StreamPosition::StreamPosition()
{
    Clear();
}

inline void CMemoryStream::StreamPosition::Clear()
{
    m_pPage  = 0;
    m_Offset = 0;
}

//////////////////////////////////////////////////////////////////////
// CMemoryStream inline methods
//////////////////////////////////////////////////////////////////////

inline DWORD CMemoryStream::get_TotalSize()
{
    return m_dwTotalSize;
}

inline CMemoryStream::StreamPage *CMemoryStream::get_Head()
{
    return m_pHead;
}


#endif //__MEMORYSTREAM_H_INCLUDED__
