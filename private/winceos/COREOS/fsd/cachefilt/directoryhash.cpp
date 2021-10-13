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
#include "Cachefilt.hpp"
#include "DirectoryHash.hpp"

DirectoryHash_t::DirectoryHash_t(CachedVolume_t *pVolume, BOOL IsRootVolume):
    m_pHashMaps(NULL),
    m_Flags(0),
    m_pVolume(pVolume)
{
    if (!IsRootVolume)
    {
        m_Flags = CACHE_VOLUME_DO_NOT_HASH;
    }
}

DirectoryHash_t::~DirectoryHash_t()
{
    DeleteHashMaps(m_pHashMaps);
    m_pHashMaps = NULL;
}

/* Convert the file name into a DWORD. */
ULONGLONG DirectoryHash_t::HashFileName( LPCWSTR pFileName )
{
    ULONGLONG ullHash = 0;
    size_t Length = 0;
    ULONGLONG ullMultiplier = 1;

    if( FAILED(StringCchLength( pFileName, MAX_PATH, &Length )) )
    {
        goto exit_computehash;
    }

    for( size_t x = 0; x < Length; x++ )
    {
        BYTE* pBytes = (BYTE*)&pFileName[x];

        // If the character is not a standard 0-9, a-z, A-Z then force a lookup
        // as we cannot easily standardize the file name.
        if( IsNonAsciiChar( pBytes ) )
        {
            ullHash = 0;
            goto exit_computehash;
        }

        // For lowercase a-z, use upcase value.
        if( IsLowercaseAsciiChar( pBytes ) )
        {
            ullHash += ullMultiplier * (pBytes[0] - 32);
        }
        else
        {
            ullHash += ullMultiplier * pBytes[0];
        }

        ullMultiplier *= CACHE_HASH_PRIME_NUMBER;
    }

    exit_computehash:

    return ullHash;
}

/* Hash all the paths specified for this volume. Only "\Windows" for now. */
BOOL DirectoryHash_t::HashVolume()
{
    BOOL fResult = FALSE;
    HashMap_t* pHashMap = NULL;

    if( (m_Flags & CACHE_VOLUME_HASHED) ||
        (m_Flags & CACHE_VOLUME_DO_NOT_HASH) )
    {
        fResult = TRUE;
        goto exit_hashvolume;
    }

    if( !CreateHashMap( &pHashMap, L"\\Windows" ) )
    {
        goto exit_hashvolume;
    }

    if( !HashPath( pHashMap ) )
    {
        goto exit_hashvolume;
    }

    // This is to prevent multiple threads from trying to create the hash map.
    // Granted, if this happens then we've had multiple threads run through the
    // whole enumeration process, but this shouldn't happen because the cache
    // manager is initialized and used so early that there aren't multiple
    // threads accessing it.  So we optimize for the most common case and avoid
    // using a critical section.
    if( InterlockedCompareExchangePointer( &m_pHashMaps, pHashMap, NULL ) == NULL )
    {
        m_Flags = m_Flags | CACHE_VOLUME_HASHED;
        pHashMap = NULL; // m_pHashMaps owns this pointer now.
    }

    fResult = TRUE;

exit_hashvolume:
    // Always delete pHashMap if we still own it.
    if( pHashMap )
    {
        DeleteHashMaps(pHashMap);
    }

    return fResult;
}

