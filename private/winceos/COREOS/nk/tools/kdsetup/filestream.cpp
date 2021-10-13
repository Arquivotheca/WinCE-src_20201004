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

/*
class FileStream : public IStream;
function CreateFileStream;

Simple file-based implementation of the COM IStream interface.

*/

#include <windows.h>
#include "FileStream.h"
#include <strsafe.h>

IStream*
CreateFileStream(
    const wchar_t* szXmlFileName,
    bool bWrite
    )
{
    HRESULT hr;
    IStream* piStream;

    hr = FileStream::OpenFile(szXmlFileName, &piStream, bWrite);

    if (FAILED(hr))
    {
        piStream = 0;
    }


    return piStream;
}

HRESULT
FileStream::OpenFile(
    __in_z const wchar_t* szName,
    __deref_out IStream** ppStream,
    bool bWrite
    )
{   
    if (!szName || !ppStream)
    {
        return E_POINTER;
    }

    *ppStream = NULL;

    HANDLE hFile = CreateFileW(
        szName,
        bWrite ? GENERIC_WRITE : GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        bWrite ? CREATE_ALWAYS : OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (INVALID_HANDLE_VALUE == hFile)
    {
        HRESULT hr = S_OK;
        //Retry now by appending \RELEASE\ (from release directory)
        WCHAR szRelfsName[MAX_PATH];
        hr = StringCchPrintfW(szRelfsName, _countof(szRelfsName), L"\\RELEASE\\%s", szName);
        if (SUCCEEDED(hr))
        {
           hFile = CreateFileW(
                   szRelfsName,
                   bWrite ? GENERIC_WRITE : GENERIC_READ,
                   FILE_SHARE_READ,
                   NULL,
                   bWrite ? CREATE_ALWAYS : OPEN_EXISTING,
                   FILE_ATTRIBUTE_NORMAL,
                   NULL);        

           if (INVALID_HANDLE_VALUE == hFile)
           {
              return HRESULT_FROM_WIN32(GetLastError());
           }
        }
    }

    *ppStream = new FileStream(hFile);

    if(*ppStream == NULL)
    {
        CloseHandle(hFile);
        return E_OUTOFMEMORY;
    }
    else
    {
        return S_OK;
    }
}

FileStream::FileStream(
    HANDLE hFile
    ) :
    m_refcount(1),
    m_hFile(hFile)
{
}

FileStream::~FileStream(
    )
{
    if (m_hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hFile);
    }
}

// IUnknown

HRESULT STDMETHODCALLTYPE
FileStream::QueryInterface(
    REFIID iid,
    __deref_out void** ppvObject
    )
{
    if (!ppvObject)
    {
        return E_POINTER;
    }

    if (iid == __uuidof(IUnknown) ||
        iid == __uuidof(IStream) ||
        iid == __uuidof(ISequentialStream))
    {
        *ppvObject = static_cast<IStream*>(this);
        AddRef();
        return S_OK;
    }
    else
    {
        return E_NOINTERFACE;
    }
}

ULONG STDMETHODCALLTYPE
FileStream::AddRef(
    )
{
    ULONG refCount = static_cast<ULONG>(InterlockedIncrement(&m_refcount));
    return refCount;
}

ULONG STDMETHODCALLTYPE
FileStream::Release(
    )
{
    ULONG refCount = static_cast<ULONG>(InterlockedDecrement(&m_refcount));
    if (refCount == 0)
    {
        delete this;
    }

    return refCount;
}

// ISequentialStream Interface

HRESULT STDMETHODCALLTYPE
FileStream::Read(
    void* pv,
    ULONG cb,
    ULONG* pcbRead
    )
{
    BOOL rc = ReadFile(m_hFile, pv, cb, pcbRead, NULL);
    return rc ? S_OK : HRESULT_FROM_WIN32(GetLastError());
}

HRESULT STDMETHODCALLTYPE
FileStream::Write(
    void const* pv,
    ULONG cb,
    ULONG* pcbWritten
    )
{
    BOOL rc = WriteFile(m_hFile, pv, cb, pcbWritten, NULL);
    return rc ? S_OK : HRESULT_FROM_WIN32(GetLastError());
}

// IStream Interface

