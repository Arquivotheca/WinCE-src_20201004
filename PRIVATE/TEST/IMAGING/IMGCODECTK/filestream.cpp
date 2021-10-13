//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "FileStream.h"
#include "main.h"
#include "ImagingHelpers.h"

CBytesRead::CBytesRead(int iSize)
{
    m_pbBytesRead = new BYTE[(iSize + 7)/8];
    if (m_pbBytesRead)
    {
        m_iSize = iSize;
        memset (m_pbBytesRead, 0x00, (m_iSize + 7)/8);
    }
    else
    {
        m_iSize = 0;
    }
}

CBytesRead::~CBytesRead()
{
    if (m_pbBytesRead)
        delete[] m_pbBytesRead;
}

bool CBytesRead::SetBytesRead(int iStart, int iCount)
{
    int iByteOffsetStart = iStart >> 3;
    int iBitOffsetStart = iStart & 7;
    int iByteOffsetEnd = (iStart + iCount - 1) >> 3;
    int iBitOffsetEnd = (iStart + iCount - 1) & 7;
    bool fMultipleBytes;
    BYTE bStartBitMask = 0xff << iBitOffsetStart;
    BYTE bEndBitMask = 0xff >> (7 - iBitOffsetEnd);

    // We can't set anything if we couldn't allocate the buffer.
    if (!m_pbBytesRead)
    {
        return false;
    }

    // Make sure the values we're setting are within the range of the buffer
    if (iStart + iCount > m_iSize || iStart < 0 || iCount < 0)
    {
        return false;
    }

    fMultipleBytes = iByteOffsetStart < iByteOffsetEnd;
    
    // If the bytes read fit into one byte of the bit mask, the start and end
    // masks need to be combined.
    if (!fMultipleBytes)
    {
        bStartBitMask = bStartBitMask & bEndBitMask;
    }

    // Set the first byte of the map using the start mask
    m_pbBytesRead[iByteOffsetStart] |= bStartBitMask;

    // Set all the other bytes of the bitmask
    if (fMultipleBytes)
    {
        // If the bytes read only spans two bytes of the bitmask, this will 
        // skip directly to setting the endmask.
        for (int iIndex = iByteOffsetStart + 1; iIndex < iByteOffsetEnd; iIndex++)
        {
            m_pbBytesRead[iIndex] = 0xff;
        }
        m_pbBytesRead[iByteOffsetEnd] |= bEndBitMask;
    }
    return true;
}

bool CBytesRead::AreAnyBytesRead(int iStart, int iCount)
{
    int iByteOffsetStart = iStart >> 3;
    int iBitOffsetStart = iStart & 7;
    int iByteOffsetEnd = (iStart + iCount - 1) >> 3;
    int iBitOffsetEnd = (iStart + iCount - 1) & 7;
    bool fMultipleBytes;
    BYTE bStartBitMask = 0xff << iBitOffsetStart;
    BYTE bEndBitMask = 0xff >> (7 - iBitOffsetEnd);
    BYTE bBits = 0;

    // We can't set anything if we couldn't allocate the buffer.
    if (!m_pbBytesRead)
    {
        return false;
    }

    // Make sure the values we're setting are within the range of the buffer
    if (iStart + iCount > m_iSize || iStart < 0 || iCount < 0)
    {
        return false;
    }

    fMultipleBytes = iByteOffsetStart < iByteOffsetEnd;
    
    // If the bytes read fit into one byte of the bit mask, the start and end
    // masks need to be combined.
    if (!fMultipleBytes)
    {
        bStartBitMask = bStartBitMask & bEndBitMask;
    }

    // We're checking to see if any of the bits are set, so we only need
    // to use one byte.
    bBits |= m_pbBytesRead[iByteOffsetStart] & bStartBitMask;
    if (fMultipleBytes)
    {
        for (int iIndex = iByteOffsetStart + 1; iIndex < iByteOffsetEnd; iIndex++)
        {
            bBits |= m_pbBytesRead[iIndex];
        }
        bBits |= m_pbBytesRead[iByteOffsetEnd] & bEndBitMask;
    }
    return !!bBits;

}

