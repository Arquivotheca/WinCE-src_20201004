//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#include <windows.h>
#include <objbase.h>
#include <stdio.h>

//
// CBytesRead
//
//   Implements a basic bit-map to keep track of which bytes have been read.
//

class CBytesRead
{
public:
    // Create the mapping, zero it out.
    CBytesRead(int iSize);
    ~CBytesRead();

    bool SetBytesRead(int iStart, int iCount);
    bool AreAnyBytesRead(int iStart, int iCount);

    bool IsInitialized()
    {
        return m_iSize != 0;
    }
private:
    // Must specify how many bytes to map to.
    CBytesRead() {};

    BYTE *  m_pbBytesRead;
    int     m_iSize;
};

//
// CFileStream
//
//   Implements a basic read-only IStream object. If writing is needed in the
//   future, support can easily be added (though not easily enough to add if no
//   support for writing is needed).
//

class CFileStream : public IStream
{
private:
    // IUnknown
    STDMETHOD (QueryInterface) (const IID & iid, void** ppv);
    STDMETHOD_ (ULONG, AddRef) ();
    STDMETHOD_ (ULONG, Release) ();

    // ISequentialStream
    STDMETHOD (Read) ( 
        /* [length_is][size_is][out] */ void *pv,
        /* [in] */ ULONG cb,
        /* [out] */ ULONG *pcbRead);
    
    STDMETHOD (Write) ( 
        /* [size_is][in] */ const void *pv,
        /* [in] */ ULONG cb,
        /* [out] */ ULONG *pcbWritten)
    {
        UNREFERENCED_PARAMETER(pv);
        UNREFERENCED_PARAMETER(cb);
        UNREFERENCED_PARAMETER(pcbWritten);

        return STG_E_ACCESSDENIED;
    }

    // IStream
    STDMETHOD (Seek)( 
        /* [in] */ LARGE_INTEGER dlibMove,
        /* [in] */ DWORD dwOrigin,
        /* [out] */ ULARGE_INTEGER *plibNewPosition);
    
    STDMETHOD (SetSize) ( 
        /* [in] */ ULARGE_INTEGER libNewSize)
    {
        UNREFERENCED_PARAMETER(libNewSize);
        
        return E_NOTIMPL;
    }
    
    STDMETHOD (CopyTo) ( 
        /* [unique][in] */ IStream *pstm,
        /* [in] */ ULARGE_INTEGER cb,
        /* [out] */ ULARGE_INTEGER  *pcbRead,
        /* [out] */ ULARGE_INTEGER *pcbWritten)
    {
        UNREFERENCED_PARAMETER(pstm);
        UNREFERENCED_PARAMETER(cb);
        UNREFERENCED_PARAMETER(pcbRead);
        UNREFERENCED_PARAMETER(pcbWritten);

        return E_NOTIMPL;
    }
    
    STDMETHOD (Commit) (
        /* [in] */ DWORD grfCommitFlags)
    {
        UNREFERENCED_PARAMETER(grfCommitFlags);

        return STG_E_ACCESSDENIED;
    }
    
    STDMETHOD (Revert) (void)
    {
        return E_NOTIMPL;
    }
    
    STDMETHOD (LockRegion) (
        /* [in] */ ULARGE_INTEGER libOffset,
        /* [in] */ ULARGE_INTEGER cb,
        /* [in] */ DWORD dwLockType)
    {
        UNREFERENCED_PARAMETER(libOffset);
        UNREFERENCED_PARAMETER(cb);
        UNREFERENCED_PARAMETER(dwLockType);

        return E_NOTIMPL;
    }
    
    STDMETHOD (UnlockRegion) (
        /* [in] */ ULARGE_INTEGER libOffset,
        /* [in] */ ULARGE_INTEGER cb,
        /* [in] */ DWORD dwLockType)
    {
        UNREFERENCED_PARAMETER(libOffset);
        UNREFERENCED_PARAMETER(cb);
        UNREFERENCED_PARAMETER(dwLockType);

        return E_NOTIMPL;
    }
    
    STDMETHOD (Stat) (
        /* [out] */ STATSTG *pstatstg,
        /* [in] */ DWORD grfStatFlag);
    
    STDMETHOD (Clone) (
        /* [out] */ IStream **ppstm)
    {
        UNREFERENCED_PARAMETER(ppstm);

        return E_NOTIMPL;
    }
public:
    // Constructor
    CFileStream(bool fRandomize = false, int iRandomSeed = 0) 
      : m_hFile(NULL), 
        m_cRef(0),
        m_fRandomize(fRandomize),
        m_fReplay(false),
        m_iRandomSeed(iRandomSeed),
        m_pbrByteMap(NULL),
        m_ui64FilePointer(0)
    {
        m_tszFilename[0] = 0; 
        m_uliFileSize.QuadPart = 0;
    }

    // Destructor
    ~CFileStream() 
    {
        CloseHandle(m_hFile);
        if (m_pbrByteMap)
        {
            delete m_pbrByteMap;
        }
    }

    // Important part
    HRESULT InitFile(const TCHAR * tszFilename, const TCHAR * tszReplayFile = NULL); 

private:
    // This object is not copyable.
    CFileStream(const CFileStream & fsOther) {}
    const CFileStream & operator=(const CFileStream & fsOther) {}

    UINT64  m_ui64FilePointer;
    HANDLE  m_hFile;
    TCHAR   m_tszFilename[MAX_PATH + 1];
    long    m_cRef;
    bool    m_fRandomize;
    bool    m_fReplay;
    int     m_iRandomSeed;
    ULARGE_INTEGER m_uliFileSize; 
    CBytesRead * m_pbrByteMap;
};
