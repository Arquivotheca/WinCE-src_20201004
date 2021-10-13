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


#include "KITLStressDev.h"
#include "Log.h"
#include "string.h"

/******************************************************************************
//    Function Name:  CKITLStressDev::CKITLStressDev
//    Description:
//      Constructor
//
//    Input:
//      None
//
//    Output:
//      None
//
******************************************************************************/
CKITLStressDev::CKITLStressDev(const CHAR szServiceName[MAX_SVC_NAMELEN],
        DWORD   dwMinPayLoadSize,
        DWORD   dwMaxPayLoadSize,
        UCHAR   ucFlags,
        UCHAR   ucWindowSize,
        DWORD   dwRcvIterationsCount,
        DWORD   dwRcvTimeout,
        BOOL    fVerifyRcv ,
        DWORD   dwSendIterationsCount,
        CHAR    cData,
        DWORD   dwDelay)
{
    // StringCchCopyExA not available on cebase build env
    strncpy_s(m_szServiceName, szServiceName, _countof(m_szServiceName) -1 );
    m_dwMinPayLoadSize = dwMinPayLoadSize;
    m_dwMaxPayLoadSize = dwMaxPayLoadSize;
    m_ucFlags = ucFlags;
    m_ucWindowSize = ucWindowSize;
    m_dwRcvIterationsCount = dwRcvIterationsCount;

    // Timeout constant is in s.  Convert to ms.
    m_dwRcvTimeout = dwRcvTimeout * 1000;
    m_fVerifyRcv = fVerifyRcv;
    m_dwSendIterationsCount = dwSendIterationsCount;
    m_cData = cData;
    m_dwDelay = dwDelay;

    m_uKITLStrmID = '\0';
    m_pBufferPool = NULL;
    m_pbKitlPayLoad = NULL;

    m_hStopTest = NULL;

}

/******************************************************************************
//    Function Name:  CKITLStressDev::~CKITLStressDev
//    Description:
//      Place holder
//
//    Input:
//      None
//
//    Output:
//      None
//
******************************************************************************/
CKITLStressDev::~CKITLStressDev()
{
}

