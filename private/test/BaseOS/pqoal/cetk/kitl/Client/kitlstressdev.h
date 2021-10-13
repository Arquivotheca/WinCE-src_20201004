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
#include <Kitlprot.h>
#include <Halether.h>
#include <tux.h>

#include "KITLPerfLog.h"

#define KITL_CANCEL_TEST  L"KITL_CANCEL_TEST"

class CKITLStressDev
{
public:
    CKITLStressDev(const CHAR szServiceName[MAX_SVC_NAMELEN],
        DWORD   dwMinPayLoadSize,
        DWORD   dwMaxPayLoadSize,
        UCHAR   ucFlags,
        UCHAR   ucWindowSize,
        DWORD   dwRcvIterationsCount,
        DWORD   dwRcvTimeout,
        BOOL    fVerifyRcv,
        DWORD   dwSendIterationsCount,
        CHAR    cData,
        DWORD   dwDelay);
    ~CKITLStressDev();

public:
    HRESULT RunTest(CKitlPerfLog* pkitlLog);

private:
    HRESULT Init();
    HRESULT Uninit();
    HRESULT BulkSend();
    HRESULT BulkRcv();
    DWORD   GetTickDetla(__in DWORD dwTickStart, __in DWORD dwTickEnd);
    DWORD   ComputeThroughput(__in DWORD dwTickDelta, __in DWORD dwPayLoadSize);

    DWORD   GetSendThroughputInKB() const;
    DWORD   GetSendThroughputInB() const;
    DWORD   GetRcvThroughputInKB() const;
    DWORD   GetRcvThroughputInB() const;
    HRESULT GetSendLatency(__deref DWORD *pdwTickCount,
        __deref DWORD *pdwPacketCount) const;
    HRESULT GetRcvLatency(__deref DWORD *pdwTickCount,
        __deref DWORD *pdwPacketCount) const;
    HRESULT PrintSendPerf();
    HRESULT PrintRcvPerf();

private:
    // Control
    HANDLE  m_hStopTest;

    // KITL priv
    CHAR    m_szServiceName[MAX_SVC_NAMELEN];
    UCHAR   m_uKITLStrmID;
    UCHAR   *m_pBufferPool;
    BYTE    *m_pbKitlPayLoad;

    // RCV
    DWORD   m_dwRcvTickDelta;
    DWORD   m_dwDataRcvSize;
    DWORD   m_dwRcvThroughput; // Unit is in bytes per milliseconds

    // Send
    DWORD   m_dwSendTickDelta;
    DWORD   m_dwDataSentSize;
    DWORD   m_dwSendThroughput;


    // Stress test settings
    DWORD   m_dwMinPayLoadSize;
    DWORD   m_dwMaxPayLoadSize;
    UCHAR   m_ucFlags;
    UCHAR   m_ucWindowSize;
    DWORD   m_dwRcvIterationsCount;
    DWORD   m_dwRcvTimeout;
    BOOL    m_fVerifyRcv;
    DWORD   m_dwSendIterationsCount;
    CHAR    m_cData;
    DWORD   m_dwDelay;
};


