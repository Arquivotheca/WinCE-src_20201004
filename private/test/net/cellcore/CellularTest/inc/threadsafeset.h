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


#ifndef _THREAD_SAFE_SET_H
#define _THREAD_SAFE_SET_H

#include <windows.h>
#include <set>
#include <string>

typedef std::basic_string<TCHAR, 
  std::char_traits<TCHAR>, std::allocator<TCHAR> > 
  tstring;

template <typename T>
class ThreadSafeSet
{
public:
    typedef BOOL (*TravelCallBack)(const T &, PVOID, BOOL *);
    ThreadSafeSet(LPCTSTR tszName);
    ThreadSafeSet(const ThreadSafeSet &SetCopy);
    ~ThreadSafeSet();
    BOOL Add(const T &Item);
    DWORD Size() const {return m_ItemSet.size();}
    BOOL Get(DWORD dwIndex, T &Item) const;
    BOOL Erase(const T &Item);
    BOOL Exist(const T &Item) const;
    VOID Clear();
    BOOL Travel(DWORD dwStart, DWORD dwEnd, TravelCallBack pFuncPtr, PVOID pParam = NULL);
    void Lock(LPCTSTR tszOwner = NULL);
    void UnLock(LPCTSTR tszOwner = NULL);
private:    
    tstring m_strName;
    CRITICAL_SECTION m_Lock;
    std::set<T> m_ItemSet;
};

template <typename T>
void ThreadSafeSet<T>::Lock(LPCTSTR tszOwner)
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

template <typename T>
void ThreadSafeSet<T>::UnLock(LPCTSTR tszOwner)
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

template <typename T>
ThreadSafeSet<T>::ThreadSafeSet(const ThreadSafeSet &SetCopy)
{

    this->m_strName = SetCopy.m_strName + TEXT("_Copy");
    this->m_ItemSet = SetCopy.m_ItemSet;
    InitializeCriticalSection(&this->m_Lock);
}

template <typename T>
ThreadSafeSet<T>::ThreadSafeSet(LPCTSTR tszName)
: m_strName(tszName)
{
    InitializeCriticalSection(&m_Lock);
}

template <typename T>
ThreadSafeSet<T>::~ThreadSafeSet()
{
    DeleteCriticalSection(&m_Lock);
}

template <typename T>
BOOL ThreadSafeSet<T>::Add(const T &Item)
{
    std::pair<std::set<T>::iterator, bool> Result = m_ItemSet.insert(Item);
    return Result.second ? TRUE : FALSE;
}

template <typename T>
BOOL ThreadSafeSet<T>::Get(DWORD dwIndex, T &Item) const
{
    std::set<T>::const_iterator Iter;
    DWORD i;
    BOOL fRet = FALSE;

    for(Iter = m_ItemSet.begin(), i = 0; 
        Iter != m_ItemSet.end() && i < dwIndex; ++ Iter, ++ i)
    {
        ;
    }

    if(i == dwIndex && Iter != m_ItemSet.end())
    {
        Item = *Iter;
        fRet = TRUE;
    }

    return fRet;
}

template <typename T>
BOOL ThreadSafeSet<T>::Erase(const T &Item)
{
    BOOL fRet = FALSE;
    
    if(m_ItemSet.erase(Item) > 0)
    {

        fRet = TRUE;
    }

    return fRet;
}

template <typename T>
BOOL ThreadSafeSet<T>::Exist(const T &Item) const
{
    BOOL fRet = FALSE;
    
    if(m_ItemSet.find(Item) != m_ItemSet.end())
    {

        fRet = TRUE;
    }

    return fRet;
}

template <typename T>
VOID ThreadSafeSet<T>::Clear()
{
    m_ItemSet.clear();
}

template <typename T>
BOOL ThreadSafeSet<T>::Travel(DWORD dwStart, DWORD dwEnd, TravelCallBack pFuncPtr, PVOID pParam)
{       
    if(0 == dwEnd)
    {
        dwStart = 0;
        dwEnd = m_ItemSet.size();
    }
    
    std::set<T>::iterator Iter;
    BOOL fOK = TRUE;
    BOOL fStop = FALSE;
    DWORD dwIter;
    
    for(Iter = m_ItemSet.begin(), dwIter = 0; 
        Iter != m_ItemSet.end() && dwIter < dwStart; ++ Iter)
    {
        ;
    }

    for(; !fStop && Iter != m_ItemSet.end() && dwIter < dwEnd; ++ Iter)
    {
        fOK &= pFuncPtr(*Iter, pParam, &fStop);
    }

    return fOK;
}

#endif

