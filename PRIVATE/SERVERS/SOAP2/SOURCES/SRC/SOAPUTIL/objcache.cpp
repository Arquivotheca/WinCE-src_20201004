//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//+---------------------------------------------------------------------------------
//
//
// File:
//      objcache.cpp
//
// Contents:
//
//      CObjCache class implementation
//
//----------------------------------------------------------------------------------

#include "Headers.h"

#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CObjCache::CObjCache()
//
//  parameters:
//
//  description:
//          CObjCache constructor
//      Use this constructor only with objects that do not need deleting.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjCache::CObjCache()
: m_cCur(0),
  m_cFound(0),
  m_cBumped(0),
  m_cObjStale(0),
  m_cAccessDenied(0)
{
    // No maximum, no delete/validate/getkey functions
    m_cMax = 0;
    m_CacheID = 0;
    m_pfnObjectOK = NULL;
    m_pfnDeleteObj = NULL;  // Object is not deleted if this function is NULL
    m_pfnGetKey = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CObjCache::CObjCache()
//
//  parameters:
//
//  description:
//          CObjCache constructor
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjCache:: CObjCache(
        ULONG ulMax,        // Max no of elements kept in cache, 0 if no limit
        ULONG cid,          // Cache id, used for tracing
        PFNOBJECTOK pfnObjectOK, //Callback method that returns info on the state of object
        PDESTROYFUNC pfnDestroyFunc, // Delete method for the object
        PFNGETKEY pfnGetKey) :  // Returns the hash key for the object
        m_cCur(0), m_cFound(0), m_cBumped(0), m_cObjStale(0), m_cAccessDenied(0)
{
    m_cMax = ulMax;
    m_CacheID = cid;
    m_pfnObjectOK = pfnObjectOK;
    m_pfnDeleteObj = pfnDestroyFunc;
    m_pfnGetKey = pfnGetKey;

    // If there is a cMax, then we must have a m_pfnGetKey
    if (m_cMax != 0)
        ASSERT(m_pfnGetKey);

}

////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CObjCache::~CObjCache()
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
CObjCache::~CObjCache()
{
    ;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CObjCache::Insert
//
//  parameters:
//
//  description:
//      Inserts an entry to the object cache, removing the LRU object
//      if the cache is full.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
HRESULT CObjCache::Insert(char * pszKey, void * pvData)
{
    HRESULT     hr = S_OK;
    CObjCacheDoubleListEntry *DblEntry = NULL;
    char *pszKey2;

    if (m_cMax && (m_cCur >= m_cMax))
    {
        // Remove an item from the LRU end
        DblEntry = (CObjCacheDoubleListEntry *) m_LRUObj.getTail();
        ASSERT(DblEntry);
        ASSERT(m_pfnGetKey);
        pszKey2 = m_pfnGetKey(DblEntry->getData());
        Delete(pszKey2);
        m_cBumped++;
        if ((m_cBumped % 32) == 0)
        {
            TRACEL((1, "CacheID: %d, Cache hits: %d, bumped from cache: %d, found stale: %d, access denied: %d\n",
                m_CacheID, m_cFound, m_cBumped, m_cObjStale, m_cAccessDenied));
        }
    }

    DblEntry = new CObjCacheDoubleListEntry(pvData);
    CHK_BOOL(DblEntry, E_OUTOFMEMORY);

    CHK(CStrHashTable::Insert(pszKey, (void *)DblEntry));

    m_LRUObj.InsertHead((CDoubleListEntry *)DblEntry);
    m_cCur++;
Cleanup:
    return hr;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CObjCache::Find()
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
OBJ_STATE CObjCache::Find(char * pszKey, void **ppvData)
{
    CObjCacheDoubleListEntry *DblEntry = NULL;
    void *pvData;
    OBJ_STATE  objstate = OBJ_OK;

    *ppvData = NULL;

    DblEntry = (CObjCacheDoubleListEntry *) CStrHashTable::Find(pszKey);
    if (!DblEntry)
        return OBJ_NOTFOUND;    // Not found

    pvData = DblEntry->getData();
    if (m_pfnObjectOK)
        objstate = m_pfnObjectOK(pvData);

    switch (objstate)
    {
    case OBJ_OK:
        // Move the object to MRU end and return it
        m_LRUObj.RemoveEntry((CDoubleListEntry *)DblEntry);
        m_LRUObj.InsertHead((CDoubleListEntry *)DblEntry);
        m_cFound++;
        if ((m_cFound % 1024) == 0)
        {
            TRACEL((1, "CacheID: %d, Cache hits: %d, bumped from cache: %d, found stale: %d, access denied: %d\n",
                m_CacheID, m_cFound, m_cBumped, m_cObjStale, m_cAccessDenied));
        }
        *ppvData = pvData;
        break;
    case OBJ_ACCESS_DENIED:
        // Return Error
        m_cAccessDenied++;
        if ((m_cAccessDenied % 32) == 0)
        {
            TRACEL((1, "CacheID: %d, Cache hits: %d, bumped from cache: %d, found stale: %d, access denied: %d\n",
                m_CacheID, m_cFound, m_cBumped, m_cObjStale, m_cAccessDenied));
        }
        break;
    case OBJ_STALE:
        // Object no longer valid. Free it and return
        Delete(pszKey);
        m_cObjStale++;
        if ((m_cObjStale % 32) == 0)
        {
            TRACEL((1, "CacheID: %d, Cache hits: %d, bumped from cache: %d, found stale: %d, access denied: %d\n",
                m_CacheID, m_cFound, m_cBumped, m_cObjStale, m_cAccessDenied));
        }
        break;
    default:
        return OBJ_NOTFOUND;    // Unidentified remark
    }

    return objstate;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CObjCache::Delete()
//
//  parameters:
//
//  description:
//
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////
void    CObjCache::Delete(char * pszKey)
{
    CObjCacheDoubleListEntry *DblEntry = NULL;

    DblEntry = (CObjCacheDoubleListEntry *) CStrHashTable::Find(pszKey);
    if (DblEntry)
    {
        m_LRUObj.RemoveEntry((CDoubleListEntry *)DblEntry);
        ASSERT(DblEntry);
        CStrHashTable::Delete(pszKey); // Calls back to DestroyData
        m_cCur--;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CObjCache::DeleteAll()
//
//  parameters:
//
//  description:
//      Deletes all the elements in the cache
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void    CObjCache::DeleteAll()
{
    CObjCacheDoubleListEntry *DblEntry = NULL;

    TRACEL((1, "Object cache terminated: CacheID: %d, Cache hits: %d, bumped from cache: %d, found stale: %d, access denied: %d\n",
              m_CacheID, m_cFound, m_cBumped, m_cObjStale, m_cAccessDenied));
    while(!m_LRUObj.IsEmpty())
    {
        DblEntry = (CObjCacheDoubleListEntry *)m_LRUObj.getHead();
        ASSERT(DblEntry);
        m_LRUObj.RemoveEntry((CDoubleListEntry *)DblEntry);
    }
    CStrHashTable::DeleteAll(); // Calls back to DestroyData for each entry
    m_cCur = 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////
//  function: CObjCache::ClearCache()
//
//  parameters:
//
//  description:
//      Clears the object cache class without freeing any memory
//      Used in the case of an AV. Cannot delete memory since it may be corrupted.
//          We have to live with memory leak in case of an AV.
//  returns:
//
////////////////////////////////////////////////////////////////////////////////////////////////////

void    CObjCache::ClearCache()
{
    CStrHashTable::ClearHashTable();
    m_LRUObj.ClearList();
    m_cCur = 0;
}

