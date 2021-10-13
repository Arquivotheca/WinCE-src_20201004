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
#pragma once

#include <stdio.h>
#include "unknown.h"

class FileStream : public _simpleunknown<IStream>
{
public:
    FileStream()
    :
        m_hFile(NULL),
        m_bRead(true)
    {
    }

    ~FileStream()
    {
        ::CloseHandle(m_hFile);
    }

    bool open(LPCTSTR name, bool m_bRead = true)
    {
        this->m_bRead = m_bRead;

        if (m_bRead)
        {
            m_hFile = ::CreateFile(
                name,
                GENERIC_READ,
                FILE_SHARE_READ,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL);
        }
        else
        {
            m_hFile = ::CreateFile(
                name,
                GENERIC_WRITE,
                FILE_SHARE_READ,
                NULL,
                CREATE_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                NULL);
        }
        return (m_hFile == INVALID_HANDLE_VALUE) ? false : true;
    }

    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Read(
        /* [out] */ void __RPC_FAR *pv,
        /* [in] */ ULONG cb,
        /* [out] */ ULONG __RPC_FAR *pcbRead)
    {
        if (!m_bRead) return E_FAIL;

        DWORD len;
        BOOL rc = ReadFile(
            m_hFile,    // handle of file to m_bRead
            pv,    // address of buffer that receives data
            cb,    // number of bytes to m_bRead
            &len,    // address of number of bytes m_bRead
            NULL     // address of structure for data
           );
        if (pcbRead)
            *pcbRead = len;
        return (rc) ? S_OK : E_FAIL;
    }

    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Write(
        /* [size_is][in] */ const void __RPC_FAR *pv,
        /* [in] */ ULONG cb,
        /* [out] */ ULONG __RPC_FAR *pcbWritten)
    {
        if (m_bRead) return E_FAIL;

        BOOL rc = WriteFile(
            m_hFile,    // handle of file to write
            pv,    // address of buffer that contains data
            cb,    // number of bytes to write
            pcbWritten,    // address of number of bytes written
            NULL     // address of structure for overlapped I/O
           );

        return (rc) ? S_OK : E_FAIL;
    }

    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Seek(
        /* [in] */ LARGE_INTEGER dlibMove,
        /* [in] */ DWORD dwOrigin,
        /* [out] */ ULARGE_INTEGER __RPC_FAR *plibNewPosition)
    {
        HRESULT hr;

        DWORD dwMoveMethod;
        switch(dwOrigin)
        {
        case STREAM_SEEK_CUR:
            dwMoveMethod = FILE_CURRENT;
            break;
        case STREAM_SEEK_END:
            dwMoveMethod = FILE_END;
            break;
        case STREAM_SEEK_SET:
            dwMoveMethod = FILE_BEGIN;
            break;
        default:
            return E_INVALIDARG;
        }

        DWORD dwRet = SetFilePointer(m_hFile, dlibMove.LowPart, NULL, dwMoveMethod);
        if (0xFFFFFFFF == dwRet && GetLastError() != NO_ERROR)
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            return hr;
        }

        if (plibNewPosition)
        {
            plibNewPosition->LowPart = dwRet;
        }

        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE SetSize(
        /* [in] */ ULARGE_INTEGER libNewSize)
    {
        UNREFERENCED_PARAMETER(libNewSize);

        return E_FAIL;
    }

    virtual /* [local] */ HRESULT STDMETHODCALLTYPE CopyTo(
        /* [unique][in] */ IStream __RPC_FAR *pstm,
        /* [in] */ ULARGE_INTEGER cb,
        /* [out] */ ULARGE_INTEGER __RPC_FAR *pcbRead,
        /* [out] */ ULARGE_INTEGER __RPC_FAR *pcbWritten)
    {
        UNREFERENCED_PARAMETER(pstm);
        UNREFERENCED_PARAMETER(cb);
        UNREFERENCED_PARAMETER(pcbRead);
        UNREFERENCED_PARAMETER(pcbWritten);

        return E_FAIL;
    }

    virtual HRESULT STDMETHODCALLTYPE Commit(
        /* [in] */ DWORD grfCommitFlags)
    {
        UNREFERENCED_PARAMETER(grfCommitFlags);

        return E_FAIL;
    }

    virtual HRESULT STDMETHODCALLTYPE Revert( void)
    {
        return E_FAIL;
    }

    virtual HRESULT STDMETHODCALLTYPE LockRegion(
        /* [in] */ ULARGE_INTEGER libOffset,
        /* [in] */ ULARGE_INTEGER cb,
        /* [in] */ DWORD dwLockType)
    {
        UNREFERENCED_PARAMETER(libOffset);
        UNREFERENCED_PARAMETER(cb);
        UNREFERENCED_PARAMETER(dwLockType);

        return E_FAIL;
    }

    virtual HRESULT STDMETHODCALLTYPE UnlockRegion(
        /* [in] */ ULARGE_INTEGER libOffset,
        /* [in] */ ULARGE_INTEGER cb,
        /* [in] */ DWORD dwLockType)
    {
        UNREFERENCED_PARAMETER(libOffset);
        UNREFERENCED_PARAMETER(cb);
        UNREFERENCED_PARAMETER(dwLockType);

        return E_FAIL;
    }

    virtual HRESULT STDMETHODCALLTYPE Stat(
        /* [out] */ STATSTG __RPC_FAR *pstatstg,
        /* [in] */ DWORD grfStatFlag)
    {
        UNREFERENCED_PARAMETER(pstatstg);
        UNREFERENCED_PARAMETER(grfStatFlag);

        return E_FAIL;
    }

