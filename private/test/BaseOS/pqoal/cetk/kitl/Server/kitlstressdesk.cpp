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
#include <windows.h>
#include <wchar.h>
#include <assert.h>
#include <strsafe.h>

#include "..\inc\TestSettings.h"

#include "KITLStressDesk.h"

/******************************************************************************
//  Function Name:  CKITLStressDesk::CKITLStressDesk
//  Description:
//              Constructor
//  Input:
//
//        __in WCHAR   wszServiceName[MAX_SVC_NAMELEN]: KITL Stream name
//        __in DWORD   dwMinPayLoadSize: Minimum amout of data to send/receive per packet
//        __in DWORD   dwMaxPayLoadSize: Max amount of data to send/receive per packet
//        __in UCHAR    ucFlags: Stream settings passed to KITL
//        __in UCHAR    ucWindowSize: Number of outstanding packets that can be sent
//                              through the wire while awaiting ack
//        __in DWORD   dwRcvIterationsCount: number of times to iterate on the receive
//                              sequence
//        __in DWORD   dwRcvTimeout: duration in seconds to wait for the receive to complete
//        __in BOOL       fVerifyRcv: set to true if data received is to be verified
//        __in DWORD   dwSendIterationsCount: Number of times to iterate ont he send
//                              sequence
//        __in CHAR      cData: actual byte sent & expected to be received
//
//  Output:
//        None
//
******************************************************************************/
CKITLStressDesk::CKITLStressDesk(__in WCHAR wszServiceName[MAX_SVC_NAMELEN],
        __in WCHAR   wszDeviceName[MAX_PATH],
        __in DWORD   dwMinPayLoadSize,
        __in DWORD   dwMaxPayLoadSize,
        __in UCHAR   ucFlags,
        __in UCHAR   ucWindowSize,
        __in DWORD   dwRcvIterationsCount,
        __in DWORD   dwRcvTimeout,
        __in BOOL    fVerifyRcv ,
        __in DWORD   dwSendIterationsCount,
        __in CHAR    cData,
        __in DWORD   dwDelay,
        __in HANDLE  hStopTest)
{
    StringCchCopyEx(m_wszServiceName,
        _countof(m_wszServiceName),
        wszServiceName,
        NULL,
        NULL,
        STRSAFE_NULL_ON_FAILURE | STRSAFE_NO_TRUNCATION);

    StringCchCopyEx(m_wszDeviceName,
        _countof(m_wszDeviceName),
        wszDeviceName,
        NULL,
        NULL,
        STRSAFE_NULL_ON_FAILURE | STRSAFE_NO_TRUNCATION);

    m_dwMinPayLoadSize = dwMinPayLoadSize;
    m_dwMaxPayLoadSize = dwMaxPayLoadSize;
    m_ucFlags = ucFlags;
    m_ucWindowSize = ucWindowSize;
    m_dwRcvIterationsCount = dwRcvIterationsCount;
    m_dwRcvTimeout = dwRcvTimeout* 1000;
    m_fVerifyRcv = fVerifyRcv;
    m_dwSendIterationsCount = dwSendIterationsCount;
    m_cData = cData;
    m_dwDelay = dwDelay;

    m_uKITLStrmID = INVALID_KITLID;
    m_pbKitlPayLoad = NULL;

    m_hStopTest = hStopTest;

    m_fKitlInitialized = FALSE;

}

/******************************************************************************
//  Function Name:  CKITLStressDesk::~CKITLStressDesk
//  Description:
//      Place holder
//  Input:
//      None
//
//  Output:
//      None
//
******************************************************************************/
CKITLStressDesk::~CKITLStressDesk()
{
}


