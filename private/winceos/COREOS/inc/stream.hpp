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
#include <xgf.h>

template <class T>
inline T MinVal(T v1, T v2) { return (v1 < v2 ? v1 : v2); }

/// <topic name="StreamBufferAPIInternal" displayname="Stream Buffer API Internal">
///   <summary>
///     <para>Implementation detail of Stream Buffer APIs.</para>
///   </summary>
///   <topic_scope tref="RenderCoreInternal"/>
/// </topic>

/// <topic_scope tref="StreamBufferAPIInternal">


/// <summary>
/// Base class for all stream types.
/// </summary>
class CStream : public IStream, public IDirectStream
{
protected:
    UINT64 m_cbSeek;  // the current seek position

private:
    LONG m_cRef;  // object reference count

public:
    // IUnknown methods
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, __deref_out LPVOID *ppvObj);
    STDMETHOD_(ULONG,AddRef)(THIS) { return InterlockedIncrement(&m_cRef); }
    STDMETHOD_(ULONG,Release)(THIS) { LONG cRef = InterlockedDecrement(&m_cRef); if (0 == cRef) { delete this; } return cRef; }

    // ISequentialStream methods
    STDMETHOD(Write)(THIS_ __in_bcount(cb) const void *pv, ULONG cb, __out_opt ULONG *pcbWritten) { UNREFERENCED_PARAMETER(pv); UNREFERENCED_PARAMETER(cb); UNREFERENCED_PARAMETER(pcbWritten); return STG_E_INVALIDFUNCTION; }

    // IStream methods
    STDMETHOD(Seek)(THIS_ LARGE_INTEGER dlibMove, DWORD dwOrigin, __out_opt ULARGE_INTEGER *plibNewPosition);
    STDMETHOD(SetSize)(THIS_ ULARGE_INTEGER libNewSize);
    STDMETHOD(CopyTo)(THIS_ __in IStream *pstm, ULARGE_INTEGER cb, __out_opt ULARGE_INTEGER *pcbRead, __out_opt ULARGE_INTEGER *pcbWritten);
    STDMETHOD(Commit)(THIS_ DWORD grfCommitFlags) { UNREFERENCED_PARAMETER(grfCommitFlags); return STG_E_INVALIDFUNCTION; }
    STDMETHOD(Revert)(THIS) { return STG_E_INVALIDFUNCTION; }
    STDMETHOD(LockRegion)(THIS_ ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) { UNREFERENCED_PARAMETER(libOffset); UNREFERENCED_PARAMETER(cb); UNREFERENCED_PARAMETER(dwLockType); return STG_E_INVALIDFUNCTION; }
    STDMETHOD(UnlockRegion)(THIS_ ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) { UNREFERENCED_PARAMETER(libOffset); UNREFERENCED_PARAMETER(cb); UNREFERENCED_PARAMETER(dwLockType); return STG_E_INVALIDFUNCTION; }

protected:
    virtual HRESULT GetSize(__out ULARGE_INTEGER *puliSize) const = 0;

protected:
    CStream() : m_cbSeek(0), m_cRef(1) {}
    CStream(const CStream &obj) : m_cbSeek(obj.m_cbSeek), m_cRef(1) {}
    virtual ~CStream() {}
};

/// <summary>
/// Base class for file-based streams.
/// </summary>
class CStreamFile : public CStream
{
protected:
    class CHandle
    {
    private:
        LONG m_cRef;  // object reference count

    public:
        DWORD  m_dwMode;  // the access mode
        HANDLE m_hFile;   // file handle
        UINT64 m_cbSize;  // the size of the file, in bytes

    public:
        static CHandle *Create(__in_ecount(cchName) LPCWSTR pszName, size_t cchName);
        ULONG AddRef() { return InterlockedIncrement(&m_cRef); }
        ULONG Release() { LONG cRef = InterlockedDecrement(&m_cRef); if (0 == cRef) { this->~CHandle(); LocalFree(this); } return cRef; }

        LPCWSTR GetName(__out_opt size_t *pcchName) const { const size_t *pcch = (size_t*)(this + 1); if (NULL != pcchName) *pcchName = *pcch; return (LPCWSTR)(pcch + 1); }
        HRESULT GetSize(__out ULARGE_INTEGER *puliSize) const;

    private:
        CHandle() : m_cRef(1) {}
        ~CHandle();
    };

protected:
    CHandle *m_phandle;    // the shared handle object
    LPVOID  m_pvData;      // the mapped data
    UINT64  m_cbMapBegin;  // the map offset
    UINT64  m_cbMapEnd;    // the number of bytes in the map

public:
    // ISequentialStream methods
    STDMETHOD(Read)(THIS_ __out_bcount(cb) void *pv, ULONG cb, __out_opt ULONG *pcbRead);

