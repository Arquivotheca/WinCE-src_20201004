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

#include "cachefilt.hpp"
#include <lockmgrhelp.h>


FSSharedFileMap_t::FSSharedFileMap_t (
    CachedVolume_t* pVolume,
    FilterHook_t*   pFilterHook,
    LPCWSTR         pFileName
    )
{
    DEBUGMSG (ZONE_FILEMAP, (L"CACHEFILT:FSSharedFileMap_t, pMap=0x%08x pVolume=0x%08x Name=%s \r\n",
                             this, pVolume, pFileName));
    
    m_pPrivList = NULL;   
    
    m_SharedMapFlags = 0;
    m_UseCount = 0;
    m_PageTreeId = 0;
    m_collectionGeneration = 0;

    m_pVolume = pVolume;
    m_UncachedHooks.pFilterHook = pFilterHook;
    m_UncachedHooks.FilePseudoHandle = INVALID_HANDLE_VALUE;
    
    FSDMGR_OpenFileLockState (&m_LockState);
    InitializeCriticalSection (&m_csFile);
    
    m_pFileName = NULL;
    const LPWSTR pCopy = DuplicateFileName (pFileName);
    if (pCopy) {
        SetName (pCopy);
    }

    m_WriteTime.dwLowDateTime = 0;
    m_WriteTime.dwHighDateTime = 0;

    m_CurrentCeLogGeneration = (DWORD)-1;
}

 
// Returns whether initialization succeeded.  The constructor actually does
// all the initialization right now.
BOOL
FSSharedFileMap_t::Init (
    HANDLE          hFile,
    BOOL            IsWritable
    )
{
    m_UncachedHooks.FilePseudoHandle = hFile;
    m_SharedMapFlags = IsWritable ? CACHE_SHAREDMAP_WRITABLE | CACHE_SHAREDMAP_HANDLE_WRITABLE : 0;

    if (!m_LockState.lpcs) {
        DEBUGMSG (ZONE_FILEMAP | ZONE_ERROR, (L"CACHEFILT:FSSharedFileMap_t::Init, !! Failed to initialize file lock state\r\n"));
        return FALSE;
    }
    if (!m_csFile.hCrit) {
        DEBUGMSG (ZONE_FILEMAP | ZONE_ERROR, (L"CACHEFILT:FSSharedFileMap_t::Init, !! Failed to initialize I/O CS\r\n"));
        return FALSE;
    }
    if (!m_pFileName) {
        DEBUGMSG (ZONE_FILEMAP | ZONE_ERROR, (L"CACHEFILT:FSSharedFileMap_t::Init, !! Failed to initialize file ID/name\r\n"));
        return FALSE;
    }
    if (!m_pVolume) {
        DEBUGMSG (ZONE_FILEMAP | ZONE_ERROR, (L"CACHEFILT:FSSharedFileMap_t::Init, !! Invalid volume\r\n"));
        return FALSE;
    }
    if (!m_UncachedHooks.pFilterHook) {
        DEBUGMSG (ZONE_FILEMAP | ZONE_ERROR, (L"CACHEFILT:FSSharedFileMap_t::Init, !! Invalid FilterHook\r\n"));
        return FALSE;
    }
    
    if (!m_UncachedHooks.pFilterHook->ReadFileWithSeek ((DWORD) m_UncachedHooks.FilePseudoHandle,
                                                       0, 0, 0, 0, 0, 0)) {
        DEBUGMSG (ZONE_FILEMAP | ZONE_ERROR, (L"CACHEFILT:FSSharedFileMap_t::Init, !! Underlying file system must be pagable.\r\n"));
        return FALSE;
    }

    return TRUE;
}