/******************************************************************************
//  Function Name:  CKITLStressDesk::BulkSend
//  Description:
//      Send packets in bulk from Desktop to device
//
//  Input:
//      None
//
//  Output:
//      The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLStressDesk::BulkSend()
{
    HRESULT hr = E_FAIL;
    DWORD dwRet = 0;
    BOOL bRet = FALSE;

    DWORD dwACK = 0;
    DWORD dwACKSize = sizeof(dwACK);

    m_dwDataSentSize = 0;

    bRet = KITLSend(m_uKITLStrmID,
        &m_dwDataSentSize,
        sizeof(m_dwDataSentSize),
        m_dwRcvTimeout);
    if (!bRet)
    {
        wprintf(L"BulkSend: Failed to send SYNC packet for cookie %c.\n",
            m_cData);
        hr = E_FAIL;
        goto Exit;
    }

    wprintf(L"Bulk Send started.  Cookie is %c\n",
        m_cData);

    // Send data to the device
    for( DWORD j = 0; j < m_dwSendIterationsCount; j++)
    {
        for (DWORD i = m_dwMinPayLoadSize; i < m_dwMaxPayLoadSize; i++)
        {
            bRet = KITLSend(m_uKITLStrmID,
                m_pbKitlPayLoad,
                i,
                m_dwRcvTimeout);
            if(!bRet)
            {
                wprintf(L"KITLSend failed on packet of size %dbytes and cookie: %c."
                    L"Timeout is: %dms\n",
                 i,
                 m_cData,
                 m_dwRcvTimeout);
                hr = E_FAIL;;
                goto Exit;
            }

            if (!(i%100))
            {
                wprintf(L"KITLSend sent packet of size %dbytes and cookie: %c\n",
                    i,
                    m_cData);
            }

            m_dwDataSentSize += i;

            dwRet = WaitForSingleObject(m_hStopTest, 0);
            if (WAIT_OBJECT_0 == dwRet)
            {
                goto Exit;
            }

            if (m_dwDelay)
            {
                Sleep(m_dwDelay);
            }
        }
    }

    wprintf(L"Bulk Send done.  Cookie is %c\n", m_cData);

    wprintf(L"Waiting for device Ack.  Cookie is %c\n", m_cData);

    bRet = KITLRecv(m_uKITLStrmID,
        &dwACK,
        &dwACKSize,
        m_dwRcvTimeout);
    if (!bRet)
    {
        wprintf(L"Failed to receive device Ack after %dms. Cookie is %c\n",
            m_dwRcvTimeout,
            m_cData);
        hr = E_FAIL;
        goto Exit;
    }
    if (dwACK != m_dwDataSentSize)
    {
        wprintf(L"Unexpected packet received %d. Cookied is %c \n",
            dwACK, 
            m_cData);
        hr = E_UNEXPECTED;
        goto Exit;
    }

    bRet = KITLSend(m_uKITLStrmID,
        &m_dwDataSentSize,
        sizeof(m_dwDataSentSize),
        m_dwRcvTimeout);
    if (!bRet)
    {
        wprintf(L"BulkSend: Failed to send Completion packet. Cookie is %c \n",
            m_cData);
        hr = E_FAIL;
        goto Exit;
    }

    wprintf(L"Device Ack received.  Cookie is %c\n", m_cData);


    hr = S_OK;

Exit:

    return hr;
}

/******************************************************************************
//  Function Name:  CKITLStressDesk::BulkRcv
//  Description:
//      Perform a bulk receive
//
//  Input:
//      None
//
//  Output:
//      The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLStressDesk::BulkRcv()
{
    HRESULT hr = E_FAIL;
    BOOL bRet = FALSE;
    DWORD dwRet = 0;

    DWORD dwBuffSize;

    m_dwDataRcvSize = 0;

    wprintf(L"Bulk Rcv started.  Cookie is set to %c\n",
        m_cData);

    bRet = KITLSend(m_uKITLStrmID,
        &m_dwDataRcvSize,
        sizeof(m_dwDataRcvSize),
        m_dwRcvTimeout);
    if (!bRet)
    {
        wprintf(L"BulkRcv: Failed to deliver SYNC packet.  Cookie is set to %c\n",
            m_cData);
        hr = E_FAIL;
        goto Exit;
    }


    // Receive Data from the desktop
    for( DWORD j = 0; j < m_dwRcvIterationsCount; j++)
    {
        for (DWORD i = m_dwMinPayLoadSize; i < m_dwMaxPayLoadSize; i++)
        {
            dwBuffSize = m_dwMaxPayLoadSize;
            bRet = KITLRecv(m_uKITLStrmID,
                m_pbKitlPayLoad,
                &dwBuffSize,
                m_dwRcvTimeout);
            if(!bRet)
            {
                wprintf(L"Failed to receive packet of size %dbytes and cookie of %c after %dms.\n",
                  i,
                  m_cData,
                  m_dwRcvTimeout);
                hr = E_FAIL;
                goto Exit;
            }

            if (!(i%100))
            {
                wprintf(L"KITLRcv received packet of size %dbytes and cookie: %c \n",
                    i,
                    m_cData);
            }

            m_dwDataRcvSize += dwBuffSize;

            dwRet = WaitForSingleObject(m_hStopTest, 0);
            if (WAIT_OBJECT_0 == dwRet)
            {
                goto Exit;
            }


            if (m_fVerifyRcv)
            {
                if (dwBuffSize != i)
                {
                    wprintf(L"Unexpected buffer size.  Cookie is set to %c\n",
                        m_cData);
                    hr = E_FAIL;
                    goto Exit;
                }

                for (DWORD dw = 0; dw < i; dw++)
                {
                    if (m_pbKitlPayLoad[dw] != m_cData)
                    {
                        wprintf(L"Unexpected packet: %c.  Cookie %c was expected\n",
                            m_pbKitlPayLoad[dw],
                            m_cData);
                        hr = E_FAIL;
                        goto Exit;
                    }
                }

            }
        }
    }

    wprintf(L"Bulk Rcv done.  Cookie %c\n", m_cData);

    wprintf(L"Sending device Ack.  Cookie %c, PayLoad: %c\n",
        m_cData, 
        m_pbKitlPayLoad[0]);

    bRet = KITLSend(m_uKITLStrmID,
        &m_dwDataRcvSize,
        sizeof(m_dwDataRcvSize),
        m_dwRcvTimeout);
    if (!bRet)
    {
        wprintf(L"Failed to send device Ack.  Cookie %c\n", m_cData);
        hr = E_FAIL;
        goto Exit;
    }

    wprintf(L"Sent device Ack.  Cookie %c, PayLoad: %c\n", m_cData, m_pbKitlPayLoad[0]);

    // Give PB a chance to send ack packet
    Sleep(5000);

    hr = S_OK;

Exit:

    return hr;
}


/******************************************************************************
//  Function Name:  CKITLStressDesk::Init
//  Description:
//      Initializes KITL
//
//  Input:
//      None
//
//  Output:
//      The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLStressDesk::Init()
{
    HRESULT hr = E_FAIL;

    DWORD dwThreadID = GetCurrentThreadId();

    wprintf (L"Initializing Thread: 0x%x\n", dwThreadID);

    // Buffer used to hold KITL data
    m_pbKitlPayLoad = new BYTE[m_dwMaxPayLoadSize];
    if (!m_pbKitlPayLoad)
    {
        hr = E_OUTOFMEMORY;
        goto Exit;
    }

    memset(m_pbKitlPayLoad,
        m_cData,
        m_dwMaxPayLoadSize);

    // KITL has already been initialized at the process level
    // No need to initialize it again

    // Register a new KITL client
    m_uKITLStrmID = KITLRegisterClient(m_wszDeviceName,
            m_wszServiceName,
            m_dwRcvTimeout,
            NULL);
    if (INVALID_KITLID == m_uKITLStrmID)
    {
        wprintf (L"Failed to register client: %d\n", GetLastError());
        goto Exit;
    }

    wprintf (L"Initialization succeeded. Cookie %c\n", m_cData);

    hr = S_OK;

Exit:
    if (FAILED(hr))
    {
        wprintf (L"Initialization failed. Cookie %c\n", m_cData);
        Uninit();
    }
    return hr;

}

/******************************************************************************
//  Function Name:  CKITLStressDesk::Uninit
//  Description:
//      Uninitialize stress test
//
//  Input:
//      None
//  Output:
//      The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLStressDesk::Uninit()
{
    HRESULT hr = S_OK;
    BOOL bRet = FALSE;

    DWORD dwThreadID = GetCurrentThreadId();

    if (m_hStopTest)
    {
        m_hStopTest = NULL;
    }

    // m_uKITLStrmID can be set to null, verify against INVALID_KITLID
    if (INVALID_KITLID != m_uKITLStrmID)
    {
        bRet = KITLDeRegisterClient(m_uKITLStrmID);
        if (!bRet)
        {
            hr = E_FAIL;
        }
        m_uKITLStrmID = INVALID_KITLID;
    }

    if (m_pbKitlPayLoad)
    {
        delete [] m_pbKitlPayLoad;
        m_pbKitlPayLoad = NULL;
    }

    wprintf (L"Uninitializing Thread (ID): 0x%x handling cookie %c\n", 
        dwThreadID,
        m_cData);

    return hr;
}

/******************************************************************************
//  Function Name:  CKITLStressDesk::RunTest
//  Description:
//
//  Input:
//      None
//  Output:
//      The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLStressDesk::RunTest()
{

    HRESULT hr = E_FAIL;

    hr = Init();
    if (FAILED(hr))
    {
        goto Exit;
    }

    hr = BulkSend();
    if (FAILED(hr))
    {
        goto Exit;
    }


    hr = BulkRcv();
    if (FAILED(hr))
    {
        goto Exit;
    }

    hr = S_OK;

Exit:
    Uninit();

    return hr;

}