/* Enumerate through the files in the given directory and add to the hash map. */
BOOL DirectoryHash_t::HashPath( HashMap_t* pHashMap )
{
    BOOL fResult = FALSE;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    TCHAR strSearch[MAX_PATH] = { 0 };
    WIN32_FIND_DATA FindData = { 0 };

    VERIFY(SUCCEEDED(StringCchCopy( strSearch, MAX_PATH, pHashMap->pMapPath )));

    if( pHashMap->pMapPath[ pHashMap->usPathLength - 1] == L'\\' )
    {
        if( FAILED(StringCchCat( strSearch, MAX_PATH, _T("*") )) )
        {
            // Do not keep trying to hash this, the path will never work.
            m_Flags = m_Flags | CACHE_VOLUME_DO_NOT_HASH;
            goto exit_hashpath;
        }
    }
    else
    {
        if( FAILED(StringCchCat( strSearch, MAX_PATH, _T("\\*") )) )
        {
            // Do not keep trying to hash this, the path will never work.
            m_Flags = m_Flags | CACHE_VOLUME_DO_NOT_HASH;
            goto exit_hashpath;
        }
    }

    // We need to use the Internal function here to avoid infinite recursion
    // or a deadlock.
    hFind = FindFirstFileInternal (
                        (PVOLUME)m_pVolume,
                        GetCurrentProcess (),
                        strSearch,
                        &FindData);

    if (INVALID_HANDLE_VALUE == hFind)
    {
        //
        // The no more files error seems obvious... there aren't any files there
        // yet.  We still want to hash later.
        //
        // The path not found case is because the cache manager will sometimes
        // be loaded and execute before FAT is ready to handle this call.  We'll
        // just have to try again later.  We return FALSE so that we try to hash
        // again later.
        //
        if( GetLastError() == ERROR_NO_MORE_FILES )
        {
            fResult = TRUE;
        }
        else if( GetLastError() != ERROR_PATH_NOT_FOUND )
        {
            m_Flags = m_Flags | CACHE_VOLUME_DO_NOT_HASH;
        }

        goto exit_hashpath;
    }

    do
    {
        if( !AddFileNameToHashMap( pHashMap, FindData.cFileName ) )
        {
            // Unless this file were deleted, this will never work.
            m_Flags = m_Flags | CACHE_VOLUME_DO_NOT_HASH;
            goto exit_hashpath;
        }

    } while( FCFILT_FindNextFileW( (PSEARCH)hFind, &FindData ) );

    if( GetLastError() != ERROR_NO_MORE_FILES )
    {
        m_Flags = m_Flags | CACHE_VOLUME_DO_NOT_HASH;
    }
    else
    {
        fResult = TRUE;
    }

exit_hashpath:
    if( hFind != INVALID_HANDLE_VALUE )
    {
        VERIFY (FCFILT_FindClose ((PSEARCH)hFind));
    }

    return fResult;
}

/* Determine if the file possibly exists based on the hash map. */
BOOL DirectoryHash_t::FileMightExist( LPCWSTR pFilePath, DWORD* pdwError )
{
    BOOL fResult = TRUE;
    size_t Length = 0;
    wchar_t pInternalPath[MAX_PATH] = { 0 };
    wchar_t* pFile = NULL;
    wchar_t* pTokens[MAX_PATH/2] = { NULL };
    DWORD dwTokens = 0;
    DWORD dwError = ERROR_FILE_NOT_FOUND;

    if( !m_pHashMaps || (m_Flags & CACHE_VOLUME_DO_NOT_HASH) )
    {
        // Short circuit all of this if no directories are hashed.
        goto exit_filemightexist;
    }

    if( FAILED(StringCchLength( pFilePath, MAX_PATH, &Length )) )
    {
        // Just to be extra careful... we aren't supporting file names larger than
        // MAX_PATH, doesn't mean they don't exist.
        goto exit_filemightexist;
    }

    // Check for any character in the path that would make us not index the path or
    // file and bail out immediately if we find one.
    for( size_t x = 0; x < Length; x++ )
    {
        BYTE* pBytes = (BYTE*)&pFilePath[x];

        // If the character is not a standard 0-9, a-z, A-Z then force a lookup
        // as we cannot easily standardize the file name.
        if( IsNonAsciiChar( pBytes ) )
        {
            goto exit_filemightexist;
        }

        // For lowercase a-z, use upcase value.
        if( IsLowercaseAsciiChar( pBytes ))
        {
            pInternalPath[x] = pFilePath[x] - 32;
        }
        else
        {
            pInternalPath[x] = pFilePath[x];
        }

        // We keep track of the location for the file name for later.
        if( pInternalPath[x] == L'\\' )
        {
            //
            // We should never end in a slash unless the root directory.  In this case
            // there is no file anyways.
            //
            // pFile = &pInternalPath[x];
            pTokens[dwTokens++] = &pInternalPath[x];
        }
    }

    //
    // There could be a map for any directory in this path.  Check at each directory
    // if the path has a map associated with it.  If it does, then check the following
    // name to see if it could exist in the path.
    //
    for( DWORD x = 0; x < dwTokens; x++ )
    {
        HashMap_t* pMap = m_pHashMaps;

        pFile = pTokens[x];
        pFile[0] = 0;
        pFile += 1;

        VERIFY(SUCCEEDED(StringCchLength( pInternalPath, MAX_PATH, &Length )));

        while( pMap )
        {
            if( Length == pMap->usPathLength )
            {
                // If the paths match then we'll check with this map to see if the
                // file name might exist.
                if( memcmp( pMap->pMapPath, pInternalPath, Length ) == 0 )
                {
                    break;
                }
            }

            pMap = pMap->pNext;
        }

        if( pMap )
        {
            WCHAR* pNextToken = NULL;
            if( x < dwTokens - 1 )
            {
                pNextToken = pTokens[x+1];
                pNextToken[0] = 0;
            }

            ULONGLONG ullHash = HashFileName( pFile );
            DWORD dwValue = (DWORD)(ullHash % (pMap->dwArraySizeInBytes * 8));

            DWORD dwByte = dwValue / 8;
            BYTE Mask = 1 << (dwValue %8);

            if( !(pMap->pArray[dwByte] & Mask) )
            {
                if( pNextToken && pdwError )
                {
                    dwError = ERROR_PATH_NOT_FOUND;
                }
                fResult = FALSE;
                goto exit_filemightexist;
            }

            if( pNextToken )
            {
                pNextToken[0] = L'\\';
            }
        }

        pFile -= 1;
        pFile[0] = L'\\';
    }

exit_filemightexist:

    if( !fResult )
    {
        if( pdwError )
        {
            *pdwError = dwError;
        }
    }

    return fResult;
}

