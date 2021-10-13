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

#ifndef __DIRECTORYHASH_HPP_INCLUDED__
#define __DIRECTORYHASH_HPP_INCLUDED__

#define CACHE_HASH_PRIME_NUMBER           (          107)  // 107 and 101 seemed to produce best results in testing.
#define CACHE_DEFAULT_HASH_MAP_BYTES      (       4*1024)  // 4KB

// NOTE: We may want to share one array across multiple paths, or have individual arrays.  This
// gives us the flexibility to do both.
typedef struct _HASH_MAP {
    DWORD dwArraySizeInBytes:20;   // The number of bytes in the hash array.
    DWORD dwMapFlags:12;           // Additonal information about this hash map.

    BYTE* pArray;                  // Each bit represents the possible existence of a file name.
    LPWSTR pMapPath;               // The path for this mapping.
    USHORT usPathLength;           // The number of characters in the path.

    struct _HASH_MAP* pNext;       // The next hash map.
} HashMap_t;

#define HASH_ARRAY_PRIMARY          (0x01)          // This map should be deleted, it is primary
#define HASH_ARRAY_SHARED           (0x02)          // Do not delete this map - it is shared
#define HASH_SUBDIRECTORIES         (0x04)          // Hash all of this path's subdirectories. - Not currently supported.

#define CACHE_VOLUME_HASHED         0x0000001       // If set, volume already hashed Windows directory.  Else not.
#define CACHE_VOLUME_DO_NOT_HASH    0x0000002       // If set, volume should not try to use hasing.  Else, use hashing.

class CachedVolume_t;

class DirectoryHash_t
{
    // The following sections contain performance additions to hash the file
    // names in a directory.  We want to quickly identify files that don't
    // exist.
    //
    // Note that the following is written to be extensible for future versions
    // where more directories may be hashed.
public:

    DirectoryHash_t(CachedVolume_t *pVolume, BOOL IsRootVolume);
    ~DirectoryHash_t();

    // Create hash maps for all directories to be hashed.
    BOOL  HashVolume();

    // Adds a file to a hash map if the path to the file is in a hash map.
    BOOL  AddFilePathToHashMap( LPCWSTR pFilePath );

    // Determine if the file possibly exists based on the hash map.
    BOOL  FileMightExist( LPCWSTR pFilePath, DWORD* pdwError = NULL );

protected:
    // Convert the file name into a numeric representation.
    ULONGLONG HashFileName( LPCWSTR pFileName );

    // Enumerate through the files in the given directory and add to the hash map.
    BOOL  HashPath( HashMap_t* pHashMap );

    // Hash the file name and set the bit in the hash map.
    BOOL  AddFileNameToHashMap( HashMap_t* pHashMap, LPCWSTR pFileName );

    // Allocate and initialize a hash map.
    BOOL  CreateHashMap( HashMap_t** ppHashMap, LPCWSTR pPath );

    // Deallocate all hash maps.
    void  DeleteHashMaps( HashMap_t* pHashMap );  

private:
    // Each bit represents the possible existence of a hashed file name.
    HashMap_t*      m_pHashMaps;                         
    
    CachedVolume_t* m_pVolume;
    DWORD           m_Flags;
};

inline BOOL IsNonAsciiChar( BYTE const* const pBytes )
{
    // If the high byte is not zero, this isn't English.
    // If the low byte isn't between 48 and 126, then we won't try to hash this.
    // We won't try to hash wildcards '*' or '?'.
    if( pBytes[1] != 0 || pBytes[0] < 32 || pBytes[0] > 126 || pBytes[0] == L'?' || pBytes[0] == L'*' )
    {
        return TRUE;
    }

    return FALSE;
}

inline BOOL IsLowercaseAsciiChar( BYTE const* const pBytes )
{
    if( pBytes[0] >= 97 && pBytes[0] <= 122 )
    {
        return TRUE;
    }

    return FALSE;
}


#endif // __DIRECTORYHASH_HPP_INCLUDED__
