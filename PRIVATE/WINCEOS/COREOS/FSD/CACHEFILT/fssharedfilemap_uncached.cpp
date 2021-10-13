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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//

#include "cachefilt.hpp"
#include <celog.h>


void
FSSharedFileMap_t::UncachedCloseHandle ()
{
    DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.CloseFile, hFile=0x%08x\r\n",
                        m_UncachedHooks.FilePseudoHandle));
    
    m_pVolume->AcquireUnderlyingIOLock ();

    BOOL result = m_UncachedHooks.pFilterHook->CloseFile (
        (DWORD) m_UncachedHooks.FilePseudoHandle);
    m_UncachedHooks.FilePseudoHandle = INVALID_HANDLE_VALUE;
    
    m_pVolume->ReleaseUnderlyingIOLock ();
    
    DEBUGMSG (ZONE_ERROR && !result,
              (L"CACHEFILT:FSSharedFileMap_t::UncachedCloseHandle, !! Failed, error=%u\r\n", GetLastError ()));
}


// ReadFileScatter / WriteFileGather
BOOL
FSSharedFileMap_t::UncachedReadWriteScatterGather (
    FILE_SEGMENT_ELEMENT aSegmentArray[], 
    DWORD cbToAccess, 
    ULARGE_INTEGER* pOffsetArray,   // FILE_SEGMENT_ELEMENT array
    BOOL  IsWrite
    )
{
    BOOL result = FALSE;
    
    m_pVolume->AcquireUnderlyingIOLock ();
    
    if (IsWrite) {
        DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.WriteFileGather, hFile=0x%08x\r\n",
                            m_UncachedHooks.FilePseudoHandle));
        result = m_UncachedHooks.pFilterHook->WriteFileGather (
            (DWORD) m_UncachedHooks.FilePseudoHandle,
            aSegmentArray, cbToAccess, (LPDWORD) pOffsetArray,
            NULL);
    } else {
        DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.ReadFileScatter, hFile=0x%08x\r\n",
                            m_UncachedHooks.FilePseudoHandle));
        result = m_UncachedHooks.pFilterHook->ReadFileScatter (
            (DWORD) m_UncachedHooks.FilePseudoHandle,
            aSegmentArray, cbToAccess, (LPDWORD) pOffsetArray,
            NULL);
    }
    
    m_pVolume->ReleaseUnderlyingIOLock ();
    
    return result;
}


// NOTENOTE called for ReadFile/WriteFile and ReadFileWithSeek/WriteFileWithSeek
BOOL
FSSharedFileMap_t::UncachedReadWriteWithSeek (
    PVOID  pBuffer, 
    DWORD  cbToAccess,
    PDWORD pcbAccessed, 
    DWORD  dwLowOffset,
    DWORD  dwHighOffset,
    BOOL   IsWrite
    )
{
    BOOL result = FALSE;

    m_pVolume->AcquireUnderlyingIOLock ();

    // Even CeLog can be too intrusive if someone is doing many small writes
    // to an uncached handle, so only record data for cached files
    if (ZONE_CELOG && IsCeLogZoneEnabled (CELZONE_DEMANDPAGE) && m_NKSharedFileMapId) {
        if (IsWrite) {
            CeLogMsg (L"WriteFile[WithSeek], hFile=0x%08x Pos=0x%08x,%08x Bytes=0x%08x\r\n",
                      m_UncachedHooks.FilePseudoHandle, dwHighOffset, dwLowOffset, cbToAccess);
        } else {
            CeLogMsg (L"ReadFile[WithSeek], hFile=0x%08x Pos=0x%08x,%08x Bytes=0x%08x\r\n",
                      m_UncachedHooks.FilePseudoHandle, dwHighOffset, dwLowOffset, cbToAccess);
        }
    }
    
    if (m_SharedMapFlags & CACHE_SHAREDMAP_PAGEABLE) {
        // ReadFileWithSeek / WriteFileWithSeek
        if (IsWrite) {
            DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.WriteFileWithSeek, hFile=0x%08x Pos=0x%08x,%08x Bytes=0x%08x\r\n",
                                m_UncachedHooks.FilePseudoHandle,
                                dwHighOffset, dwLowOffset, cbToAccess));
            result = m_UncachedHooks.pFilterHook->WriteFileWithSeek (
                (DWORD) m_UncachedHooks.FilePseudoHandle, pBuffer,
                cbToAccess, pcbAccessed, NULL, dwLowOffset, dwHighOffset);
        } else {
            DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.ReadFileWithSeek, hFile=0x%08x Pos=0x%08x,%08x Bytes=0x%08x\r\n",
                                m_UncachedHooks.FilePseudoHandle,
                                dwHighOffset, dwLowOffset, cbToAccess));
            result = m_UncachedHooks.pFilterHook->ReadFileWithSeek (
                (DWORD) m_UncachedHooks.FilePseudoHandle, pBuffer,
                cbToAccess, pcbAccessed, NULL, dwLowOffset, dwHighOffset);
        }
    
    } else {
        // ReadFile / WriteFile
        DEBUGCHK (m_pVolume->OwnUnderlyingIOLock ());  // Protects file pointer
        ULARGE_INTEGER Offset;
        Offset.HighPart = dwHighOffset;
        Offset.LowPart = dwLowOffset;
        if (UncachedSetFilePointer (Offset)) {
            if (IsWrite) {
                DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.WriteFile, hFile=0x%08x Pos=0x%08x,%08x Bytes=0x%08x\r\n",
                                    m_UncachedHooks.FilePseudoHandle,
                                    dwHighOffset, dwLowOffset, cbToAccess));
                result = m_UncachedHooks.pFilterHook->WriteFile (
                    (DWORD) m_UncachedHooks.FilePseudoHandle,
                    pBuffer, cbToAccess, pcbAccessed, NULL);
            } else {
                DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.ReadFile, hFile=0x%08x Pos=0x%08x,%08x Bytes=0x%08x\r\n",
                                    m_UncachedHooks.FilePseudoHandle,
                                    dwHighOffset, dwLowOffset, cbToAccess));
                result = m_UncachedHooks.pFilterHook->ReadFile (
                    (DWORD) m_UncachedHooks.FilePseudoHandle,
                    pBuffer, cbToAccess, pcbAccessed, NULL);
            }
        }
    }

    m_pVolume->ReleaseUnderlyingIOLock ();

    return result;
}


