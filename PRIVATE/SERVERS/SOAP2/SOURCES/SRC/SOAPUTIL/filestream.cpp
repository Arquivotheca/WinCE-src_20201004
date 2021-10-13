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
//      FileStream.cpp
//
// Contents:
//
//      CFileStream class implementation
//
//-----------------------------------------------------------------------------

#include "Headers.h"

#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface map
////////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_INTERFACE_MAP(CFileStream)
    ADD_IUNKNOWN(CFileStream, IStream)
    ADD_INTERFACE(CFileStream, IStream)
    ADD_INTERFACE(CFileStream, ISequentialStream)
END_INTERFACE_MAP(CFileStream)

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CFileStream::CFileStream()
//
//  parameters:
//          
//  description:
//          CFileStream constructor
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
CFileStream::CFileStream()
: m_hFile(INVALID_HANDLE_VALUE)
{
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CFileStream::~CFileStream()
//
//  parameters:
//          
//  description:
//          Destructor
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
CFileStream::~CFileStream()
{
    if (m_hFile != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(m_hFile);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CFileStream::Initialize(HANDLE hFile)
//
//  parameters:
//          
//  description:
//          Initilizes CFileStream with given file handle
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CFileStream::Initialize(HANDLE hFile)
{
    if (m_hFile != INVALID_HANDLE_VALUE)
    {
        return E_FAIL;
    }

    if (hFile == INVALID_HANDLE_VALUE)
    {
        return E_INVALIDARG;
    }

    m_hFile = hFile;

    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: HRESULT CFileStream::Initialize(LPCWSTR lpFileName, DWORD dwAccess, DWORD dwShare, DWORD dwCreation, DWORD dwFlags)
//
//  parameters:
//          
//  description:
//          Creates new file with respect to given flags
//  returns:
//          
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CFileStream::Initialize(LPCWSTR lpFileName, DWORD dwAccess, DWORD dwShare, DWORD dwCreation, DWORD dwFlags)
{
    if (m_hFile != INVALID_HANDLE_VALUE)
    {
        return E_FAIL;
    }

    m_hFile = ::CreateFile(lpFileName, dwAccess, dwShare, 0, dwCreation, dwFlags, 0);

    if (m_hFile == INVALID_HANDLE_VALUE)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CFileStream::Read(void *pv, ULONG cb, ULONG *pcbRead)
//
//  parameters:
//          
//  description:
//          Reads from file
//  returns:
//          S_OK if succeeded, otherwise HRESULT_FROM_WIN32(GetLastError())
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CFileStream::Read(void *pv, ULONG cb, ULONG *pcbRead)
{
    DWORD   dwRead = 0;
    HRESULT hr     = S_OK;

    CHK_BOOL(::ReadFile(m_hFile, pv, cb, &dwRead, 0) == TRUE, HRESULT_FROM_WIN32(GetLastError()));

Cleanup:
    if (pcbRead)
        *pcbRead = dwRead;
    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CFileStream::Write(const void *pv, ULONG cb, ULONG *pcbWritten)
//
//  parameters:
//          
//  description:
//          
//  returns:
//          S_OK if succeeded, otherwise HRESULT_FROM_WIN32(GetLastError())
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CFileStream::Write(const void *pv, ULONG cb, ULONG *pcbWritten)
{
    DWORD   dwWritten = 0;
    HRESULT hr        = S_OK;

    CHK_BOOL(::WriteFile(m_hFile, pv, cb, &dwWritten, 0) == TRUE, HRESULT_FROM_WIN32(GetLastError()));

Cleanup:
    if (pcbWritten)
        *pcbWritten = dwWritten;
    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CFileStream::Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition)
//
//  parameters:
//          
//  description:
//          Not implemented yet
//  returns:
//          E_FAIL
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CFileStream::Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition)
{
    switch (dwOrigin)
    {
    case STREAM_SEEK_SET:
        dwOrigin = FILE_BEGIN;
        break;
    case STREAM_SEEK_CUR:
        dwOrigin = FILE_CURRENT;
        break;
    case STREAM_SEEK_END:
        dwOrigin = FILE_END;
        break;
    }

    dlibMove.LowPart = ::SetFilePointer(m_hFile,  dlibMove.LowPart, &dlibMove.HighPart, dwOrigin);

    if (plibNewPosition)
    {
        memcpy(plibNewPosition, &dlibMove, sizeof(LARGE_INTEGER));
    }

    return S_OK;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CFileStream::SetSize(ULARGE_INTEGER libNewSize)
//
//  parameters:
//          
//  description:
//          Not implemented yet
//  returns:
//          E_FAIL
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CFileStream::SetSize(ULARGE_INTEGER libNewSize)
{
    return E_FAIL;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CFileStream::CopyTo(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten)
//
//  parameters:
//          
//  description:
//          Not implemented yet
//  returns:
//          E_FAIL
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CFileStream::CopyTo(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten)
{
    return E_FAIL;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CFileStream::Commit(DWORD grfCommitFlags)
//
//  parameters:
//          
//  description:
//          Not implemented yet
//  returns:
//          E_FAIL
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CFileStream::Commit(DWORD grfCommitFlags)
{
    return ::FlushFileBuffers(m_hFile) ? S_OK : HRESULT_FROM_WIN32(GetLastError());
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CFileStream::Revert(void)
//
//  parameters:
//          
//  description:
//          Not implemented yet
//  returns:
//          E_FAIL
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CFileStream::Revert(void)
{
    return E_FAIL;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CFileStream::LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
//
//  parameters:
//          
//  description:
//          Not implemented yet
//  returns:
//          E_FAIL
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CFileStream::LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
{
    return E_FAIL;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CFileStream::UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
//
//  parameters:
//          
//  description:
//          Not implemented yet
//  returns:
//          E_FAIL
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CFileStream::UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
{
    return E_FAIL;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CFileStream::Stat(STATSTG *pstatstg, DWORD grfStatFlag)
//
//  parameters:
//          
//  description:
//          Not implemented yet
//  returns:
//          E_FAIL
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CFileStream::Stat(STATSTG *pstatstg, DWORD grfStatFlag)
{
    return E_FAIL;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: STDMETHODIMP CFileStream::Clone(IStream **ppstm)
//
//  parameters:
//          
//  description:
//          Not implemented yet
//  returns:
//          E_FAIL
////////////////////////////////////////////////////////////////////////////////////////////////////
STDMETHODIMP CFileStream::Clone(IStream **ppstm)
{
    return E_FAIL;
}