/******************************************************************************
//    Function Name:  CKITLStressDev::BulkSend
//    Description:
//      Send a number of packets to the desktop and wait for an ACK.  Along with that it saves
//      the time slice and the payload size internally.  Verifying the packet content
//      is also performed here but is generally disabled through the test settings
//
//    Input:
//      None
//
//    Output:
//      The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/

HRESULT CKITLStressDev::BulkSend()
{
    HRESULT hr = E_FAIL;
    DWORD dwRet = 0;
    BOOL bRet = FALSE;

    DWORD dwTickStart;
    DWORD dwTickEnd;

    m_dwSendTickDelta = 0;
    m_dwDataSentSize = 0;
    m_dwSendThroughput = 0;
    DWORD dwACK = 0;
    DWORD dwACKSize = sizeof(dwACK);

    // Verify that Desktop component is ready to start test
    bRet = CallEdbgRecv(m_uKITLStrmID,
        (UCHAR *)&dwACK,
        &dwACKSize,
        m_dwRcvTimeout);
    if (!bRet)
    {
        FAILLOGA(L"%d: Desktop READY ACK not Received within %dms on thread serving payload %d",
            m_uKITLStrmID,
            m_dwRcvTimeout,
            m_pbKitlPayLoad[0]);
        hr = E_FAIL;
        goto Exit;
    }

    if (dwACK != 0)
    {
        FAILLOGA(L"%d:Invalid ACK %d Received by thread servicing payload %d",
            m_uKITLStrmID,
            dwACK,
            m_pbKitlPayLoad[0]);
        hr = E_UNEXPECTED;
        goto Exit;
    }

    dwTickStart = GetTickCount();

    // Send data to the desktop
    for( DWORD j = 0; j < m_dwSendIterationsCount; j++)
    {
        for (DWORD i = m_dwMinPayLoadSize; i < m_dwMaxPayLoadSize; i++)
        {
            bRet = CallEdbgSend(m_uKITLStrmID, m_pbKitlPayLoad, i);
            if (!bRet)
            {
                FAILLOGA(L"%d:Failed to send packet of size %d by thread servicing payload %d",
                    m_uKITLStrmID,
                    i,
                    m_pbKitlPayLoad[0]);
                hr = E_FAIL;
                goto Exit;
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

    dwTickEnd = GetTickCount();

    // Verify that the desktop received the expected number of packets
    bRet = CallEdbgRecv(m_uKITLStrmID,
        (UCHAR *)&dwACK,
        &dwACKSize,
        m_dwRcvTimeout);
    if (!bRet)
    {
        FAILLOGA(L"%d:Desktop DONE ACK not Received within %dms on thread servicing payload %c",
            m_uKITLStrmID,
            m_dwRcvTimeout,
            m_pbKitlPayLoad[0]);
        hr = E_FAIL;
        goto Exit;
    }

    if (dwACK != m_dwDataSentSize)
    {
        FAILLOGA(L"%d:Device sent %dBytes while desktop received %dBytes",
            m_uKITLStrmID,
            m_dwDataSentSize,
            dwACK);
        hr = E_UNEXPECTED;
        goto Exit;
    }


    m_dwSendTickDelta = GetTickDetla(dwTickStart, dwTickEnd);

    m_dwSendThroughput = ComputeThroughput(m_dwDataSentSize, m_dwSendTickDelta);


    hr = S_OK;

Exit:

    return hr;
}

/******************************************************************************
//    Function Name:  CKITLStressDev::BulkRcv
//    Description:
//      Receives a number of packets from the desktop then sends an ACK. Along with that it also
//      saves the time slice and the payload size internally.  Verifying the packet content
//      is also performed here but is generally disabled through the test settings
//
//    Input:
//      None
//
//    Output:
//      The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLStressDev::BulkRcv()
{
    HRESULT hr = E_FAIL;
    BOOL bRet = FALSE;
    DWORD dwRet = 0;

    DWORD dwBuffSize;

    DWORD dwTickStart;
    DWORD dwTickEnd;

    m_dwRcvTickDelta = 0;
    m_dwDataRcvSize = 0;
    m_dwRcvThroughput = 0;

    DWORD dwACK = 0;
    DWORD dwACKSize = sizeof(dwACK);

    // Verify that Desktop component is ready to start test
    bRet = CallEdbgRecv(m_uKITLStrmID,
        (UCHAR *)&dwACK,
        &dwACKSize,
        m_dwRcvTimeout);
    if (!bRet)
    {
        FAILLOGA(L"%d: Desktop READY ACK not Received within %dms on thread serving payload %c",
            m_uKITLStrmID,
            m_dwRcvTimeout,
            m_pbKitlPayLoad[0]);
        hr = E_FAIL;
        goto Exit;
    }

    if (dwACK != 0)
    {
        FAILLOGA(L"%d:Invalid ACK %d Received by thread servicing payload %c",
            m_uKITLStrmID,
            dwACK,
            m_pbKitlPayLoad[0]);
        hr = E_UNEXPECTED;
        goto Exit;
    }

    dwTickStart = GetTickCount();

    // Receive Data from the desktop
    for( DWORD j = 0; j < m_dwRcvIterationsCount; j++)
    {
        for (DWORD i = m_dwMinPayLoadSize; i < m_dwMaxPayLoadSize; i++)
        {
            dwBuffSize = m_dwMaxPayLoadSize;
            bRet = CallEdbgRecv(m_uKITLStrmID,
                m_pbKitlPayLoad,
                &dwBuffSize,
                m_dwRcvTimeout);
            if(!bRet)
            {
                FAILLOGA(L"%d:Failed to receive packet of size %d by thread servicing payload %c",
                    m_uKITLStrmID,
                    i,
                    m_pbKitlPayLoad[0]);
                hr = E_FAIL;
                goto Exit;
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
                    FAILLOGA(L"Buffer of size %d received while expecting buffer of size %d",
                        i,
                        dwBuffSize);
                    hr = E_FAIL;
                    goto Exit;
                }

                for (DWORD dw = 0; dw < i; dw++)
                {
                    if (m_pbKitlPayLoad[dw] != m_cData)
                    {
                        FAILLOGA(L"%d: Data mismatch.  Received %d while expecting %c",
                            m_uKITLStrmID,
                            m_pbKitlPayLoad[dw],
                            m_cData);
                        hr = E_FAIL;
                        goto Exit;
                    }
                }

            }
        }
    }

    dwTickEnd = GetTickCount();

    // Send ACK
    bRet = CallEdbgSend(m_uKITLStrmID,
            (UCHAR *)&m_dwDataRcvSize,
            sizeof(m_dwDataRcvSize));
    if (!bRet)
    {
        FAILLOGA(L"%d: failed to send completion ACK to desktop. Thread Servicing payload %c",
            m_uKITLStrmID,
            m_pbKitlPayLoad[0]);
        hr = E_FAIL;
        goto Exit;
    }

    // Verify that device is ready
    bRet = CallEdbgRecv(m_uKITLStrmID,
        (UCHAR *)&dwACK,
        &dwACKSize,
        m_dwRcvTimeout);
    if (!bRet)
    {
        FAILLOGA(L"%d:Desktop DONE ACK not Received within %dms on thread servicing payload %c",
            m_uKITLStrmID,
            m_dwRcvTimeout,
            m_pbKitlPayLoad[0]);
        hr = E_FAIL;
        goto Exit;
    }

    m_dwRcvTickDelta = GetTickDetla(dwTickStart, dwTickEnd);

    m_dwRcvThroughput = ComputeThroughput(m_dwDataRcvSize, m_dwRcvTickDelta);

    hr = S_OK;

Exit:

    return hr;
}

/******************************************************************************
//    Function Name:  CKITLStressDev::GetTickDetla
//    Description:
//      Given a starting and ending tickcount, this function computes the actual time slice
//      while taking into consideration that the tickcount could overflow
//
//    Input:
//      None
//
//    Output:
//      The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
DWORD CKITLStressDev::GetTickDetla(__in DWORD dwTickStart,
        __in DWORD dwTickEnd)
{
    if (dwTickStart > dwTickEnd)
    {
        return ((DWORD) -1) - dwTickStart + dwTickEnd;
    }
    else
    {
        return (dwTickEnd - dwTickStart);
    }
}
/******************************************************************************
//    Function Name:  CKITLStressDev::ComputeThroughput
//    Description:
//      Compute the throughput given the data size and the time slice it took to transfer it
//
//    Input:
//      __in DWORD dwPayLoadSize: Actual data size transfered
//      __in DWORD dwTickDelta: duration in ms
//
//    Output:
//      The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
DWORD CKITLStressDev::ComputeThroughput(__in DWORD dwPayLoadSize,
        __in DWORD dwTickDelta)
{
    if (!dwTickDelta)
    {
        return 0;
    }
    return (dwPayLoadSize/dwTickDelta);
}

/******************************************************************************
//    Function Name:  CKITLStressDev::
//    Description:
//      Initialize memory, resources along with the KITL stream to be used by this object
//
//    Input:
//      None
//
//    Output:
//      The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLStressDev::Init()
{
    HRESULT hr = E_FAIL;
    BOOL    bRet = FALSE;

    PREFAST_SUPPRESS(25084, TEXT("Warning incorrect on CE/Mobile"));
    m_hStopTest = OpenEvent(EVENT_ALL_ACCESS, NULL, KITL_CANCEL_TEST);
    if (!m_hStopTest)
    {
        FAILLOGV();
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Exit;
    }

    // KITL Priv
    m_pbKitlPayLoad = new BYTE[m_dwMaxPayLoadSize];
    if (!m_pbKitlPayLoad)
    {
        FAILLOGV();
        hr = E_OUTOFMEMORY;
        goto Exit;
    }

    memset(m_pbKitlPayLoad,
        m_cData,
        m_dwMaxPayLoadSize);

    // Initialize the pool size as documented on CallEdbgRegisterClient
    UINT uiPoolSize = m_ucWindowSize * 2 * KITL_MTU;


    m_pBufferPool = new UCHAR[uiPoolSize];
    if (!m_pBufferPool)
    {
        FAILLOGV();
        hr = E_OUTOFMEMORY;
        goto Exit;
    }

    bRet = CallEdbgRegisterClient(&m_uKITLStrmID,
          m_szServiceName,
          m_ucFlags,
          m_ucWindowSize,
          m_pBufferPool);
    if (!bRet)
    {
        FAILLOGV();
        hr = E_FAIL;
        goto Exit;
    }

    hr = S_OK;

Exit:
    if (FAILED(hr))
    {
        Uninit();
    }
    return hr;

}

/******************************************************************************
//    Function Name:  CKITLStressDev::Uninit
//    Description:
//      Free any outstanding resources
//
//    Input:
//      None
//
//    Output:
//      The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLStressDev::Uninit()
{
    HRESULT hr = S_OK;
    BOOL bRet = FALSE;
    if (m_hStopTest)
    {
        CloseHandle(m_hStopTest);
        m_hStopTest = NULL;
    }
    if (m_uKITLStrmID)
    {
        bRet = CallEdbgDeregisterClient(m_uKITLStrmID);
        if (!bRet)
        {
            FAILLOGV();
            hr = E_FAIL;
        }
        m_uKITLStrmID = 0;
    }

    if (m_pBufferPool)
    {
        delete [] m_pBufferPool;
        m_pBufferPool = NULL;
    }

    if (m_pbKitlPayLoad)
    {
        delete [] m_pbKitlPayLoad;
        m_pbKitlPayLoad = NULL;
    }

    return hr;
}

/******************************************************************************
//    Function Name:  CKITLStressDev::

//    Description:
//      This function initializes the test, perform a bulk received followed by a bulk send
//
//    Input:
//      None
//
//    Output:
//      The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLStressDev::RunTest(CKitlPerfLog* pkitlLog)
{

    HRESULT hr = E_FAIL;
    bool fRet = false;

    if (pkitlLog)
    {
        fRet = pkitlLog->PerfInitialize();
        if (fRet == false)
        {
            goto Exit;
        }
    }

    hr = Init();
    if (FAILED(hr))
    {
        goto Exit;
    }

    // KITL Send never returns back if desktop
    // client is not registered so perform bulk Rcv
    // first
    hr = BulkRcv();
    if (FAILED(hr))
    {
        goto Exit;
    }

    PrintRcvPerf();

    hr = BulkSend();
    if (FAILED(hr))
    {
        goto Exit;
    }

    PrintSendPerf();

    if (pkitlLog)
    {
        fRet = pkitlLog->PerfFinalize(GetSendThroughputInKB(), GetRcvThroughputInKB());
        if (fRet == false)
        {
            goto Exit;
        }
    }

    hr = S_OK;

Exit:
    Uninit();

    return hr;

}

/******************************************************************************
//    Function Name:  CKITLStressDev::GetSendThroughputInKB
//    Description:
//      Throughput's returned unit is in LBytes/seconds
//
//    Input:
//      None
//
//    Output:
//      Actual throughput value
//
******************************************************************************/
DWORD CKITLStressDev::GetSendThroughputInKB() const
{
    return m_dwSendThroughput*1000/1024;
}

