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


#ifndef _THREAD_SAFE_MAP_H
#define _THREAD_SAFE_MAP_H

#include <windows.h>
#include <map>
#include <string>

typedef std::basic_string<TCHAR, 
  std::char_traits<TCHAR>, std::allocator<TCHAR> > 
  tstring;

template <typename T_Key, typename T_Item>
class ThreadSafeMap
{
public:
    typedef BOOL (*TravelCallBack)(const T_Key &, T_Item &, PVOID, BOOL *);
    ThreadSafeMap(LPCTSTR tszName);
    ThreadSafeMap(const ThreadSafeMap &MapCopy);
    ~ThreadSafeMap();
    BOOL Add(const T_Key &Key, const T_Item &Item);
    DWORD Size() const {return m_ItemMap.size();}
    BOOL Get(const T_Key &Key, T_Item &Item) const;
    BOOL Get(DWORD dwIndex, T_Key &Key, T_Item &Item) const;
    BOOL Set(const T_Key &Key, T_Item &Item);
    BOOL Erase(const T_Key &Key);
    BOOL Exist(const T_Key &Key) const;
    VOID Clear();
    BOOL Travel(DWORD dwStart, DWORD dwEnd, TravelCallBack pFuncPtr, PVOID pParam = NULL);
    void Lock(LPCTSTR tszOwner = NULL);
    void UnLock(LPCTSTR tszOwner = NULL);
private:    
    tstring m_strName;
    CRITICAL_SECTION m_Lock;
    std::map<T_Key, T_Item> m_ItemMap;
};

template <typename T_Key, typename T_Item>
void ThreadSafeMap<T_Key, T_Item>::Lock(LPCTSTR tszOwner)
{

    if(tszOwner)
    {
        Log(TEXT("INFO: %s acquire lock %s\r\n"), tszOwner, m_strName.c_str()); 
        EnterCriticalSection(&m_Lock); 
        Log(TEXT("INFO: %s locked %s\r\n"), tszOwner, m_strName.c_str());
    }
    else
    {
        Log(TEXT("INFO: Acquire lock %s\r\n"), m_strName.c_str()); 
        EnterCriticalSection(&m_Lock); 
        Log(TEXT("INFO: Locked %s\r\n"), m_strName.c_str());
    }
}

template <typename T_Key, typename T_Item>
void ThreadSafeMap<T_Key, T_Item>::UnLock(LPCTSTR tszOwner)
{

    if(tszOwner)
    {
        LeaveCriticalSection(&m_Lock); 
        Log(TEXT("INFO: %s released lock %s\r\n"), tszOwner, m_strName.c_str());
    }
    else
    {
        LeaveCriticalSection(&m_Lock); 
        Log(TEXT("INFO: Released lock %s\r\n"), m_strName.c_str());
    }
}

template <typename T_Key, typename T_Item>
ThreadSafeMap<T_Key, T_Item>::ThreadSafeMap(const ThreadSafeMap &MapCopy)
{

    this->m_strName = MapCopy.SetCopy + TEXT("_Copy");
    this->m_ItemMap = MapCopy.m_ItemMap;
    InitializeCriticalSection(&this->m_Lock);
}

template <typename T_Key, typename T_Item>
ThreadSafeMap<T_Key, T_Item>::ThreadSafeMap(LPCTSTR tszName)
: m_strName(tszName)
{
    InitializeCriticalSection(&m_Lock);
}

template <typename T_Key, typename T_Item>
ThreadSafeMap<T_Key, T_Item>::~ThreadSafeMap()
{
    DeleteCriticalSection(&m_Lock);
}

template <typename T_Key, typename T_Item>
BOOL ThreadSafeMap<T_Key, T_Item>::Add(const T_Key &Key, const T_Item &Item)
{

    std::pair<std::map<T_Key, T_Item>::iterator, bool> Result = 
        m_ItemMap.insert(std::pair<T_Key, T_Item>(Key, Item));

    return Result.second ? TRUE : FALSE;
}

template <typename T_Key, typename T_Item>
BOOL ThreadSafeMap<T_Key, T_Item>::Get(DWORD dwIndex, T_Key &Key, T_Item &Item) const
{
    std::map<T_Key, T_Item>::const_iterator Iter;
    DWORD i;
    BOOL fRet = FALSE;

    for(Iter = m_ItemMap.begin(), i = 0; 
        Iter != m_ItemMap.end() && i < dwIndex; ++ Iter, ++ i)
    {
        ;
    }

    if(i == dwIndex && Iter != m_ItemMap.end())
    {
        Key = Iter->first;
        Item = Iter->second;
        fRet = TRUE;
    }

    return fRet;
}

template <typename T_Key, typename T_Item>
BOOL ThreadSafeMap<T_Key, T_Item>::Get(const T_Key &Key, T_Item &Item) const
{
    std::map<T_Key, T_Item>::const_iterator Iter;
    BOOL fRet = FALSE;

    Iter = m_ItemMap.find(Key);

    if(Iter != m_ItemMap.end())
    {
        Item = Iter->second;
        fRet = TRUE;
    }
    else
    {
        fRet = FALSE;
    }

    return fRet;
}

template <typename T_Key, typename T_Item>
BOOL ThreadSafeMap<T_Key, T_Item>::Set(const T_Key &Key, T_Item &Item)
{
    std::map<T_Key, T_Item>::iterator Iter;
    BOOL fRet = FALSE;

    Iter = m_ItemMap.find(Key);

    if(Iter != m_ItemMap.end())
    {
        Iter->second = Item;
        fRet = TRUE;
    }
    else
    {
        fRet = FALSE;
    }

    return fRet;
}

template <typename T_Key, typename T_Item>
BOOL ThreadSafeMap<T_Key, T_Item>::Erase(const T_Key &Key)
{
    BOOL fRet = FALSE;
    
    if(m_ItemMap.erase(Key) > 0)
    {

        fRet = TRUE;
    }

    return fRet;
}

template <typename T_Key, typename T_Item>
BOOL ThreadSafeMap<T_Key, T_Item>::Exist(const T_Key &Key) const
{
    return (m_ItemMap.find(Key) == m_ItemMap.end() ? FALSE : TRUE);
}

template <typename T_Key, typename T_Item>
VOID ThreadSafeMap<T_Key, T_Item>::Clear()
{
    m_ItemMap.clear();
}

template <typename T_Key, typename T_Item>
BOOL ThreadSafeMap<T_Key, T_Item>::Travel(DWORD dwStart, DWORD dwEnd, TravelCallBack pFuncPtr, PVOID pParam)
{       
    if(0 == dwEnd)
    {
        dwStart = 0;
        dwEnd = m_ItemMap.size();
    }
    
    std::map<T_Key, T_Item>::iterator Iter;
    BOOL fOK = TRUE;
    BOOL fStop = FALSE;
    DWORD dwIter;
    
    for(Iter = m_ItemMap.begin(), dwIter = 0; 
        Iter != m_ItemMap.end() && dwIter < dwStart; ++ Iter)
    {
        ;
    }

    for(; !fStop && Iter != m_ItemMap.end() && dwIter < dwEnd; ++ Iter)
    {
        fOK &= pFuncPtr(Iter->first, Iter->second, pParam, &fStop);
    }

    return fOK;
}

#endif

