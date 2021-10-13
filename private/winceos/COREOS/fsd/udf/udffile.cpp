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
#include "Udf.h"

// /////////////////////////////////////////////////////////////////////////////
// GetHashValue
//
inline DWORD GetHashValue( CFile* pFile, DWORD dwHashSize )
{
    //
    // TODO::Is there something better to do here?
    //
    return (DWORD)(pFile->GetUniqueId() % dwHashSize);
}

// -----------------------------------------------------------------------------
// CHandleManager
// -----------------------------------------------------------------------------

// /////////////////////////////////////////////////////////////////////////////
// Constructor
//
CHandleManager::CHandleManager()
: m_pVolume( NULL ),
  m_pHandleMap( NULL ),
  m_dwNumHandles( 0 ),
  m_dwHashSize( DEFAULT_HASH_SIZE )
{
    InitializeCriticalSection( &m_cs );
}

// /////////////////////////////////////////////////////////////////////////////
// Destructor
//
CHandleManager::~CHandleManager()
{
    Lock();

    if( m_pHandleMap )
    {
        for( DWORD x = 0; x < m_dwHashSize; x++ )
        {
            PHANDLE_MAP pMap = &m_pHandleMap[x];
            PHANDLE_LIST pList = pMap->pHandles;
            PHANDLE_LIST pTmpList = NULL;

            //
            // Delete all the handles attached to the first file in this
            // list.
            //
            while( pList )
            {
                pTmpList = pList;
                pList = pList->pNext;

                if( pTmpList->pHandle )
                {
                    delete pTmpList->pHandle;
                }

                delete pTmpList;
            }

            //
            // Go through the rest of the files in this list.  Delete all
            // handles attached to each file, then delete the map for the
            // file.
            //
            pMap = pMap->pNextMap;
            while( pMap )
            {
                PHANDLE_MAP pTmpMap = pMap;

                pList = pMap->pHandles;
                pTmpList = NULL;

                while( pList )
                {
                    pTmpList = pList;
                    pList = pList->pNext;

                    if( pTmpList->pHandle )
                    {
                        delete pTmpList->pHandle;
                    }

                    delete pTmpList;
                }

                pMap = pMap->pNextMap;

                delete pTmpMap;
            }
        }

        //
        // Delete our hash table.  All contents should be destroyed.
        //
        delete [] m_pHandleMap;
        m_pHandleMap = NULL;
    }

    m_dwNumHandles = 0;

    Unlock();
    DeleteCriticalSection( &m_cs );
}

// /////////////////////////////////////////////////////////////////////////////
// Initialize
//
BOOL CHandleManager::Initialize( CVolume* pVolume )
{
    ASSERT( pVolume != NULL );

    m_pVolume = pVolume;

    //
    // TODO::Check to see if there's a registry setting for the hash size.
    //

    m_pHandleMap = new (UDF_TAG_HANDLE_MAP) HANDLE_MAP[m_dwHashSize];
    if( m_pHandleMap == NULL )
    {
        return FALSE;
    }

    ZeroMemory( m_pHandleMap, sizeof(HANDLE_MAP) * m_dwHashSize );

    return TRUE;
}

