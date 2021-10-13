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

#ifndef UNITTEST_CACHEFILT
#include "cachefilt.hpp"
#include "FileMapCollection.hpp"
#endif


FileMapCollection::FileMapCollection() :
    m_unreferencedMapCount(0),
    m_totalMapCount(0),
    m_pfnCalculateHash(NULL),
    m_collectionGeneration(1),
    m_pLock(NULL)
{
}

FileMapCollection::~FileMapCollection()
{
    CloseAllMapFiles();
    m_mapCollection.clear();
}

VOID FileMapCollection::Lock()
{
    if (m_pLock != NULL)
    {
        EnterCriticalSection(m_pLock);
    }
}

VOID FileMapCollection::Unlock()
{
    if (m_pLock != NULL)
    {
        LeaveCriticalSection(m_pLock);
    }
}

VOID FileMapCollection::Initialize(__in CRITICAL_SECTION *pLock, __in PFNCalculateFileNameHash pfnHashCalculator)
{
    m_pLock = pLock;
    m_pfnCalculateHash = pfnHashCalculator;
}

ULONGLONG FileMapCollection::CalculateFileNameHash(__in_z LPCWSTR wszFileName)
{
    ULONGLONG hash = 0;

    if (m_pfnCalculateHash != NULL)
    {
        hash = m_pfnCalculateHash(wszFileName, FALSE);
    }

    return hash;
}


ULONGLONG FileMapCollection::CalculateFileMapHash(__in FSSharedFileMap_t* pMap)
{
    return CalculateFileNameHash(pMap->GetName());
}

DWORD FileMapCollection::GetCollectionGeneration()
{
    return m_collectionGeneration;
}

DWORD FileMapCollection::GetTotalCount()
{
    return m_totalMapCount;
}

LONG FileMapCollection::GetUnreferencedFileMapCount()
{
    return m_unreferencedMapCount;
}

VOID FileMapCollection::IncrementUnreferencedMapCount()
{
    InterlockedIncrement(&m_unreferencedMapCount);
}

VOID FileMapCollection::DecrementUnreferencedMapCount()
{
    InterlockedDecrement(&m_unreferencedMapCount);
}

BOOL FileMapCollection::AddMap(__in FSSharedFileMap_t* pMap)
{
    return AddMap(CalculateFileMapHash(pMap), pMap);
}

BOOL FileMapCollection::AddMap(const ULONGLONG hashValue, __in_opt FSSharedFileMap_t* pMap)
{
    BOOL mapAdded = FALSE;

    Lock();
    
    FileMapHashTable::iterator collectionIterator;

    //lookup the hash in the collection
    collectionIterator = m_mapCollection.find(hashValue);

    //if we cannot find a bucket for that hash value then add it
    if (collectionIterator == m_mapCollection.end())
    {
        //new bucket, we need to add a bucket to the collection
        FileMapBucketList bucketList;       

        //add the map to the bucket
        if (bucketList.push_back(pMap))
        {
            //then insert the bucket into the collection.
            if (m_mapCollection.insert(hashValue, bucketList) != m_mapCollection.end())
            {
                mapAdded = TRUE;
            }
        }
    }
    else
    {
        //if a bucket already exists then we have a hash collision, insert the map into the existing bucket 
        
        FileMapBucketList *pBucketList = &(collectionIterator->second);
        FileMapBucketList::iterator bucketIterator = pBucketList->begin();

        if ((*bucketIterator) == NULL)
        {
            //if the first element of the bucket is an empty slot, then use it (instead of adding a new one)
            *bucketIterator = pMap;
            mapAdded = TRUE;
        }
        else
        {
            //push the new map into the bucket
            mapAdded = pBucketList->push_front(pMap);
        }        
    }

    if (mapAdded)
    {
        //if we inserted the map successfully and the map is not NULL then increment the counters
        if (pMap != NULL)
        {
            m_totalMapCount++;
            IncrementUnreferencedMapCount();
            
            //mark the inserted map with the current collection generation number.
            //The generation number is used during re-hashing updates due to ::UpdateMapDirectory.
            pMap->SetCollectionGeneration(GetCollectionGeneration());

            //Adding elements to the collection does not invalidate interleaved iteators (used in CloseAllMaps and WriteBackAllMaps)
            //since it doesn't invalidate them, we don't update the collection generation number on adds.

            DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:FileMapCollection::AddMap, FileMapCollection=0x%08x pMap=0x%08x\r\n",
                      this, pMap)); 
        }
        else
        {
            //if the map is NULL we are simply reserving a slot in a bucket that will be used later
            //(this is used when we need to ensure we will be able to add a map to the hash table
            DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:FileMapCollection::AddMap, FileMapCollection=0x%08x Reserving slot in hash %I64u\r\n",
                      this, hashValue)) ;          
        }
    }
    else
    {
        //we must be completely out of memory for this to happen.
        DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:FileMapCollection::AddMap, Failed Add for FileMapCollection=0x%08x pMap=0x%08x\r\n",
                            this, pMap));    
    }

    Unlock();
    
    return mapAdded;
}

