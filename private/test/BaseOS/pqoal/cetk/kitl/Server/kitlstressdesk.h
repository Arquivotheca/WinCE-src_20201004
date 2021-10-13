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
#include <Kitlclnt_priv.h>


#define MAX_SVC_NAMELEN  19
#define KITL_MAX_DATA_SIZE 1446
#define REG_KEY_KITL        L"Software\\Microsoft\\KITL"

/******************************************************************************
//    Class Name:  CKITLStressDesk
//
//    Description:
//        CKITLStressDesk implements the actual KITL tests and abstracts it from the client
//        class.  Once an object instance is created, starting a test is a matter of call RunTest.
//
******************************************************************************/
class CKITLStressDesk
{
public:
    CKITLStressDesk(WCHAR wszServiceName[MAX_SVC_NAMELEN],
        WCHAR   wszDeviceName[MAX_PATH],
        DWORD   dwMinPayLoadSize,
        DWORD   dwMaxPayLoadSize,
        UCHAR   ucFlags,
        UCHAR   ucWindowSize,
        DWORD   dwRcvIterationsCount,
        DWORD   dwRcvTimeout,
        BOOL    fVerifyRcv,
        DWORD   dwSendIterationsCount,
        CHAR    cData,
        DWORD   dwDelay,
        HANDLE  hStopTest);
    ~CKITLStressDesk();

public:
    HRESULT RunTest();

private:
    HRESULT Init();
    HRESULT Uninit();
    HRESULT BulkSend();
    HRESULT BulkRcv();

private:
    // Control
    HANDLE  m_hStopTest;    // Event used to instruct the tests
                            // to stop execution prematurely

    // KITL priv
    WCHAR    m_wszServiceName[MAX_SVC_NAMELEN]; // KITL Stream Name
    WCHAR    m_wszDeviceName[MAX_PATH];
    KITLID   m_uKITLStrmID;  //
    BYTE    *m_pbKitlPayLoad;
    BOOL     m_fKitlInitialized;

    // RCV
    DWORD   m_dwDataRcvSize;

    // Send
    DWORD   m_dwDataSentSize;


    // Stress test settings
    DWORD   m_dwMinPayLoadSize;
    DWORD   m_dwMaxPayLoadSize;
    UCHAR   m_ucFlags;          // KITL Stream settings
    UCHAR   m_ucWindowSize; // number of KITL packets outstanding on the wire ans waiting for ack
    DWORD   m_dwRcvIterationsCount;
    DWORD   m_dwRcvTimeout;
    BOOL    m_fVerifyRcv;
    DWORD   m_dwSendIterationsCount;
    CHAR    m_cData;  // Actual character to use to fill the pay load
    DWORD   m_dwDelay;
};
