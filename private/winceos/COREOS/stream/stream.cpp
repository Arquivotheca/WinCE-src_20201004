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
#include "stream.hpp"
#include <new>
#include <intsafe.h>
#include <guts.h>
#include <ehm.h>

// ehm supplements
inline HRESULT HRFromWin32Err(DWORD dwErr) { return (NOERROR == dwErr ? E_FAIL : MAKE_HRESULT(1,FACILITY_WIN32,dwErr)); }
#define CBRGLEEx(e, dwFail)      CBREx(e, HRFromWin32Err(dwFail))
#define CWRGLEEx(e, dwFail)      CWREx(e, HRFromWin32Err(dwFail))
#define CBRGLE(e)                CBRGLEEx(e, GetLastError())
#define CWRGLE(e)                CWRGLEEx(e, GetLastError())

#define ReleaseInterfaceNoNULL(p) if ((p)) {(p)->Release();}

// standard API implementation, see IStream documentation
HRESULT CStream::QueryInterface(REFIID riid, __deref_out LPVOID *ppvObj)
{
    HRESULT  hr = S_OK;
    IUnknown *punk = NULL;

    // Initialize output
    CBREx(NULL != ppvObj, E_INVALIDARG);
    *ppvObj = NULL;

    // Check for standard interfaces
    if (IsEqualIID(riid, IID_IStream) ||
        IsEqualIID(riid, IID_ISequentialStream) ||
        IsEqualIID(riid, IID_IUnknown))
    {
        punk = static_cast<IStream*>(this);
    }

    // Check for the direct data interface
    if (IsEqualIID(riid, IID_IDirectStream))
    {
        punk = static_cast<IDirectStream*>(this);
    }

    // Did we find an interface?
    CBREx(NULL != punk, E_NOINTERFACE);
    punk->AddRef();
    *ppvObj = punk;

Error:
    return hr;
}

// standard API implementation, see IStream documentation
HRESULT CStream::Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin,
                      __out_opt ULARGE_INTEGER *plibNewPosition)
{
    HRESULT        hr = S_OK;
    ULARGE_INTEGER uliBase;

    switch (dwOrigin)
    {
    case STREAM_SEEK_SET:
        // Set the pointer
        m_cbSeek = (UINT64)dlibMove.QuadPart;
        break;

    case STREAM_SEEK_CUR:
    case STREAM_SEEK_END:
        // Get the base offset
        if (STREAM_SEEK_END == dwOrigin)
        {
            hr = GetSize(&uliBase);
            CHR(hr);
        }
        else
        {
            uliBase.QuadPart = m_cbSeek;
        }

        // Range check
        if (dlibMove.QuadPart < 0)
        {
            // Check that the offset would not make the pointer negative
            CBREx((UINT64)-dlibMove.QuadPart <= uliBase.QuadPart,
                STG_E_INVALIDFUNCTION);
        }
        else
        {
            // Check for integer overflow
            CBREx((UINT64)-1 - uliBase.QuadPart >= (UINT64)dlibMove.QuadPart,
                STG_E_INVALIDFUNCTION);
        }

        // Set the pointer
        m_cbSeek = uliBase.QuadPart + dlibMove.QuadPart;
        break;

    default:
        CHR(E_INVALIDARG);
        break;
    }

Error:
    if (NULL != plibNewPosition)
    {
        plibNewPosition->QuadPart = m_cbSeek;
    }

    return hr;
}

// standard API implementation, see IStream documentation
HRESULT CStream::SetSize(ULARGE_INTEGER libNewSize)
{
    HRESULT        hr = S_OK;
    ULARGE_INTEGER cbSize;

    // Get the current size
    hr = GetSize(&cbSize);
    CHR(hr);

    // We only succeed if the file size is not changing
    hr = (libNewSize.QuadPart == cbSize.QuadPart ? S_FALSE : STG_E_INVALIDFUNCTION);

Error:
    return hr;
}