BOOL FileMapCollection::RemoveMap(__in FSSharedFileMap_t* pMap)
{
    BOOL mapRemoved = FALSE;

    Lock();

    ASSERT(pMap != NULL);
    
    //calculate the hash for the map we are removing.
    const ULONGLONG hashValue = CalculateFileMapHash(pMap);
    
    FileMapHashTable::iterator  collectionIterator;
    FileMapBucketList::iterator bucketListIterator;

    //find the hash bucket first
    collectionIterator = m_mapCollection.find(hashValue);

    if (collectionIterator != m_mapCollection.end())
    {
        //then find the exact map in the bucket     
        FileMapBucketList *pBucketList = &(collectionIterator->second);
        
        for (bucketListIterator = pBucketList->begin();
             (bucketListIterator != pBucketList->end());
             bucketListIterator++)
        {
            if (*bucketListIterator == pMap)
            {
                //found an exact pointer match for the map.
                //we will remove it from the collection
                mapRemoved = TRUE;

                //adjust the counters
                m_totalMapCount--;
                DecrementUnreferencedMapCount();
                
                DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:FileMapCollection::RemoveMap, FileMapCollection=0x%08x pMap=0x%08x\r\n",
                                        this, pMap));

                //erase the map from the bucket and erase the bucket from the table if it becomes empty
                bucketListIterator = pBucketList->erase(bucketListIterator);
                if (pBucketList->begin() == pBucketList->end())
                {
                    m_mapCollection.erase(collectionIterator);
                }

                //erasing elements from the collection implies a generation update. (as removing elements
                //invalidates interleaved iterators). Update the generation
                m_collectionGeneration++;
                break;
            }
        }
    }

    Unlock();
    
    return mapRemoved;
}

FSSharedFileMap_t* FileMapCollection::FindMap(__in_z LPCWSTR wszFileName,const BOOL lockMapIfFound)
{
    FSSharedFileMap_t* pMap = NULL;
    
    FileMapHashTable::iterator  collectionIterator;
    FileMapBucketList::iterator bucketListIterator;

    Lock();
    
    //find the hash bucket first
    collectionIterator = m_mapCollection.find(CalculateFileNameHash(wszFileName));

    if (collectionIterator != m_mapCollection.end())
    {
        //then find the exact map in the bucket
        for (bucketListIterator = (collectionIterator->second).begin();
             (bucketListIterator != (collectionIterator->second).end());
             bucketListIterator++)
        {
            pMap = *bucketListIterator;

            if ((pMap != NULL) && pMap->MatchName(wszFileName))
            {
                //found the exact map, lock it if requested and return it.
                if (lockMapIfFound && !pMap->IncUseCount())
                {
                    // Map exists but we couldn't add a reference
                    DEBUGCHK(0);  // Probably a leak
                    SetLastError (ERROR_TOO_MANY_OPEN_FILES);
                    pMap = NULL;
                }
                break;
            }
            else
            {
                pMap = NULL;
            }
        }
    }

    Unlock();

    return pMap;
}

BOOL FileMapCollection::UpdateMapDirectory(__in_z LPCWSTR wszSourceName, __in_z LPCWSTR wszDestinationName)
{
    BOOL fResult = TRUE;

    //The function will traverse the entire collection looking for elements
    //that need to be updated. The update itself invalidates the hash value
    //for the element so it needs to be "removed" from the collection and re-inserted
    //with the appropriate hash.

    Lock();
    
    FileMapHashTable::iterator  collectionIterator;

    //Move the collection generation forward, this helps us knowing which elements
    //we have already updated (as they can get re-inserted forward and we will find them
    //again while iterating)
    m_collectionGeneration++;

    //for each entry in the table
    for (collectionIterator = m_mapCollection.begin();
         collectionIterator != m_mapCollection.end();
         collectionIterator++)
    {
        FileMapBucketList* pBucketList = &(collectionIterator->second);
        FileMapBucketList::iterator bucketListIterator;

        //for each element in each bucket
        for (bucketListIterator = pBucketList->begin();
             bucketListIterator != pBucketList->end();
             bucketListIterator++)
        {
            FSSharedFileMap_t* pMap = *bucketListIterator;

            //check if we need to update the map. (verify we haven't done it already on this pass
            //by comparing the collection generations)
            if ((pMap != NULL) && (pMap->GetCollectionGeneration() != m_collectionGeneration))
            {
                //Tell the map to create the updated name, if it returns a name (not NULL) then the map needs to be updated.
                //if it returns NULL then it means it doesn't need to be updated or that it can't be updated (OOM) anyway
                LPWSTR wszUpdatedName = pMap->CreateNameForDirectoryUpdate(wszSourceName, wszDestinationName);
                
                if (wszUpdatedName != NULL)
                {
                    //if the map was updated then we need to re-insert it into the collection (as it needs a new hash).
                    //We want to make sure the re-insert will always succeed before updating the map. (Reserve the bucket for it)
                    if (AddMap(CalculateFileNameHash(wszUpdatedName), NULL))
                    {
                       //remove the map. We simply null out the current location in the collection and adjust the counters
                       //then we add it back using AddMap as usual.
                       //we are not altering the collection, simply null the element out. Nulled slots are reused or trimmed
                       //later
                       *bucketListIterator = NULL;
                       m_totalMapCount--;
                       DecrementUnreferencedMapCount();
       
                       //insert the map back (this will insert it into the right hash bucket)
                       pMap->SetName(wszUpdatedName);
                       VERIFY(AddMap(pMap));
                    }
                    else
                    {
                       //we won't update the map if we cannot ensure we can re-insert it
                       delete wszUpdatedName;
                       fResult = FALSE;
                    }
                }
                else
                {
                    fResult = FALSE;
                }
            }
        }
    }

    Unlock();

    return fResult;
}

