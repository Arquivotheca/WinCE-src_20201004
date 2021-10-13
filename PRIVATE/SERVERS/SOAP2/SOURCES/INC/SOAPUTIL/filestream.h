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
//      FileStream.h
//
// Contents:
//
//      CFileStream class declaration - file wrapper class
//
//-----------------------------------------------------------------------------

#ifndef __FILESTREAM_H_INCLUDED__
#define __FILESTREAM_H_INCLUDED__

class CFileStream : public IStream
{
private:
    HANDLE  m_hFile;

protected:
    CFileStream();
    ~CFileStream();

public:

    HRESULT Initialize(HANDLE hFile);
    HRESULT Initialize(LPCWSTR lpFileName, DWORD dwAccess, DWORD dwShare, DWORD dwCreation, DWORD dwFlags);

public:

    //
    // ISequentialStream
    //

    STDMETHOD(Read)(void *pv, ULONG cb, ULONG *pcbRead);
    STDMETHOD(Write)(const void *pv, ULONG cb, ULONG *pcbWritten);

    //
    // IStream
    //

    STDMETHOD(Seek)(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER *plibNewPosition);
    STDMETHOD(SetSize)(ULARGE_INTEGER libNewSize);
    STDMETHOD(CopyTo)(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten);
    STDMETHOD(Commit)(DWORD grfCommitFlags);
    STDMETHOD(Revert)(void);
    STDMETHOD(LockRegion)(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType);
    STDMETHOD(UnlockRegion)(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType);
    STDMETHOD(Stat)(STATSTG *pstatstg, DWORD grfStatFlag);
    STDMETHOD(Clone)(IStream **ppstm);

    DECLARE_INTERFACE_MAP;
};

#endif //__FILESTREAM_H_INCLUDED__