BOOL
FSSharedFileMap_t::UncachedSetEndOfFile (
    ULARGE_INTEGER Offset
    )
{
    BOOL result = FALSE;
    
    m_pVolume->AcquireUnderlyingIOLock ();  // Protects file pointer

    // SetFilePointer always modifies lasterr, but SetEndOfFile should
    // only modify it on failure.  Preserve lasterr for the success case.
    DWORD SuccessError = GetLastError ();
    
    if (UncachedSetFilePointer (Offset)) {
        DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.SetEndOfFile, hFile=0x%08x\r\n",
                            m_UncachedHooks.FilePseudoHandle));
        if (m_UncachedHooks.pFilterHook->SetEndOfFile ((DWORD) m_UncachedHooks.FilePseudoHandle)) {
            // Restore the previous lasterr
            SetLastError (SuccessError);
            result = TRUE;
        }
    }

    m_pVolume->ReleaseUnderlyingIOLock ();

    return result;
}


BOOL
FSSharedFileMap_t::UncachedFlushFileBuffers()
{
    m_pVolume->AcquireUnderlyingIOLock ();
    
    DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.FlushFileBuffers, hFile=0x%08x\r\n",
                        m_UncachedHooks.FilePseudoHandle));
    BOOL result =  m_UncachedHooks.pFilterHook->FlushFileBuffers (
        (DWORD) m_UncachedHooks.FilePseudoHandle);
    
    m_pVolume->ReleaseUnderlyingIOLock ();

    return result;
}
        

BOOL
FSSharedFileMap_t::UncachedDeviceIoControl (
    DWORD  dwIoControlCode, 
    PVOID  pInBuf, 
    DWORD  nInBufSize, 
    PVOID  pOutBuf, 
    DWORD  nOutBufSize,
    PDWORD pBytesReturned
    )
{
    m_pVolume->AcquireUnderlyingIOLock ();

    DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.DeviceIoControl, hFile=0x%08x\r\n",
                        m_UncachedHooks.FilePseudoHandle));
    BOOL result = m_UncachedHooks.pFilterHook->DeviceIoControl (
        (DWORD) m_UncachedHooks.FilePseudoHandle,
        dwIoControlCode, pInBuf, nInBufSize, pOutBuf, nOutBufSize,
        pBytesReturned, NULL);

    m_pVolume->ReleaseUnderlyingIOLock ();

    return result;
}


