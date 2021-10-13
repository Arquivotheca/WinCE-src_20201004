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

#ifndef __SYNCOBJ_H__
#define __SYNCOBJ_H__


/////////////////////////////////////////////////////////////////////////////
// AFXMT - MFC Multithreaded Extensions (Syncronization Objects)

// Classes declared in this file

//CObject
class CSyncObject;
class CMutex;
class CEvent;
class CCriticalSection;
class CSingleLock;


/////////////////////////////////////////////////////////////////////////////
// Basic synchronization object

class CSyncObject 
{

// Constructor
public:
    CSyncObject(LPCTSTR pstrName);

// Attributes
public:
    operator HANDLE() const;
    HANDLE  m_hObject;

// Operations
    virtual BOOL Lock(DWORD dwTimeout = INFINITE);
    virtual BOOL Unlock() = 0;
    virtual BOOL Unlock(LONG /* lCount */, LPLONG /* lpPrevCount=NULL */)
        { return TRUE; }

// Implementation
public:
    virtual ~CSyncObject();
    friend class CSingleLock;
//  friend class CMultiLock; Unsupported by WinCE
};

/////////////////////////////////////////////////////////////////////////////
// CSemaphore
/*  Not supported by WinCE */
class CSemaphore : public CSyncObject
{

// Constructor
public:
    CSemaphore(LONG lInitialCount = 1, LONG lMaxCount = 1,
        LPCTSTR pstrName=NULL, LPSECURITY_ATTRIBUTES lpsaAttributes = NULL);
    DWORD GetLockedCount() { return dw_Locked; };
// Implementation
public:
    virtual ~CSemaphore();
    virtual BOOL Lock(DWORD dwTimeout = INFINITE) {
        dw_Locked++;
        BOOL bReturn=CSyncObject::Lock(dwTimeout);
        dw_Locked--;
        return bReturn;
    }
    virtual BOOL Unlock() { return Unlock(1); };
    virtual BOOL Unlock(LONG lCount, LPLONG lprevCount = NULL);
private:
    DWORD dw_Locked;
};

/////////////////////////////////////////////////////////////////////////////
// CMutex

class CMutex : public CSyncObject
{

// Constructor
public:
    CMutex(BOOL bInitiallyOwn = FALSE, LPCTSTR lpszName = NULL,
        LPSECURITY_ATTRIBUTES lpsaAttribute = NULL);

// Implementation
public:
    virtual ~CMutex();
    BOOL Unlock();
};

/////////////////////////////////////////////////////////////////////////////
// CEvent

class CEvent : public CSyncObject
{

// Constructor
public:
    CEvent(BOOL bInitiallyOwn = FALSE, BOOL bManualReset = FALSE,
        LPCTSTR lpszNAme = NULL, LPSECURITY_ATTRIBUTES lpsaAttribute = NULL);

// Operations
public:
    BOOL Unlock();
    BOOL SetEvent()
    { ASSERT(m_hObject != NULL); return ::SetEvent(m_hObject); }
    BOOL PulseEvent()
    { ASSERT(m_hObject != NULL); return ::PulseEvent(m_hObject); }
    BOOL ResetEvent()
    { ASSERT(m_hObject != NULL); return ::ResetEvent(m_hObject); }

// Implementation
public:
    virtual ~CEvent();
};

/////////////////////////////////////////////////////////////////////////////
// CCriticalSection


class CCriticalSection : public CSyncObject
{

// Constructor
public:
    CCriticalSection(): CSyncObject(NULL)
    { ::InitializeCriticalSection(&m_sect); };

// Attributes
public:
    operator CRITICAL_SECTION*() { return &m_sect; };
    CRITICAL_SECTION m_sect;

// Operations
public:
    BOOL Unlock()
    { ::LeaveCriticalSection(&m_sect); return TRUE; };
    BOOL Lock()
    { ::EnterCriticalSection(&m_sect); return TRUE; };
    BOOL Lock(DWORD /*dwTimeout*/) { return Lock();};

// Implementation
public:
    virtual ~CCriticalSection()
    { ::DeleteCriticalSection(&m_sect); };
};

/////////////////////////////////////////////////////////////////////////////
// CSingleLock

class CSingleLock
{
// Constructors
public:
    CSingleLock(CSyncObject* pObject, BOOL bInitialLock = FALSE);

// Operations
public:
    BOOL Lock(DWORD dwTimeOut = INFINITE);
    BOOL Unlock();
    BOOL Unlock(LONG lCount, LPLONG lPrevCount = NULL);
    BOOL IsLocked();

// Implementation
public:
    ~CSingleLock();

protected:
    CSyncObject* m_pObject;
    HANDLE  m_hObject;
    BOOL    m_bAcquired;
};

#endif  // __SYNCOBJ_H__

/////////////////////////////////////////////////////////////////////////////
