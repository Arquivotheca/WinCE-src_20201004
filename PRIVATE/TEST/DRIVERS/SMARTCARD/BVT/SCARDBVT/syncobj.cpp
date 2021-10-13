//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
// This is a part of the Microsoft Foundation Classes C++ library.
// All rights reserved.
//
// This source code is only intended as a supplement to the
// Microsoft Foundation Classes Reference and related
// electronic documentation provided with the library.
// See these sources for detailed information regarding the
// Microsoft Foundation Classes product.

#include "stdafx.h"
#include "SyncObj.h"

/////////////////////////////////////////////////////////////////////////////
// Basic synchronization object

CSyncObject::CSyncObject(LPCTSTR /*pstrName*/)
{

    m_hObject = NULL;

}

CSyncObject::~CSyncObject()
{
    if (m_hObject != NULL)
    {
        ::CloseHandle(m_hObject);
        m_hObject = NULL;
    }
}

BOOL CSyncObject::Lock(DWORD dwTimeout)
{
    if (::WaitForSingleObject(m_hObject, dwTimeout) == WAIT_OBJECT_0)
        return TRUE;
    else
        return FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// CSemaphore
CSemaphore::CSemaphore(LONG lInitialCount, LONG lMaxCount,
    LPCTSTR pstrName, LPSECURITY_ATTRIBUTES lpsaAttributes)
    :  CSyncObject(pstrName)
{
    ASSERT(lMaxCount > 0);
    ASSERT(lInitialCount <= lMaxCount);
    dw_Locked=0;

    m_hObject = ::CreateSemaphore(lpsaAttributes, lInitialCount, lMaxCount,
        pstrName);
    ASSERT(m_hObject);
}

CSemaphore::~CSemaphore()
{
}

BOOL CSemaphore::Unlock(LONG lCount, LPLONG lpPrevCount /* =NULL */)
{
    return ::ReleaseSemaphore(m_hObject, lCount, lpPrevCount);
}

 

/////////////////////////////////////////////////////////////////////////////
// CMutex

CMutex::CMutex(BOOL bInitiallyOwn, LPCTSTR pstrName,
    LPSECURITY_ATTRIBUTES lpsaAttribute /* = NULL */)
    : CSyncObject(pstrName)
{
    m_hObject = ::CreateMutex(lpsaAttribute, bInitiallyOwn, pstrName);
    ASSERT(m_hObject!=NULL);
}

CMutex::~CMutex()
{
}

BOOL CMutex::Unlock()
{
    return ::ReleaseMutex(m_hObject);
}

/////////////////////////////////////////////////////////////////////////////
// CEvent

CEvent::CEvent(BOOL bInitiallyOwn, BOOL bManualReset, LPCTSTR pstrName,
    LPSECURITY_ATTRIBUTES lpsaAttribute)
    : CSyncObject(pstrName)
{
    m_hObject = ::CreateEvent(lpsaAttribute, bManualReset,
        bInitiallyOwn, pstrName);
    ASSERT(m_hObject != NULL);
}

CEvent::~CEvent()
{
}

BOOL CEvent::Unlock()
{
    return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CSingleLock

CSingleLock::CSingleLock(CSyncObject* pObject, BOOL bInitialLock)
{
    ASSERT(pObject != NULL);
//  ASSERT(pObject->IsKindOf(RUNTIME_CLASS(CSyncObject)));

    m_pObject = pObject;
    m_hObject = pObject->m_hObject;
    m_bAcquired = FALSE;

    if (bInitialLock)
        Lock();
}

BOOL CSingleLock::Lock(DWORD dwTimeOut /* = INFINITE */)
{
    ASSERT(m_pObject != NULL || m_hObject != NULL);
    ASSERT(!m_bAcquired);

    m_bAcquired = m_pObject->Lock(dwTimeOut);
    return m_bAcquired;
}

BOOL CSingleLock::Unlock()
{
    ASSERT(m_pObject != NULL);
    if (m_bAcquired)
        m_bAcquired = !m_pObject->Unlock();

    // successfully unlocking means it isn't acquired
    return !m_bAcquired;
}

BOOL CSingleLock::Unlock(LONG lCount, LPLONG lpPrevCount /* = NULL */)
{
    ASSERT(m_pObject != NULL);
    if (m_bAcquired)
        m_bAcquired = !m_pObject->Unlock(lCount, lpPrevCount);

    // successfully unlocking means it isn't acquired
    return !m_bAcquired;
}