FSSharedFileMap_t::~FSSharedFileMap_t ()
{
    DEBUGMSG (ZONE_FILEMAP, (L"CACHEFILT:~FSSharedFileMap_t, pMap=0x%08x pVolume=0x%08x hFile=0x%08x Name=%s\r\n",
                             this, m_pVolume, m_UncachedHooks.FilePseudoHandle, m_pFileName ? m_pFileName : TEXT("")));
       
    // Clean up all kernel resources. If we didn't write-back unocommited data
    // at this point then we're not going to write it now. All uncommitted data
    // in views associated with this map will be lost. CachedVolume_t::CloseMap
    // should have flushed any data if necessary before destroying this object.
    CleanupCaching (FALSE);

    // All handles should already have been closed
    DEBUGCHK (!m_pPrivList);
    m_pPrivList = NULL;   
    
    // Should not be deleted until all references are gone
    DEBUGCHK (!m_UseCount);
    m_UseCount = s_MaxUseCount;  // Cannot increment any more
    
    FSDMGR_CloseFileLockState (&m_LockState, FSDMGR_EmptyLockContainer);
        
    // Free m_pFileName
    SetName (NULL);

    if (m_UncachedHooks.pFilterHook) {
        if (INVALID_HANDLE_VALUE != m_UncachedHooks.FilePseudoHandle) {
            UncachedCloseHandle();
        }
        m_UncachedHooks.FilePseudoHandle = INVALID_HANDLE_VALUE;
        m_UncachedHooks.pFilterHook = NULL;
    }
    
    m_pVolume = NULL;

    DeleteCriticalSection (&m_csFile);
}


// Returns FALSE if the use count is already maxed out
BOOL
FSSharedFileMap_t::IncUseCount ()
{
    BOOL result = FALSE;
    
    m_pVolume->AcquireVolumeLock ();

    DEBUGCHK (m_UseCount < s_MaxUseCount);  // Probably a leak
    if (m_UseCount < s_MaxUseCount) {
        m_UseCount++;
        result = TRUE;
    } else {
        DEBUGMSG (ZONE_ERROR, (L"CACHEFILT:FSSharedFileMap_t::IncUseCount, pMap=0x%08x use count already maxed out\r\n", this));
        DEBUGCHK (0);  // Probably a leak
        SetLastError (ERROR_TOO_MANY_OPEN_FILES);
    }
    m_pVolume->ReleaseVolumeLock ();

    return result;
}


// Returns TRUE if the use count is 0. (mostly for debug purposes)
BOOL
FSSharedFileMap_t::DecUseCount ()
{
    BOOL fDelete = FALSE;
    DWORD NewUseCount = 0;

    m_pVolume->AcquireVolumeLock ();

    DEBUGCHK (m_UseCount > 0);
    if (m_UseCount > 0) {
        m_UseCount--;
    }

    fDelete = ((0 == m_UseCount) && IsInvalid());
    NewUseCount = m_UseCount;
    
    m_pVolume->ReleaseVolumeLock ();

    if (fDelete) {
        // Removed the last reference from the file, destroy this object
        ASSERT (!AreHandlesInMap());    
        delete this;
    }

    return (NewUseCount == 0);
}

BOOL
FSSharedFileMap_t::CloseMap(__in FSSharedFileMap_t *pMap, const BOOL performFlush)
{
    ASSERT(pMap != NULL);

    BOOL result = TRUE;
    
    if (performFlush) 
    {
        result = pMap->FlushMap();
    }
   
    // This call will destroy this map object once the use count goes to zero.
    pMap->SetFlags(CACHE_SHAREDMAP_INVALID);
    pMap->DecUseCount();

    return result;
}

BOOL
FSSharedFileMap_t::CloseFileHandle (
    PrivateFileHandle_t* pHandle
    )
{
    BOOL result = TRUE;    

    if (!m_pVolume->CommitTransactionsOnIdle()) {
        // Flush buffered data even if there are still other handles open
        result = FlushMap();
        if (result) {
            result = CommitTransactions();
        }
    } else {
        // Update the write file time in the underlying file system if it
        // is cached.
        AcquireMapLock();
        if (IsWriteTimeCached()) {
            SetFileTime (NULL, NULL, &m_WriteTime);
        }    
        ReleaseMapLock();
    }
    
    // The volume owns the flow of cleanup.
    // This will call back into RemoveHandle eventually.
    if (!m_pVolume->CloseFile (this, pHandle)) {  // May delete "this"
        result = FALSE;
    }
    
    return result;
}


