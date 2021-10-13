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

#pragma once

#ifndef CACHEFILT_FILEMAPCOLLECTION
#define CACHEFILT_FILEMAPCOLLECTION

#include <windows.h>
#include <hash_map.hxx>
#include <list.hxx>


typedef ce::list<FSSharedFileMap_t*> FileMapBucketList;
typedef ce::hash_map<ULONGLONG, FileMapBucketList> FileMapHashTable;

typedef ULONGLONG (*PFNCalculateFileNameHash)(__in_z LPCWSTR wszFileName, const BOOL strictlyAscii);

///<summary>Represents a collection of map files</summary>
///<remarks>
///   Class is thread safe if a lock is provided during initialization.
///   The collection works using an internal hash table with collision handling.
///</remarks>
class FileMapCollection
{
    private:

        ///<summary>Internal hash table that holds the maps</summary>
        FileMapHashTable m_mapCollection;

        ///<summary>total maps currently stored the collection</summary>
        volatile LONG m_totalMapCount;

        ///<summary>total unreferenced maps currently stored in the collection. (always a subset of m_totalMapCount)</summary>
        volatile LONG m_unreferencedMapCount;

        ///<summary>
        ///  control value used to indicate when the collection structural properties has been altered in a way
        ///  that invalidates any iterators if the lock is released during the iterations.
        ///</summary>
        DWORD m_collectionGeneration;

        ///<summary>Lock used to protect the collection. The lock is provided during Initialization</summary>
        CRITICAL_SECTION   *m_pLock;

        ///<summary>Hash function to use to calculate the hash value for the map names as they get inserted and searched in the collection</summary>
        PFNCalculateFileNameHash m_pfnCalculateHash;

    public:
        ///<summary>total unreferenced maps currently stored in the collection. (always a subset of m_totalMapCount)</summary>
        FileMapCollection();

        ///<summary>total unreferenced maps currently stored in the collection. (always a subset of m_totalMapCount)</summary>
       ~FileMapCollection();

        ///<summary>Initializes the object with a lock and the hash calculation function to use</summary>
        ///<remarks>Objects should always be initialized after instantiated</remarks>
        VOID Initialize(__in CRITICAL_SECTION *pLock, __in PFNCalculateFileNameHash pfnHashCalculator);

    public:

        BOOL AddMap(__in FSSharedFileMap_t* pMap);
        BOOL AddMap(const ULONGLONG hashValue, __in_opt FSSharedFileMap_t* pMap);
        BOOL RemoveMap(__in FSSharedFileMap_t* pMap);
        
        FSSharedFileMap_t* FindMap(__in_z LPCWSTR wszFileName, const BOOL lockMapIfFound);

        BOOL UpdateMapDirectory(__in_z LPCWSTR wszSourceName, __in_z LPCWSTR wszDestinationName);

        VOID CloseUnreferencedMapFiles(const size_t numberOfUnreferencedMapsToClose = INFINITE);
        VOID CloseAllMapFiles();
        
        BOOL WriteBackAllMaps(const BOOL fStopOnIO = FALSE, BOOL fForceFlush = FALSE);
        
    public:
        ULONGLONG CalculateFileNameHash(__in_z LPCWSTR wszFileName);
        ULONGLONG CalculateFileMapHash(__in FSSharedFileMap_t* pMap);

    public:
        DWORD GetCollectionGeneration();
        DWORD GetTotalCount();
        LONG  GetUnreferencedFileMapCount();
        VOID  IncrementUnreferencedMapCount();
        VOID  DecrementUnreferencedMapCount();

    protected:
        VOID Lock();
        VOID Unlock();
        VOID CloseMultipleMapFiles(const BOOL unreferencedOnly, const size_t numberOfMapsToClose);

    private:
    
        ///<summary>Prevent use of shallow copy</summary>
        FileMapCollection& operator =(const FileMapCollection&);
        
};

#ifdef UNITTEST_CACHEFILT
#include "FileMapCollection.cpp"
#endif

#endif

