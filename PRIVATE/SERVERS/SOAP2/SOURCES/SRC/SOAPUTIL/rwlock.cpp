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
//      RwLock.cpp
//
// Contents:
//
//      Read/Write lock implementation
//
//----------------------------------------------------------------------------------

#include "Headers.h"

#ifdef UNDER_CE
#include "WinCEUtils.h"
#endif


CRWLock::CRWLock()
: m_hMutex(0),
  m_hWriterMutex(0),
  m_hReaderEvent(0),
  m_iCounter(0)
{
#ifdef CE_NO_EXCEPTIONS
    if(FAILED(Initialize()))
        RaiseException( EXCEPTION_NONCONTINUABLE_EXCEPTION, 0, 0, 0);
#else
    if (FAILED(Initialize()))
        throw;
#endif

}

CRWLock::~CRWLock()
{
    Cleanup();
}

HRESULT CRWLock::Initialize()
{
    HANDLE hReaderEvent = 0;
    HANDLE hMutex       = 0;
    HANDLE hWriterMutex = 0;
    HRESULT hr          = S_OK;

#ifndef UNDER_CE
    hReaderEvent = ::CreateEventA(NULL, TRUE, FALSE, NULL);
#else
    hReaderEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
#endif
    CHK_BOOL(hReaderEvent, HRESULT_FROM_WIN32(::GetLastError()));

#ifndef UNDER_CE
    hMutex = ::CreateEventA(NULL, FALSE, TRUE, NULL);
#else
    hMutex = ::CreateEvent(NULL, FALSE, TRUE, NULL);
#endif
    CHK_BOOL(hMutex, HRESULT_FROM_WIN32(::GetLastError()));

#ifndef UNDER_CE
    hWriterMutex = ::CreateMutexA(NULL, FALSE, NULL);
#else
    hWriterMutex = ::CreateMutex(NULL, FALSE, NULL);
#endif
    CHK_BOOL(hWriterMutex, HRESULT_FROM_WIN32(::GetLastError()));

    m_hReaderEvent = hReaderEvent;
    m_hWriterMutex = hWriterMutex;
    m_hMutex       = hMutex;
    hReaderEvent   = 0;
    hWriterMutex   = 0;
    hMutex         = 0;

    m_iCounter = -1;

Cleanup:
    if (hReaderEvent)
        ::CloseHandle(hReaderEvent);
    if (hWriterMutex)
        ::CloseHandle(hWriterMutex);
    if (hMutex)
        ::CloseHandle(hMutex);

    return hr;
}

void CRWLock::Cleanup()
{
    if (m_hReaderEvent)
    {
        ::CloseHandle(m_hReaderEvent);
        m_hReaderEvent = 0;
    }
    if (m_hMutex)
    {
        ::CloseHandle(m_hMutex);
        m_hMutex = 0;
    }
    if (m_hWriterMutex)
    {
        ::CloseHandle(m_hWriterMutex);
        m_hWriterMutex = 0;
    }
};


void CRWLock::AcquireReaderLock()
{
    if (::InterlockedIncrement(&m_iCounter) == 0)
    {
        ::WaitForSingleObject(m_hMutex, INFINITE);
        ::SetEvent(m_hReaderEvent);
    }

    ::WaitForSingleObject(m_hReaderEvent,INFINITE);
}


void CRWLock::AcquireWriterLock()
{
    ::WaitForSingleObject(m_hWriterMutex,INFINITE);
    ::WaitForSingleObject(m_hMutex, INFINITE);
}

void CRWLock::ReleaseWriterLock()
{
    ::SetEvent(m_hMutex);
    ::ReleaseMutex(m_hWriterMutex);
}


void CRWLock::DowngradeFromWriterLock()
{
    ::SetEvent(m_hMutex);           // readers can go in, but no writers, as we hold on to m_hWriterMutex
    AcquireReaderLock();            // I can go in as a reader now
    ::ReleaseMutex(m_hWriterMutex); // now other readers can
}

void CRWLock::UpgradeToWriterLock()
{
    ReleaseReaderLock();
    AcquireWriterLock();
}

void CRWLock::ReleaseReaderLock()
{
    if (::InterlockedDecrement(&m_iCounter) < 0)
    {
        ::ResetEvent(m_hReaderEvent);
        ::SetEvent(m_hMutex);
    }
}