// standard API implementation, see IStream documentation
HRESULT CStream::CopyTo(__in IStream *pstm, ULARGE_INTEGER cb,
                        __out_opt ULARGE_INTEGER *pcbRead,
                        __out_opt ULARGE_INTEGER *pcbWritten)
{
    HRESULT hr = S_OK;
    UINT64  cbCopied = 0;

    // Check inputs
    CBREx(NULL != pstm || 0 == cb.QuadPart, E_INVALIDARG);

    // Copy the data
    while (0 != cb.QuadPart)
    {
        LPCVOID pvBuffer;
        size_t  cbBuffer;
        ULONG   cbWritten = 0;

        // Get the data buffer
        hr = GetDirectData(1, &pvBuffer, &cbBuffer);
        CHR(hr);

        // Copy as much data as we have
        cbBuffer = MinVal<size_t>((ULONG)-1, (size_t)MinVal<ULONGLONG>(cb.QuadPart, cbBuffer));
        hr = pstm->Write(pvBuffer, (ULONG)cbBuffer, &cbWritten);
        cb.QuadPart -= cbWritten;
        m_cbSeek -= cbBuffer - cbWritten;
        cbCopied += cbWritten;

        // Make sure we succeeded and copied some data
        CHR(hr);
        CBREx(0 != cbWritten, STG_E_ABNORMALAPIEXIT);
    }

    // Success
    hr = S_OK;

Error:
    if (NULL != pcbRead)
    {
        pcbRead->QuadPart = cbCopied;
    }

    if (NULL != pcbWritten)
    {
        pcbWritten->QuadPart = cbCopied;
    }

    return hr;
}