// Private helper function
BOOL
FSSharedFileMap_t::UncachedSetFilePointer (
    ULARGE_INTEGER liFileOffset
    )
{
    BOOL result;

    DEBUGCHK (m_pVolume->OwnUnderlyingIOLock ());  // Protects file pointer

    // SetFilePointer uses signed numbers, while file offsets are unsigned.  Avoid
    // negative seeks by cutting down to positive numbers and moving up to 3 times.
    BOOL fFirst = TRUE;
    do {
        LONG lMoveHigh, lMoveLow;

        if (liFileOffset.HighPart & 0x80000000) {
            lMoveHigh = 0x7FFFFFFF;
        } else {
            lMoveHigh = liFileOffset.HighPart;
        }
        liFileOffset.HighPart -= lMoveHigh;
        
        if (liFileOffset.LowPart & 0x80000000) {
            lMoveLow = 0x7FFFFFFF;
        } else {
            lMoveLow = liFileOffset.LowPart;
        }
        liFileOffset.LowPart -= lMoveLow;

        // On the first seek, start at the beginning; else use current position.
        result = FALSE;
        DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.SetFilePointer, hFile=0x%08x\r\n",
                            m_UncachedHooks.FilePseudoHandle));
        DWORD NewPos = m_UncachedHooks.pFilterHook->SetFilePointer (
            (DWORD) m_UncachedHooks.FilePseudoHandle,
            lMoveLow, &lMoveHigh, fFirst ? FILE_BEGIN : FILE_CURRENT);
        // Have to do extra checking to see if the call failed, in case the file
        // size is 4GB-1
        if (((DWORD) -1 != NewPos) || (NO_ERROR == GetLastError ())) {
            result = TRUE;
        }
        if (!result) {
            break;
        }
        fFirst = FALSE;

    } while (liFileOffset.QuadPart);

    return result;
}


BOOL
FSSharedFileMap_t::UncachedGetFileSize (
    ULARGE_INTEGER* pFileSize
    )
{
    BOOL result = FALSE;

    m_pVolume->AcquireUnderlyingIOLock ();

    DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.GetFileSize, hFile=0x%08x\r\n",
                        m_UncachedHooks.FilePseudoHandle));
    pFileSize->LowPart = m_UncachedHooks.pFilterHook->GetFileSize (
        (DWORD) m_UncachedHooks.FilePseudoHandle, &pFileSize->HighPart);
    
    m_pVolume->ReleaseUnderlyingIOLock ();

    if (((DWORD)-1 == pFileSize->LowPart)
        && ((DWORD)-1 == pFileSize->HighPart)
        && (NO_ERROR != GetLastError ())) {
        DEBUGMSG (ZONE_ERROR,
                  (L"CACHEFILT:FSSharedFileMap_t::UncachedGetFileSize, !! Failed, error=%u\r\n", GetLastError ()));
    } else {
        result = TRUE;
    }
    
    return result;
}


BOOL
FSSharedFileMap_t::UncachedGetFileInformationByHandle (
    PBY_HANDLE_FILE_INFORMATION pFileInfo
    )
{
    m_pVolume->AcquireUnderlyingIOLock ();

    DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.GetFileInformationByHandle, hFile=0x%08x\r\n",
                        m_UncachedHooks.FilePseudoHandle));
    BOOL result = m_UncachedHooks.pFilterHook->GetFileInformationByHandle (
        (DWORD) m_UncachedHooks.FilePseudoHandle, pFileInfo);
    
    m_pVolume->ReleaseUnderlyingIOLock ();
    
    DEBUGMSG (ZONE_ERROR && !result,
              (L"CACHEFILT:FSSharedFileMap_t::UncachedGetFileInformationByHandle, !! Failed, error=%u\r\n", GetLastError ()));
    return result;
}


BOOL
FSSharedFileMap_t::UncachedGetFileTime (
    FILETIME* pCreation,
    FILETIME* pLastAccess, 
    FILETIME* pLastWrite
    )
{
    m_pVolume->AcquireUnderlyingIOLock ();

    DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.GetFileTime, hFile=0x%08x\r\n",
                        m_UncachedHooks.FilePseudoHandle));
    BOOL result = m_UncachedHooks.pFilterHook->GetFileTime (
        (DWORD) m_UncachedHooks.FilePseudoHandle,
        pCreation, pLastAccess, pLastWrite);
    
    m_pVolume->ReleaseUnderlyingIOLock ();
    
    DEBUGMSG (ZONE_ERROR && !result,
              (L"CACHEFILT:GetFileTime, !! Failed, error=%u\r\n", GetLastError ()));
    return result;
}


BOOL
FSSharedFileMap_t::UncachedSetFileTime (
    const FILETIME* pCreation, 
    const FILETIME* pLastAccess, 
    const FILETIME* pLastWrite
    )
{
    m_pVolume->AcquireUnderlyingIOLock ();
    
    DEBUGMSG (ZONE_IO, (L"CACHEFILT:FilterHook.SetFileTime, hFile=0x%08x\r\n",
                        m_UncachedHooks.FilePseudoHandle));
    BOOL result = m_UncachedHooks.pFilterHook->SetFileTime (
        (DWORD) m_UncachedHooks.FilePseudoHandle,
        pCreation, pLastAccess, pLastWrite);
    
    m_pVolume->ReleaseUnderlyingIOLock ();

    return result;
}