BOOL
FSSharedFileMap_t::AddHandle (
    PrivateFileHandle_t* pHandle
    )
{
    DEBUGCHK (OwnMapLock());
    DEBUGMSG (ZONE_FILEMAP, (L"CACHEFILT:FSSharedFileMap_t::AddHandle, pMap=0x%08x pHandle=0x%08x\r\n",
                             this, pHandle));
   
    if (!m_pPrivList) 
    {
        m_pVolume->DecrementUnreferencedMapCount();
        SetFlags (CACHE_SHAREDMAP_HANDLE_OPEN);
    }

    pHandle->m_pNext = m_pPrivList;
    m_pPrivList = pHandle;

    return TRUE;
}


// Removes a PrivateFileHandle_t from the FileMap's list of handles.
// Returns TRUE if this was the last open handle for the FileMap.
BOOL
FSSharedFileMap_t::RemoveHandle (
    PrivateFileHandle_t* pHandle
    )
{
    DEBUGCHK (OwnMapLock());
    
    BOOL Found = FALSE;
    if (m_pPrivList) {
        if (pHandle == m_pPrivList) {
            m_pPrivList = pHandle->m_pNext;
            Found = TRUE;
        } else {
            PrivateFileHandle_t* pCurHandle = m_pPrivList;
            while (pCurHandle->m_pNext) {
                if (pHandle == pCurHandle->m_pNext) {
                    pCurHandle->m_pNext = pHandle->m_pNext;
                    Found = TRUE;
                    break;
                }
                pCurHandle = pCurHandle->m_pNext;
            }
        }
    }
    pHandle->m_pNext = NULL;
    
    if (Found) {
        DEBUGMSG (ZONE_FILEMAP, (L"CACHEFILT:FSSharedFileMap_t::RemoveHandle, pMap=0x%08x pHandle=0x%08x\r\n",
                                 this, pHandle));
        if (!m_pPrivList) 
        {
            m_pVolume->IncrementUnreferencedMapCount();

            DEBUGMSG (ZONE_FILEMAP, (L"CACHEFILT:FSSharedFileMap_t::RemoveHandle, m_pPrivList==NULL, clearing CACHE_SHAREDMAP_HANDLE_OPEN flag"));
            ClearFlags (CACHE_SHAREDMAP_HANDLE_OPEN);
        }
    } else {
        // This is just a warning because there are CreateFile failure cases that
        // delete the handle before it's added to the map list
        DEBUGMSG (ZONE_WARNING,
                  (L"CACHEFILT:FSSharedFileMap_t::RemoveHandle, Not found, pMap=0x%08x pHandle=0x%08x\r\n",
                   this, pHandle));
    }
    return !m_pPrivList;
}