// /////////////////////////////////////////////////////////////////////////////
// AddFileHandle
//
// Rules for adding handles:
// 1. If adding a new map and allocation fails for the handle list, then remove
//    the new map entirely.
//
// Syncronization: Lock the handle manager.
//
LRESULT CHandleManager::AddHandle( PFILE_HANDLE pFileHandle )
{
    LRESULT lResult = ERROR_SUCCESS;
    PHANDLE_MAP pMap = NULL;
    PHANDLE_LIST pList = NULL;
    PFILE_HANDLE pExistingHandle = NULL;
    DWORD dwHash = 0;
    CFile* pFile = pFileHandle->pStream->GetFile();

    ASSERT( pFileHandle != NULL );

    dwHash = GetHashValue( pFile, m_dwHashSize );
    ASSERT( dwHash < m_dwHashSize );

    Lock();

    //
    // If the hash table is empty, then it's a simple matter of allocation
    // and adding the new data.
    //
    if( !m_pHandleMap )
    {
        m_pHandleMap = new (UDF_TAG_HANDLE_MAP) HANDLE_MAP[m_dwHashSize];
        if( !m_pHandleMap )
        {
            lResult = ERROR_NOT_ENOUGH_MEMORY;
            goto exit_addhandle;
        }

        ZeroMemory( m_pHandleMap, sizeof(HANDLE_MAP) * m_dwHashSize );

        pMap = &m_pHandleMap[dwHash];

        pMap->pHandles = new (UDF_TAG_HANDLE_LIST) HANDLE_LIST;
        if( !pMap->pHandles )
        {
            lResult = ERROR_NOT_ENOUGH_MEMORY;
            goto exit_addhandle;
        }

        pMap->pNextMap = NULL;
        pMap->pFile = pFile;

        pMap->pHandles->pHandle = pFileHandle;
        pMap->pHandles->pNext = NULL;

        goto exit_addhandle;
    }

    pMap = &m_pHandleMap[dwHash];

    //
    // If the map exists, and we don't have a collision, then just add the
    // handle.
    //
    if( pMap->pFile == NULL )
    {
        pMap->pHandles = new (UDF_TAG_HANDLE_LIST) HANDLE_LIST;
        if( !pMap->pHandles )
        {
            lResult = ERROR_NOT_ENOUGH_MEMORY;
            goto exit_addhandle;
        }

        pMap->pFile = pFile;
        pMap->pNextMap = NULL;
        pMap->pHandles->pNext = NULL;
        pMap->pHandles->pHandle = pFileHandle;

        goto exit_addhandle;
    }

    //
    // We need to check if the file we're referencing has already been
    // added.
    //
    while( pMap->pFile != pFile && pMap->pNextMap != NULL )
    {
        pMap = pMap->pNextMap;
    }

    //
    // If we had a collision, but haven't already added this file, then just
    // add it to the list, no other checks required.
    //
    if( pMap->pFile != pFile )
    {
        pMap->pNextMap = new (UDF_TAG_HANDLE_MAP) HANDLE_MAP;
        if( !pMap->pNextMap )
        {
            lResult = ERROR_NOT_ENOUGH_MEMORY;
            goto exit_addhandle;
        }

        pMap->pNextMap->pHandles = new (UDF_TAG_HANDLE_LIST) HANDLE_LIST;
        if( !pMap->pNextMap->pHandles )
        {
            delete pMap->pNextMap;
            pMap->pNextMap = NULL;

            lResult = ERROR_NOT_ENOUGH_MEMORY;
            goto exit_addhandle;
        }

        pMap = pMap->pNextMap;

        pMap->pFile = pFile;
        pMap->pNextMap = NULL;

        pMap->pHandles->pNext = NULL;
        pMap->pHandles->pHandle = pFileHandle;

        goto exit_addhandle;
    }

    //
    // If we've reached this point then we need to actually check the access
    // rights requested for the file.
    //
    ASSERT( pMap->pHandles != NULL );
    ASSERT( pMap->pHandles->pHandle != NULL );

    pList = pMap->pHandles;
    pExistingHandle = pList->pHandle;

    do
    {
        //
        // If we want read access, there has to be FILE_SHARE_READ sharing.
        //
        if( pFileHandle->dwAccess & GENERIC_READ )
        {
            if( !(pExistingHandle->dwShareMode & FILE_SHARE_READ) )
            {
                lResult = ERROR_SHARING_VIOLATION;
                goto exit_addhandle;
            }
        }

        //
        // If we want write access, there has to be FILE_SHARE_WRITE sharing.
        //
        if( pFileHandle->dwAccess & GENERIC_WRITE )
        {
            if( !(pExistingHandle->dwShareMode & FILE_SHARE_WRITE) )
            {
                lResult = ERROR_SHARING_VIOLATION;
                goto exit_addhandle;
            }
        }

        //
        // Now we need to check that the sharing granted by the new handle is
        // compatible with the access already granted to existing handles.
        //
        if( pExistingHandle->dwAccess & GENERIC_READ )
        {
            if( !(pFileHandle->dwShareMode & FILE_SHARE_READ) )
            {
                lResult = ERROR_SHARING_VIOLATION;
                goto exit_addhandle;
            }
        }

        if( pExistingHandle->dwAccess & GENERIC_WRITE )
        {
            if( !(pFileHandle->dwShareMode & FILE_SHARE_WRITE) )
            {
                lResult = ERROR_SHARING_VIOLATION;
                goto exit_addhandle;
            }
        }

        pExistingHandle = NULL;

        //
        // This handle checks out.
        //
        if( pList->pNext != NULL )
        {
            pList = pList->pNext;
            pExistingHandle = pList->pHandle;

            ASSERT( pExistingHandle != NULL );
        }

    } while( pExistingHandle );

    //
    // The sharing and access writes seem to be ok.
    //
    pList->pNext = new (UDF_TAG_HANDLE_LIST) HANDLE_LIST;
    if( !pList->pNext )
    {
        lResult = ERROR_NOT_ENOUGH_MEMORY;
        goto exit_addhandle;
    }

    pList = pList->pNext;
    pList->pHandle = pFileHandle;
    pList->pNext = NULL;

exit_addhandle:
    Unlock();

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// RemoveHandle
//
// Syncronization::Lock the handle manager to prevent compares to this handle
// as it's going away.
//
LRESULT CHandleManager::RemoveHandle( PFILE_HANDLE pFileHandle )
{
    LRESULT lResult = S_OK;
    PHANDLE_MAP pMap = NULL;
    PHANDLE_MAP pPrevMap = NULL;
    PHANDLE_LIST pList = NULL;
    PHANDLE_LIST pPrevList = NULL;
    PFILE_HANDLE pExistingHandle = NULL;
    DWORD dwHash = 0;
    CFile* pFile = NULL;

    ASSERT( pFileHandle != NULL );
    ASSERT( pFileHandle->pStream != NULL );

    __try
    {
        pFile = pFileHandle->pStream->GetFile();
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        //
        // If the handle was already closed, then the stream object may have
        // been deleted and when we try to reference the file, it will AV.
        //
        lResult = ERROR_INVALID_HANDLE;
        goto exit_removehandle;
    }

    dwHash = GetHashValue( pFile, m_dwHashSize );
    ASSERT( dwHash < m_dwHashSize );

    Lock();

    pMap = &m_pHandleMap[dwHash];

    //
    // If the file is not in the handle manager we have a problem.  This could
    // just be the user trying to close the file twice, so it's not necessarily
    // an internal error.
    //
    if( pMap->pFile == NULL )
    {
        DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!CHandleManager::RemoveHandle - The file handle is not found in the handle list.\n")));
        lResult = ERROR_INVALID_HANDLE;
        goto exit_removehandle;
    }

    //
    // We need to check if the file we're referencing has already been
    // added.
    //
    while( pMap->pFile != pFile && pMap->pNextMap != NULL )
    {
        pPrevMap = pMap;
        pMap = pMap->pNextMap;
    }

    //
    // If we didn't find this file, then this is an invalid file handle.
    //
    if( pMap->pFile != pFile )
    {
        DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!CHandleManager::RemoveHandle - The file handle is not found in the handle list.\n")));
        lResult = ERROR_INVALID_HANDLE;
        goto exit_removehandle;
    }

    pList = pMap->pHandles;
    while( pList && pList->pHandle != pFileHandle )
    {
        pPrevList = pList;
        pList = pList->pNext;
    }

    if( !pList )
    {
        //
        // We found the file associated with this handle, but we never found
        // the handle itself.  Was the handle closed twice?
        //
        DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!CHandleManager::RemoveHandle - The file handle is not found in the handle list.\n")));
        lResult = ERROR_INVALID_HANDLE;
        goto exit_removehandle;
    }

    if( pPrevList )
    {
        pPrevList->pNext = pList->pNext;
    }
    else if( pList == pMap->pHandles )  // is the "if" here necessary?  when won't this happen?
    {
        pMap->pHandles = pList->pNext;
    }

    //
    // We've found the entry in the handle list for this file handle and have
    // removed it from the list.  Now we just need to delete the entry, delete
    // the file handle, and de-reference the file associated with the handle.
    //
    // Also, if this was the only handle in the map, then we need to remove the
    // map.
    //
    delete pList;

    pFileHandle->pStream->GetFile()->Dereference();

    delete pFileHandle;

    //
    // Now check if we have to remove the map.
    //
    if( pMap->pHandles == NULL )
    {
        if( pPrevMap )
        {
            pPrevMap->pNextMap = pMap->pNextMap;
        }
        else
        {
            //
            // This was the first map in the list.  We don't want to delete this
            // map.
            //
            if( pMap->pNextMap )
            {
                //
                // There is another map in this entry, so copy the data from
                // the other map into this one, and then move the pointer to
                // the next map so that it will be deleted.
                //
                PHANDLE_MAP pTemp = pMap->pNextMap;
                CopyMemory( pMap, pMap->pNextMap, sizeof(HANDLE_MAP) );
                pMap = pTemp;
            }
            else
            {
                //
                // There are no other maps in this hash location, so just zero
                // out this initial entry and set the map to null so that we
                // won't try to delete it.
                //
                ZeroMemory( pMap, sizeof(HANDLE_MAP) );
                pMap = NULL;
            }
        }

        if( pMap )
        {
            delete pMap;
        }
    }

exit_removehandle:

    Unlock();

    return lResult;
}

// -----------------------------------------------------------------------------
// CFileManager Implementation
// -----------------------------------------------------------------------------

// /////////////////////////////////////////////////////////////////////////////
// Constructor
//
CFileManager::CFileManager()
: m_pVolume( NULL ),
  m_pFileList( NULL ),
  m_dwHashSize( DEFAULT_HASH_SIZE ),
  m_fAttemptCleanup( FALSE )
{
    InitializeCriticalSection( &m_cs );
}

// /////////////////////////////////////////////////////////////////////////////
// Destructor
//
// Syncronization: Should the volume be locked?  Locking the file manager.
//
CFileManager::~CFileManager()
{
    PFILE_LIST pList = NULL;
    Lock();

    if( m_pFileList )
    {
        for( DWORD x = 0; x < m_dwHashSize; x++ )
        {
            pList = m_pFileList[x].pNextFile;

            //
            // TODO::Do we really want to tear down all files here?  Probably, but
            // haven't thought through it yet.
            //
            if( m_pFileList[x].pFile )
            {
                delete m_pFileList[x].pFile;
            }

            while( pList )
            {
                PFILE_LIST pTmpList = pList;

                if( pList->pFile )
                {
                    delete pList->pFile;
                }

                pList = pList->pNextFile;

                delete pTmpList;
            }
        }

        delete [] m_pFileList;
        m_pFileList = NULL;
    }

    Unlock();
    DeleteCriticalSection( &m_cs );
}

// /////////////////////////////////////////////////////////////////////////////
// Initialize
//
BOOL CFileManager::Initialize( CVolume* pVolume )
{
    ASSERT( pVolume != NULL );

    m_pVolume = pVolume;

    //
    // TODO::Check the registry for the hash size to use.
    //

    m_pFileList = new (UDF_TAG_FILE_LIST) FILE_LIST[m_dwHashSize];

    if( !m_pFileList )
    {
        return FALSE;
    }

    ZeroMemory( m_pFileList, sizeof(FILE_LIST) * m_dwHashSize );

    return TRUE;
}

// /////////////////////////////////////////////////////////////////////////////
// AddFile
//
// Syncronize: The file manager should be locked when entering this function.
//             The only exception to this during the initialization of the
//             volume, before anyone else can touch the file manager.
//
LRESULT CFileManager::AddFile( CFile* pFile, CFile** ppExistingFile )
{
    LRESULT lResult = ERROR_SUCCESS;
    CFile* pExistingFile = NULL;

    lResult = FindFile( pFile, &pExistingFile );

    if( lResult == ERROR_SUCCESS )
    {
        lResult = ERROR_ALREADY_EXISTS;
        *ppExistingFile = pExistingFile;

        //
        // Make sure that this file will not go away during cleanup.
        //
        pExistingFile->Reference();
    }
    else if( lResult == ERROR_NOT_FOUND )
    {
        DWORD dwHash = GetHashValue( pFile, m_dwHashSize );
        PFILE_LIST pList = &m_pFileList[dwHash];

        if( pList->pFile == NULL )
        {
            pList->pFile = pFile;
            lResult = ERROR_SUCCESS;
        }
        else
        {
            while( pList->pNextFile )
            {
                pList = pList->pNextFile;
            }

            pList->pNextFile = new (UDF_TAG_FILE_LIST) FILE_LIST;
            if( !pList->pNextFile )
            {
                lResult = ERROR_NOT_ENOUGH_MEMORY;
            }
            else
            {
                pList = pList->pNextFile;
                pList->pNextFile = NULL;
                pList->pFile = pFile;
                lResult = ERROR_SUCCESS;
            }
        }
    }

    if( lResult == ERROR_SUCCESS )
    {
        //
        // Make sure that the file will not go away during Cleanup.
        //
        pFile->Reference();
    }

    //
    // TODO::Should we only call this after we've reached a threshold?
    //
    Cleanup();

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// Cleanup
//
// Syncronization: The file manager should be locked upon entering this
// function.
//
// This method searches through all the files in the file manager that do not
// have any references to them and removes them from memory.
//
void CFileManager::Cleanup()
{
    if( !m_fAttemptCleanup )
    {
        return;
    }

    m_fAttemptCleanup = FALSE;

    for( DWORD x = 0; x < m_dwHashSize; x++ )
    {
        PFILE_LIST pList = &m_pFileList[x];
        CFile* pFile = NULL;

        while( pList && pList->pFile )
        {
            pFile = pList->pFile;
            pFile->Lock();

            if( (pFile->GetReferenceCount() == 0) &&
                (!pFile->IsDirectory() || ((CDirectory*)pFile)->IsEmpty()) )
            {
                pList = CleanFile( pFile );
            }
            else
            {
                pList = pList->pNextFile;
                pFile->Unlock();
            }
        }
    }
}

// /////////////////////////////////////////////////////////////////////////////
// CleanFile
//
PFILE_LIST CFileManager::CleanFile( CFile* pFile )
{
    DWORD dwHash = GetHashValue( pFile, m_dwHashSize );
    CDirectory* pParent = NULL;
    PFILE_LIST pList = NULL;

    ASSERT( pFile->GetReferenceCount() == 0 );
    ASSERT( !pFile->IsDirectory() || ((CDirectory*)pFile)->IsEmpty() );

    //
    // We will recursively remove all parents that are freed because we remove
    // this file.  Note that we must remove the parents before we change the hash
    // links or the file list pointer that we return may be invalid.
    //
    while( (pParent = pFile->GetFirstParent()) != NULL )
    {
        VERIFY(pParent->RemoveChildPointer( pFile ));

        if( pParent->IsEmpty() && (pParent->GetReferenceCount() == 0) )
        {
            CleanFile( pParent );
        }
    }

    //
    // If this file list is actually part of the hash array that we allocated,
    // then we don't want to delete it.  Instead we will either zero it out if
    // there is not "pNextFile", or copy the "pNextFile" file list into this
    // entry.
    //
    if( m_pFileList[dwHash].pFile == pFile )
    {
        if( m_pFileList[dwHash].pNextFile == NULL )
        {
            ZeroMemory( &m_pFileList[dwHash], sizeof(FILE_LIST) );
        }
        else
        {
            //
            // Shift the list down and free one node.
            //
            FILE_LIST* pNextFile = m_pFileList[dwHash].pNextFile;
            CopyMemory( &m_pFileList[dwHash],
                        pNextFile,
                        sizeof(FILE_LIST) );
            delete pNextFile;
        }

        pList = &m_pFileList[dwHash];
    }
    else
    {
        //
        // This is not part of the original hash array that we allocated, so
        // we just need to remap the linked list and then delete the entry.
        //
        PFILE_LIST pCurrent = &m_pFileList[dwHash];
        PFILE_LIST pPrev = NULL;

        while( pCurrent->pFile != pFile )
        {
            pPrev = pCurrent;
            pCurrent = pCurrent->pNextFile;
        }

        ASSERT(pCurrent->pFile == pFile);

        pPrev->pNextFile = pCurrent->pNextFile;
        pList = pCurrent->pNextFile;

        delete pCurrent;
    }

    delete pFile;

    return pList;
}

// /////////////////////////////////////////////////////////////////////////////
// FindFile
//
// Syncronization: The file manager should be locked when calling this method.
//
LRESULT CFileManager::FindFile( CFile* pFile, CFile** ppExistingFile )
{
    LRESULT lResult = ERROR_NOT_FOUND;
    DWORD dwHash = GetHashValue( pFile, m_dwHashSize );
    PFILE_LIST pList = &m_pFileList[dwHash];

    ASSERT( pFile != NULL );
    ASSERT( ppExistingFile != NULL );

    while( pList &&
           pList->pFile &&
           pList->pFile->GetUniqueId() != pFile->GetUniqueId() )
    {
        pList = pList->pNextFile;
    }

    if( pList &&
        pList->pFile &&
        pList->pFile->GetUniqueId() == pFile->GetUniqueId() )
    {
        CFile* pTmpFile = pList->pFile;

        if( !pFile->IsSameFile( pTmpFile ) )
        {
            DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!CFileManager::FindFile found two files with the same ID: %d, %d\n"),pFile->GetLBN(), pTmpFile->GetLBN()));
            lResult = ERROR_DISK_CORRUPT;
        }
        else
        {
            lResult = ERROR_SUCCESS;
            *ppExistingFile = pTmpFile;
        }

        pList = pList->pNextFile;
    }

    return lResult;
}

// -----------------------------------------------------------------------------
// CAllocExtent Implementation
// -----------------------------------------------------------------------------

// /////////////////////////////////////////////////////////////////////////////
// Constructor
//
CAllocExtent::CAllocExtent()
: m_StreamOffset( 0 ),
  m_NumBlocks( 0 ),
  m_Length( 0 ),
  m_pStream( NULL ),
  m_Lbn( 0 ),
  m_Partition( INVALID_REFERENCE )
{
}

// /////////////////////////////////////////////////////////////////////////////
// Destructor
//
CAllocExtent::~CAllocExtent()
{
}

// /////////////////////////////////////////////////////////////////////////////
// Initialize
//
BOOL CAllocExtent::Initialize( Uint64 FileOffset,
                               Uint32 Length,
                               Uint32 Lbn,
                               Uint16 Partition,
                               CStream* pStream )
{
    BOOL fResult = TRUE;

    ASSERT( pStream != NULL );
    ASSERT( pStream->GetVolume()->GetSectorSize() != 0 );

    m_StreamOffset = FileOffset;
    m_Length = Length;
    m_Lbn = Lbn;
    m_Partition = Partition;

    m_pStream = pStream;

    //
    // TODO::Take a look at what NumBlocks really means.  The file offset
    // could start somewhere in the middle of a block, so this count could
    // be off by one.  Note that this should only be the case for file tails
    // because the extent lengths of the allocation descriptors in the file
    // body must be a multiple of the block size.
    //
    m_NumBlocks = m_Length / pStream->GetVolume()->GetSectorSize();

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// Consume
//
// See if the blocks can be combined.
// 1. The must be contiguous in the file.
// 2. They must be contiguous on disk.
// 3. The extent size has to be an integer multiple of the blocksize.
// 4. They both must be in the same partition number.
// 5. The combined size of the blocks is <= EXTENTAD_LEN_MASK
// 6. For VirtualSpace (incremental media) extent length <= Sector Size
//
BOOL CAllocExtent::Consume( CAllocExtent* pSegment )
{
    BOOL fResult = FALSE;

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// ContainsOffset
//
BOOL CAllocExtent::ContainsOffset( Uint64 Offset )
{
    BOOL fResult = FALSE;

    if( Offset >= m_StreamOffset )
    {
        if( Offset < m_StreamOffset + m_Length )
        {
            fResult = TRUE;
        }
    }

    return fResult;
}

// -----------------------------------------------------------------------------
// CStreamAllocation Implementation
// -----------------------------------------------------------------------------

// /////////////////////////////////////////////////////////////////////////////
// Constructor
//
CStreamAllocation::CStreamAllocation()
: m_pStream( NULL ),
  m_ExtentsPerChunk( 0 )
{
    ZeroMemory( &m_Allocation, sizeof( m_Allocation ) );
}

// /////////////////////////////////////////////////////////////////////////////
// Destructor
//
CStreamAllocation::~CStreamAllocation()
{
    PALLOC_CHUNK pChunk = m_Allocation.pNext;
    PALLOC_CHUNK pChunkToDelete = NULL;

    if( m_Allocation.pSegments )
    {
        delete [] m_Allocation.pSegments;
    }

    while( pChunk )
    {
        if( pChunk->pSegments )
        {
            delete [] pChunk->pSegments;
        }

        pChunkToDelete = pChunk;
        pChunk = pChunk->pNext;

        delete pChunkToDelete;
    }
}

// /////////////////////////////////////////////////////////////////////////////
// Initialize
//
BOOL CStreamAllocation::Initialize( CStream* pStream )
{
    ASSERT( pStream != NULL );
    CVolume* pVolume = pStream->GetVolume();

    if( m_pStream != NULL )
    {
        ASSERT(FALSE);
        return TRUE;
    }

    m_pStream = pStream;
    m_ExtentsPerChunk = pVolume->GetAllocExtentsPerChunk();

    //
    // The number of extents per chunk is loaded from the registry.  The
    // default value is DEFAULT_ALLOC_SEGMENTS_PER_CHUNK.
    //
    m_Allocation.pSegments = new (UDF_TAG_ALLOC_EXTENT) CAllocExtent[m_ExtentsPerChunk];
    if( m_Allocation.pSegments == NULL )
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        return FALSE;
    }

    return TRUE;
}

// /////////////////////////////////////////////////////////////////////////////
// AddExtent
//
// Syncronization: This should be syncronized at the stream level.
//     Anytime the allocation is read/written, it should grab the stream's
//     critical section.
//
// TODO::Check to see if we can combine different extents.
//
LRESULT CStreamAllocation::AddExtent( Uint64 FileOffset, // In blocks
                                      Uint32 Length,
                                      Uint32 Lbn,
                                      Uint16 Partition )
{
    LRESULT lResult = ERROR_SUCCESS;
    PALLOC_CHUNK pChunk = &m_Allocation;

    //
    // First, find a chunk of extents with room in it.  If all the current
    // blocks are full, then we'll try to allocate a new chunk.
    //
    while( pChunk->ExtentsInChunk >= m_ExtentsPerChunk )
    {
        if( pChunk->pNext )
        {
            pChunk = pChunk->pNext;
        }
        else
        {
            pChunk->pNext = new (UDF_TAG_ALLOC_CHUNK) ALLOC_CHUNK;
            if( !pChunk->pNext )
            {
                lResult = ERROR_NOT_ENOUGH_MEMORY;
                break;
            }

            ZeroMemory( pChunk->pNext, sizeof(ALLOC_CHUNK) );

            pChunk->pNext->pPrev = pChunk;
            pChunk = pChunk->pNext;

            pChunk->pSegments = new (UDF_TAG_ALLOC_EXTENT) CAllocExtent[m_ExtentsPerChunk];
            if( !pChunk->pSegments )
            {
                lResult = ERROR_NOT_ENOUGH_MEMORY;
            }
        }
    }

    //
    // If we have a chunk with room for another extent, then add it.
    //
    if( (pChunk->ExtentsInChunk < m_ExtentsPerChunk) && pChunk->pSegments )
    {
        CAllocExtent& Extent = pChunk->pSegments[pChunk->ExtentsInChunk];
        if( !Extent.Initialize( FileOffset,
                                Length,
                                Lbn,
                                Partition,
                                m_pStream ) )
        {
            lResult = GetLastError();
        }
        else
        {
            pChunk->ExtentsInChunk += 1;
        }
    }

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// GetExtent
//
// Syncronization: This should be syncronized at the stream level.
//     Anytime the allocation is read/written, it should grab the stream's
//     critical section.
//
LRESULT CStreamAllocation::GetExtent( Uint64 StreamOffset,
                                      CAllocExtent** ppExtent )
{
    LRESULT lResult = ERROR_SECTOR_NOT_FOUND;
    PALLOC_CHUNK pChunk = &m_Allocation;
    CAllocExtent* pExtent = NULL;
    BOOL fFound = FALSE;

    ASSERT( ppExtent );

    //
    // TODO::This isn't right for file tails.
    //
    if( StreamOffset >= m_pStream->GetFileSize() )
    {
        //
        // Setting pChunk to NULL to skip the search completely.
        //
        pChunk = NULL;
        lResult = ERROR_HANDLE_EOF;
    }

    while( pChunk && !fFound )
    {
        for( Uint32 Indx = 0; !fFound && Indx < pChunk->ExtentsInChunk; Indx++ )
        {
            pExtent = &pChunk->pSegments[Indx];

            if( pExtent->ContainsOffset( StreamOffset ) )
            {
                lResult = ERROR_SUCCESS;
                *ppExtent = pExtent;
                fFound = TRUE;
            }
        }

        pChunk = pChunk->pNext;
    }

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// GetNextExtent
//
// This will return the extent (if any) that exists after the extent containing
// the file offset.
//
// Syncronization: This should be syncronized at the stream level.
//     Anytime the allocation is read/written, it should grab the stream's
//     critical section.
//
LRESULT CStreamAllocation::GetNextExtent( Uint64 FileOffset,
                                          CAllocExtent** ppExtent )
{
    LRESULT lResult = ERROR_SUCCESS;
    PALLOC_CHUNK pChunk = &m_Allocation;
    CAllocExtent* pExtentToReturn = NULL;
    CAllocExtent* pExtent = NULL;

    ASSERT( ppExtent != NULL );

    if( FileOffset >= m_pStream->GetFileSize() )
    {
        //
        // Setting pChunk to NULL to skip the search completely.
        //
        pChunk = NULL;
        lResult = ERROR_HANDLE_EOF;
    }

    while( pChunk )
    {
        for( Uint32 Indx = 0; Indx < pChunk->ExtentsInChunk; Indx++ )
        {
            pExtent = &pChunk->pSegments[Indx];

            if( pExtent->GetStreamOffset() > FileOffset )
            {
                if( pExtentToReturn )
                {
                    if( pExtentToReturn->GetStreamOffset() >
                        pExtent->GetStreamOffset() )
                    {
                        pExtentToReturn = pExtent;
                    }
                }
                else
                {
                    lResult = ERROR_SUCCESS;
                    pExtentToReturn = pExtent;
                }
            }
        }

        if( pChunk->pNext )
        {
            pChunk = pChunk->pNext;
        }
    }

    *ppExtent = pExtentToReturn;

    return lResult;
}

// -----------------------------------------------------------------------------
// CFileInfo Implementation
// -----------------------------------------------------------------------------

// /////////////////////////////////////////////////////////////////////////////
// Constructor
//
CFileInfo::CFileInfo()
{
    ZeroMemory( &m_FindData, sizeof(m_FindData) );
}

// /////////////////////////////////////////////////////////////////////////////
// SetFileAttribute
//
void CFileInfo::SetFileAttribute( DWORD dwAttribute )
{
    m_FindData.dwFileAttributes |= dwAttribute;
}

// /////////////////////////////////////////////////////////////////////////////
// SetFileCreationTime
//
void CFileInfo::SetFileCreationTime( const FILETIME& CreateTime )
{
    m_FindData.ftCreationTime = CreateTime;
}

// /////////////////////////////////////////////////////////////////////////////
// SetFileModifyTime
//
void CFileInfo::SetFileModifyTime( const FILETIME& ModifyTime )
{
    m_FindData.ftLastWriteTime = ModifyTime;
}

// /////////////////////////////////////////////////////////////////////////////
// SetFileAccessTime
//
void CFileInfo::SetFileAccessTime( const FILETIME& AccessTime )
{
    m_FindData.ftLastAccessTime = AccessTime;
}

// /////////////////////////////////////////////////////////////////////////////
// SetFileSize
//
void CFileInfo::SetFileSize( Uint64 FileSize )
{
    m_FindData.nFileSizeHigh = (Uint32)(FileSize >> 32);
    m_FindData.nFileSizeLow = (Uint32)(FileSize & 0xFFFFFFFF);
}

// /////////////////////////////////////////////////////////////////////////////
// SetFileSizeHigh
//
void CFileInfo::SetFileSizeHigh( Uint32 FileSizeHigh )
{
    m_FindData.nFileSizeHigh = FileSizeHigh;
}

// /////////////////////////////////////////////////////////////////////////////
// SetFileSizeLow
//
void CFileInfo::SetFileSizeLow( Uint32 FileSizeLow )
{
    m_FindData.nFileSizeLow = FileSizeLow;
}

// /////////////////////////////////////////////////////////////////////////////
// SetObjectId
//
void CFileInfo::SetObjectId( DWORD ObjectId )
{
    m_FindData.dwOID = ObjectId;
}

// /////////////////////////////////////////////////////////////////////////////
// SetFileName
//
BOOL CFileInfo::SetFileName( const WCHAR* strFileName, DWORD dwCount )
{
    if( !strFileName )
    {
        return FALSE;
    }

    if( dwCount > MAX_PATH )
    {
        return FALSE;
    }

    StringCchPrintfW( m_FindData.cFileName,
                      MAX_PATH,
                      L"%s",
                      strFileName );

    return TRUE;
}

// /////////////////////////////////////////////////////////////////////////////
// GetFindData
//
BOOL CFileInfo::GetFindData( PWIN32_FIND_DATAW pWin32FindData )
{
    if( pWin32FindData )
    {
        CopyMemory( pWin32FindData,
                    &m_FindData,
                    sizeof(m_FindData) );

        return TRUE;
    }

    return FALSE;
}

// -----------------------------------------------------------------------------
// CFileLink Implementation
// -----------------------------------------------------------------------------

// /////////////////////////////////////////////////////////////////////////////
// Constructor
//
CFileLink::CFileLink()
: m_pChild( NULL ),
  m_pParent( NULL ),
  m_pNextChild( NULL ),
  m_pNextParent( NULL ),
  m_strFileName( NULL ),
  m_fHidden( FALSE )
{
    InitializeCriticalSection( &m_cs );
}

// /////////////////////////////////////////////////////////////////////////////
// Destructor
//
CFileLink::~CFileLink()
{
    DeleteCriticalSection( &m_cs );

    if( m_strFileName )
    {
        delete [] m_strFileName;
    }
}

// /////////////////////////////////////////////////////////////////////////////
// Initialize
//
BOOL CFileLink::Initialize( CFile* pChild,
                            CDirectory* pParent,
                            const WCHAR* strFileName )
{
    DWORD dwSize = 0;

    ASSERT(pChild != NULL);
    ASSERT(pParent != NULL);
    ASSERT(strFileName != NULL);

    m_pChild = pChild;
    m_pParent = pParent;

    dwSize = wcslen( strFileName ) + 1;

    m_strFileName = new (UDF_TAG_STRING) WCHAR[ dwSize ];
    if( !m_strFileName )
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        return FALSE;
    }

    dwSize *= sizeof(WCHAR);

    CopyMemory( m_strFileName, strFileName, dwSize );

    return TRUE;
}

// -----------------------------------------------------------------------------
// CFile Implementation
// -----------------------------------------------------------------------------

// /////////////////////////////////////////////////////////////////////////////
// Constructor
//
CFile::CFile()
: m_pVolume( NULL ),
  m_pMainStream( NULL ),
  m_pIncomingLink( NULL ),
  m_pNamedStreams( NULL ),
  m_RefCount( 0 ),
  m_UniqueId( 0 )
{
    InitializeCriticalSection( &m_cs );

    ZeroMemory( &m_IcbLocation, sizeof(m_IcbLocation) );
}

// /////////////////////////////////////////////////////////////////////////////
// Destructor
//
CFile::~CFile()
{
    //
    // Make sure to delete any streams.  There are no references to the file, so
    // we don't need to worry about locking it.
    //
    // TODO::If we add named stream support then we will need to delete those
    // here as well.
    //

    if( m_pMainStream )
    {
        delete m_pMainStream;
    }

    DeleteCriticalSection( &m_cs );

}

// /////////////////////////////////////////////////////////////////////////////
// Initialize
//
// Syncronization: This is only called during initialization of a file and
// should not need any other syncronization.
//
LRESULT CFile::Initialize( CVolume* pVolume, NSRLBA Location )
{
    LRESULT lResult = ERROR_SUCCESS;

    ASSERT( pVolume != NULL );

    m_pVolume = pVolume;
    m_IcbLocation = Location;

    //
    // For now I've included CFile::Read here which will also initialize the
    // main stream - reading in it's allocation and preparing it for read.
    //
    lResult = Read();

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// Read
//
// Syncronization: This is only called during initialization of a file and
// should not need any other syncronization.
//
// This method initializes the main stream in the file ICB.  This does not
// read in user data.
//
LRESULT CFile::Read()
{
    LRESULT lResult = ERROR_SUCCESS;
    BYTE* pBuffer = NULL;
    PDESTAG pDesTag = NULL;

    if( m_pMainStream != NULL )
    {
        //
        // If we've already read in the file, then there's no reason to do it
        // again.  We shouldn't be calling this, but it won't hurt anything
        // either.
        //
        ASSERT(FALSE);
        goto exit_read;
    }

    //
    // Read in the ICB which has to fit into a sector (UDF r2.50 2.3.6).
    //
    pBuffer = new (UDF_TAG_BYTE_BUFFER) BYTE[m_pVolume->GetSectorSize()];
    if( !pBuffer )
    {
        lResult = ERROR_NOT_ENOUGH_MEMORY;
        goto exit_read;
    }

    lResult = m_pVolume->GetPartition()->Read( m_IcbLocation,
                                               0,
                                               m_pVolume->GetSectorSize(),
                                               pBuffer );
    if( lResult != ERROR_SUCCESS )
    {
        goto exit_read;
    }

    //
    // Now check to see what type of file it is - either a regular ICB or an
    // extended ICB.  Nothing else is supported and should be reported as a
    // corrupt disk.
    //
    pDesTag = (PDESTAG)pBuffer;
    if( pDesTag->Ident == DESTAG_ID_NSR_FILE )
    {
        //
        // We've found a standard ICB.  Verify that it is correct and use
        // it to initialize the main stream.
        //
        PICBFILE pFile = (PICBFILE)pBuffer;

        if( !VerifyDescriptor( m_IcbLocation.Lbn,
                               pBuffer,
                               sizeof(ICBFILE) + pFile->AllocLength + pFile->EALength ) )
        {
            DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!CFile.Read Found an invalid ICB descriptor.\n")));
            lResult = ERROR_DISK_CORRUPT;
            goto exit_read;
        }

        m_UniqueId = pFile->UniqueID;

        m_pMainStream = new (UDF_TAG_STREAM) CStream();
        if( !m_pMainStream )
        {
            lResult = ERROR_NOT_ENOUGH_MEMORY;
            goto exit_read;
        }

        if( !m_pMainStream->Initialize( this, pFile, m_IcbLocation.Partition ) )
        {
            goto exit_read;
        }
    }
    else if( pDesTag->Ident == DESTAG_ID_NSR_EXT_FILE )
    {
        //
        // We've found an extended ICB.  Verify that it is correct and use
        // it to initialize the main stream.
        //
        PICBEXTFILE pFile = (PICBEXTFILE)pBuffer;

        if( !VerifyDescriptor( m_IcbLocation.Lbn,
                               pBuffer,
                               sizeof(ICBEXTFILE) + pFile->AllocLength + pFile->EALength ) )
        {
            DEBUGMSG(ZONEMASK_MEDIA,(TEXT("UDFS!CFile.Read Found an invalid ICB descriptor.\n")));
            lResult = ERROR_DISK_CORRUPT;
            goto exit_read;
        }

        m_UniqueId = pFile->UniqueID;

        m_pMainStream = new (UDF_TAG_STREAM) CStream();
        if( !m_pMainStream )
        {
            lResult = ERROR_NOT_ENOUGH_MEMORY;
            goto exit_read;
        }

        if( !m_pMainStream->Initialize( this, pFile, m_IcbLocation.Partition ) )
        {
            goto exit_read;
        }
    }
    else
    {
        //
        // Who knows what this is? But we do know it's busted.
        //
        DEBUGMSG(ZONEMASK_MEDIA,(TEXT("UDFS!CFile.Read The file descriptor tag is corrupt.\n")));
        lResult = ERROR_DISK_CORRUPT;
        goto exit_read;
    }

exit_read:
    if( pBuffer )
    {
        delete [] pBuffer;
    }

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// AddParent
//
// Syncronization: At the file level.
//      CDirectory::AddChildPointer - Lock the directory.
//
BOOL CFile::AddParent( CFileLink* pParent )
{
    BOOL fResult = TRUE;
    BOOL fUnlock = FALSE;

    ASSERT( pParent != NULL );

    Lock();
    fUnlock = TRUE;

    if( m_pIncomingLink )
    {
        CFileLink* pCurrent = m_pIncomingLink;

        while( pCurrent && pCurrent->GetNextParent() )
        {
            pCurrent = pCurrent->GetNextParent();
        }

        pCurrent->SetNextParent( pParent );
    }
    else
    {
        m_pIncomingLink = pParent;
    }

    if( fUnlock )
    {
        Unlock();
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// RemoveParent
//
// Syncronization: At the file level.
//      CDirectory::AddChildPointer - Lock the directory.
//
BOOL CFile::RemoveParent( CFileLink* pParent )
{
    BOOL fResult = TRUE;
    CFileLink* pCurrent = NULL;
    CFileLink* pPrev = NULL;

    Lock();

    if( !m_pIncomingLink )
    {
        fResult = FALSE;
        ASSERT( FALSE );
        goto exit_removeparent;
    }

    pCurrent = m_pIncomingLink;
    while( pCurrent && pCurrent != pParent )
    {
        pPrev = pCurrent;
        pCurrent = pCurrent->GetNextParent();
    }

    if( !pCurrent )
    {
        fResult = FALSE;
        ASSERT( FALSE );
        goto exit_removeparent;
    }

    if( pPrev )
    {
        pPrev->SetNextParent( pCurrent->GetNextParent() );
    }
    else
    {
        m_pIncomingLink = pCurrent->GetNextParent();
    }

exit_removeparent:
    Unlock();

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// IsSameFile
//
BOOL CFile::IsSameFile( CFile* pFile )
{
    BOOL fResult = TRUE;

    if( pFile->m_IcbLocation.Lbn != m_IcbLocation.Lbn )
    {
        fResult = FALSE;
    }

    if( pFile->m_IcbLocation.Partition != m_IcbLocation.Partition )
    {
        fResult = FALSE;
    }

    if( pFile->m_UniqueId != m_UniqueId )
    {
        fResult = FALSE;
    }

    if( pFile->IsDirectory() != IsDirectory() )
    {
        fResult = FALSE;
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// Dereference
//
void CFile::Dereference()
{
    #ifdef DBG
    Lock();
    #endif

    if( !InterlockedDecrement( &m_RefCount ) )
    {
        //
        // The next time we attempt to allocate something and we're passed
        // the threshold, then we know that we've dereferenced a file and
        // it's worth the time to try and delete some files.
        //
        m_pVolume->GetFileManager()->SetCleanup( TRUE );
    }

    #ifdef DBG
    DEBUGMSG(ZONE_FUNCTION,(TEXT("UDFS!Dereference - %d\r\n"), m_RefCount );
    Unlock();
    #endif
}

// -----------------------------------------------------------------------------
// CDirectory Implementation
// -----------------------------------------------------------------------------

// /////////////////////////////////////////////////////////////////////////////
// Constructor
//
CDirectory::CDirectory()
: m_pChildren( NULL )
{
    ZeroMemory( &m_FidList, sizeof(m_FidList) );
}

// /////////////////////////////////////////////////////////////////////////////
// Destructor
//
CDirectory::~CDirectory()
{
    PFID_LIST pFidList = m_FidList.pNext;
    PFID_LIST pToDelete = NULL;
    CFileLink* pLink = NULL;

    if( m_FidList.pFids )
    {
        delete [] m_FidList.pFids;
    }

    if( pFidList )
    {
        pToDelete = pFidList;

        if( pFidList->pFids )
        {
            delete [] pFidList->pFids;
        }

        pFidList = pFidList->pNext;

        delete pToDelete;
    }

    Lock();

    //
    // This should only happen when tearing down the volume.  Otherwise we
    // should never tear down a file that still has references to it's
    // children.  In this case there is no guarantee that the child even
    // exists any more so we will just delete the links without regard for
    // the integrity of the links.
    //
    while( m_pChildren )
    {
        pLink = m_pChildren;

        m_pChildren = m_pChildren->GetNextChild();

        delete pLink;
    }

    Unlock();
}

// /////////////////////////////////////////////////////////////////////////////
// FindFileName
//
// This will look for a file name starting at the given offset within the
// directory's data stream.  it will return the offset into the data
// stream to the start of the FID, or 0 if the file name is not found.
//
LRESULT CDirectory::FindFileName( Uint64 StartingOffset,
                                  CFileName* pFileName,
                                  __in_ecount(dwCharCount) WCHAR* strFileName,
                                  DWORD dwCharCount,
                                  PNSR_FID pStaticFid,
                                  Uint64* pNextFid )
{
    LRESULT lResult = ERROR_FILE_NOT_FOUND;
    Uint64 ReturnOffset = StartingOffset;
    PFID_LIST pList = &m_FidList;
    DWORD dwListOffset = 0;
    NSR_FID Fid;
    PNSR_FID pFid = NULL;
    DWORD dwFidOffset = 0;
    DWORD dwFidSize = 0;
    DWORD dwPaddingSize = 0;
    DWORD dwSector = 0;
    BOOL fFound = FALSE;

    ASSERT( pFileName != NULL );
    ASSERT( strFileName != NULL );
    ASSERT( pNextFid != NULL );

    Lock();

    if( StartingOffset > m_pMainStream->GetFileSize() )
    {
        lResult = ERROR_NO_MORE_FILES;
        goto exit_findfilename;
    }

    if( StartingOffset + sizeof(NSR_FID) > m_pMainStream->GetFileSize() )
    {
        lResult = ERROR_NO_MORE_FILES;
        goto exit_findfilename;
    }

    //
    // Find the FID list that contains this offset.
    //
    while( StartingOffset > pList->dwSize )
    {
        ASSERT( pList->pNext != NULL );

        StartingOffset -= pList->dwSize;
        pList = pList->pNext;
    }

    dwListOffset = (DWORD)StartingOffset;

    do
    {
        dwSector = pList->Location.Lbn;

        //
        // Copy the entire static FID structure into the FID.  Note 2 things:
        // 1. The FID might be fragmented across two different extents.
        // 2. This does not include the implementation use or the file name.
        //
        if( dwListOffset + sizeof(Fid) > pList->dwSize )
        {
            //
            // The FID is split across extents.  To make life easier, if this
            // happens, then we'll just allocate a buffer big enough for the
            // entire FID and copy the data into this buffer.
            //
            DWORD dwSizeToCopy = pList->dwSize - dwListOffset;

            //
            // Copy the first part of the FID.
            //
            CopyMemory( &Fid, &pList->pFids[dwListOffset], dwSizeToCopy );

            //
            // Move to the next FID list.  Since we've already checked the file
            // size, we know that the next list should not be NULL.
            //
            ASSERT( pList->pNext != NULL );
            pList = pList->pNext;

            //
            // Copy the remainder of the FID.
            //
            dwListOffset = sizeof(Fid) - dwSizeToCopy;
            CopyMemory( &((BYTE*)&Fid)[dwSizeToCopy],
                        pList->pFids,
                        dwListOffset );

        }
        else
        {
            CopyMemory( &Fid, &pList->pFids[dwListOffset], sizeof(Fid) );
            dwListOffset += sizeof(Fid);
        }

        //
        // The dwListOffset should now point just after the FID in memory.  We
        // need to determine if the FID is valid first.
        //
        if( Fid.Destag.Ident != DESTAG_ID_NSR_FID )
        {
            lResult = ERROR_DISK_CORRUPT;
            goto exit_findfilename;
        }

        //
        // We need to determine the entire FID size.  This include the size
        // of the static FID structure, the size of the implementation use,
        // the size of the name, and the size of the padding for the FID.
        //
        dwFidSize = Fid.FileIDLen + Fid.ImpUseLen + sizeof(Fid);
        dwPaddingSize = 4 *
                        ((Fid.FileIDLen + Fid.ImpUseLen + sizeof(NSR_FID) + 3)/4) -
                        (Fid.FileIDLen + Fid.ImpUseLen + sizeof(NSR_FID));
        dwFidSize += dwPaddingSize;

        //
        // Check if we have a FID that won't fit in the file size.
        // TODO::Make sure we don't overflow a 64-bit integer here.
        //
        if( ReturnOffset + dwFidSize > m_pMainStream->GetFileSize() )
        {
            lResult = ERROR_DISK_CORRUPT;
            goto exit_findfilename;
        }

        //
        // Check if we have a FID that won't fit in the sector size.
        //
        if( dwFidSize > m_pVolume->GetSectorSize() )
        {
            lResult = ERROR_DISK_CORRUPT;
            goto exit_findfilename;
        }

        //
        // Allocate a buffer big enough for the entire FID.  This way we can
        // validate that the FID is correct.
        //
        pFid = (PNSR_FID)new (UDF_TAG_BYTE_BUFFER) BYTE[dwFidSize];
        if( !pFid )
        {
            lResult = ERROR_NOT_ENOUGH_MEMORY;
            goto exit_findfilename;
        }

        CopyMemory( pFid, &Fid, sizeof(Fid) );
        dwFidOffset = sizeof(Fid);

        //
        // Copy the Implementation Use, the file name, and the padding.  Again,
        // this could be split across extents.
        //
        if( dwFidSize - sizeof(Fid) + dwListOffset > pList->dwSize )
        {
            //
            // This is split across extents, so copy in two pieces.
            //
            DWORD dwSizeToCopy = pList->dwSize - dwListOffset;

            CopyMemory( (BYTE*)pFid + dwFidOffset,
                         pList->pFids + dwListOffset,
                         dwSizeToCopy );

            dwListOffset = 0;
            dwFidOffset += dwSizeToCopy;

            //
            // Since we've already checked the file size, we know the next list
            // should not be NULL.
            //
            ASSERT( pList->pNext != NULL );
            pList = pList->pNext;

            CopyMemory( (BYTE*)pFid + dwFidOffset,
                         pList->pFids + dwListOffset,
                         dwFidSize - sizeof(Fid) - dwSizeToCopy );

            dwFidOffset += dwFidSize - sizeof(Fid) - dwSizeToCopy;
            dwListOffset += dwFidSize - sizeof(Fid) - dwSizeToCopy;

        }
        else
        {
            CopyMemory( (BYTE*)pFid + dwFidOffset,
                        pList->pFids + dwListOffset,
                        dwFidSize - sizeof(Fid) );

            dwFidOffset += dwFidSize - sizeof(Fid);
            dwListOffset += dwFidSize - sizeof(Fid);
        }

        //
        // After all the work of getting the FID into one buffer, make sure that
        // it is not corrupt.
        //
        if( !VerifyDescriptor( dwSector, (BYTE*)pFid, dwFidSize ) )
        {
            lResult = ERROR_DISK_CORRUPT;
            goto exit_findfilename;
        }

        //
        // Good, it's not corrupt, but if it is deleted, then we must skip it
        // anyways.
        //
        if( (pFid->Flags & NSR_FID_F_DELETED) == 0 )
        {
            //
            // We've finally got the entire FID in one memory location and
            // determined that it is a valid FID.  Now we just need to see if the
            // file name stored in this FID matches the file name passed in.
            //
            if( GetFidName( strFileName, dwCharCount, pFid ) )
            {
                if( pFileName->IsMatch( strFileName ) )
                {
                    //
                    // We found a match at StartingOffset
                    //
                    fFound = TRUE;
                }
                else
                {
                    ZeroMemory( strFileName, dwCharCount * sizeof(WCHAR) );
                }
            }
        }

        //
        // If we didn't find a match, then we need to move the ReturnOffset and
        // free our buffer.
        //
        if( !fFound )
        {
            ReturnOffset += dwFidSize;
            delete [] pFid;
            pFid = NULL;
        }

    } while( !fFound &&
             ReturnOffset < m_pMainStream->GetFileSize() - sizeof(Fid) );


exit_findfilename:
    Unlock();

    if( fFound )
    {
        CopyMemory( pStaticFid, pFid, sizeof(NSR_FID) );
        *pNextFid = ReturnOffset + dwFidSize;
        lResult = ERROR_SUCCESS;
    }

    if( pFid )
    {
        delete [] pFid;
    }

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// FindNextFile
//
// Syncronization: Lock the directory.
//
// We search for a FID that has a matching file name (and is not deleted).  Once
// we have the file name, we either need to reference an existing file or read
// in a new file from the disk.  We also must setup the search handle for the
// next iteration.
//
LRESULT CDirectory::FindNextFile( PSEARCH_HANDLE pSearchHandle,
                                  CFileInfo* pFileInfo )
{
    LRESULT lResult = ERROR_SUCCESS;
    NSR_FID Fid;
    CFileName FileName;
    CFileLink* pLink = NULL;
    CFile* pFile = NULL;
    CStream* pStream = NULL;
    Uint64 NextSearch = 0;
    BOOL fUnlock = FALSE;
    CFileManager* pFileManager = m_pVolume->GetFileManager();

    BY_HANDLE_FILE_INFORMATION FileInfo;

    //
    // We cannot lock the directory and then try to lock the file manager
    // later as that would reverse the locking order since the file manager
    // sometimes locks files/directories it contains.
    //
    pFileManager->Lock();

    Lock();
    fUnlock = TRUE;

    //
    // Search the directory for a FID that matches the search criteria.
    //
    lResult = FindFileName( pSearchHandle->Position,
                            pSearchHandle->pFileName,
                            pFileInfo->GetFileNameBuffer(),
                            pFileInfo->GetMaxFileNameCount(),
                            &Fid,
                            &NextSearch );

    if( lResult != ERROR_SUCCESS )
    {
        goto exit_findnextfile;
    }

    if( !FileName.Initialize( pFileInfo->GetFileNameBuffer() ) )
    {
        lResult = GetLastError();
        goto exit_findnextfile;
    }

    //
    // If this file is not currently opened, then go ahead and open the file.
    // The file was referenced if it was found already open.
    //
    pLink = FindOpenFile( &FileName );
    if( pLink )
    {
        pFile = pLink->GetChild();
    }

    if( !pFile )
    {
        CFile* pExistingFile = NULL;

        //
        // We don't currently have this file open, so try to read it in.
        //
        if( Fid.Flags & NSR_FID_F_DIRECTORY )
        {
            pFile = new (UDF_TAG_DIRECTORY) CDirectory();
        }
        else
        {
            pFile = new (UDF_TAG_FILE) CFile();
        }

        if( !pFile )
        {
            lResult = ERROR_NOT_ENOUGH_MEMORY;
            goto exit_findnextfile;
        }

        //
        // Read in the file allocation and ICB for the main stream.
        //
        lResult = pFile->Initialize( m_pVolume, Fid.Icb.Start );
        if( lResult != ERROR_SUCCESS )
        {
            goto exit_findnextfile;
        }

        //
        // Check in the file manager to see if this file already exists and
        // add it if it's not there.
        //
        lResult = pFileManager->AddFile( pFile, &pExistingFile );
        if( lResult == ERROR_ALREADY_EXISTS )
        {
            lResult = ERROR_SUCCESS;

            delete pFile;

            pFile = pExistingFile;
        }
        else if( lResult != ERROR_SUCCESS )
        {
            goto exit_findnextfile;
        }

        //
        // This file is now in the file manager and has been read in.  We just
        // need to link it to this directory now.
        //
        pLink = AddChildPointer( pFile,
                                 pFileInfo->GetFileNameBuffer(),
                                 (Fid.Flags & NSR_FID_F_HIDDEN) != 0 );
        if( !pLink )
        {
            lResult = GetLastError();
        }
    }

    //
    // Either we've found an already open file in the directory, or we've
    // attached a new one.  It's time now to get the information from this
    // file.
    //
    pStream = pFile->GetMainStream();

    lResult = pStream->GetFileInformation( pLink, &FileInfo );
    if( lResult != ERROR_SUCCESS )
    {
        goto exit_findnextfile;
    }

    pFileInfo->SetFileAccessTime( FileInfo.ftLastAccessTime );
    pFileInfo->SetFileModifyTime( FileInfo.ftLastWriteTime );
    pFileInfo->SetFileCreationTime( FileInfo.ftCreationTime );

    pFileInfo->SetFileAttribute( FileInfo.dwFileAttributes );

    pFileInfo->SetFileSizeHigh( FileInfo.nFileSizeHigh );
    pFileInfo->SetFileSizeLow( FileInfo.nFileSizeLow );

exit_findnextfile:
    if( fUnlock )
    {
        Unlock();
    }

    pFileManager->Unlock();

    //
    // After we have the attributes from the file we don't need to keep it
    // around.
    //
    if( pFile )
    {
        pFile->Dereference();
    }

    //
    // Make sure that we start looking after the current FID the next time we
    // call FindNextFile.
    //
    pSearchHandle->Position = NextSearch;

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// Initialize
//
// Syncronization: This should only be called when the directory is first
// initialized, and so it shouldn't need any other syncronization.
//
// We will read in the directory data here.  The directory data is unlike a file
// in that we need to keep track of where each extent of data comes from, so it
// is tied very closely to the file allocation.  This will be very important
// when it comes time to modify, create, and delete FIDs in the directory.  We
// will only want to write the file extents that we must for incremental media.
//
LRESULT CDirectory::Initialize( CVolume* pVolume, NSRLBA Location )
{
    LRESULT lResult = CFile::Initialize( pVolume, Location );
    Uint64 FileOffset = 0;
    Uint64 RemainingBytes = 0;
    Uint32 BytesToRead = MAX_READ_SIZE;
    Uint32 BytesRead = 0;
    LONGAD ExtentInfo;
    PFID_LIST pList = &m_FidList;

    if( lResult != ERROR_SUCCESS )
    {
        goto exit_initialize;
    }

    ASSERT( m_pMainStream != NULL );

    SetLastError( ERROR_SUCCESS );

    RemainingBytes = m_pMainStream->GetFileSize();

    //
    // We have a very simple (and not necessarily good) way to read in
    // directories right now.  Allocate enough space to hold all of the FIDs
    // in one big chunk and read them in.  If the directory is larger than
    // 4GB (good luck enumerating this directory) then we won't be able to
    // allocate enough space for it.  This isn't a realistic worry right now
    // anyways.
    //
    if( RemainingBytes > ULONG_MAX )
    {
        // TODO::Is there a better error code here?
        DEBUGMSG(ZONE_ERROR|ZONE_ALLOC,(TEXT("UDFS!CDirectory.Initialize The directory is too big to read in.\n")));
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        goto exit_initialize;
    }

    //
    // Create a simple list for the embedded data case.  Note that we could
    // do this without allocating more memory for the FIDS, but given that they
    // are small enough to be embedded in an ICB and all the extra checking we'd
    // have to do later, it is simpler for now to just allocate this buffer.
    //
    if( m_pMainStream->IsEmbedded() )
    {
        pList->Location = Location;
        pList->fModified = FALSE;
        pList->dwSize = (Uint32)RemainingBytes;
        pList->pFids = new (UDF_TAG_BYTE_BUFFER) BYTE[pList->dwSize];
        if( !pList->pFids )
        {
            SetLastError( ERROR_NOT_ENOUGH_MEMORY );
            goto exit_initialize;
        }

        lResult = m_pMainStream->ReadFile( 0,
                                           pList->pFids,
                                           pList->dwSize,
                                           &BytesRead );

        SetLastError( lResult );
        goto exit_initialize;
    }

    //
    // Now read in the file entries.  We are careful here to keep each fid list
    // associated with a particular extent on the disk.  This is extremely
    // important, especially for incremental media.  This will allow us to know
    // which extents have been modified and need to be written to the disk.
    //
    while( RemainingBytes )
    {
        DWORD dwExtentOffset = 0;

        //
        // Get the extent size and location for the given file offset.
        //
        lResult = m_pMainStream->GetExtentInformation( FileOffset, &ExtentInfo );
        if( lResult != ERROR_SUCCESS )
        {
            SetLastError( lResult );
            goto exit_initialize;
        }

        if( ExtentInfo.Length.Length == 0 )
        {
            //
            // There should never be an empty or sparse directory.
            //
            ASSERT( FALSE );

            SetLastError( ERROR_DISK_CORRUPT );
            goto exit_initialize;
        }

        //
        // Record this information in the FID list.
        //
        pList->Location = ExtentInfo.Start;
        pList->dwSize = ExtentInfo.Length.Length;
        pList->pFids = new (UDF_TAG_BYTE_BUFFER) BYTE[pList->dwSize];
        if( !pList->pFids )
        {
            DEBUGMSG(ZONE_ERROR|ZONE_ALLOC,(TEXT("UDFS!CDirectory.Initialize The directory is too big to read in.\n")));
            SetLastError( ERROR_NOT_ENOUGH_MEMORY );
            goto exit_initialize;
        }

        while( ExtentInfo.Length.Length )
        {
            //
            // ExtentInfo.Length.Length will tell us how much data we have left to
            // read from the extent at any point in time.
            //
            if( ExtentInfo.Length.Length < BytesToRead )
            {
                BytesToRead = ExtentInfo.Length.Length;
            }

            lResult = m_pMainStream->ReadFile( FileOffset,
                                               pList->pFids + dwExtentOffset,
                                               BytesToRead,
                                               &BytesRead );
            if( lResult != ERROR_SUCCESS )
            {
                SetLastError( lResult );
                goto exit_initialize;
            }

            // ReadFile returns true and BytesRead as 0 at "End of file" condition, even though this is valid return value for ReadFile 
            // we should not be here as this will cause an infinite loop here, break out, may be a bad Disk 
            if(lResult == ERROR_SUCCESS && BytesRead == 0) 
            {
                break;
            }
            
            dwExtentOffset += BytesRead;
            ExtentInfo.Length.Length -= BytesRead;
            FileOffset += BytesRead;
        }

        //
        // We've successfully read in the entire extent, so update the number of
        // bytes remaining to read for the whole file.
        //
        if( pList->dwSize > RemainingBytes )
        {
            RemainingBytes = 0;
            SetLastError ( ERROR_UNRECOGNIZED_VOLUME );
            lResult = ERROR_UNRECOGNIZED_VOLUME;
            goto exit_initialize;            
        }
        else
        {
            RemainingBytes -= pList->dwSize;
        }

        //
        // If there are more bytes to read, then allocate the next list segment
        // and attach it to the current list.  Then transition the pointer to
        // the newly allocated segment so that we will read into that segment
        // on the next trip through the loop.
        //
        if( RemainingBytes )
        {
            pList->pNext = new (UDF_TAG_FID_LIST) FID_LIST;
            if( !pList->pNext )
            {
                SetLastError( ERROR_NOT_ENOUGH_MEMORY );
                goto exit_initialize;
            }

            ZeroMemory( pList->pNext, sizeof(FID_LIST) );

            pList = pList->pNext;
        }

        BytesToRead = MAX_READ_SIZE;
    }

exit_initialize:

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// AddChildPointer
//
// Syncronization: At the directory level.
//      The directory must be locked.
//
// This will add a link in the m_pChildren list.  This will also all into the
// child and add the parent link.
//
CFileLink* CDirectory::AddChildPointer( CFile* pFile,
                                        const WCHAR* strFileName,
                                        BOOL fHidden )
{
    BOOL fResult = TRUE;
    CFileLink* pNewLink = NULL;
    BOOL fUnlock = FALSE;

    ASSERT( pFile != NULL );

    pNewLink = new (UDF_TAG_FILE_LINK) CFileLink();
    if( !pNewLink )
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        goto exit_addchildpointer;
    }

    fResult = pNewLink->Initialize( pFile, this, strFileName );
    if( !fResult )
    {
        goto exit_addchildpointer;
    }

    pNewLink->SetHidden( fHidden );

    Lock();
    fUnlock = TRUE;

    if( !m_pChildren )
    {
        m_pChildren = pNewLink;
    }
    else
    {
        CFileLink* pCurrent = m_pChildren;
        while( pCurrent->GetNextChild() )
        {
            if( pCurrent->GetChild() == pFile )
            {
                ASSERT(FALSE);
                SetLastError( ERROR_INTERNAL_ERROR );
                goto exit_addchildpointer;
            }

            pCurrent = pCurrent->GetNextChild();
        }

        if( pCurrent->GetChild() == pFile )
        {
            ASSERT(FALSE);
            SetLastError( ERROR_INTERNAL_ERROR );
            goto exit_addchildpointer;
        }
        pCurrent->SetNextChild( pNewLink );
    }

    fResult = pFile->AddParent( pNewLink );
    if( !fResult )
    {
        //
        // For some reason we were unable to add the pointer to the child.
        //
        ASSERT( FALSE );

        //
        // TODO::Currently there is no reason we should get here.  However
        // we might add code to remove the link from the parent if we ever
        // do have a code path to get here.
        //
    }

    Reference();

exit_addchildpointer:
    if( fUnlock )
    {
        Unlock();
    }

    if( !fResult && pNewLink )
    {
        delete pNewLink;
    }

    return pNewLink;
}

// /////////////////////////////////////////////////////////////////////////////
// RemoveChildPointer
//
// This will delete the pointer and will remove it from the list in both the
// parent and child.
//
BOOL CDirectory::RemoveChildPointer( CFile* pFile )
{
    BOOL fResult = TRUE;
    CFileLink* pLink = NULL;

    Lock();

    pLink = m_pChildren;
    while( pLink && pLink->GetChild() != pFile )
    {
        pLink = pLink->GetNextChild();
    }

    if( !pLink )
    {
        fResult = FALSE;
        ASSERT( FALSE );
        goto exit_removechildpointer;
    }

    fResult = RemoveChildPointer( pLink );
    if( fResult )
    {
        delete pLink;
    }

exit_removechildpointer:
    Unlock();

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// RemoveChildPointer
//
// This will not delete the pointer, but will remove it from the list.  This is
// only called internally and is a private method.
//
BOOL CDirectory::RemoveChildPointer( CFileLink* pLink )
{
    BOOL fResult = TRUE;
    BOOL fUnlock = FALSE;
    CFileLink* pCurrent = NULL;
    CFileLink* pPrev = NULL;

    ASSERT( pLink != NULL );
    ASSERT( pLink->GetParent() == this );

    Lock();
    fUnlock = TRUE;

    if( !m_pChildren )
    {
        ASSERT(FALSE);
        SetLastError( ERROR_INTERNAL_ERROR );
        goto exit_removechildpointer;
    }

    pCurrent = m_pChildren;
    while( pCurrent && pCurrent != pLink )
    {
        pPrev = pCurrent;
        pCurrent = pCurrent->GetNextChild();
    }

    if( !pCurrent )
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        fResult = FALSE;
        goto exit_removechildpointer;
    }

    if( !pPrev )
    {
        m_pChildren = pCurrent->GetNextChild();
    }
    else
    {
        pPrev->SetNextChild( pCurrent->GetNextChild() );
    }

    //
    // Ignoring the return value for now.  A FALSE return will throw
    // an assert and should never happen.
    //
    pCurrent->GetChild()->RemoveParent( pCurrent );

    Unlock();
    fUnlock = FALSE;

exit_removechildpointer:
    if( fUnlock )
    {
        Unlock();
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// FindOpenFile
//
// Syncronize: Shared lock at directory level.
//             We must have locked the file manager or we could have issues
//             where the file manager is trying to delete a file but the
//             object is still linked in the directory and we try to reference
//             it.
//
CFileLink* CDirectory::FindOpenFile( CFileName* pFileName )
{
    BOOL fUnlock = FALSE;
    CFileLink* pLink = NULL;
    BOOL fFound = FALSE;

    ASSERT( pFileName != NULL );

    Lock();
    fUnlock = TRUE;

    if( m_pChildren == NULL )
    {
        goto exit_findopenfile;
    }

    pLink = m_pChildren;

    while( pLink && !fFound )
    {
        if( pFileName->IsMatch( pLink->GetFileName() ) )
        {
            pLink->GetChild()->Reference();
            fFound = TRUE;
        }
        else
        {
            pLink = pLink->GetNextChild();
        }
    }

exit_findopenfile:
    if( fUnlock )
    {
        Unlock();
    }

    return pLink;
}

// /////////////////////////////////////////////////////////////////////////////
// GetFidName
//
BOOL CDirectory::GetFidName( __in_ecount(dwCount) WCHAR* strName,
                             DWORD dwCount,
                             PNSR_FID pFid )
{
    ASSERT( strName != NULL );
    ASSERT( pFid != NULL );

    BOOL fResult = TRUE;
    Uint8 Encoding = 0;
    const BYTE* FileName = &((BYTE*)(pFid + 1) + pFid->ImpUseLen)[1];
    DWORD dwNameSize = pFid->FileIDLen ? pFid->FileIDLen - 1 : 0;
    BYTE* Buffer = NULL;

    Buffer = new (UDF_TAG_BYTE_BUFFER) BYTE[dwNameSize + sizeof(WCHAR)];
    if( !Buffer )
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        return FALSE;
    }

    ZeroMemory( Buffer, dwNameSize + sizeof(WCHAR) );
    CopyMemory( Buffer, FileName, dwNameSize );

    __try
    {
        Encoding = ((BYTE*)(pFid + 1) + pFid->ImpUseLen)[0];
        switch( Encoding )
        {
        case 8:
        case 254:
            //
            // Add one for the null terminator.
            //
            dwNameSize += 1;
            if( dwCount < dwNameSize )
            {
                fResult = FALSE;
                goto exit_getfidname;
            }

#ifdef _PREFAST_
#pragma prefast( disable : 302 )
#endif
            StringCchPrintf( strName,
                             dwNameSize,
                             L"%S",
                             Buffer );

#ifdef _PREFAST_
#pragma prefast( enable : 302 )
#endif
            break;

        case 16:
        case 255:
            if( dwNameSize & 1 )
            {
                SetLastError( ERROR_FILE_CORRUPT );
                fResult = FALSE;
                goto exit_getfidname;
            }

            //
            // Size in chars, plus one for null terminator.
            //
            dwNameSize = dwNameSize / sizeof(WCHAR) + 1;
            if( dwCount < dwNameSize )
            {
                fResult =  FALSE;
                goto exit_getfidname;
            }

            StringCchPrintf( strName,
                             dwNameSize,
                             L"%s",
                             (WCHAR*)Buffer );

            //
            // Convert from CS0 to Unicode.
            //
            for( DWORD x = 0; x < dwNameSize * sizeof(WCHAR); x += 2 )
            {
                BYTE tmpChar = ((BYTE*)strName)[x];
                ((BYTE*)strName)[x] = ((BYTE*)strName)[x + 1];
                ((BYTE*)strName)[x + 1] = tmpChar;
            }
            break;

        default:
            fResult = FALSE;
            break;
        }

    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!GetFidName Invalid memory access.\n")));
        ASSERT(FALSE);
        fResult = FALSE;
    }

exit_getfidname:
    if( Buffer != NULL )
    {
        delete [] Buffer;
    }

    return fResult;
}

// -----------------------------------------------------------------------------
// CStream Implementation
// -----------------------------------------------------------------------------

// /////////////////////////////////////////////////////////////////////////////
// Constructor
//
CStream::CStream()
: m_pFile( NULL ),
  m_pVolume( NULL ),
  m_pNextStream( NULL ),
  m_pEmbeddedData( NULL ),
  m_pAEDs( NULL ),
  m_LinkCount( 0 ),
  m_InformationLength( 0 ),
  m_BlocksRecorded( 0 ),
  m_Checkpoint( 0 ),
  m_EALength( 0 ),
  m_ADLength( 0 ),
  m_FileType( 0 ),
  m_IcbFlags( 0 ),
  m_PartitionIndx( INVALID_REFERENCE )
{
    InitializeCriticalSection( &m_cs );
}

// /////////////////////////////////////////////////////////////////////////////
// Destructor
//
CStream::~CStream()
{
    DeleteCriticalSection( &m_cs );

    if( m_pEmbeddedData )
    {
        delete [] m_pEmbeddedData;
    }

    while( m_pAEDs )
    {
        PAED_LIST pToDelete = m_pAEDs;
        m_pAEDs = m_pAEDs->pNext;

        delete pToDelete;
    }
}

// /////////////////////////////////////////////////////////////////////////////
// Initialize (with regular ICB file)
//
// TODO::Add support for EAs.
//
BOOL CStream::Initialize( CFile* pFile, PICBFILE pIcb, Uint16 Partition )
{
    BOOL fResult = TRUE;

    ASSERT(pFile != NULL);

    if( m_pVolume )
    {
        ASSERT(FALSE);
        return TRUE;
    }

    m_pFile = pFile;
    m_pVolume = pFile->GetVolume();
    m_PartitionIndx = Partition;

    m_InformationLength = pIcb->InfoLength;
    m_EALength = pIcb->EALength;
    m_ADLength = pIcb->AllocLength;
    m_BlocksRecorded = pIcb->BlocksRecorded;
    m_LinkCount = pIcb->LinkCount;
    m_Checkpoint = pIcb->Checkpoint;
    m_FileType = pIcb->Icbtag.FileType;
    m_IcbFlags = pIcb->Icbtag.Flags;

    UdfConvertUdfTimeToFileTime( &pIcb->AccessTime, &m_AccessTime );
    UdfConvertUdfTimeToFileTime( &pIcb->ModifyTime, &m_ModifyTime );
    InitCreationFromEAs( pIcb );

    fResult = CommonInitialize( sizeof(ICBFILE),
                                (BYTE*)(pIcb + 1) + m_EALength );

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// Initialize (with extended ICB file)
//
BOOL CStream::Initialize( CFile* pFile, PICBEXTFILE pIcb, Uint16 Partition )
{
    BOOL fResult = TRUE;

    ASSERT(pFile != NULL);

    if( m_pVolume )
    {
        ASSERT(FALSE);
        return TRUE;
    }

    m_pFile = pFile;
    m_pVolume = pFile->GetVolume();
    m_PartitionIndx = Partition;

    m_InformationLength = pIcb->InfoLength;
    m_EALength = pIcb->EALength;
    m_ADLength = pIcb->AllocLength;
    m_BlocksRecorded = pIcb->BlocksRecorded;
    m_LinkCount = pIcb->LinkCount;
    m_Checkpoint = pIcb->Checkpoint;
    m_FileType = pIcb->Icbtag.FileType;
    m_IcbFlags = pIcb->Icbtag.Flags;

    UdfConvertUdfTimeToFileTime( &pIcb->AccessTime, &m_AccessTime );
    UdfConvertUdfTimeToFileTime( &pIcb->ModifyTime, &m_ModifyTime );
    UdfConvertUdfTimeToFileTime( &pIcb->CreationTime, &m_CreationTime );

    fResult = CommonInitialize( sizeof(ICBEXTFILE),
                                (BYTE*)(pIcb + 1) + m_EALength );

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// CommonInitialize
//
BOOL CStream::CommonInitialize( Uint32 IcbSize, const BYTE* pADs )
{
    BOOL fResult = TRUE;

    //
    // Even if the stream is curently embedded, it might change and we need to
    // intialize the allocation information for the stream.
    //
    if( !m_RecordedSpace.Initialize( this ) ||
        !m_NotRecordedSpace.Initialize( this ) )
    {
        return FALSE;
    }

    //
    // Validate the sizes in specified in the ICB.
    //
    if( m_EALength + m_ADLength + IcbSize > m_pVolume->GetSectorSize() )
    {
        DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!CStream.Initialize The EALength or AllocLength is too large.\n")));
        SetLastError( ERROR_FILE_CORRUPT );
        return FALSE;
    }

    //
    // Whether the file is embedded or not, we want to read in the data
    // in the allocation descriptors field.
    //
    m_pEmbeddedData = new (UDF_TAG_BYTE_BUFFER) BYTE[ m_ADLength ];
    if( !m_pEmbeddedData )
    {
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        return FALSE;
    }

    CopyMemory( m_pEmbeddedData,
                pADs,
                m_ADLength );

    if( !IsEmbedded() )
    {
        //
        // The file is not embedded, so read in the allocation information.
        //
        SetLastError( ReadStreamAllocation() );
        if( GetLastError() != ERROR_SUCCESS )
        {
            fResult = FALSE;
        }
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// InitCreationFromEAs
//
BOOL CStream::InitCreationFromEAs( PICBFILE pIcb )
{
    BOOL fResult = TRUE;
    BYTE* pBuffer = NULL;
    DWORD dwBufferSize = 0;
    DWORD dwEASize = 0;
    DWORD dwEAOffset = 0;
    PNSR_EAH pEAHeader = NULL;
    PNSR_EA_GENERIC pGenEA = NULL;
    CFile* pEAFile = NULL;
    CStream* pEAStream = NULL;

    //
    // There are EAs embedded in the file icb itself.
    //
    pEAHeader = (PNSR_EAH)(pIcb + 1);
    if( pIcb->EALength && pEAHeader->EAImp )
    {
        fResult = ValidateEAHeader( pEAHeader, m_EALength, &dwEASize );
        if( !fResult )
        {
            goto exit_initcreationfromeas;
        }

        fResult = ExtractEATime( (BYTE*)(pEAHeader + 1), dwEASize );
        if( fResult )
        {
            goto exit_initcreationfromeas;
        }
    }

    //
    // Now check for an EA ICB.
    //
    if( pIcb->IcbEA.Length.Length )
    {
        NSR_EAH Header = { 0 };
        DWORD dwBytesRead = 0;
        LRESULT lResult = ERROR_SUCCESS;

        pEAFile = new CFile;
        if( !pEAFile )
        {
            fResult = FALSE;
            goto exit_initcreationfromeas;
        }

        lResult = pEAFile->Initialize( m_pVolume, pIcb->IcbEA.Start );
        if( lResult != ERROR_SUCCESS )
        {
            SetLastError( lResult );
            fResult = FALSE;
            goto exit_initcreationfromeas;
        }

        pEAStream = pEAFile->GetMainStream();
        if( pEAStream->GetFileSize() < sizeof(NSR_EAH) +
                                       sizeof(NSR_EA_FILETIMES) +
                                       sizeof(TIMESTAMP)  )
        {
            fResult = FALSE;
            goto exit_initcreationfromeas;
        }

        lResult = pEAStream->ReadFile( 0,
                                       &Header,
                                       sizeof(Header),
                                       &dwBytesRead );
        if( lResult != ERROR_SUCCESS )
        {
            SetLastError( lResult );
            fResult = FALSE;
            goto exit_initcreationfromeas;
        }

        fResult = ValidateEAHeader( &Header,
                                    (DWORD)pEAStream->GetFileSize(),
                                    &dwEASize );
        if( !fResult )
        {
            goto exit_initcreationfromeas;
        }

        pBuffer = new BYTE[dwEASize];
        if( !pBuffer )
        {
            fResult = FALSE;
            goto exit_initcreationfromeas;
        }

        lResult = pEAStream->ReadFile( Header.EAImp,
                                       pBuffer,
                                       dwEASize,
                                       &dwBytesRead );
        if( lResult != ERROR_SUCCESS )
        {
            SetLastError( lResult );
            fResult = FALSE;
            goto exit_initcreationfromeas;
        }

        fResult = ExtractEATime( pBuffer, dwEASize );
    }

exit_initcreationfromeas:
    if( pBuffer )
    {
        delete [] pBuffer;
    }

    if( pEAFile )
    {
        delete pEAFile;
    }

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// ValidateEAHeader
//
BOOL CStream::ValidateEAHeader( PNSR_EAH pHeader,
                                DWORD dwEALength,
                                DWORD* pdwImpSize )
{
    BOOL fResult = TRUE;

    if( dwEALength < sizeof(NSR_EAH) )
    {
        SetLastError( ERROR_FILE_CORRUPT );
        fResult = FALSE;
        goto exit_validateeaheader;
    }

    if( !pHeader->EAImp )
    {
        *pdwImpSize = 0;
        goto exit_validateeaheader;
    }

    //
    // Make sure that the EAs start within the EA range.
    //
    if( pHeader->EAImp > dwEALength )
    {
        SetLastError( ERROR_FILE_CORRUPT );
        fResult = FALSE;
        goto exit_validateeaheader;
    }

    if( pHeader->EAImp < sizeof(NSR_EAH) )
    {
        SetLastError( ERROR_FILE_CORRUPT );
        fResult = FALSE;
        goto exit_validateeaheader;
    }

    if( pHeader->EAApp &&
        (pHeader->EAApp > dwEALength ||
         pHeader->EAApp < sizeof(NSR_EAH)) )
    {
        SetLastError( ERROR_FILE_CORRUPT );
        fResult = FALSE;
        goto exit_validateeaheader;
    }

    if( pHeader->EAApp == pHeader->EAImp )
    {
        SetLastError( ERROR_FILE_CORRUPT );
        fResult = FALSE;
        goto exit_validateeaheader;
    }

    //
    // Determine the amount of data to look through for the creation time
    // EA.
    //
    if( pHeader->EAApp && pHeader->EAApp < pHeader->EAImp )
    {
        *pdwImpSize = dwEALength - pHeader->EAImp;
    }
    else if( pHeader->EAApp )
    {
        *pdwImpSize = pHeader->EAApp - pHeader->EAImp;
    }
    else
    {
        *pdwImpSize = dwEALength - sizeof(NSR_EAH);
    }

    if( *pdwImpSize < sizeof(NSR_EA_GENERIC) && *pdwImpSize )
    {
        SetLastError( ERROR_FILE_CORRUPT );
        fResult = FALSE;
        goto exit_validateeaheader;
    }

exit_validateeaheader:

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// ExtractEATime
//
BOOL CStream::ExtractEATime( BYTE* pBuffer, DWORD dwEASize )
{
    BOOL fResult = TRUE;
    DWORD dwEAOffset = 0;
    PNSR_EA_GENERIC pGenEA = NULL;
    PNSR_EA_FILETIMES pTimeEA = NULL;

    while( dwEAOffset + sizeof(NSR_EA_GENERIC) < dwEASize )
    {
        pGenEA = (PNSR_EA_GENERIC)(pBuffer);

        dwEAOffset += pGenEA->EALength;
        if( pGenEA->EAType != EA_TYPE_FILETIMES )
        {
            continue;
        }

        if( pGenEA->EALength < sizeof(NSR_EA_FILETIMES) + sizeof(TIMESTAMP) )
        {
            SetLastError( ERROR_FILE_CORRUPT );
            fResult = FALSE;
            goto exit_extracteatime;
        }

        pTimeEA = (PNSR_EA_FILETIMES)pGenEA;
        if( pTimeEA->EASubType != 1 )
        {
            SetLastError( ERROR_FILE_CORRUPT );
            fResult = FALSE;
            goto exit_extracteatime;
        }

        if( (pTimeEA->Existence & 1) == 0 )
        {
            //
            // There is a file time but it is not the creation time.
            //
            continue;
        }

        UdfConvertUdfTimeToFileTime( (PTIMESTAMP)(pTimeEA + 1),
                                   &m_CreationTime );
        goto exit_extracteatime;
    }

    fResult = FALSE;

exit_extracteatime:

    return fResult;
}

// /////////////////////////////////////////////////////////////////////////////
// GetExtentInformation
//
// This will find the extent for a stream and return it's starting point and
// size.
//
LRESULT CStream::GetExtentInformation( Uint64 StreamOffset, PLONGAD pLongAd )
{
    LRESULT lResult = ERROR_SUCCESS;
    BOOL fUnlock = FALSE;
    CAllocExtent* pExtent = NULL;

    ASSERT( pLongAd != NULL );

    //
    // To make sure that we aren't changing the file size/allocation while we
    // are reading it - we must lock the file.
    //
    Lock();
    fUnlock = TRUE;

    //
    // Make sure we're not reading passed the end of the file.
    //
    if( StreamOffset > GetFileSize() )
    {
        goto exit_getextentinformatin;
    }

    lResult = m_RecordedSpace.GetExtent( StreamOffset, &pExtent );
    if( lResult == ERROR_SUCCESS )
    {
        ASSERT( pExtent != NULL );

        pLongAd->Start = pExtent->GetLocation();
        pLongAd->Length.Length = pExtent->GetNumBytes();
    }
    else
    {
        lResult = m_NotRecordedSpace.GetExtent( StreamOffset, &pExtent );
        if( lResult == ERROR_SUCCESS )
        {
            //
            // This is marked as allocated and not recorded.
            //
            pLongAd->Start = pExtent->GetLocation();
            pLongAd->Length.Length = pExtent->GetNumBytes();
        }
        else
        {
            //
            // There is no information on this extent, as it is a sparse area
            // in the file.
            //
            goto exit_getextentinformatin;
        }

    }

exit_getextentinformatin:
    if( fUnlock )
    {
        Unlock();
    }

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// Close
//
LRESULT CStream::Close()
{
    LRESULT lResult = ERROR_CALL_NOT_IMPLEMENTED;

    //
    // TODO::Do we need this for something?
    //

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// ReadFile
//
// Syncronization: The stream must be locked.
//     In order to ensure that the stream allocation is not changed while we're
//     trying to read the stream, we must lock it.
//
LRESULT CStream::ReadFile( Uint64 Position,
                           __out_bcount(Length) PVOID pData,
                           DWORD Length,
                           PDWORD pLengthRead )
{
    LRESULT lResult = ERROR_SUCCESS;
    BOOL fUnLock = FALSE;
    CAllocExtent* pExtent = NULL;
    Uint32 BuffIndx = 0;
    Uint64 FileSize = 0;

    ASSERT( pData != NULL );
    ASSERT( pLengthRead != NULL );

    //
    // Check for bad parameters.
    // TODO::These values should have been checked by now.  Validate and remove this code.
    //
    if( !pLengthRead || !pData )
    {
        return ERROR_INVALID_PARAMETER;
    }

    //
    // To make sure that we aren't changing the file size/allocation while we
    // are reading it - we must lock the file.
    //
    Lock();
    fUnLock = TRUE;

    //
    // Even though the file is locked we get prefast errors if we call
    // GetFileSize more than once.  This is a work around for that.
    //
    FileSize = GetFileSize();

    //
    // Make sure we're not reading passed the end of the file.
    //
    if( Position > FileSize )
    {
        *pLengthRead = 0;
        lResult = ERROR_HANDLE_EOF;
        goto exit_readfile;
    }

    //
    // Only read up to the end of the file.
    //
    if( FileSize - Position < Length )
    {
        Length = (Uint32)(FileSize - Position);
    }

    //
    // This one is easy.
    //
    if( Length == 0 )
    {
        *pLengthRead = 0;
        goto exit_readfile;
    }

    if( IsEmbedded() )
    {
        __try
        {
            CopyMemory( (BYTE*)pData, m_pEmbeddedData + Position, Length );
            *pLengthRead = Length;
        }
        __except( EXCEPTION_EXECUTE_HANDLER )
        {
            lResult = ERROR_INVALID_PARAMETER;
        }

        goto exit_readfile;
    }

    while( Length )
    {
        lResult = m_RecordedSpace.GetExtent( Position, &pExtent );
        if( lResult == ERROR_SUCCESS )
        {
            ASSERT(pExtent != NULL);

            //
            // We've found a mapping for this file offset, so we need to read
            // it from the disc.  Determine how many bytes to read from this
            // extent.
            //
            Uint32 BytesToRead = Length;
            Uint32 OffsetInExtent = (Uint32)(Position - pExtent->GetStreamOffset());
            NSRLBA LocationToRead;

            if( BytesToRead > (pExtent->GetNumBytes() - OffsetInExtent) )
            {
                BytesToRead = pExtent->GetNumBytes() - OffsetInExtent;
            }

            LocationToRead.Partition = pExtent->GetPartition();
            LocationToRead.Lbn = pExtent->GetBlock() +
                                 (OffsetInExtent >> m_pVolume->GetShiftCount());

            lResult = m_pVolume->GetPartition()->Read( LocationToRead,
                                                       OffsetInExtent & (m_pVolume->GetSectorSize() - 1),
                                                       BytesToRead,
                                                       &((BYTE*)pData)[BuffIndx] );
            if( lResult != ERROR_SUCCESS )
            {
                goto exit_readfile;
            }

            BuffIndx += BytesToRead;
            Length -= BytesToRead;
            Position += BytesToRead;
        }
        else
        {
            //
            // We must determine the size of this hole, thus knowing how much
            // we can read from it.
            //
            CAllocExtent* pTmpExtent = NULL;
            Uint64 SpaceToZero = 0;

            lResult = m_RecordedSpace.GetNextExtent( Position, &pTmpExtent );
            if( lResult == ERROR_SUCCESS )
            {
                //
                // We can zero data up to the stream offset reported by the
                // extent returned.
                //
                SpaceToZero = pTmpExtent->GetStreamOffset() - Position;
                if( SpaceToZero > Length )
                {
                    SpaceToZero = Length;
                }
            }
            else if( lResult == ERROR_SECTOR_NOT_FOUND )
            {
                //
                // This means that the rest of the file should be reported as
                // zeros.
                //
                SpaceToZero = Length;
            }
            else
            {
                goto exit_readfile;
            }

            ZeroMemory( &((BYTE*)pData)[BuffIndx], (Uint32)SpaceToZero );

            Length -= (Uint32)SpaceToZero;
            BuffIndx += (Uint32)SpaceToZero;
            Position += SpaceToZero;
        }

    }

exit_readfile:
    if( fUnLock )
    {
        Unlock();
    }

    if( !IsEmbedded() && lResult == ERROR_SUCCESS )
    {
        *pLengthRead = BuffIndx;
    }

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// ReadFileScatter
//
LRESULT CStream::ReadFileScatter( FILE_SEGMENT_ELEMENT SegmentArray[],
                                  DWORD NumberOfBytesToRead,
                                  FILE_SEGMENT_ELEMENT* OffsetArray,
                                  Uint64 Position )
{
    LRESULT lResult = ERROR_CALL_NOT_IMPLEMENTED;

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// WriteFile
//
LRESULT CStream::WriteFile( Uint64 Position,
                            PVOID pData,
                            DWORD Length,
                            PDWORD pLengthWritten )
{
    LRESULT lResult = ERROR_ACCESS_DENIED;

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// WriteFileGather
//
LRESULT CStream::WriteFileGather( FILE_SEGMENT_ELEMENT SegmentArray[],
                                  DWORD NumberOfBytesToWrite,
                                  FILE_SEGMENT_ELEMENT* OffsetArray,
                                  Uint64 Position )
{
    LRESULT lResult = ERROR_ACCESS_DENIED;

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// SetEndOfFile
//
LRESULT CStream::SetEndOfFile( Uint64 Position )
{
    LRESULT lResult = ERROR_ACCESS_DENIED;

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// GetFileInformation
//
LRESULT CStream::GetFileInformation( CFileLink* pLink,
                                     LPBY_HANDLE_FILE_INFORMATION pFileInfo )
{
    LRESULT lResult = ERROR_SUCCESS;
    CFile* pFile = NULL;

    ASSERT( pFileInfo != NULL );
    ASSERT( pLink != NULL );

    pFile = pLink->GetChild();

    pFileInfo->dwOID = 0;

    pFileInfo->dwFileAttributes = GetFileAttributes( pLink );

    //
    // TODO::Is there something we should use here?
    //
    pFileInfo->dwVolumeSerialNumber = 0;

    pFileInfo->ftCreationTime = m_CreationTime;
    pFileInfo->ftLastAccessTime = m_AccessTime;
    pFileInfo->ftLastWriteTime = m_ModifyTime;

    pFileInfo->nFileIndexHigh = (DWORD)(pFile->GetUniqueId() >> 32);
    pFileInfo->nFileIndexLow = (DWORD)(pFile->GetUniqueId() & 0xffffffff );

    if( pFile->IsDirectory() )
    {
        pFileInfo->nFileSizeHigh = 0;
        pFileInfo->nFileSizeLow = 0;
    }
    else
    {
        pFileInfo->nFileSizeHigh = (DWORD)(GetFileSize() >> 32);
        pFileInfo->nFileSizeLow = (DWORD)(GetFileSize() & 0xffffffff );
    }

    pFileInfo->nNumberOfLinks = m_LinkCount;

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// FlushFileBuffers
//
LRESULT CStream::FlushFileBuffers()
{
    LRESULT lResult = ERROR_ACCESS_DENIED;

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// GetFileTime
//
LRESULT CStream::GetFileTime( LPFILETIME pCreation,
                              LPFILETIME pLastAccess,
                              LPFILETIME pLastWrite )
{
    LRESULT lResult = ERROR_SUCCESS;

    CopyMemory( pCreation, &m_CreationTime, sizeof(FILETIME) );
    CopyMemory( pLastAccess, &m_AccessTime, sizeof(FILETIME) );
    CopyMemory( pLastWrite, &m_ModifyTime, sizeof(FILETIME) );

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// SetFileTime
//
LRESULT CStream::SetFileTime( const FILETIME* pCreation,
                              const FILETIME* pLastAccess,
                              const FILETIME* pLastWrite )
{
    LRESULT lResult = ERROR_ACCESS_DENIED;

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// GetFileAttributes
//
DWORD CStream::GetFileAttributes( CFileLink* pLink )
{
    DWORD dwAttributes = 0;

    ASSERT( pLink != NULL );

    if( pLink->IsHidden() )
    {
        dwAttributes |= FILE_ATTRIBUTE_HIDDEN;
    }

    if( m_pFile->IsDirectory() )
    {
        dwAttributes |= FILE_ATTRIBUTE_DIRECTORY;
    }

    if( m_IcbFlags & ICBTAG_F_ARCHIVE )
    {
        dwAttributes |= FILE_ATTRIBUTE_ARCHIVE;
    }

    if( dwAttributes == 0 )
    {
        dwAttributes |= FILE_ATTRIBUTE_NORMAL;
    }

    return dwAttributes;
}

// /////////////////////////////////////////////////////////////////////////////
// SetFileAttributes
//
LRESULT CStream::SetFileAttributes( DWORD Attributes )
{
    LRESULT lResult = ERROR_ACCESS_DENIED;

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// GetFileSize
//
ULONGLONG CStream::GetFileSize()
{
    return m_InformationLength;
}

// /////////////////////////////////////////////////////////////////////////////
// DeviceIoControl
//
LRESULT CStream::DeviceIoControl( PFILE_HANDLE pHandle,
                                  DWORD IoControlCode,
                                  LPVOID pInBuf,
                                  DWORD InBufSize,
                                  LPVOID pOutBuf,
                                  DWORD OutBufSize,
                                  LPDWORD pBytesReturned,
                                  LPOVERLAPPED pOverlapped )
{
    LRESULT lResult = ERROR_CALL_NOT_IMPLEMENTED;

    switch( IoControlCode )
    {
    case IOCTL_DVD_START_SESSION:
    case IOCTL_DVD_READ_KEY:
    case IOCTL_DVD_END_SESSION:
    case IOCTL_DVD_SEND_KEY:
    {
        //
        // We need to fill in the key's start address, which is used to get the
        // title key.  According to the MMC 5.0 Revision 2c spec:
        // "The Reserved/Logical Block Address field in the CDB specifies the
        // logical block address that contains the TITLE KEY to be sent to the
        // Host obfuscated by a Bus Key."
        //
        PDVD_COPY_PROTECT_KEY pKey = (PDVD_COPY_PROTECT_KEY)pInBuf;
        LONGAD LongAd;
        DWORD PhysicalBlock = 0;

        if( pHandle->Position > ULONG_MAX )
        {
            return ERROR_INVALID_PARAMETER;
        }

        __try
        {
            if( pKey )
            {
                //
                // We will get the logical block at the start of the file.
                //
                // Note: For some reason the old version of UDF was using the
                // byte index into the file according to the file handle.  I'm
                // not sure the reason for this, but we may have to come back
                // and look at this later if it's not working.
                //
                lResult = GetExtentInformation( 0, &LongAd );
                if( lResult != ERROR_SUCCESS )
                {
                    goto exit_deviceiocontrol;
                }

                //
                // Now we have to add the partition offset to the logical block.
                //
                PhysicalBlock = m_pVolume->GetPartitionStart( LongAd.Start.Partition );
                PhysicalBlock += LongAd.Start.Lbn;

                pKey->StartAddr += PhysicalBlock;

                //
                // Return call not implemented, so that IOCTL will be passed down
                // to block driver.
                //
                lResult = ERROR_CALL_NOT_IMPLEMENTED;
            }
        }
        __except( EXCEPTION_EXECUTE_HANDLER )
        {
            SetLastError( ERROR_INVALID_PARAMETER );
            return FALSE;
        }

        break;
    }

    }

exit_deviceiocontrol:

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// CopyFileExternal
//
LRESULT CStream::CopyFileExternal( PFILE_COPY_EXTERNAL pInCopyReq,
                                   LPVOID pOutBuf,
                                   DWORD OutBufSize )
{
    LRESULT lResult = ERROR_CALL_NOT_IMPLEMENTED;

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// ReadStreamAllocation
//
// Synchronization: Synchronized by the volume.
//      Since this will only happen when the volume is either being initialized
//      in the case of the root directory, or during a CreateFile call, the
//      volume should be locked to prevent the same file/stream from being
//      opened more than once.
//
// This will create a mapping of the file data to physical blocks.  There are
// actually two different mappings - one for recorded sectors, and one for
// unrecorded sectors.  Any part of the file not found in either mapping is
// a sparse hole in the file.
//
LRESULT CStream::ReadStreamAllocation()
{
    LRESULT lResult = ERROR_SUCCESS;
    Uint64 FileOffset = 0;
    PAED_LIST pAEDs = NULL;
    BYTE* pADs = NULL;

    ASSERT( IsEmbedded() == FALSE );

    if( GetAllocType() != ICBTAG_F_ALLOC_SHORT &&
        GetAllocType() != ICBTAG_F_ALLOC_LONG )
    {
        //
        // We should only reach here if the file is not embedded.
        //
        DEBUGMSG(ZONE_ERROR,(TEXT("UDFS!CStream.ReadStreamAllocation Unknown allocation type found.\n")));
        lResult = ERROR_FILE_CORRUPT;
        goto exit_ReadStreamAllocation;
    }

    if( m_InformationLength == 0 )
    {
        //
        // Our job is very easy if there's nothing to read in.
        //
        goto exit_ReadStreamAllocation;
    }

    //
    // Parse all of the allocation descriptors in embedded in the ICB itself.
    //
    lResult = ParseADBlock( m_pEmbeddedData, m_ADLength, &FileOffset );
    if( lResult != ERROR_SUCCESS )
    {
        goto exit_ReadStreamAllocation;
    }

    //
    // Go through any/all allocation extent descriptors.  Note that the list
    // of AEDs can grow each time through the loop.
    //
    pAEDs = m_pAEDs;
    pADs = new (UDF_TAG_BYTE_BUFFER) BYTE[ m_pVolume->GetSectorSize() ];
    if( !pADs )
    {
        lResult = ERROR_NOT_ENOUGH_MEMORY;
        goto exit_ReadStreamAllocation;
    }

    while( pAEDs )
    {
        ZeroMemory( pADs, m_pVolume->GetSectorSize() );

        //
        // Read in the AED extent.
        //
        lResult = m_pVolume->GetPartition()->Read( pAEDs->Location,
                                                   0,
                                                   m_pVolume->GetSectorSize(),
                                                   pADs );
        if( lResult != ERROR_SUCCESS )
        {
            goto exit_ReadStreamAllocation;
        }

        lResult = ParseADBlock( pADs, m_pVolume->GetSectorSize(), &FileOffset );
        if( lResult != ERROR_SUCCESS )
        {
            goto exit_ReadStreamAllocation;
        }

        pAEDs = pAEDs->pNext;
    }

exit_ReadStreamAllocation:
    if( pADs )
    {
        delete [] pADs;
    }

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// ParseADBlock
//
// This function takes a buffer full of allocation descriptors and parses them.
//
LRESULT CStream::ParseADBlock( __in_ecount(BufferSize) BYTE* pBuffer,
                               Uint32 BufferSize,
                               Uint64* pFileOffset )
{
    LRESULT lResult = ERROR_SUCCESS;
    Uint64 FileBytes = 0;
    Uint32 AdOffset = 0;
    Uint32 AllocSize = sizeof(SHORTAD);
    NSRLBA Location;
    NSRLENGTH Length;

    ASSERT( pBuffer != NULL );
    ASSERT( pFileOffset != NULL );

    if( GetAllocType() == ICBTAG_F_ALLOC_LONG )
    {
        AllocSize = sizeof(LONGAD);
    }

    while( AdOffset <= BufferSize - AllocSize )
    {
        if( GetAllocType() == ICBTAG_F_ALLOC_SHORT )
        {
            PSHORTAD pShortAd = (PSHORTAD)&pBuffer[AdOffset];

            Location.Lbn = pShortAd->Start;
            Location.Partition = m_PartitionIndx;
            Length = pShortAd->Length;
        }
        else
        {
            PLONGAD pLongAd = (PLONGAD)&pBuffer[AdOffset];

            Location = pLongAd->Start;
            Length = pLongAd->Length;
        }

        lResult = AddExtent( *pFileOffset, Location, Length );
        if( lResult != ERROR_SUCCESS )
        {
            break;
        }

        if( Length.Type != NSRLENGTH_TYPE_CONTINUATION )
        {
            *pFileOffset += Length.Length;
            AdOffset += AllocSize;
        }
        else
        {
            //
            // An AED AD marks the end of this run, whether there is more data
            // or not.
            //
            break;
        }
    }

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// AddAED
//
// Syncronization: This should be syncronized at the stream level.
//     The stream should be locked upon entry to this method except when calling
//     during the volume mount time on the root directory.
//
LRESULT CStream::AddAED( const NSRLBA& Location )
{
    LRESULT lResult = ERROR_SUCCESS;

    PAED_LIST pAED = new (UDF_TAG_AED_LIST) AED_LIST;

    if( !pAED )
    {
        lResult = ERROR_NOT_ENOUGH_MEMORY;
    }
    else
    {
        ZeroMemory( pAED, sizeof( AED_LIST ) );
        pAED->Location = Location;

        if( m_pAEDs == NULL )
        {
            m_pAEDs = pAED;
        }
        else
        {
            PAED_LIST pCurrent = m_pAEDs;
            while( pCurrent->pNext )
            {
                pCurrent = pCurrent->pNext;
            }

            pCurrent->pNext = pAED;
        }
    }

    return lResult;
}

// /////////////////////////////////////////////////////////////////////////////
// AddExtent
//
// Syncronization: This should be syncronized at the stream level.
//     The stream should be locked upon entry to this method except when calling
//     during the volume mount time on the root directory.
//
LRESULT CStream::AddExtent( Uint64 Offset,
                            const NSRLBA& Location,
                            const NSRLENGTH& Length )
{
    LRESULT lResult = ERROR_SUCCESS;

    switch( Length.Type )
    {
    case NSRLENGTH_TYPE_RECORDED:
    {
        lResult = m_RecordedSpace.AddExtent( Offset,
                                             Length.Length,
                                             Location.Lbn,
                                             Location.Partition );
    }
    break;

    case NSRLENGTH_TYPE_UNRECORDED:
    {
        if( m_pFile->IsDirectory() )
        {
            if( Offset < m_InformationLength )
            {
                lResult = ERROR_DISK_CORRUPT;
                break;
            }
        }

        lResult = m_NotRecordedSpace.AddExtent( Offset,
                                                Length.Length,
                                                Location.Lbn,
                                                Location.Partition );
    }
    break;

    case NSRLENGTH_TYPE_UNALLOCATED:
    {
        if( m_pFile->IsDirectory() )
        {
            lResult = ERROR_DISK_CORRUPT;
            break;
        }
    }
    break;

    case NSRLENGTH_TYPE_CONTINUATION:
    {
        lResult = AddAED( Location );
    }
    break;

    default:
        lResult = ERROR_FILE_CORRUPT;
        break;
    }

    return lResult;
}

