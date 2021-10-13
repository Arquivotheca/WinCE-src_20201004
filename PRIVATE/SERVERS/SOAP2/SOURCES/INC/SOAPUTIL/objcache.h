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
//      objcache.h
//
// Contents:
//
//      SOAP SDK utility class
//
//----------------------------------------------------------------------------------

#ifndef __OBJCACHE_H_INCLUDED__
#define __OBJCACHE_H_INCLUDED__

class CObjCacheDoubleListEntry : public CDoubleListEntry
{
public:
    CObjCacheDoubleListEntry(void * pvData): m_pvData(pvData) {  };
    ~CObjCacheDoubleListEntry() { };
    inline void *getData() { return m_pvData; };
private:
    void   *m_pvData;
};


// Return codes from PFNOBJECTOK
typedef enum {
    OBJ_OK = 0,
    OBJ_ACCESS_DENIED = 1,
    OBJ_STALE = 2,
    OBJ_NOTFOUND = 3,
} OBJ_STATE;

typedef OBJ_STATE ( *PFNOBJECTOK) (void *pvData);
typedef char * ( *PFNGETKEY) (void *pvData);

// This object has no synchronization. The user needs to provide
// appropriate synchronization if multi-threaded access is needed. 
class CObjCache : public CStrHashTable
{
public:
    CObjCache::CObjCache();
    CObjCache(ULONG ulMax, ULONG cid,
                PFNOBJECTOK pfnObjectOK, 
                PDESTROYFUNC pfnDestroyFunc,
                PFNGETKEY pfnGetKey);     
    ~CObjCache();
    HRESULT Insert(char * m_pszKey, void * pvData);
    OBJ_STATE Find (char * m_pszKey, void **ppvData);
    void    Delete(char * pszKey);
    void    DeleteAll();
    
    // Used in the case of an AV. Cannot delete memory since it may be corrupted.
    //  We have to live with memory leak in case of an AV.
    void    ClearCache();

protected:
    virtual void DestroyData(void * pvData)
    {
        CObjCacheDoubleListEntry *DblEntry = (CObjCacheDoubleListEntry *)pvData;
        if (m_pfnDeleteObj)
            m_pfnDeleteObj(DblEntry->getData());
        delete DblEntry;    
    }
    
private:
    ULONG           m_cMax;     // Max no of items to keep
    ULONG           m_cCur;     // No. of items in the cache
    PDESTROYFUNC    m_pfnDeleteObj; // Function used to delete object
    PFNOBJECTOK     m_pfnObjectOK;   // Function called to validate object state
    PFNGETKEY       m_pfnGetKey;    // Function that returns the object's key
    CDoubleList     m_LRUObj;   // LRU/MRU list
    
    // Cache statistics, Write these out to trace file 
    ULONG   m_CacheID;  // Cache identifier
    ULONG   m_cFound;  // No. of times we found an item in the cache
    ULONG   m_cBumped;  // No. of times we bumped an item from LRU end
    ULONG   m_cObjStale; // No. of times we found an object invalid    
    ULONG   m_cAccessDenied;    // No. of times we got access denied
};

#endif  // __OBJCACHE_H_INCLUDED__
