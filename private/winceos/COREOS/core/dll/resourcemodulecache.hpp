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
#define _OLEAUT32_
#include <hash_map.hxx>
#include <ehm.h>

    //
    // Module Cache Storage
    //

class ResourceModuleCache
{
    template<class T>
    class StringHash
    {
        typedef ce::hash_map<ce::wstring, T> HASHTYPE;
        HASHTYPE m_vStringCache;
        T m_number;
    public:
        StringHash(size_t nBuckets = 32):m_vStringCache(nBuckets),m_number(0){};
        // return 0 if the key is not found
        T GetID(LPCWSTR pwszKey)
        {
            T tRn = 0;

            ce::hash_map<ce::wstring, T>::iterator it = m_vStringCache.find(pwszKey);
            if (m_vStringCache.end() != it)
            {
                tRn  = it->second;
            }
            return tRn;
        };

        // return 0 if adding key is failed
        T SetID(LPCWSTR pwszKey)
        {
            T tRn = 0;

            if (m_vStringCache.end() != m_vStringCache.insert(pwszKey, m_number+1))
            {
                m_number++;
                tRn = m_number;
            }
            return tRn;
        };

    };
    template <size_t Increment>
    class StringVector
    {
        ce::vector<ce::wstring, ce::allocator, ce::incremental_growth<Increment>> m_vStringCache;
    public:
        // return 0 if the key is not found
        WORD GetID(LPCWSTR pwszKey)
        {
            WORD wRn = 0;
            for(WORD i = 0 ; i < m_vStringCache.size(); i++)
            {
                if (m_vStringCache[i] == pwszKey)
                {
                    wRn = i+1;
                    break;
                }
            }
            return wRn;
        };
        // return 0 if adding key is failed
        WORD SetID(LPCWSTR pwszKey)
        {
            WORD wRn = 0;
            if (m_vStringCache.push_back(pwszKey))
            {
                wRn = (WORD) m_vStringCache.size();
            }
            return wRn;
        };

    };

typedef ULONG64 CacheKey;
typedef ce::hash_map<CacheKey, DWORD> ResourceModuleHash;
private:
    StringHash<WORD>        m_vModuleBaseCache;
    StringHash<WORD>        m_vNameCache;
    StringVector<4>         m_vTypeCache;
    ResourceModuleHash      m_vModuleCache;
    CRITICAL_SECTION        m_csReadWriteCache;

private:
    void AcquireLock();
    void ReleaseLock();
    CacheKey  GetKey(LPCWSTR pwszModuleBase, LPCWSTR pwszName, LPCWSTR pwszType);
    CacheKey  SetKey(LPCWSTR pwszModuleBase, LPCWSTR pwszName, LPCWSTR pwszType);
public:
    ResourceModuleCache();
    ~ResourceModuleCache();

    virtual BOOL Find(
    __in_z LPCWSTR pwszModuleBase, 
    __in_z LPCWSTR pwszName, 
    __in_z LPCWSTR pwszType, 
    __out  DWORD *pdwFallbackStepID
        );

    virtual BOOL AddToCache(
    __in_z LPCWSTR pwszModuleBase, 
    __in_z LPCWSTR pwszName, 
    __in_z LPCWSTR pwszType, 
    __in   DWORD   dwFallbackStepID
        );

};