    // IStream methods
    STDMETHOD(Stat)(THIS_ __out STATSTG *pstatstg, DWORD grfStatFlag);
    STDMETHOD(Clone)(THIS_ __deref_out IStream **ppstm);

    // IDirectStream methods
    STDMETHOD(GetDirectData)(THIS_ size_t cbRequired, __deref_opt_out LPCVOID *ppvBuffer, __out_opt size_t *pcbBuffer);

public:
    static HRESULT Create(__in_z LPCWSTR pszFilename, DWORD dwMode, __deref_out CStreamFile **ppstm);

protected:
    virtual HRESULT GetSize(__out ULARGE_INTEGER *puliSize) const { return m_phandle->GetSize(puliSize); }
    virtual DWORD GetLockSupport() const { return 0; }
    HRESULT Map(size_t cbRequired, BOOL fExtend);
    void Unmap();

protected:
    CStreamFile(__in CHandle *phandle) : m_phandle(phandle), m_pvData(NULL), m_cbMapBegin(0), m_cbMapEnd(0) {}
    CStreamFile(const CStreamFile &obj) : m_phandle(obj.m_phandle), m_pvData(NULL), m_cbMapBegin(0), m_cbMapEnd(0) { m_phandle->AddRef(); }
    virtual ~CStreamFile() { Unmap(); m_phandle->Release(); }
};

/// <summary>
/// A read-only memory stream.
/// </summary>
class CStreamMemory : public CStream
{
protected:
    LPVOID m_pvData;  // the data buffer
    size_t m_cbData;  // the size of the buffer

public:
    // ISequentialStream methods
    STDMETHOD(Read)(THIS_ __out_bcount(cb) void *pv, ULONG cb, __out_opt ULONG *pcbRead);

    // IStream methods
    STDMETHOD(Stat)(THIS_ __out STATSTG *pstatstg, DWORD grfStatFlag);
    STDMETHOD(Clone)(THIS_ __deref_out IStream **ppstm);

    // IDirectStream methods
    STDMETHOD(GetDirectData)(THIS_ size_t cbRequired, __deref_opt_out LPCVOID *ppvBuffer, __out_opt size_t *pcbBuffer);

protected:
    virtual HRESULT GetSize(__out ULARGE_INTEGER *puliSize) const { puliSize->QuadPart = m_cbData; return S_OK; }
    virtual DWORD GetMode() const { return STGM_READ | STGM_SHARE_DENY_NONE; }

public:
    CStreamMemory(LPVOID pvData, size_t cbData) : m_pvData(pvData), m_cbData(cbData) {}
};

/// <summary>
/// A read-only DLL memory stream.
/// </summary>
class CStreamDllResource : public CStreamMemory
{
private:
    HINSTANCE m_hLibrary;  // the DLL handle

public:
    CStreamDllResource(HINSTANCE hInstance, LPVOID pvData, size_t cbData) : CStreamMemory(pvData, cbData), m_hLibrary(hInstance) {}
    virtual ~CStreamDllResource() { FreeLibrary(m_hLibrary); }
};

/// <summary>
/// A read/write memory stream.
/// </summary>
class CStreamMemoryWrite : public CStreamMemory
{
private:
    size_t m_cbAlloc;  // the allocated buffer size

public:
    // ISequentialStream methods
    STDMETHOD(Write)(THIS_ __in_bcount(cb) const void *pv, ULONG cb, __out_opt ULONG *pcbWritten);

    // IStream methods
    STDMETHOD(SetSize)(THIS_ ULARGE_INTEGER libNewSize);
    STDMETHOD(Commit)(THIS_ DWORD grfCommitFlags) { UNREFERENCED_PARAMETER(grfCommitFlags); return S_FALSE; }
    STDMETHOD(Clone)(THIS_ __deref_out IStream **ppstm);

protected:
    virtual DWORD GetMode() const { return STGM_READWRITE | STGM_SHARE_EXCLUSIVE; }

private:
    HRESULT Resize(size_t cbAlloc);
    void Clear();

public:
    CStreamMemoryWrite() : CStreamMemory(NULL, 0), m_cbAlloc(0) {}
    virtual ~CStreamMemoryWrite() { Clear(); }
};


// function declarations
extern HRESULT StgFromError(DWORD dwErr);

/// </topic_scope> StreamBufferAPIInternal