// Check if a handle open request conflicts with any existing handles.
// Returns TRUE if the result is okay and FALSE if there's a conflict.
BOOL
FSSharedFileMap_t::CheckHandleSharing (
    DWORD NewSharing,
    DWORD NewAccess,
    DWORD CreationDisposition
    )
{
    DEBUGCHK (OwnMapLock());

    // Asking to create or truncate is effectively asking for write access
    if ((CREATE_ALWAYS == CreationDisposition)
        || (TRUNCATE_EXISTING == CreationDisposition)) {
        NewAccess |= GENERIC_WRITE;
    }

    for (PrivateFileHandle_t* pCurHandle = m_pPrivList;
         pCurHandle;
         pCurHandle = pCurHandle->m_pNext) {
        
        // Check for sharing violation with open handles to the file
        // (1)  Handle already open with exclusive access.
        // (2)  Handle already open with privileges we want to exclude.
        if ((!(FILE_SHARE_READ & pCurHandle->GetSharing ()) && (GENERIC_READ & NewAccess))
            || (!(FILE_SHARE_WRITE & pCurHandle->GetSharing ()) && (GENERIC_WRITE & NewAccess))
            || (!(FILE_SHARE_READ & NewSharing) && (GENERIC_READ & pCurHandle->GetAccess ()))
            || (!(FILE_SHARE_WRITE & NewSharing) && (GENERIC_WRITE & pCurHandle->GetAccess ()))) {
            DEBUGMSG (ZONE_WARNING, (L"CACHEFILT:FSSharedFileMap_t::CheckHandleSharing, !! Sharing violation with pHandle=0x%08x %s\r\n",
                                     pCurHandle, m_pFileName ? m_pFileName : TEXT("")));
            SetLastError (ERROR_SHARING_VIOLATION);
            return FALSE;
        }
    }
    return TRUE;
}


// Check if a move/DeleteAndRenameFile request conflicts with any existing handles.
// Returns TRUE if the result is okay and FALSE if there's a conflict.
BOOL
FSSharedFileMap_t::CheckHandleMoveSharing ()
{
    DEBUGCHK (OwnMapLock());

    for (PrivateFileHandle_t* pCurHandle = m_pPrivList;
         pCurHandle;
         pCurHandle = pCurHandle->m_pNext) {
        
        // Consistent with object store and FATFS: All open handles
        // must have write sharing, or read-only + read sharing, for
        // the file to be moved.
        if (!(FILE_SHARE_WRITE & pCurHandle->GetSharing ())
            && ((GENERIC_WRITE & pCurHandle->GetAccess ())
                || !(FILE_SHARE_READ & pCurHandle->GetSharing ()))) {
            DEBUGMSG (ZONE_WARNING, (L"CACHEFILT:FSSharedFileMap_t::CheckHandleMoveSharing, !! Sharing violation with pHandle=0x%08x %s\r\n",
                                     pCurHandle, m_pFileName ? m_pFileName : TEXT("")));
            SetLastError (ERROR_SHARING_VIOLATION);
            return FALSE;
        }
    }
    return TRUE;
}


// DOES NOT copy the name -- takes the pointer directly
void
FSSharedFileMap_t::SetName (
    LPCWSTR pNewName
    )
{
    // The volume CS protects the name, unless we're in the constructor/destructor
    DEBUGCHK (m_pVolume->OwnVolumeLock () || !m_pFileName || !pNewName);

    if (m_pFileName) 
    {      
        DEBUGMSG (ZONE_ALLOC, (L"CACHEFILT:FSSharedFileMap_t::SetName, free string 0x%08x\r\n", m_pFileName));
        delete [] m_pFileName;
    }
    m_pFileName = pNewName;
    
    DEBUGMSG (ZONE_FILEMAP, (L"CACHEFILT:FSSharedFileMap_t::SetName, pMap=0x%08x Name=%s\r\n",
                             this, m_pFileName ? m_pFileName : TEXT("")));

}