// IUnknown interface implementation. Unlike the sample codec, the CFileStream
// object knows nothing of aggregation.
STDMETHODIMP CFileStream::QueryInterface (const IID & iid, void** ppv)
{
    if (IsBadWritePtr(ppv, sizeof(IStream*)))
        return E_POINTER;
   
    if (iid == IID_IUnknown)
    {
        *ppv = static_cast<IStream*> (this);
    }
    else if (iid == IID_ISequentialStream)
    {
        *ppv = static_cast<IStream*> (this);
    }
    else if (iid == IID_IStream)
    {
        *ppv = static_cast<IStream*> (this);
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    reinterpret_cast<IUnknown*>(*ppv)->AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) CFileStream::AddRef ()
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_ (ULONG) CFileStream::Release ()
{
    if (InterlockedDecrement(&m_cRef) == 0)
    {
        delete this;
        return 0;
    }
    return m_cRef;
}

//#define DEBUG_READ

// IStream implementation.
STDMETHODIMP CFileStream::Read ( 
    /* [length_is][size_is][out] */ void *pv,
    /* [in] */ ULONG cb,
    /* [out] */ ULONG *pcbRead)
{
    DWORD dwBytesRead = 0;
    bool fReplayChanged = false;
    bool fDirty = false;
    ULARGE_INTEGER uliCurrentLocation = {0};
    if (!m_hFile)
    {
        info(DETAIL, _FT("Called without being initialized"));
        return E_UNEXPECTED;
    }

#ifdef DEBUG_READ
    info(DETAIL, _FT("Reading: %d bytes"), cb);
#endif

    // Get the current location of the file being read from
    uliCurrentLocation.LowPart = SetFilePointer(m_hFile, 0, NULL, FILE_CURRENT);

    if (!ReadFile(m_hFile, pv, cb, &dwBytesRead, NULL))
    {
        HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
        info(DETAIL, _FT("ReadFile failed, hr = 0x%08x"), hr);
        return hr;
    }

    if (pcbRead)
        *pcbRead = dwBytesRead;

    if (m_fRandomize && 0 != dwBytesRead)
    {
        // Need to make sure we can replay a randomized file read without
        // having to go through all the test things as well. Is there a better
        // way to do this?
        if (m_iRandomSeed)
            srand(m_iRandomSeed);

        const DWORD c_dwCaseCount = 4;

        // How many changes are we going to make for this read?
        DWORD dwChangeCount = 0;

        // Choose the probability of byte change based on the size being read,
        // which assumes that smaller blocks contain more vital data, so change them more.

        // We don't want to always change the value read. The best way I can
        // see to do this is to determine a byte change probablility, like only
        // 1 out of 5000 bytes should be changed. The idea is to do this enough
        // times that the overall effect hits a lot of cases.
        if (dwBytesRead < 250 && !(rand() % 2))
        {
            dwChangeCount = ImagingHelpers::RandomGaussianInt(0, cb * 2, cb/10, 10);
        }
        else if (dwBytesRead >= 250 && dwBytesRead < 2000 && !(rand() % 2))
        {
            dwChangeCount = ImagingHelpers::RandomGaussianInt(0, cb * 2, cb/100, 100);
        }
        else if (dwBytesRead >= 2000 && !(rand() % 2))
        {
            dwChangeCount = ImagingHelpers::RandomGaussianInt(0, cb * 2, cb/1000, 1000);
        }

        for (DWORD dwChanges = 0; dwChanges < dwChangeCount; dwChanges++)
        {
            // Either adjust 1, 2, or 4 bytes at a time (byte, short, and long)
            int iDataType = 1 << (rand() % 3);
            if (dwBytesRead < iDataType)
                continue;
                
            int iOffset = ImagingHelpers::RandomInt(0, dwBytesRead - iDataType) & ~(iDataType - 1);
            
            if (m_pbrByteMap)
            {
                if (m_pbrByteMap->AreAnyBytesRead(iOffset, iDataType))
                    continue;
            }
            
            switch (rand() % c_dwCaseCount)
            {
                // Choose completely random values for a few of the bytes
                case 0:
                {
                    ImagingHelpers::GetRandomData(pv, iDataType);
                    break;
                }
                
                // Add or subtract 1 from a few of the bytes
                case 1:
                {
                    int iIncrement = rand() % 2;
                    if (!iIncrement)
                        iIncrement--;
                    if (1 == iDataType)
                    {
                        ((BYTE*)pv)[iOffset] += iIncrement;
                    }
                    if (2 == iDataType)
                    {
                        SHORT siTemp;
                        memcpy(&siTemp, ((PBYTE)pv) + iOffset, sizeof(SHORT));
                        siTemp += iIncrement;
                        memcpy(((PBYTE)pv) + iOffset, &siTemp, sizeof(SHORT));
                    }
                    if (4 == iDataType)
                    {
                        LONG liTemp;
                        memcpy(&liTemp, ((PBYTE)pv) + iOffset, sizeof(LONG));
                        liTemp += iIncrement;
                        memcpy(((PBYTE)pv) + iOffset, &liTemp, sizeof(LONG));
                    }
                    break;
                }

                // Set a few of the bytes to 0
                case 2:
                {
                    memset((BYTE*)pv + iOffset, 0x00, iDataType);
                    break;
                }
                
                // Set a few of the bytes to MAX
                case 3:
                {
                    memset((BYTE*)pv + iOffset, 0xFF, iDataType);
                    break;
                }
            }
            fDirty = true;
        }
        
        if (m_iRandomSeed)
            while(0 == (m_iRandomSeed = rand()));
    }

    // Make sure we can reproduce this by writing to the replay file if available.
    if (m_fReplay && fDirty)
    {
        DWORD dwReplayBytesWritten;
        SetFilePointer(m_hFile, uliCurrentLocation.LowPart, NULL, FILE_BEGIN);
        if (!WriteFile(m_hFile, pv, cb, &dwReplayBytesWritten, NULL)
            || dwReplayBytesWritten != cb)
        {
            info(DETAIL, _FT("Didn't write all randomized bytes to replay file, GLE %d"),
                GetLastError());
        }
        FlushFileBuffers(m_hFile);
    }

    if (m_pbrByteMap)
    {
        m_pbrByteMap->SetBytesRead(uliCurrentLocation.LowPart, cb);
    }
        
    return S_OK;
}
STDMETHODIMP CFileStream::Seek ( 
    /* [in] */ LARGE_INTEGER dlibMove,
    /* [in] */ DWORD dwOrigin,
    /* [out] */ ULARGE_INTEGER *plibNewPosition)
{
    DWORD dwMoveMethod;
    DWORD dwRet;
    HRESULT hr;
    if (!m_hFile)
    {
        info(DETAIL, _FT("Called without being initialized"));
        return E_UNEXPECTED;
    }

#ifdef DEBUG_READ
    info(DETAIL, _FT("Seeking: %d using seek %d"), dlibMove.LowPart, dwOrigin);
#endif

    // TODO: Consider removing this, since the STREAM_SEEK and FILE_ consts are the same.
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
        info(DETAIL, _FT("Called with invalid origin %d"), dwOrigin);
        return E_INVALIDARG;
    }

    dwRet = SetFilePointer(m_hFile, dlibMove.LowPart, NULL, dwMoveMethod);
    if (0xFFFFFFFF == dwRet && GetLastError() != NO_ERROR)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        info(DETAIL, _FT("SetFilePointer failed, hr = 0x%08x"), hr);
        return hr;
    }

    if (plibNewPosition)
    {
        plibNewPosition->LowPart = dwRet;
    }
    
    return S_OK;
}
STDMETHODIMP CFileStream::Stat (
    /* [out] */ STATSTG *pstatstg,
    /* [in] */ DWORD grfStatFlag)
{
    WCHAR* wszFilename = NULL; 
    if (IsBadWritePtr(pstatstg, sizeof(STATSTG)))
    {
        info(DETAIL, TEXT(__FUNCTION__) TEXT(": Unwriteable pointer"));
        return STG_E_INVALIDPOINTER;
    }
    if (grfStatFlag && grfStatFlag != STATFLAG_NONAME)
    {
        info(DETAIL, TEXT(__FUNCTION__) TEXT(": Bad statflag"));
        return STG_E_INVALIDFLAG;
    }

    memset(pstatstg, 0x00, sizeof(STATSTG));
    
    if (m_hFile &&
        !GetFileTime(m_hFile, 
                     &(pstatstg->ctime),
                     &(pstatstg->atime),
                     &(pstatstg->mtime)))
    {
        HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
        info(DETAIL, _FT("GetFileTime failed, hr = 0x%08x"), hr);
        return hr;
    }
    
    if (STATFLAG_NONAME != grfStatFlag)
    {
        wszFilename = (WCHAR*)CoTaskMemAlloc(countof(m_tszFilename) * sizeof(WCHAR));
        if (!wszFilename)
        {
            info(DETAIL, TEXT(__FUNCTION__) TEXT(": MemAlloc failed"));
            return STG_E_ACCESSDENIED;
        }
#ifdef UNICODE
        // Everything here is in bytes, hence no sizeof(TCHAR)
        memcpy(wszFilename, m_tszFilename, sizeof(m_tszFilename));
#else
        MultiByteToWideChar(CP_ACP, 0, m_tszFilename, -1, wszFilename, MAX_PATH+1);
        wszFilename[MAX_PATH] = 0;
#endif
    }

    pstatstg->pwcsName = wszFilename;
    pstatstg->type = STGTY_STREAM;
    pstatstg->cbSize.QuadPart = m_uliFileSize.QuadPart;
    pstatstg->grfMode = STGM_READ;
    pstatstg->clsid = CLSID_NULL;
    return S_OK;
}