    virtual HRESULT STDMETHODCALLTYPE Clone(
        /* [out] */ IStream __RPC_FAR *__RPC_FAR *ppstm)
    {
        UNREFERENCED_PARAMETER(ppstm);

        return E_FAIL;
    }

private:
    HANDLE m_hFile;
    bool m_bRead;
};


class MemoryStream : public _simpleunknown<IStream>
{
public:
    MemoryStream(LPBYTE pData, DWORD dwSize)
    :
        m_pBase(pData),
        m_dwSize(dwSize),
        m_dwLoc(0)
    {
    }

    ~MemoryStream()
    {

    }

    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Read(
        /* [out] */ void __RPC_FAR *pv,
        /* [in] */ ULONG cb,
        /* [out] */ ULONG __RPC_FAR *pcbRead)
    {
        if( m_dwLoc > m_dwSize )
        {
            return E_FAIL;
        }

        ULONG free=m_dwSize-m_dwLoc;

        if( cb > free )
        {
            cb = free;
        }

        CopyMemory( pv, m_pBase+m_dwLoc, cb );
        *pcbRead=cb;
        m_dwLoc+=cb;
        return S_OK;
    }

    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Write(
        /* [size_is][in] */ const void __RPC_FAR *pv,
        /* [in] */ ULONG cb,
        /* [out] */ ULONG __RPC_FAR *pcbWritten)
    {
        UNREFERENCED_PARAMETER(pv);
        UNREFERENCED_PARAMETER(cb);
        UNREFERENCED_PARAMETER(pcbWritten);

        return E_NOTIMPL;
    }

    virtual /* [local] */ HRESULT STDMETHODCALLTYPE Seek(
        /* [in] */ LARGE_INTEGER dlibMove,
        /* [in] */ DWORD dwOrigin,
        /* [out] */ ULARGE_INTEGER __RPC_FAR *plibNewPosition)
    {
        ULONG dwLocNew;

        switch(dwOrigin)
        {
        case STREAM_SEEK_CUR:
            dwLocNew = m_dwLoc + dlibMove.LowPart;
            break;
        case STREAM_SEEK_END:
            dwLocNew = m_dwSize - dlibMove.LowPart;
            break;
        case STREAM_SEEK_SET:
            dwLocNew = dlibMove.LowPart;
            break;
        default:
            return E_INVALIDARG;
        }

        if (dwLocNew>m_dwSize)
        {
            return E_INVALIDARG;
        }

        m_dwLoc = dwLocNew;

        if (plibNewPosition)
        {
            plibNewPosition->LowPart = m_dwLoc;
        }

        return S_OK;
    }

    virtual HRESULT STDMETHODCALLTYPE SetSize(
        /* [in] */ ULARGE_INTEGER libNewSize)
    {
        UNREFERENCED_PARAMETER(libNewSize);

        return E_FAIL;
    }

    virtual /* [local] */ HRESULT STDMETHODCALLTYPE CopyTo(
        /* [unique][in] */ IStream __RPC_FAR *pstm,
        /* [in] */ ULARGE_INTEGER cb,
        /* [out] */ ULARGE_INTEGER __RPC_FAR *pcbRead,
        /* [out] */ ULARGE_INTEGER __RPC_FAR *pcbWritten)
    {
        UNREFERENCED_PARAMETER(pstm);
        UNREFERENCED_PARAMETER(cb);
        UNREFERENCED_PARAMETER(pcbRead);
        UNREFERENCED_PARAMETER(pcbWritten);

        return E_FAIL;
    }

    virtual HRESULT STDMETHODCALLTYPE Commit(
        /* [in] */ DWORD grfCommitFlags)
    {
        UNREFERENCED_PARAMETER(grfCommitFlags);

        return E_FAIL;
    }

    virtual HRESULT STDMETHODCALLTYPE Revert( void)
    {
        return E_FAIL;
    }

    virtual HRESULT STDMETHODCALLTYPE LockRegion(
        /* [in] */ ULARGE_INTEGER libOffset,
        /* [in] */ ULARGE_INTEGER cb,
        /* [in] */ DWORD dwLockType)
    {
        UNREFERENCED_PARAMETER(libOffset);
        UNREFERENCED_PARAMETER(cb);
        UNREFERENCED_PARAMETER(dwLockType);

        return E_FAIL;
    }

    virtual HRESULT STDMETHODCALLTYPE UnlockRegion(
        /* [in] */ ULARGE_INTEGER libOffset,
        /* [in] */ ULARGE_INTEGER cb,
        /* [in] */ DWORD dwLockType)
    {
        UNREFERENCED_PARAMETER(libOffset);
        UNREFERENCED_PARAMETER(cb);
        UNREFERENCED_PARAMETER(dwLockType);

        return E_FAIL;
    }

    virtual HRESULT STDMETHODCALLTYPE Stat(
        /* [out] */ STATSTG __RPC_FAR *pstatstg,
        /* [in] */ DWORD grfStatFlag)
    {
        UNREFERENCED_PARAMETER(pstatstg);
        UNREFERENCED_PARAMETER(grfStatFlag);

        return E_FAIL;
    }

    virtual HRESULT STDMETHODCALLTYPE Clone(
        /* [out] */ IStream __RPC_FAR *__RPC_FAR *ppstm)
    {
        UNREFERENCED_PARAMETER(ppstm);

        return E_FAIL;
    }

private:
    LPBYTE m_pBase;
    ULONG  m_dwSize;
    ULONG  m_dwLoc;
};