HRESULT STDMETHODCALLTYPE
FileStream::Seek(
    LARGE_INTEGER liDistanceToMove,
    DWORD dwOrigin,
    ULARGE_INTEGER* pliNewPosition
    )
{
    HRESULT hr = S_OK;
    DWORD dwMoveMethod;

    switch(dwOrigin)
    {
    case STREAM_SEEK_SET:
        dwMoveMethod = FILE_BEGIN;
        break;

    case STREAM_SEEK_CUR:
        dwMoveMethod = FILE_CURRENT;
        break;

    case STREAM_SEEK_END:
        dwMoveMethod = FILE_END;
        break;

    default:
        dwMoveMethod = 0;
        hr = STG_E_INVALIDFUNCTION;
        break;
    }

    if (SUCCEEDED(hr) &&
        0 != liDistanceToMove.HighPart)
    {
        hr = STG_E_SEEKERROR;
    }

    if (SUCCEEDED(hr))
    {
        DWORD dwNewPosition = SetFilePointer(
            m_hFile,
            liDistanceToMove.LowPart,
            0,
            dwMoveMethod);

        if (INVALID_SET_FILE_POINTER == dwNewPosition)
        {
            // May actually be success.
            hr = HRESULT_FROM_WIN32(GetLastError());
        }

        if (SUCCEEDED(hr) && pliNewPosition)
        {
            pliNewPosition->HighPart = 0;
            pliNewPosition->LowPart = dwNewPosition;
        }
    }

    return hr;
}

HRESULT STDMETHODCALLTYPE
FileStream::SetSize(
    ULARGE_INTEGER liNewSize
    )
{
    HRESULT hr;
    DWORD dwOriginalPosition = 0;

    if (0 == liNewSize.HighPart)
    {
        hr = S_OK;
    }
    else
    {
        hr = STG_E_INVALIDFUNCTION;
    }

    // Save original position.
    if (SUCCEEDED(hr))
    {
        LONG dwOriginalPositionHigh = 0;
        dwOriginalPosition = SetFilePointer(m_hFile, 0, &dwOriginalPositionHigh, FILE_CURRENT);

        if (dwOriginalPositionHigh)
        {
            hr = STG_E_SEEKERROR;
        }
        else if (INVALID_SET_FILE_POINTER == dwOriginalPosition)
        {
            // May actually be success.
            hr = HRESULT_FROM_WIN32(GetLastError());
        }
    }

    // Move file pointer to desired end of file position.
    if (SUCCEEDED(hr))
    {
        DWORD dwSetFilePointerResult = SetFilePointer(m_hFile, liNewSize.LowPart, 0, FILE_BEGIN);
        if (INVALID_SET_FILE_POINTER == dwSetFilePointerResult)
        {
            // May actually be success.
            hr = HRESULT_FROM_WIN32(GetLastError());
        }
    }

    if (SUCCEEDED(hr))
    {
        // Expand file to current end of file position.
        if (!SetEndOfFile(m_hFile))
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
        }

        // Restore original file position.
        DWORD dwSetFilePointerResult = SetFilePointer(m_hFile, dwOriginalPosition, 0, FILE_BEGIN);
        if (SUCCEEDED(hr) && INVALID_SET_FILE_POINTER == dwSetFilePointerResult)
        {
            // May actually be success.
            hr = HRESULT_FROM_WIN32(GetLastError());
        }
    }

    return hr;
}

HRESULT STDMETHODCALLTYPE
FileStream::CopyTo(
    __in IStream* /* pIStream */,
    ULARGE_INTEGER /* cb */,
    __out ULARGE_INTEGER* /* pcbRead */,
    __out ULARGE_INTEGER* /* pcbWritten */
    )
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
FileStream::Commit(
    DWORD /* dwFlags */
    )
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
FileStream::Revert(
    )
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
FileStream::LockRegion(
    ULARGE_INTEGER /* liOffset */,
    ULARGE_INTEGER /* cb */,
    DWORD /* dwLockType */
    )
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
FileStream::UnlockRegion(
    ULARGE_INTEGER /* liOffset */,
    ULARGE_INTEGER /* cb */,
    DWORD /* dwLockType */
    )
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
FileStream::Stat(
    STATSTG* pStatstg,
    DWORD dwStatFlag
    )
{
    pStatstg->cbSize.LowPart = GetFileSize(m_hFile, &pStatstg->cbSize.HighPart);
    if (INVALID_FILE_SIZE == pStatstg->cbSize.LowPart)
    {
        // May actually be success.
        return HRESULT_FROM_WIN32(GetLastError());
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE
FileStream::Clone(
    IStream** /* pIStream */
    )
{
    return E_NOTIMPL;
}