LPWSTR FSSharedFileMap_t::CreateNameForDirectoryUpdate(
    __in_z LPCWSTR pSourceName, 
    __in_z LPCWSTR pDestName)
{
    DEBUGCHK (m_pVolume->OwnVolumeLock ());

    BOOL result = FALSE;
    LPWSTR pNewFileName = NULL;
    size_t SourceLength = 0;
    size_t CurrentNameLength = 0;
    VERIFY(SUCCEEDED(StringCchLength(pSourceName, MAX_PATH, &SourceLength)));
    VERIFY(SUCCEEDED(StringCchLength(m_pFileName, MAX_PATH, &CurrentNameLength)));

    // If the source name is a prefix of the map's full path, then
    // update the path with the destination directory
    if ((CurrentNameLength > SourceLength) &&
        (_wcsnicmp(m_pFileName, pSourceName, SourceLength) == 0) &&
        (m_pFileName[SourceLength] == L'\\')) {
        
        size_t DestLength = 0;
        VERIFY(SUCCEEDED(StringCchLength(pDestName, MAX_PATH, &DestLength)));

        DWORD NewPathLength = CurrentNameLength - SourceLength + DestLength;
        pNewFileName = new WCHAR[NewPathLength + 1];
        if (!pNewFileName) {
            goto exit;
        } 
        if (FAILED(StringCchCopy(pNewFileName, NewPathLength + 1, pDestName))) {
            goto exit;
        }
        if (FAILED(StringCchCat(pNewFileName, NewPathLength + 1, m_pFileName + SourceLength))) {
            goto exit;
        }

        result = TRUE;
    }

exit:
    if (!result && pNewFileName) {
        delete[] pNewFileName;
        pNewFileName = NULL;
    }
    return pNewFileName;
}

void
FSSharedFileMap_t::SetUnderlyingFileHandle(
    HANDLE hNewHandle
    )
{
    m_UncachedHooks.FilePseudoHandle = hNewHandle;
}

BOOL 
FSSharedFileMap_t::SetWritable (
    BOOL IsWritable
    ) 
{
    BOOL result = TRUE;
    DEBUGCHK (OwnMapLock());
    
    if (IsWritable) {            
        // If the handle is not opened for GENERIC_WRITE, then re-open
        // the handle.
        if (!(m_SharedMapFlags & CACHE_SHAREDMAP_HANDLE_WRITABLE)) {

            BOOL IsReadOnly = FALSE;
            HANDLE hFile = m_pVolume->OpenUnderlyingFile(OPEN_EXISTING, m_pFileName, NULL, 0, NULL, &IsReadOnly);

            if (hFile != INVALID_HANDLE_VALUE) {
                UncachedCloseHandle();
                SetUnderlyingFileHandle(hFile);
            } else {
                result = FALSE;
            }

            if (!IsReadOnly) {
                SetFlags (CACHE_SHAREDMAP_HANDLE_WRITABLE);
            } else {
                result = FALSE;
            }            
        }

        if (result) {
            SetFlags (CACHE_SHAREDMAP_WRITABLE);
        }
    } else {
        ClearFlags (CACHE_SHAREDMAP_WRITABLE);
    }

    return result;
}    
 
BOOL
FSSharedFileMap_t::AcquireFileLockState (
    PFILELOCKSTATE* ppFileLockState,
    ULARGE_INTEGER  FilePosition,
    DWORD           AccessFlags
    )
{
    if (m_LockState.lpcs) {
        // acquire file lock state; leave in ReleaseFileLockState
        EnterCriticalSection (m_LockState.lpcs);

        // fetch file position 
        m_LockState.dwPosLow = FilePosition.LowPart;
        m_LockState.dwPosHigh = FilePosition.HighPart;

        // fetch create access 
        m_LockState.dwAccess = AccessFlags;

        // fetch file lock container 
        *ppFileLockState = &m_LockState;

        return TRUE;
    }
    
    DEBUGMSG (ZONE_ERROR, (L"CACHEFILT:FSSharedFileMap_t::AcquireFileLockState, !! Invalid handle\r\n"));
    DEBUGCHK (0);
    return FALSE;
}


BOOL
FSSharedFileMap_t::ReleaseFileLockState (
    PFILELOCKSTATE* ppFileLockState
    )
{
    if (m_LockState.lpcs) {
        // release file lock state; acquired in AcquireFileLockState
        LeaveCriticalSection (m_LockState.lpcs);
        return TRUE;
    }
    
    DEBUGMSG (ZONE_ERROR, (L"CACHEFILT:FSSharedFileMap_t::ReleaseFileLockState, !! Invalid handle\r\n"));
    DEBUGCHK (0);
    return FALSE;
}

