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
//      RwLock.h
//
// Contents:
//
//      Read/Write lock declarations
//
//----------------------------------------------------------------------------------
#ifndef __RWLOCK_H_INCLUDED__
#define __RWLOCK_H_INCLUDED__

class CRWLock
{
private:
    HANDLE m_hMutex;
    HANDLE m_hWriterMutex;
    HANDLE m_hReaderEvent;
    long   m_iCounter;

public:
    CRWLock();
    ~CRWLock();

    HRESULT Initialize();
    void Cleanup();

    void AcquireReaderLock();
    void ReleaseReaderLock();

    void AcquireWriterLock();
    void ReleaseWriterLock();

    void DowngradeFromWriterLock();
    void UpgradeToWriterLock();
};


#endif  // __RWLOCK_H_INCLUDED__
