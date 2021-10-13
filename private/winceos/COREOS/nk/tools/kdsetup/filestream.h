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

#pragma once

#include <unknwn.h>
#include <objidl.h>

class Logger;

IStream*
CreateFileStream(
    __in_z const wchar_t* szXmlFileName,
    bool bWrite
    );

class FileStream
    : public IStream
{
public:

    static HRESULT OpenFile(
        __in_z const wchar_t* szName,
        __deref_out IStream** ppStream,
        __in bool bWrite
        );

public: // IUnknown

    // Recognized: IUnknown, IStream, ISequentialStream.
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(
        __in REFIID iid,
        __deref_out void** ppvObject
        );

    virtual ULONG STDMETHODCALLTYPE AddRef(
        );

    virtual ULONG STDMETHODCALLTYPE Release(
        );

public: // ISequentialStream Interface

    virtual HRESULT STDMETHODCALLTYPE Read(
        __out_bcount_part(cb, *pcbRead) void* pv,
        __in ULONG cb,
        __out ULONG* pcbRead
        );

    virtual HRESULT STDMETHODCALLTYPE Write(
        __in_bcount(cb) void const* pv,
        __in ULONG cb,
        __out ULONG* pcbWritten
        );

public: // IStream Interface

    virtual HRESULT STDMETHODCALLTYPE Seek(
        __in LARGE_INTEGER liDistanceToMove,
        __in DWORD dwOrigin,
        __out ULARGE_INTEGER* pliNewPosition
        );

    virtual HRESULT STDMETHODCALLTYPE SetSize(
        __in ULARGE_INTEGER liNewSize
        );

    // E_NOTIMPL
    virtual HRESULT STDMETHODCALLTYPE CopyTo(
        __in IStream* pIStream,
        __in ULARGE_INTEGER cb,
        __out ULARGE_INTEGER* pcbRead,
        __out ULARGE_INTEGER* pcbWritten
        );

    // E_NOTIMPL
    virtual HRESULT STDMETHODCALLTYPE Commit(
        __in DWORD dwFlags
        );

    // E_NOTIMPL
    virtual HRESULT STDMETHODCALLTYPE Revert(
        );

    // E_NOTIMPL
    virtual HRESULT STDMETHODCALLTYPE LockRegion(
        __in ULARGE_INTEGER liOffset,
        __in ULARGE_INTEGER cb,
        __in DWORD dwLockType
        );

    // E_NOTIMPL
    virtual HRESULT STDMETHODCALLTYPE UnlockRegion(
        __in ULARGE_INTEGER liOffset,
        __in ULARGE_INTEGER cb,
        __in DWORD dwLockType
        );

    virtual HRESULT STDMETHODCALLTYPE Stat(
        __out STATSTG* pStatstg,
        __in DWORD dwStatFlag
        );

    // E_NOTIMPL
    virtual HRESULT STDMETHODCALLTYPE Clone(
        __deref_out IStream** ppIStream
        );

private:

    FileStream(
        __in HANDLE hFile
        );

    ~FileStream(
        );

private:

    const HANDLE m_hFile;
    LONG m_refcount;
};