/// <summary>
/// Creates a new file stream object.
/// </summary>
/// <param name="pszFilename">The file to open.</param>
/// <param name="dwMode">Combination of STGM flags.</param>
/// <param name="ppstm">
/// Address of a variable that receives a referenced pointer to the newly
/// created object if successful, or NULL in all other cases.
/// </param>
/// <returns>Returns S_OK if successful, or an error code if not.</returns>
HRESULT CStreamFile::Create(__in_z LPCWSTR pszFilename, DWORD dwMode,
                            __deref_out CStreamFile **ppstm)
{
    HRESULT        hr = S_OK;
    HANDLE         hFile = INVALID_HANDLE_VALUE;
    ULARGE_INTEGER uliSize;
    CHandle        *phandle = NULL;

    // Create the file handle
    hFile = CreateFileW(pszFilename, GENERIC_READ,
        FILE_SHARE_READ | ((STGM_SHARE_DENY_WRITE & dwMode) ? 0 : FILE_SHARE_WRITE), NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    CBREx(INVALID_HANDLE_VALUE != hFile, StgFromError(GetLastError()));

    // Should we get the file size?
    if (0 != (STGM_SHARE_DENY_WRITE & dwMode))
    {
        // Get the current size of the file
        uliSize.LowPart = GetFileSize(hFile, &uliSize.HighPart);
        CHR((DWORD)-1 == uliSize.LowPart ? HRESULT_FROM_WIN32(GetLastError()) : S_OK);
    }
    else
    {
        // Mark the file size as "volatile"
        uliSize.QuadPart = (UINT64)-1;
    }

    // Create the shared object
    phandle = CHandle::Create(pszFilename, wcslen(pszFilename));
    CPR(phandle);

    // Initialize the object
    phandle->m_dwMode = dwMode;
    phandle->m_hFile = hFile;
    phandle->m_cbSize = uliSize.QuadPart;
    hFile = INVALID_HANDLE_VALUE;

    // Create the stream object
    *ppstm = new CStreamFile(phandle);
    CPR(*ppstm);
    phandle = NULL;

Error:
    if (INVALID_HANDLE_VALUE != hFile)
    {
        CloseHandle(hFile);
    }

    ReleaseInterfaceNoNULL(phandle);
    return hr;
}

// standard API implementation, see IStream documentation
HRESULT CStreamFile::Read(__out_bcount(cb) void *pv, ULONG cb,
                          __out_opt ULONG *pcbRead)
{
    HRESULT    hr = S_OK;
    BYTE       *pbWrite = static_cast<BYTE*>(pv);
    const BYTE *pbEnd = pbWrite + cb;

    // Check inputs
    CBREx(NULL != pv || 0 == cb, E_INVALIDARG);

    while (pbWrite != pbEnd)
    {
        ULONG cbCopy;

        // Map at least one byte
        hr = Map(1, FALSE);
        CBREx(E_NOMOREDATA != hr, (pbWrite == pv ? S_FALSE : S_OK));
        CBREx(SUCCEEDED(hr), (pbWrite == pv ? hr : S_OK));

        // Copy as much data as we have
        cbCopy = (ULONG)MinVal<size_t>(pbEnd - pbWrite, (size_t)(m_cbMapEnd - m_cbSeek));
        memcpy(pbWrite, static_cast<BYTE*>(m_pvData) + (m_cbSeek - m_cbMapBegin), cbCopy);

        // Update state
        pbWrite += cbCopy;
        m_cbSeek += cbCopy;
    }

Error:
    if (NULL != pcbRead)
    {
        *pcbRead = (ULONG)(pbWrite - static_cast<BYTE*>(pv));
    }

    return hr;
}

// standard API implementation, see IStream documentation
HRESULT CStreamFile::Stat(__out STATSTG *pstatstg, DWORD grfStatFlag)
{
    HRESULT hr = S_OK;
    LPWSTR  pszName = NULL;

    // Initialize output
    CBREx(NULL != pstatstg, E_INVALIDARG);
    ZeroMemory(pstatstg, sizeof(STATSTG));

    // Do we need to return the filename?
    CBREx(0 == (~STATFLAG_NONAME & grfStatFlag), E_INVALIDARG);
    if (0 == (STATFLAG_NONAME & grfStatFlag))
    {
        // Get the stream name
        size_t  cchFilename;
        LPCWSTR pszFilename = m_phandle->GetName(&cchFilename);

        // Allocate a buffer (we trust the length, so we can't overflow)
        pszName = (WCHAR*)CoTaskMemAlloc(sizeof(WCHAR) * (cchFilename + 1));
        CPR(pszName);

        // Copy the string
        memcpy(pszName, pszFilename, sizeof(WCHAR) * cchFilename);
        pszName[cchFilename] = '\0';
    }

    // Get the file information
    BY_HANDLE_FILE_INFORMATION bhfi;
    CBRGLE(GetFileInformationByHandle(m_phandle->m_hFile, &bhfi));

    // Get the file size
    hr = GetSize(&pstatstg->cbSize);
    CHR(hr);

    // Fill in the data
    pstatstg->pwcsName = pszName;
    pstatstg->type = STGTY_STREAM;
    pstatstg->mtime = bhfi.ftLastWriteTime;
    pstatstg->ctime = bhfi.ftCreationTime;
    pstatstg->atime = bhfi.ftLastAccessTime;
    pstatstg->grfMode = m_phandle->m_dwMode;
    pstatstg->grfLocksSupported = GetLockSupport();

    // Success
    pszName = NULL;
    hr = S_OK;

Error:
    if (NULL != pszName)
    {
        CoTaskMemFree(pszName);
    }

    return hr;
}

// standard API implementation, see IStream documentation
HRESULT CStreamFile::Clone(__deref_out IStream **ppstm)
{
    HRESULT hr = S_OK;

    // Initialize output
    CBREx(NULL != ppstm, E_INVALIDARG);
    *ppstm = NULL;

    // Create the new object
    *ppstm = new CStreamFile(*this);
    CPR(*ppstm);

Error:
    return hr;
}

// standard API implementation, see IDirectStream documentation
HRESULT CStreamFile::GetDirectData(size_t cbRequired,
                                   __deref_opt_out LPCVOID *ppvBuffer,
                                   __out_opt size_t *pcbBuffer)
{
    HRESULT hr = S_OK;

    // Is this a zero-byte request?
    if (0 == cbRequired)
    {
        // Return S_OK if there's any buffer at all, S_FALSE if not
        BOOL fAvail = (m_cbMapBegin <= m_cbSeek && m_cbSeek < m_cbMapEnd);
        hr = (fAvail ? S_OK : S_FALSE);
    }
    else
    {
        // Map the request
        hr = Map(cbRequired, FALSE);
    }

//Error:
    if (NULL != ppvBuffer)
    {
        *ppvBuffer = (S_OK == hr ? static_cast<BYTE*>(m_pvData) + (m_cbSeek - m_cbMapBegin) : NULL);
    }

    if (NULL != pcbBuffer)
    {
        *pcbBuffer = (S_OK == hr ? (size_t)(m_cbMapEnd - m_cbSeek) : 0);
        if (NULL != ppvBuffer)
        {
            m_cbSeek += *pcbBuffer;
        }
    }

    return hr;
}

/// <summary>
/// Ensures that file data is available at the seek pointer.
/// </summary>
/// <param name="cbRequired">
/// The minimum amount of data required, in bytes. If the required number of
/// bytes is not currently available in the buffer, the method will attempt to
/// load it. This parameter must be non-zero.
/// </param>
/// <param name="fExtend">
/// TRUE to extend the file if the requested data is beyond the current end of
/// the file, FALSE to fail in that circumstance.
/// </param>
/// <returns>
/// Returns S_OK if there are at least <paramref name="cbRequired"/> bytes
/// available at the seek position. Returns E_NOMOREDATA if there is not
/// enough data in the stream to satisfy the request. The method may also
/// return other error codes, such as E_FAIL, in less common circumstances.
/// </returns>
HRESULT CStreamFile::Map(size_t cbRequired, BOOL fExtend)
{
    HRESULT hr = S_OK;
    UINT64  cbExtended;
    HANDLE  hMap = NULL;

    // Calculate the end point
    hr = ULongLongAdd(m_cbSeek, cbRequired, &cbExtended);
    CHREx(hr, fExtend ? STG_E_MEDIUMFULL : E_NOMOREDATA);

    // Do we need to adjust the current map?
    if (m_cbSeek < m_cbMapBegin || cbExtended > m_cbMapEnd)
    {
        ULARGE_INTEGER uliSize;
        BOOL           fUpdateSize = FALSE;
        ULARGE_INTEGER uliOffset;
        ULARGE_INTEGER uliLength;
        DWORD_PTR      cbLength;

        static DWORD s_cbAllocGranularity = 0;

        // Clear the current map
        Unmap();

        // Get the current file size
        hr = GetSize(&uliSize);
        CHR(hr);

        if(0 == s_cbAllocGranularity)
        {

            // Get the allocation granularity
            SYSTEM_INFO si;
            GetSystemInfo(&si);
            s_cbAllocGranularity = si.dwAllocationGranularity;
        }

        // Get the map offset
        UINT64 uMask = ~(UINT64)(s_cbAllocGranularity - 1);
        uliOffset.QuadPart = m_cbSeek & uMask;
        if (fExtend && cbExtended - uliOffset.QuadPart > s_cbAllocGranularity)
        {
            // If we're extending the file, just map to the next allocation
            // boundary. This allows us to write even large amounts of data in
            // chunks, rather than allocating a ton of virtual memory at once.
            cbExtended = uliOffset.QuadPart + s_cbAllocGranularity;
        }

        // Do we need to resize the file?
        if (cbExtended > uliSize.QuadPart)
        {
            CBREx(fExtend, E_NOMOREDATA);
            uliSize.QuadPart = cbExtended;
            fUpdateSize = ((UINT64)-1 != m_phandle->m_cbSize);
        }

        // Calculate the map length
        uliLength.QuadPart = MinVal(uliSize.QuadPart - uliOffset.QuadPart, ((cbExtended + (s_cbAllocGranularity - 1)) & uMask) - uliOffset.QuadPart);
        CBREx(0 != uliLength.QuadPart, E_OUTOFMEMORY);  // integer overflow

        // Add the length to the offset
        CBREx((DWORD_PTR)-1 >= uliLength.QuadPart, E_OUTOFMEMORY);
        cbLength = (DWORD_PTR)uliLength.QuadPart;
        uliLength.QuadPart += uliOffset.QuadPart;  // it's ok if this overflows to zero

        // Create the mapping object
        hMap = CreateFileMapping(m_phandle->m_hFile, NULL,
            SEC_COMMIT | ((STGM_READWRITE & m_phandle->m_dwMode) ? PAGE_READWRITE : PAGE_READONLY),
            uliLength.HighPart, uliLength.LowPart, NULL);
        CBREx(NULL != hMap, StgFromError(GetLastError()));

        // Do we need to update the size?
        if (fUpdateSize)
        {
            // CreateFileMapping has changed the file size
            m_phandle->m_cbSize = uliSize.QuadPart;
        }

        // Map the file view
        m_pvData = MapViewOfFile(hMap,
            (STGM_READWRITE & m_phandle->m_dwMode) ? FILE_MAP_WRITE : FILE_MAP_READ,
            uliOffset.HighPart, uliOffset.LowPart, cbLength);
        CBREx(NULL != m_pvData, StgFromError(GetLastError()));

        // Save state
        m_cbMapEnd = (m_cbMapBegin = uliOffset.QuadPart) + cbLength;
    }

Error:
    if (NULL != hMap)
    {
        CloseHandle(hMap);
    }

    return hr;
}

/// <summary>
/// Closes the file map, and frees associated resources.
/// </summary>
void CStreamFile::Unmap()
{
    if (NULL != m_pvData)
    {
        // Unmap the file view
        VBR(UnmapViewOfFile(m_pvData));
        m_pvData = NULL;

        // Reset state
        m_cbMapBegin = 0;
        m_cbMapEnd = 0;
    }
}

CStreamFile::CHandle::~CHandle()
{
    CloseHandle(m_hFile);
    if (0 != (STGM_DELETEONRELEASE & m_dwMode))
    {
        // Delete the file if necessary
        VWR(DeleteFile(GetName(NULL)));
    }
}

/// <summary>
/// Creates a handle object.
/// </summary>
/// <param name="pszName">The object name.</param>
/// <param name="cchName">The length of the object name.</param>
/// <returns>
/// Returns a pointer to the newly created object if successful, or NULL if
/// there's not enough memory.
/// </returns>
CStreamFile::CHandle *CStreamFile::CHandle::Create(__in_ecount(cchName) LPCWSTR pszName, size_t cchName)
{
    HRESULT hr = S_OK;
    size_t  cbAlloc;
    LPVOID  pvBuffer;
    CHandle *phandle = NULL;
    LPWSTR  pszBuffer;

    // Allocate the object buffer
    cbAlloc = sizeof(CHandle) + sizeof(size_t) + sizeof(WCHAR) * (cchName + 1);
    pvBuffer = LocalAlloc(LMEM_FIXED, cbAlloc);
    CPR(pvBuffer);

    // Initialize the data
    phandle = new(pvBuffer) CHandle;
    pszBuffer = const_cast<LPWSTR>(phandle->GetName(NULL));
    ((size_t*)pszBuffer)[-1] = cchName;
    memcpy(pszBuffer, pszName, sizeof(WCHAR) * cchName);
    pszBuffer[cchName] = '\0';

Error:
    return phandle;
}

HRESULT CStreamFile::CHandle::GetSize(__out ULARGE_INTEGER *puliSize) const
{
    HRESULT hr = S_OK;

    // Is the size "volatile"?
    if ((UINT64)-1 == m_cbSize)
    {
        puliSize->LowPart = GetFileSize(m_hFile, &puliSize->HighPart);
        if ((DWORD)-1 == puliSize->LowPart)
        {
            DWORD dwErr = GetLastError();
            if (NO_ERROR != dwErr)
            {
                puliSize->QuadPart = 0;
                CHR(HRESULT_FROM_WIN32(dwErr));
            }
        }
    }
    else
    {
        // We already know the size
        puliSize->QuadPart = m_cbSize;
    }

Error:
    return hr;
}

// standard API implementation, see IStream documentation
HRESULT CStreamMemory::Read(__out_bcount(cb) void *pv, ULONG cb,
                            __out_opt ULONG *pcbRead)
{
    HRESULT hr = S_OK;
    ULONG   cbCopy = 0;

    if (0 != cb)
    {
        // Check inputs
        CBREx(NULL != pv, E_INVALIDARG);

        // Do we have any data?
        size_t cbAvail = m_cbData - (size_t)MinVal<UINT64>(m_cbData, m_cbSeek);
        CBREx(0 != cbAvail, S_FALSE);

        // Copy as much data as we have
        cbCopy = (ULONG)MinVal<size_t>(cb, cbAvail);
        memcpy(pv, static_cast<const BYTE*>(m_pvData) + m_cbSeek, cbCopy);
        m_cbSeek += cbCopy;
    }

Error:
    if (NULL != pcbRead)
    {
        *pcbRead = cbCopy;
    }

    return hr;
}

// standard API implementation, see IStream documentation
HRESULT CStreamMemory::Stat(__out STATSTG *pstatstg, DWORD grfStatFlag)
{
    HRESULT hr = S_OK;
    LPWSTR  pszName = NULL;

    // Initialize output
    CBREx(NULL != pstatstg, E_INVALIDARG);
    ZeroMemory(pstatstg, sizeof(STATSTG));

    // Do we need to return the stream name?
    CBREx(0 == (~STATFLAG_NONAME & grfStatFlag), E_INVALIDARG);
    if (0 == (STATFLAG_NONAME & grfStatFlag))
    {
        // We have no name; allocate an empty string
        pszName = (WCHAR*)CoTaskMemAlloc(sizeof(WCHAR));
        CPR(pszName);
        pszName[0] = '\0';
    }

    // Fill in the data
    pstatstg->pwcsName = pszName;
    pstatstg->type = STGTY_STREAM;
    VHR(GetSize(&pstatstg->cbSize));
    pstatstg->grfMode = GetMode();

Error:
    return hr;
}

// standard API implementation, see IStream documentation
HRESULT CStreamMemory::Clone(__deref_out IStream **ppstm)
{
    HRESULT hr = S_OK;

    // Check inputs
    CBREx(NULL != ppstm, E_INVALIDARG);
    *ppstm = new CStreamMemory(*this);
    CPR(*ppstm);

Error:
    return hr;
}

// standard API implementation, see IDirectStream documentation
HRESULT CStreamMemory::GetDirectData(size_t cbRequired,
                                     __deref_opt_out LPCVOID *ppvBuffer,
                                     __out_opt size_t *pcbBuffer)
{
    HRESULT hr = S_OK;
    size_t  cbAvail;

    // Calculate the amount of available data
    cbAvail = m_cbData - (size_t)MinVal<UINT64>(m_cbData, m_cbSeek);
    CBREx(cbAvail >= cbRequired, E_NOMOREDATA);
    CBREx(0 != cbAvail, S_FALSE);

Error:
    if (NULL != ppvBuffer)
    {
        *ppvBuffer = (S_OK == hr ? static_cast<const BYTE*>(m_pvData) + m_cbSeek : NULL);
    }

    if (NULL != pcbBuffer)
    {
        *pcbBuffer = (S_OK == hr ? cbAvail : 0);
        if (NULL != ppvBuffer)
        {
            m_cbSeek += *pcbBuffer;
        }
    }

    return hr;
}

// standard API implementation, see ISequentialStream documentation
HRESULT CStreamMemoryWrite::Write(__in_bcount(cb) const void *pv, ULONG cb, __out_opt ULONG *pcbWritten)
{
    HRESULT hr = S_OK;
    ULONG   cbCopy = 0;

    if (0 != cb)
    {
        // Check inputs
        CBREx(NULL != pv, E_INVALIDARG);

        // Do we need to extend the buffer?
        UINT64 cbData;
        CHREx(ULongLongAdd(m_cbSeek, cb, &cbData), STG_E_MEDIUMFULL);
        if (cbData > m_cbAlloc)
        {
            // Allocate the new buffer
            CBREx(cbData <= (size_t)-1, STG_E_MEDIUMFULL);
            hr = Resize((size_t)cbData);
            CHR(hr);
        }

        // Copy the memory
        memcpy(static_cast<BYTE*>(m_pvData) + m_cbSeek, pv, cb);
        m_cbSeek = cbData;
        cbCopy = cb;
        ASSERT(m_cbSeek < UINT_MAX);
        if (m_cbSeek > m_cbData)
        {
            m_cbData = (UINT)m_cbSeek;
        }
    }

Error:
    if (NULL != pcbWritten)
    {
        *pcbWritten = cbCopy;
    }

    return hr;
}

// standard API implementation, see IStream documentation
HRESULT CStreamMemoryWrite::SetSize(ULARGE_INTEGER libNewSize)
{
    HRESULT hr = S_OK;

    // Check inputs
    CBREx(libNewSize.QuadPart != m_cbData, S_FALSE);

    // Resize the buffer
    CBREx(libNewSize.QuadPart <= (size_t)-1, STG_E_MEDIUMFULL);
    hr = Resize((size_t)libNewSize.QuadPart);
    CHR(hr);

Error:
    return hr;
}

// standard API implementation, see IStream documentation
HRESULT CStreamMemoryWrite::Clone(__deref_out IStream **ppstm)
{
    HRESULT hr = S_OK;

    // Initialize output
    CBREx(NULL != ppstm, E_INVALIDARG);
    *ppstm = NULL;

    // TODO: In order to implement cloning for writable memory blocks, we'd
    // need to create a critical-section-protected, reference-counted buffer
    // object similar to CStreamFile::CHandle.
    CHR(E_NOTIMPL);

Error:
    return hr;
}

/// <summary>
/// Resizes the buffer.
/// </summary>
/// <param name="cbCapacity">The new capacity, in bytes.</param>
/// <returns>Returns TRUE if successful, FALSE if out of memory.</returns>
HRESULT CStreamMemoryWrite::Resize(size_t cbCapacity)
{
    HRESULT hr = S_OK;
    static DWORD s_cbPageSize = 0;

    // Free if possible
    if (0 == cbCapacity)
    {
        Clear();
        goto Error;
    }

    if(0 == s_cbPageSize)
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        s_cbPageSize = si.dwPageSize;
    }
    
    // Adjust the allocation size
    size_t uMask = ~(size_t)(s_cbPageSize - 1);
    size_t cbAlloc = (cbCapacity + (s_cbPageSize - 1)) & uMask;
    CBREx(0 != cbAlloc, STG_E_MEDIUMFULL);  // overflow

    if (NULL == m_pvData)
    {
        // Allocate a new buffer
        m_pvData = LocalAlloc(LMEM_FIXED, cbAlloc);
        CPR(m_pvData);
    }
    else if (cbAlloc != m_cbAlloc)
    {
        // Reallocate the buffer
        LPVOID pvData = LocalReAlloc(m_pvData, cbAlloc, LMEM_MOVEABLE);
        CPR(pvData);

        // Save the pointer
        m_pvData = pvData;
    }

    // Update state
    m_cbData = cbCapacity;
    m_cbAlloc = cbAlloc;

Error:
    return hr;
}