/* Hash the file name and set the bit in the hash map. */
BOOL DirectoryHash_t::AddFileNameToHashMap( HashMap_t* pHashMap, LPCWSTR pFileName )
{
    ULONGLONG ullHash = HashFileName( pFileName );
    DWORD dwValue = (DWORD)(ullHash % ((ULONGLONG)pHashMap->dwArraySizeInBytes << 3));

    DWORD dwByte = dwValue / 8;
    BYTE Mask = 1 << (dwValue % 8);

    pHashMap->pArray[dwByte] = pHashMap->pArray[dwByte] | Mask;

    return TRUE;
}

/* Adds a file to a hash map if the path to the file is in a hash map. */
BOOL DirectoryHash_t::AddFilePathToHashMap( LPCWSTR pFilePath )
{
    BOOL fResult = TRUE;
    size_t Length = 0;
    wchar_t pInternalPath[MAX_PATH] = { 0 };
    wchar_t* pFile = NULL;
    HashMap_t* pMap = m_pHashMaps;

    if( !pMap ||
        !(m_Flags & CACHE_VOLUME_HASHED) ||
        (m_Flags & CACHE_VOLUME_DO_NOT_HASH) )
    {
        // Short circuit all of this if no directories are hashed, the volume
        // isn't being hashed, or there was a hashing error somewhere.
        goto exit_addfilepathtohashmap;
    }

    if( FAILED(StringCchLength( pFilePath, MAX_PATH, &Length )) )
    {
        // This path is too long and we just won't add it.
        goto exit_addfilepathtohashmap;
    }

    // Check for any character in the path that would make us not index the path or
    // file and bail out immediately if we find one.
    for( size_t x = 0; x < Length; x++ )
    {
        BYTE* pBytes = (BYTE*)&pFilePath[x];

        // If the character is not a standard 0-9, a-z, A-Z then force a lookup
        // as we cannot easily standardize the file name.
        if( IsNonAsciiChar( pBytes ) )
        {
            goto exit_addfilepathtohashmap;
        }

        // For lowercase a-z, use upcase value.
        if( IsLowercaseAsciiChar( pBytes ) )
        {
            pInternalPath[x] = pFilePath[x] - 32;
        }
        else
        {
            pInternalPath[x] = pFilePath[x];
        }

        // We keep track of the location for the file name for later.
        if( pInternalPath[x] == L'\\' )
        {
            //
            // We should never end in a slash unless the root directory.  In this case
            // there is no file anyways.
            //
            pFile = &pInternalPath[x];
        }
    }

    if( !pFile )
    {
        goto exit_addfilepathtohashmap;
    }
    else
    {
        //
        // Zero out the backslash before the file name so we can access the path and
        // file name separately.
        //
        pFile[0] = 0;
        pFile += 1;
    }

    VERIFY(SUCCEEDED(StringCchLength( pInternalPath, MAX_PATH, &Length )));

    while( pMap )
    {
        if( Length == pMap->usPathLength )
        {
            // If the paths match then we'll check with this map to see if the
            // file name might exist.
            if( memcmp( pMap->pMapPath, pInternalPath, Length ) == 0 )
            {
                fResult = AddFileNameToHashMap( pMap, pFile );
                break;
            }
        }

        pMap = pMap->pNext;
    }

exit_addfilepathtohashmap:

    return fResult;
}