/******************************************************************************
//    Function Name:  CKITLStressDev::GetSendThroughputInB
//    Description:
//      Throughput's returned unit is in Bytes/seconds
//
//    Input:
//      None
//
//    Output:
//      Actual throughput value
//
******************************************************************************/
DWORD CKITLStressDev::GetSendThroughputInB() const
{
    return m_dwSendThroughput*1000;
}

/******************************************************************************
//    Function Name:  CKITLStressDev::GetRcvThroughputInKB
//    Description:
//      Throughput's returned unit is in KBytes/seconds
//
//    Input:
//      None
//
//    Output:
//      Actual throughput value
//
******************************************************************************/
DWORD CKITLStressDev::GetRcvThroughputInKB() const
{
    return m_dwRcvThroughput*1000/1024;
}
/******************************************************************************
//    Function Name:  CKITLStressDev::GetRcvThroughputInB
//    Description:
//      Throughput's returned unit is in Bytes/seconds
//
//    Input:
//      None
//
//    Output:
//      Actual throughput value
//
******************************************************************************/
DWORD CKITLStressDev::GetRcvThroughputInB() const
{
    return m_dwRcvThroughput*1000;
}

/******************************************************************************
//    Function Name:  CKITLStressDev::
//    Description:
//
//
//    Input:
//      None
//
//    Output:
//      The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLStressDev::GetSendLatency(__deref DWORD *pdwTickCount,
        __deref DWORD *pdwPacketCount) const
{
    HRESULT hr = E_FAIL;
    DWORD dwPacketCount = 0;

    if (!pdwTickCount || !pdwPacketCount)
    {
        hr = E_INVALIDARG;
        goto Exit;
    }
    if (!m_dwSendTickDelta)
    {
        hr = E_UNEXPECTED;
        goto Exit;
    }
    else if (m_dwMaxPayLoadSize <= m_dwMinPayLoadSize)
    {
        hr = E_UNEXPECTED;
        goto Exit;
    }
    else if(!m_dwSendIterationsCount)
    {
        hr = E_UNEXPECTED;
        goto Exit;
    }

    dwPacketCount = (m_dwMaxPayLoadSize - m_dwMinPayLoadSize)*m_dwSendIterationsCount;
    if (dwPacketCount < m_dwSendIterationsCount)
    {
        hr = E_UNEXPECTED;
        goto Exit;
    }

    *pdwTickCount = m_dwSendTickDelta;
    *pdwPacketCount = dwPacketCount;

    hr = S_OK;

Exit:
    return hr;
}

/******************************************************************************
//    Function Name:  CKITLStressDev::GetRcvLatency
//    Description:
//      This function verifies the actual state of this object and computes/extract the number o
//      packets sent and its coresponding tickcount delta in ms
//
//    Input:
//      __deref DWORD *pdwTickCount: Duration in ms
//      __deref DWORD *pdwPacketCount: number of packets sent
//
//    Output:
//      The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLStressDev::GetRcvLatency(__deref DWORD *pdwTickCount,
        __deref DWORD *pdwPacketCount) const
{
    HRESULT hr = E_FAIL;
    DWORD dwPacketCount = 0;

    if (!pdwTickCount || !pdwPacketCount)
    {
        hr = E_INVALIDARG;
        goto Exit;
    }

    if (!m_dwRcvTickDelta)
    {
        hr = E_UNEXPECTED;
        goto Exit;
    }
    else if (m_dwMaxPayLoadSize <= m_dwMinPayLoadSize)
    {
        hr = E_UNEXPECTED;
        goto Exit;
    }
    else if(!m_dwRcvIterationsCount)
    {
        hr = E_UNEXPECTED;
        goto Exit;
    }

    dwPacketCount = (m_dwMaxPayLoadSize - m_dwMinPayLoadSize)*m_dwRcvIterationsCount;
    if (dwPacketCount < m_dwRcvIterationsCount)
    {
        hr = E_UNEXPECTED;
        goto Exit;
    }


    *pdwTickCount = m_dwRcvTickDelta;
    *pdwPacketCount = dwPacketCount;


    hr = S_OK;

Exit:
    return hr;

}

/******************************************************************************
//    Function Name:  CKITLStressDev::PrintSendPerf
//    Description:
//      This functions computes and prints the latency and throughput of KITL.  It is important
//      to note here that the latency collected depends on the KITL stream settings.  For instance
//      if the window size is set to 1 and if KITL performs the ack then the packet will be sent from
//      the device an no other packet, belonging to this stream, will be sent till the desktop
//      acknowledge it.  This leads makes the latency collected a round trop latency.
//      On the other hand, if KITL were not to own the ack and given a large number of packets
//      sent, this function will return the one way latency.
//
//    Input:
//      None
//
//    Output:
//      The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLStressDev::PrintSendPerf()
{
    HRESULT hr = E_FAIL;
    DWORD dwThroughput = 0;

    DWORD dwTickCounts = 0;
    DWORD dwPacketsCount = 0;

    DWORD dwThreadID = GetCurrentThreadId();

    dwThroughput = GetSendThroughputInKB();
    if (dwThroughput)
    {
        LOG(L"Send througput is: %dKB/s for Thread: 0x%x", dwThroughput, dwThreadID);
    }
    else {
        dwThroughput = GetSendThroughputInB();
        if (dwThroughput)
        {
            LOG(L"Send througput is: %dB/s for Thread: 0x%x", dwThroughput, dwThreadID);
        }
        else if(m_dwSendThroughput)
        {
         LOG(L"Send througput is: %dB/ms for Thread: 0x%x", m_dwSendThroughput, dwThreadID);
        }
        else if (m_dwSendTickDelta)
        {
            LOG(L"Send %dBytes in %dms for Thread: 0x%x", m_dwDataSentSize, m_dwSendTickDelta, dwThreadID);
        }
        else
        {
            LOG(L"Send %dBytes in less then 1 ms for Thread: 0x%x", m_dwDataSentSize, dwThreadID);
        }
    }


    hr = GetSendLatency(&dwTickCounts, &dwPacketsCount);
    if (FAILED(hr))
    {
        goto Exit;
    }

    if (!dwPacketsCount)
    {
        FAILLOGV();
        hr = E_UNEXPECTED;
        goto Exit;
    }
    else if (!dwTickCounts)
    {
        LOG(L"%d packets were sent in less then 1ms for Thread: 0x%x", dwPacketsCount, dwThreadID);
    }
    else if(dwPacketsCount > dwTickCounts)
    {
        LOG(L"%d packets were received in %dms for Thread: 0x%x", dwPacketsCount,dwTickCounts, dwThreadID);
    }

    else
    {
        LOG(L"Send average latency is: %dms for Thread: 0x%x", dwTickCounts/dwPacketsCount, dwThreadID);
    }


    hr = S_OK;

Exit:
    return hr;
}

/******************************************************************************
//    Function Name:  CKITLStressDev::PrintRcvPerf
//    Description:
//      This functions computes and prints the latency and throughput of KITL.  It is important
//      to note here that the latency collected depends on the KITL stream settings.  For instance
//      if the window size is set to 1 and if KITL performs the ack then the packet will be sent from
//      the device an no other packet, belonging to this stream, will be sent till the desktop
//      acknowledge it.  This leads makes the latency collected a round trop latency.
//      On the other hand, if KITL were not to own the ack and given a large number of packets
//      sent, this function will return the one way latency.
//
//    Input:
//      None
//
//    Output:
//      The HRESULT returned describes whether the call succeeded or not
//
******************************************************************************/
HRESULT CKITLStressDev::PrintRcvPerf()
{
    HRESULT hr = E_FAIL;
    DWORD dwThroughput = 0;

    DWORD dwTickCounts = 0;
    DWORD dwPacketsCount = 0;

    DWORD dwThreadID = GetCurrentThreadId();

    dwThroughput = GetRcvThroughputInKB();
    if(dwThroughput)
    {
        LOG(L"Rcv througput is: %dKB/s for Thread: 0x%x", dwThroughput, dwThreadID);
    }
    else {

        dwThroughput = GetRcvThroughputInB();
        if(dwThroughput)
        {
            LOG(L"Rcv througput is: %dB/s for Thread: 0x%x", dwThroughput, dwThreadID);
        }
        else if(m_dwRcvThroughput)
        {
            LOG(L"Rcv througput is: %dB/ms for Thread: 0x%x", m_dwRcvThroughput, dwThreadID);
        }
        else if (m_dwRcvTickDelta)
        {
            LOG(L"Rcv %dBytes in %dms for Thread: 0x%x", m_dwDataRcvSize, m_dwRcvTickDelta, dwThreadID);
        }
        else
        {
            LOG(L"Rcv %dBytes in less then 1 ms for Thread: 0x%x", m_dwDataSentSize, dwThreadID);
        }
    }


    hr = GetRcvLatency(&dwTickCounts, &dwPacketsCount);
    if (FAILED(hr))
    {
        goto Exit;
    }

    if (!dwPacketsCount)
    {
        FAILLOGV();
        hr = E_UNEXPECTED;
        goto Exit;
    }
    else if (!dwTickCounts)
    {
        LOG(L"%d packets were received in less then 1ms for Thread: 0x%x", dwPacketsCount, dwThreadID);
    }
    else if(dwPacketsCount > dwTickCounts)
    {
        LOG(L"%d packets were received in %dms for Thread: 0x%x", dwPacketsCount,dwTickCounts, dwThreadID);
    }
    else
    {
        LOG(L"Rcv average latency is: %dms for Thread: 0x%x", dwTickCounts/dwPacketsCount, dwThreadID);
    }


    hr = S_OK;

Exit:
    return hr;
}

