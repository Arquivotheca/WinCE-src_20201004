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
//******************************************************************************
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//******************************************************************************

#include <windows.h>
#include "SyncObj.h"

/////////////////////////////////////////////////////////////////////////////
// Basic synchronization object

CSyncObject::CSyncObject(LPCTSTR /*pstrName*/)
{
//	UNUSED(pstrName);   // unused in release builds

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

