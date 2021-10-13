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

#include "KITLStressDesk.h"


#include "..\inc\TestSettings.h"

#define SAFE_RELEASE(x) \
    if ((x)) {\
        (x)->Release();\
        (x) = NULL;\
    }

/******************************************************************************
//    Class Name:  CKITLTestDriverDesk
//
//    Description:
//        CKITLTestDriverDesk synchronizes the test settings with the device, then creates
//        a number of threads that concurrently excercise KITL functionality.  Also given that
//        the functionality provided by this class is to be shared by all test cases, it implements
//        the singleton pattern to ensure that only one instance of this class exists at any 
//        given time.
//
******************************************************************************/

class CKITLTestDriverDesk
{
public:
    static HRESULT CreateInstance(__deref CKITLTestDriverDesk **ppKITLTestDriver,
        __deref WCHAR *wszDeviceName);
    HRESULT Release();
    HRESULT RunTest();
    HRESULT TestDone(__deref BOOL *pfRet);

private:
    HRESULT Init(__deref WCHAR *pwszDeviceName);
    HRESULT Uninit();
    HRESULT SyncSettings();  // Synchronizes the test settings with the desktop
    HRESULT SetSettings(__in TEST_SETTINGS testSettings);
    HRESULT ClearSettings();
    static DWORD WINAPI KITLThreadProc( __in  LPVOID lpParameter);
    static DWORD WINAPI WaitForTerminate(LPVOID lpVoid);


private:
    CKITLTestDriverDesk();
    
private:
    // Test Sync Stream
    UCHAR                       *m_pBufferPool;
    KITLID                      m_uKITLStrmID;   

    // List of KITL client ojects
    DWORD                       m_dwThreadCount;
    CKITLStressDesk              **m_ppKITLStressDesk;
    HANDLE                      *m_phThread;

    // 
    HANDLE                      m_hStopTest;
    HANDLE                        m_hTerminationThread;

    // Test Settings
    TEST_SETTINGS               m_TestSettings;
    WCHAR                       m_wszDeviceName[MAX_PATH];

    // Ref Count
    static LONG                 m_lRefCount;
    static CKITLTestDriverDesk     *m_pKITLTestDriver;

    // 
    BOOL                         m_fKitlInitialized;
};


