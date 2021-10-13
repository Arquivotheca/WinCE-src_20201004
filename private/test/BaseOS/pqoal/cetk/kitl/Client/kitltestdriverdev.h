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
#pragma once

#include "KITLStressDev.h"

#include "..\inc\TestSettings.h"

/******************************************************************************
//  Class Name:  CKITLTestDriverDev
//
//    Description:
//        CKITLTestDriverDev synchronizes the test settings with the desktop, then creates
//        a number of threads that concurrently excercise KITL functionality.  Also given that
//        the functionality provided by this class is to be shared by all test cases, it implements
//        the singleton pattern to ensure that only one instance of this class exists at any 
//        given time.
//
******************************************************************************/

typedef FUNCTION_TABLE_ENTRY* LPFUNCTION_TABLE_ENTRY;
class CKITLTestDriverDev
{
public:
    static HRESULT CreateInstance(__deref CKITLTestDriverDev **ppKITLTestDriver);
    HRESULT Release();
    HRESULT RunTest(__in const TEST_SETTINGS& testSettings, LPFUNCTION_TABLE_ENTRY lpFTE);

private:
    HRESULT Init();
    HRESULT Uninit();
    HRESULT SyncSettings();  // Synchronizes the test settings with the desktop
    HRESULT SetSettings(__in const TEST_SETTINGS& testSettings);
    HRESULT ClearSettings();
    static DWORD WINAPI KITLThreadProc( __in_opt  LPVOID lpParameter);


private:
    CKITLTestDriverDev();
    
private:
    // Test Sync Stream
    UCHAR                   *m_pBufferPool;
    UCHAR                   m_uKITLStrmID;   
    UCHAR                   m_ucWindowSize;

    // List of KITL client ojects
    DWORD                   m_dwThreadCount;
    CKITLStressDev          **m_ppKITLStressClient;
    HTHREAD                 *m_phThread;

    // 
    HANDLE                  m_hCancelTest;

    // Test Settings
    TEST_SETTINGS           m_TestSettings;

    // Ref Count
    static LONG             m_lRefCount;
    static CKITLTestDriverDev *m_pKITLTestDriver;
};