VOID FileMapCollection::CloseUnreferencedMapFiles(const size_t numberOfUnreferencedMapsToClose)
{
    //call close on unreferenced maps
    CloseMultipleMapFiles(TRUE, numberOfUnreferencedMapsToClose);
}

VOID FileMapCollection::CloseAllMapFiles()
{
    //call close on all maps
    CloseMultipleMapFiles(FALSE, INFINITE);
}

VOID FileMapCollection::CloseMultipleMapFiles(const BOOL unreferencedOnly, const size_t numberOfMapsToClose)
{
    //This function is implemented as an "interleaved" iterator.
    //The function will walk the collection protected by the lock but will leave
    //the lock while executing the map Close.
    //the function will not restart from the beginning of the collection unless
    //the collection structual properties has been altered (collection generation mismatch)
    //from outside this function
    
    BOOL doneTraversing = FALSE;
    DWORD traversalGeneration = 0;
    size_t closedMaps = 0;
    
    FileMapHashTable::iterator  collectionIterator;
    FileMapBucketList::iterator bucketListIterator;
    FileMapBucketList* pBucketList = NULL;
    
    while(!doneTraversing && (closedMaps < numberOfMapsToClose))
    {
        FSSharedFileMap_t *pMap = NULL;
        
        //Lock while we look for the get the next map to close
        Lock();

        if (traversalGeneration != m_collectionGeneration)
        {
            //If the collection generation has changed or this is the first iteration
            //then start from the beginning      
            collectionIterator = m_mapCollection.begin();
            bucketListIterator = NULL;

            DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:FileMapCollection::CloseMultipleMapFiles, Collection traversal restarted. Generation %u -> %u\r\n",
                                    traversalGeneration, m_collectionGeneration));
            
            traversalGeneration = m_collectionGeneration;
        }

        //if we are not done traversing all buckets
        while (collectionIterator != m_mapCollection.end())
        {
            //iterate the current bucket
            pBucketList = &(collectionIterator->second);
            pMap = NULL;
    
            if (bucketListIterator == NULL)
            { 
                //if we haven't started iterating the bucket then start from the beginning
                bucketListIterator = pBucketList->begin();
            }
    
            while (bucketListIterator != pBucketList->end())
            {
                //if we haven' reached the end of the current bucket
                //then keep taking elements out of it and moving forward

                pMap = *bucketListIterator;

                if (pMap != NULL)
                {
                    if (!unreferencedOnly || (!pMap->AreHandlesInMap() && (pMap->GetUseCount() == 0)))
                    {
                        //if it is OK to close the map then
                        //increment the use count as we will leave the lock and use the map. The call
                        //to CloseMap will decrement it.
                        pMap->IncUseCount();
                        break;
                    }
                    else
                    {
                        //Clear the map pointer, is not ready to be closed and move to the next element
                        pMap = NULL;
                        bucketListIterator++;
                    }
                }
                else
                {
                    //adjust the collection generation (counters don't need to be updated because we are not removing a map)
                    //we are removing an empty slot in the collection.
                    m_collectionGeneration++;
                    traversalGeneration = m_collectionGeneration;

                    //erase the empty slot.
                    bucketListIterator = pBucketList->erase(bucketListIterator);
                }
            }

            if (bucketListIterator == pBucketList->end())
            {
                //we reached the end of a bucket, check if the bucket itself is empty (if begin == end)
                if (bucketListIterator == pBucketList->begin())
                {
                    //if it is, remove it from the collection and reflect that on the collection generation.
                    collectionIterator = m_mapCollection.erase(collectionIterator);
                    m_collectionGeneration++;
                    traversalGeneration = m_collectionGeneration;
                }
                else
                {
                    //if not, then just move to the next bucket
                    collectionIterator++;
                }

                //set the iterator to null so we start from the beginning on the next bucket
                bucketListIterator = NULL;
            }

            if (pMap != NULL)
            {
                //if we have a map ready to close then break out of the loop to close it
                //outside of the lock
                break;
            }
        }
        
        Unlock();

        //if we have a map at this point, it means we want to close it.
        if (pMap != NULL)
        {
            // Flush the map outside of the volume.
            pMap->FlushMap();

            Lock();

            // Make sure that the collection has not changed, that there
            // are still no handles in the map, and the use count is still 1.
            if ((traversalGeneration == m_collectionGeneration) && 
                !pMap->AreHandlesInMap() && (pMap->GetUseCount() == 1)) 
            {                
                //Adjust the collection counters and the generation
                m_totalMapCount--;
                DecrementUnreferencedMapCount();
                m_collectionGeneration++;
                traversalGeneration = m_collectionGeneration;                       

                //erase the element from the bucket
                bucketListIterator = pBucketList->erase(bucketListIterator);

                //CloseMap will decrement the use count
                FSSharedFileMap_t::CloseMap(pMap, TRUE);
                closedMaps++;
            }
            else
            {
                pMap->DecUseCount();
                DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:FileMapCollection::CloseMultipleMapFiles, Skipping map 0x%08x (%u, %u, %u).\r\n",
                                        pMap, traversalGeneration, m_collectionGeneration, pMap->AreHandlesInMap()));
            }
            
            Unlock();
        }
        else
        {
            //we reached the end of the collection, we are done
            doneTraversing = TRUE;
        }
    }
}

