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

#include "ResourceModuleCache.hpp"

ResourceModuleCache::ResourceModuleCache():m_vModuleBaseCache(8)
{
    InitializeCriticalSection(&m_csReadWriteCache);
}


ResourceModuleCache::~ResourceModuleCache()
{
    DeleteCriticalSection(&m_csReadWriteCache);
}
void ResourceModuleCache::AcquireLock()
{
    EnterCriticalSection(&m_csReadWriteCache);
}

void ResourceModuleCache::ReleaseLock()
{
    LeaveCriticalSection(&m_csReadWriteCache);
}
/// <summary>
/// Get the key from cache table.  
/// </summary>
/// <param name="pwszModuleName">The module name.</param>
/// <param name="pwszName">The resource name </param>
/// <param name="pwszType">The resource type </param>
/// <returns>Returns key if id is found, or 0 if not.</returns>
//  63                            31             15             0
//  +--------------+--------------+--------------+--------------+ 
//  |Resoues Name ID              |Type Name ID  |Module Name ID| 
//  +--------------+--------------+--------------+--------------+ 
ResourceModuleCache::CacheKey  ResourceModuleCache::GetKey(LPCWSTR pwszModuleBase, LPCWSTR pwszName, LPCWSTR pwszType)
{
    HRESULT hr = S_OK;
    WORD  wModuleBaseID;
    DWORD dwNameID = 0;
    WORD  wTypeID = 0;
    
    wModuleBaseID = m_vModuleBaseCache.GetID(pwszModuleBase);
    CBR(wModuleBaseID != 0);
    dwNameID =(DWORD)pwszName;
    //if pwszName has been a number,store it in the high 16 bits, otherwise store it's id in the low 16 bits 
    if(dwNameID & 0xFFFF0000)
    {
        dwNameID = m_vNameCache.GetID(pwszName) & 0x0000FFFF  ;
        CBR(dwNameID != 0);
    }
    else
    {
        dwNameID <<= 16;
    }
    wTypeID = m_vTypeCache.GetID(pwszType);
    CBR(wTypeID != 0);
Error:
    if(hr != S_OK)
    {
        return 0;
    }
    return (ULONG64)(((WORD)wModuleBaseID) | (((DWORD)wTypeID) << 16) | (((ULONG64)dwNameID)  << 32));
}
/// <summary>
/// Set the key from cache table.  
/// </summary>
/// <param name="pwszModuleName">The module name.</param>
/// <param name="pwszName">The resource name </param>
/// <param name="pwszType">The resource type </param>
/// <returns>Returns key if succeed, or 0 if not.</returns>
ResourceModuleCache::CacheKey  ResourceModuleCache::SetKey(LPCWSTR pwszModuleBase, LPCWSTR pwszName, LPCWSTR pwszType)
{
    HRESULT hr = S_OK;
    WORD  wModuleBaseID;
    DWORD dwNameID=0;
    WORD  wTypeID=0;
    
    wModuleBaseID = m_vModuleBaseCache.GetID(pwszModuleBase);
    if (wModuleBaseID == 0)
    {
        wModuleBaseID = m_vModuleBaseCache.SetID(pwszModuleBase);
    }
    CBR(wModuleBaseID != 0);
    dwNameID =(DWORD)pwszName;
    //if pwszName has been a number,store it in the high 16 bits, otherwise store it's id in the low 16 bits 
    if(dwNameID & 0xFFFF0000)
    {
        dwNameID = m_vNameCache.GetID(pwszName);
        if (dwNameID == 0)
        {
            dwNameID = m_vNameCache.SetID(pwszName);
        }
        dwNameID &= 0x0000FFFF;
        CBR(dwNameID != 0);
    }
    else
    {
        dwNameID <<= 16;
    }
    wTypeID = m_vTypeCache.GetID(pwszType);
    if (wTypeID == 0)
    {
        wTypeID = m_vTypeCache.SetID(pwszType);
    }
    CBR(wTypeID != 0);
Error:
    if(hr != S_OK)
    {
        return 0;
    }
    return (ULONG64)(((WORD)wModuleBaseID) | (((DWORD)wTypeID) << 16) | (((ULONG64)dwNameID)  << 32));
}


/// <summary>
/// look up fallback step ID from the cache table.  
/// </summary>
/// <param name="pwszModuleName">The module name.</param>
/// <param name="pwszName">The resource name </param>
/// <param name="pwszType">The resource type </param>
/// <param name="pdwFallbackStepID">
/// Address of a variable that receives the fallback step ID, in DWORD.
/// </param>
/// <returns>Returns TRUE if id is found, or FALSEif not.</returns>
BOOL ResourceModuleCache::Find(
    __in_z LPCWSTR pwszModuleBase, 
    __in_z LPCWSTR pwszName, 
    __in_z LPCWSTR pwszType, 
    __out  DWORD *pdwFallbackStepID
)
{
    HRESULT hr = S_OK;
    CacheKey key;
    ResourceModuleHash::iterator it;

    AcquireLock();
    key = GetKey(pwszModuleBase, pwszName, pwszType);
    CBR(key != 0);
    it = m_vModuleCache.find(key);
    CBR(m_vModuleCache.end() != it);
    *pdwFallbackStepID = it->second;
Error:
    ReleaseLock();
    return SUCCEEDED(hr);


}
/// <summary>
/// store fallback step ID to the cache table.
/// </summary>
/// <param name="pwszModuleName">The module name.</param>
/// <param name="pwszName">The resource name </param>
/// <param name="pwszType">The resource type </param>
/// <param name="dwFallbackStepID">The fallback step ID </param>
/// <returns>Returns TRUE if id is found, or FALSEif not.</returns>
BOOL ResourceModuleCache::AddToCache(
    __in_z LPCWSTR pwszModuleBase, 
    __in_z LPCWSTR pwszName, 
    __in_z LPCWSTR pwszType, 
    __in   DWORD   dwFallbackStepID
)
{
    HRESULT hr = S_OK;
    CacheKey key;

    AcquireLock();
    key = SetKey(pwszModuleBase, pwszName, pwszType);
    CBR(key != 0);
    CBR(m_vModuleCache.end() != m_vModuleCache.insert(key,dwFallbackStepID));
Error:
    ReleaseLock();
    return SUCCEEDED(hr);
}