/* Allocate and initialize a hash map. */
BOOL  DirectoryHash_t::CreateHashMap( HashMap_t** ppHashMap, LPCWSTR pPath )
{
    HashMap_t* pNewMap = new HashMap_t;
    size_t Length = 0;
    BOOL fResult = FALSE;

    if( !pNewMap )
    {
        goto exit_createhashmap;
    }

    // Create primary array - to be deleted.
    // Do not search subdirectories.
    ZeroMemory( pNewMap, sizeof(HashMap_t) );
    pNewMap->dwArraySizeInBytes = CACHE_DEFAULT_HASH_MAP_BYTES;
    pNewMap->dwMapFlags = HASH_ARRAY_PRIMARY;

    pNewMap->pArray = new BYTE[pNewMap->dwArraySizeInBytes];
    if( !pNewMap->pArray )
    {
        goto exit_createhashmap;
    }

    ZeroMemory( pNewMap->pArray, pNewMap->dwArraySizeInBytes );

    if( FAILED(StringCchLength( pPath, MAX_PATH, &Length )) )
    {
        goto exit_createhashmap;
    }

    pNewMap->pMapPath = new wchar_t[Length + 1];
    if( !pNewMap->pMapPath )
    {
        goto exit_createhashmap;
    }

    if( FAILED(StringCchCopy( pNewMap->pMapPath, Length + 1, pPath )) )
    {
        goto exit_createhashmap;
    }

    pNewMap->usPathLength = (USHORT)Length;

    for( size_t x = 0; x < Length; x++ )
    {
        BYTE* pBytes = (BYTE*)&pNewMap->pMapPath[x];

        // We only support standard English names in the path for now.
        if( IsNonAsciiChar( pBytes ) )
        {
            goto exit_createhashmap;
        }

        // For lowercase a-z, use upcase value.
        if( IsLowercaseAsciiChar( pBytes ) )
        {
            pBytes[0] -= 32;
        }
    }

    fResult = TRUE;
    *ppHashMap = pNewMap;

exit_createhashmap:
    if( !fResult )
    {
        if( pNewMap )
        {
            if( pNewMap->pArray )
            {
                delete [] pNewMap->pArray;
            }

            if( pNewMap->pMapPath )
            {
                delete [] pNewMap->pMapPath;
            }

            delete pNewMap;
        }
    }

    return fResult;
}

/* Deallocate all hash maps. */
void DirectoryHash_t::DeleteHashMaps(HashMap_t* pHashMap)
{
    while( pHashMap )
    {
        HashMap_t* pMap = pHashMap;
        pHashMap = pHashMap->pNext;

        if( pMap->dwMapFlags & HASH_ARRAY_PRIMARY )
        {
            ASSERT( pMap->pArray );
            delete [] pMap->pArray;
        }

        ASSERT( pMap->pMapPath );
        delete [] pMap->pMapPath;

        delete pMap;
    }
}



