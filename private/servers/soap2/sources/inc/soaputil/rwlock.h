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