/// <summary>
/// Frees the memory buffer.
/// </summary>
void CStreamMemoryWrite::Clear()
{
    if (NULL != m_pvData)
    {
        m_pvData = LocalFree(m_pvData);
        VBR(NULL == m_pvData);
        m_cbAlloc = 0;
        m_cbData = 0;
    }
}

/// <summary>
/// Converts a Win32 error code into a storage-type HRESULT.
/// </summary>
/// <param name="dwErr">A Win32 error code.</param>
/// <returns>Returns an appropriate HRESULT error code.</returns>
extern HRESULT StgFromError(DWORD dwErr)
{
    HRESULT hr = S_OK;

    switch (dwErr)
    {
    case ERROR_DISK_FULL:
        hr = STG_E_MEDIUMFULL;
        break;

    case ERROR_LOCK_VIOLATION:
    case ERROR_NOT_LOCKED:
    case ERROR_LOCK_FAILED:
    case ERROR_CANCEL_VIOLATION:
        hr = STG_E_LOCKVIOLATION;
        break;

    case ERROR_ACCESS_DENIED:
        hr = STG_E_ACCESSDENIED;
        break;

    case ERROR_SHARING_VIOLATION:
        hr = STG_E_SHAREVIOLATION;
        break;

    case ERROR_FILE_NOT_FOUND:
    case ERROR_MOD_NOT_FOUND:
    case ERROR_RESOURCE_NAME_NOT_FOUND:
    case ERROR_RESOURCE_LANG_NOT_FOUND:
        hr = STG_E_FILENOTFOUND;
        break;

    case ERROR_PATH_NOT_FOUND:
    case ERROR_BAD_NET_NAME:
    case ERROR_INVALID_NAME:
    case ERROR_BAD_PATHNAME:
    case ERROR_RESOURCE_DATA_NOT_FOUND:
    case ERROR_RESOURCE_TYPE_NOT_FOUND:
        hr = STG_E_PATHNOTFOUND;
        break;

    default:
        hr = HRFromWin32Err(dwErr);
        break;
    }

//Error:
    return hr;
}