HRESULT CFileStream::InitFile(const TCHAR * tszFilename, const TCHAR * tszReplayFile)
{
    HRESULT hr;
    if (m_hFile)
    {
        info(DETAIL, _FT("called when already inited"));
        return E_UNEXPECTED;
    }

    m_fReplay = false;
    
    if (tszReplayFile)
    {
        tstring tsPath = tszReplayFile;
        ImagingHelpers::RecursiveCreateDirectory(tsPath.substr(0, tsPath.rfind(TEXT("\\"))));
        if (!CopyFile(tszFilename, tszReplayFile, FALSE))
        {
            info(DETAIL, _FT("could not copy original file for replay file: \"%s\" to \"%s\", GLE %d"),
                tszFilename, tszReplayFile, GetLastError());
        }
        else
        {
            m_hFile = CreateFile(
                tszReplayFile,
                GENERIC_WRITE | GENERIC_READ,
                0,
                NULL,
                OPEN_EXISTING,
                FILE_FLAG_RANDOM_ACCESS,
                NULL);
            if (INVALID_HANDLE_VALUE == m_hFile)
            {
                info(DETAIL, _FT("could not open replay file \"%s\" for write, GLE = %d, continuing anyway"), 
                    tszReplayFile, GetLastError());
                m_hFile = NULL;
            }
            else
            {
                m_fReplay = true;
            }
        }
    }
    else
    {
        m_hFile = CreateFile(
            tszFilename, 
            GENERIC_READ, 
            FILE_SHARE_READ, 
            NULL, 
            OPEN_EXISTING, 
            FILE_FLAG_RANDOM_ACCESS, 
            NULL);
        if (INVALID_HANDLE_VALUE == m_hFile)
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            info(DETAIL, _FT("could not open \"%s\", hr = 0x%08x"), tszFilename, hr);
            return hr;
        }
    }

    // m_tszFilename is MAX_PATH + 1 characters long (leaving room for a null
    // terminator if the filename is actually MAX_PATH characters long.
    _tcsncpy(m_tszFilename, tszFilename, MAX_PATH);
    m_tszFilename[MAX_PATH] = 0;

    m_uliFileSize.LowPart = ::GetFileSize(m_hFile, &m_uliFileSize.HighPart);
    if (0xFFFFFFFF == m_uliFileSize.LowPart && GetLastError() != NO_ERROR)
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        info(DETAIL, _FT("GetFileSize failed, hr = 0x%08x"), hr);
        return hr;
    }

    if (m_fReplay)
    {
        m_pbrByteMap = new CBytesRead(m_uliFileSize.LowPart);
        if (!m_pbrByteMap || !m_pbrByteMap->IsInitialized())
        {
            // Since the replay file is a copy of the actual file, we can
            // continue to just use the replay file.
            info(DETAIL, _FT("Could not create bytes read map, not randomizing file read"));
            m_fRandomize = false;
            m_fReplay = false;
        }
    }

    return S_OK;
}

