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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// Definitions and declarations for the AutoMutex_t class.
//
// ----------------------------------------------------------------------------

#ifndef _DEFINED_AutoMutex_t_
#define _DEFINED_AutoMutex_t_
#pragma once

#ifndef DEBUG
#ifndef NDEBUG
#define NDEBUG
#endif
#endif

#include <windows.h>
#include <sync.hxx>

namespace ce {
namespace qa {

// ----------------------------------------------------------------------------
//
// Wraps a ce::mutex object to add local synchronization.
//
class AutoMutex_t
{
private:
    
    // Mutex:
    typedef ce::mutex _myMutex_t;
    _myMutex_t m_Mutex;

    // Synchronization object:
    ce::critical_section m_CSection;
    
    // Name:
    void * volatile m_pName;

    // Has the mutex been initialize?
    bool m_bCreated;
        
    // Copy and assignment are deliberately disabled:
    AutoMutex_t(const AutoMutex_t &src);
    AutoMutex_t &operator = (const AutoMutex_t &src);
    
    // Compares the specified name to this object's:
    bool 
    IsSameName(
        IN LPCTSTR pNewName) const     
    {
        LPCTSTR pOldName = GetName();
        bool same;
        if (NULL == pOldName)
        {
            if (NULL == pNewName)
                 same = true;
            else same = false;
        }
        else
        {
            if (NULL == pNewName)
                 same = false;
            else same = _tcscmp(pOldName, pNewName) == 0;
        }
        return same;
    }

    // Determines whether the mutex needs to be created:
    // Leaves the critical-section locked if so.
    bool
    IsCreateNeeded(
        IN LPCTSTR pNewName)
    {
        if (!m_bCreated || !IsSameName(pNewName))
        {
            m_CSection.lock();

            if (!m_bCreated || !IsSameName(pNewName))
            {
                free(InterlockedExchangePointer(&m_pName, NULL));

                if (m_bCreated)
                {
                    m_bCreated = false;
                    m_Mutex.~_myMutex_t();
                    new (&m_Mutex) _myMutex_t();
                }

                if (NULL == pNewName)
                {
                    return true;
                }

                size_t nameSize = (1 + _tcslen(pNewName)) * sizeof(TCHAR);
                void *pNameAlloc = malloc(nameSize);
                if (NULL != pNameAlloc)
                {
                    memcpy(pNameAlloc, pNewName, nameSize);
                    InterlockedExchangePointer(&m_pName, pNameAlloc);
                    return true;
                }

                SetLastError(ERROR_OUTOFMEMORY);
            }

            m_CSection.unlock();
        }
        
        return false;
    }
        
public:

    // Constructor and destructor:
    AutoMutex_t(void)
        : m_pName(NULL),
          m_bCreated(false)
    { }
   ~AutoMutex_t(void)
    {
        m_bCreated = false;
        free(InterlockedExchangePointer(&m_pName, NULL));
    }

    // Accessors:
    LPCTSTR 
    GetName(void) const
    { 
        return (LPCTSTR) InterlockedCompareExchangePointer(&m_pName, NULL, NULL);
    }
    bool    
    IsCreated(void) const
    { 
        return m_bCreated;
    }
    
    // Creates the mutex with the specified parameters (see CreateMutex):
    // Does nothing if the mutex was already initiaized with the same name.
    bool 
    create(
        IN LPCTSTR               pMutexName = NULL, 
        IN BOOL                  bInitialOwner = FALSE, 
        IN LPSECURITY_ATTRIBUTES pMutexAttributes = NULL)
    {
        if (IsCreateNeeded(pMutexName))
        {
            if (m_Mutex.create(GetName(), bInitialOwner, pMutexAttributes))
            {
                m_bCreated = true;
            }
            m_CSection.unlock();
        }
        return m_bCreated;
    }

    // Opens an existing mutex (see OpenMutex):
    // Equivalent to create(name, FALSE, NULL) in CE.
    // Does nothing if the mutex was already initialized with the same name.
    bool 
    open(
        IN LPCTSTR pMutexName = NULL, 
        IN DWORD   DesiredAccess = MUTANT_ALL_ACCESS, 
        IN BOOL    bInheritHandle = FALSE)
    {
        if (IsCreateNeeded(pMutexName))
        {
            if (m_Mutex.open(GetName(), DesiredAccess, bInheritHandle))
            {
                m_bCreated = true;
            }
            m_CSection.unlock();
        }
        return m_bCreated;
    }

    // Locks the mutex:
    void 
    lock(void)
    {
        m_Mutex.lock();
    }
    
    // Waits for a limited time for a lock:
    bool
    try_lock(
        IN DWORD MaxWaitMS)
    {
        return m_Mutex.try_lock(MaxWaitMS);
    }

    // Unlocks the mutex:
    void
    unlock(void)
    {
        m_Mutex.unlock();
    }
};

};
};
#endif /* _DEFINED_AutoMutex_t_ */
// ----------------------------------------------------------------------------