BOOL FileMapCollection::WriteBackAllMaps(BOOL fStopOnIO, BOOL fForceFlush)
{
    //This function is implemented as an "interleaved" iterator.
    //The function will walk the collection protected by the lock but will leave
    //the lock while executing the map WriteBack.
    //the function will not restart from the beginning of the collection unless
    //the collection structual properties has been altered (collection generation mismatch)
    //from outside this function

    BOOL fResult = TRUE;

    BOOL doneTraversing = FALSE;
    DWORD traversalGeneration = 0;
    FileMapHashTable::iterator  collectionIterator;
    FileMapBucketList::iterator bucketListIterator;

    while(!doneTraversing)
    {
        FSSharedFileMap_t *pMap = NULL;

        Lock();

        if (traversalGeneration != m_collectionGeneration)
        {
            //If the collection generation has changed or this is the first iteration
            //then start from the beginning      
            collectionIterator = m_mapCollection.begin();
            bucketListIterator = NULL;

            DEBUGMSG (ZONE_VOLUME, (L"CACHEFILT:FileMapCollection::WriteBackAllMaps, Collection traversal restarted. Generation %u -> %u\r\n",
                                    traversalGeneration, m_collectionGeneration));
            
            traversalGeneration = m_collectionGeneration;
        }

        //if we are not done traversing all buckets
        if (collectionIterator != m_mapCollection.end())
        {
            //iterate the current bucket
            FileMapBucketList *pBucketList = &(collectionIterator->second);
    
            if (bucketListIterator == NULL)
            { 
                //if we haven't started iterating the bucket then start from the beginning
                bucketListIterator = pBucketList->begin();
            }
    
            if (bucketListIterator != pBucketList->end())
            {
                //if we haven' reached the end of the current bucket
                //then keep taking elements out of it and moving forward

                pMap = *bucketListIterator;

                if (pMap && (pMap->IsDirty() || fForceFlush))
                {
                    //increment the use count as we will leave the lock and use the map.
                    //it will be decremented below adter the WriteBackMap call
                    pMap->IncUseCount();
                }
                else
                {
                    //set the map to null to skip writing back
                    pMap = NULL;
                }                    

                //if we are not ready to remove the entry, then just move to the next entry in the bucket
                bucketListIterator++;

            }
            else
            {   
                //just move to the next bucket
                //set the iterator to null so we start from the beginning on the new bucket
                collectionIterator++;
                bucketListIterator = NULL;
            }

        }
        else
        {
            //we reached the end of the collection, we are done
            doneTraversing = TRUE;
        }
        
        Unlock();

        //if we have a map at this point, it means we want to write it.
        //do so while holding the write lock if one was provided by the caller.
        if (pMap != NULL)
        {
            fResult = pMap->WriteBackMap (fStopOnIO);

            //decrement the map use count after the write call (to match the increment done above)
            pMap->DecUseCount();            

            // the write back operation was cancelled due to I/O, so break.
            if (fStopOnIO && !fResult) 
            {
                break;
            }
        }
    }

    return fResult;
}
