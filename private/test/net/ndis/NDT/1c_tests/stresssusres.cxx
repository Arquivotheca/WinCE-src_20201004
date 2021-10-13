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
#include "StdAfx.h"
#include "ShellProc.h"
#include "NDTNdis.h"
#include "NDTMsgs.h"
#include "NDTError.h"
#include "NDTLog.h"
#include "NDTLib.h"
#include "NDTLibEx.h"
#include "ndt_1c.h"
#include <Pkfuncs.h>
#include <Pm.h>
#include <nkintr.h>
#include <ndis.h>

HANDLE g_hEventStartStress = NULL;
HANDLE g_hEventStopStress = NULL;
HANDLE g_hEventUnblockStress = NULL;
HANDLE g_hEventWaitOnceTestCompleted[2] = { NULL, NULL};
DWORD WINAPI StressSendRecv(LPVOID pIgnored);
DWORD WINAPI StressOIDs(LPVOID pIgnored);

#define        _1_MiliSec  (1)
#define        _1_Sec        (1000 * _1_MiliSec)
#define        _1_Min        (60 * _1_Sec)

HRESULT IsMiniportPowerManagabled(BOOL *pbPMAware);
DWORD SetMiniportPower(CEDEVICE_POWER_STATE Dx);
HRESULT CloseAllProtocolDrivers(HANDLE *ahDriverList);
HRESULT OpenAndBindDriver(HANDLE *ahAdapter);

//------------------------------------------------------------------------------

TEST_FUNCTION(TestStressSusRes)
{
    TEST_ENTRY;

    HRESULT hr = S_OK;
    DWORD rc = TPR_PASS;
    HANDLE hThOIDs = NULL; HANDLE hThSendRecv = NULL;
    DWORD dwThreadId1,dwThreadId2;
    HANDLE ahAdapter = NULL;
    NDIS_MEDIUM ndisMedium = g_ndisMedium;
    UINT  cbAddr = 0;
    UINT  cbHeader = 0;
    BOOL bForce30 = FALSE;
    BOOL bPMAware = FALSE;
    HANDLE ahDriverList[3] = { NULL, NULL, NULL};

    // If we ever controlled the power of device then in the end of test we have to let
    // Power manager to know that we are done with controlling & now on you handle it.
    BOOL bControlledPower = FALSE;

    NDTLogMsg(
        _T("Start 1c_StressSusRes (Stress in Suspend/Resume) test on the adapter %s"), g_szTestAdapter
        );

    g_hEventStartStress = CreateEvent(NULL, TRUE, FALSE, NULL);
    g_hEventStopStress = CreateEvent(NULL, TRUE, FALSE, NULL);
    g_hEventUnblockStress = CreateEvent(NULL, TRUE, FALSE, NULL);
    g_hEventWaitOnceTestCompleted [0] = CreateEvent(NULL, TRUE, FALSE, NULL);
    g_hEventWaitOnceTestCompleted [1] = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (  (g_hEventStartStress == NULL) || (g_hEventStopStress == NULL) || 
        (g_hEventUnblockStress == NULL) || (g_hEventWaitOnceTestCompleted == NULL)) 
        return TPR_FAIL;

    //
    // Find out if the miniport driver supports Power Management
    //
    if (IsMiniportPowerManagabled(&bPMAware) != S_OK)
    {
        NDTLogErr(_T("Failed to query power management capabilities."));
        return TPR_FAIL;
    }

    if (bPMAware == FALSE)
    {
        NDTLogMsg(_T("Miniport is not PM aware. Skipping the test."));
        return TPR_SKIP;
    }

    for (int i = 1; i < 3; i++)
    {
        //
        NDTLogMsg(_T("Opening adapter %d"), i);
        hr = NDTOpen(g_szTestAdapter, &ahAdapter);
        if (FAILED(hr)) {
            ahAdapter = NULL;
            NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
            break;
        }

        NDTLogMsg(_T("Binding adapter %d"), i);
        // Bind
        hr = NDTBind(ahAdapter, bForce30, ndisMedium);
        if (FAILED(hr)) {
            NDTLogErr(g_szFailBind, hr);
            break;
        }

        //add to the cleanup array
        ahDriverList[i] = ahAdapter;
    }

    for(int i = 0; i < 1; i++)
    {

        hThOIDs = CreateThread(NULL,                                // lpThreadAttributes : must be NULL
            0,                                // dwStackSize : ignored
            StressOIDs,                        // lpStartAddress : pointer to function
            &ahDriverList[1],                                // lpParameter : pointer passed to thread
            0,                                // dwCreationFlags 
            &dwThreadId1                    // lpThreadId 
            );

        if (!hThOIDs) break;


        hThSendRecv = CreateThread(NULL,                            // lpThreadAttributes : must be NULL
            0,                            // dwStackSize : ignored
            StressSendRecv,                // lpStartAddress : pointer to function
            &ahDriverList[2],                            // lpParameter : pointer passed to thread
            0,                            // dwCreationFlags 
            &dwThreadId2                // lpThreadId 
            );

        if (!hThSendRecv) break;

        NDTLogVbs(_T("Thread StressOIDs : %d"),hThOIDs);
        NDTLogVbs(_T("Thread StressSendRecv : %d"),hThSendRecv);

        //
        ahAdapter = ahDriverList[0];

        // Get some information about a media
        NDTGetMediumInfo(ndisMedium, &cbAddr, &cbHeader);

        // Let child threads run & initialize
        Sleep(50);
        SetEvent(g_hEventStartStress);
        SetEvent(g_hEventUnblockStress);
        SetEvent(g_hEventWaitOnceTestCompleted[0]);
        SetEvent(g_hEventWaitOnceTestCompleted[1]);
        NDTLogVbs(_T("Stress threads signaled to Start"));
        // Let child threads start stress
        Sleep(1000);

        DWORD dwWait = 1000;
        DWORD dw = 0;
        // 2 suspend/resume the system for say 10 times.
        for (INT i=0;i<20;++i)
        {
            //This is just to make wait time variable.
            switch (i)
            {
            case 5: 
                dwWait = 2000;
                break;
            case 10: 
                dwWait = 4000;
                break;
            case 15: 
                dwWait = 5000;
                break;
            }

            // Block stress test thread for set Power to D4.
            ResetEvent(g_hEventUnblockStress);
            // Make sure stress test thread were blocked.
            Sleep(1000);
            // It will take some time for StressSendRecv thread to complete once NDT test,
            // We wait for it complete so that the adatper handle could be closed safely.
            if((WaitForSingleObject(g_hEventWaitOnceTestCompleted[0], 300000) != WAIT_OBJECT_0) || 
               (WaitForSingleObject(g_hEventWaitOnceTestCompleted[1], 300000) != WAIT_OBJECT_0))
            {
                NDTLogErr(_T("WaitOnceTestCompleted Failed"));
                rc = TPR_FAIL;
                break;
            } 

            hr = CloseAllProtocolDrivers(ahDriverList);
            if (FAILED(hr))
            {
                NDTLogErr(_T("Failed to unbind and close protocol drivers."));
                rc = TPR_FAIL;
                break;
            }

            NDTLogMsg(_T("Transitioning the adapter to D4 state %d time"),i+1);

            dw = SetMiniportPower(D4);
            if (dw != ERROR_SUCCESS)
            {
                NDTLogErr(_T("Failed to Set Power D4 to %s adapter. Error=%d"), g_szTestAdapter, dw);
                rc = TPR_FAIL;
            }
            else
                bControlledPower = TRUE;

            Sleep(1000);
            NDTLogMsg(_T("Transitioning back to D0 state %d time"),i+1);

            dw = SetMiniportPower(D0);
            if (dw != ERROR_SUCCESS)
            {
                NDTLogErr(_T("Failed to Set Power D0 to %s adapter. Error=%d"), g_szTestAdapter, dw);
                rc = TPR_FAIL;
            }
            else
                bControlledPower = TRUE;

            // Set Power D4 to adapter will unbind all protocols on the adapter and 
            // the adapter handle in NDT protocol will closed by NdisCloseAdapter(),
            // Hence, we should obtain adapter handle again 
            if( (S_OK != OpenAndBindDriver(&ahDriverList[1])) || 
                (S_OK != OpenAndBindDriver(&ahDriverList[2])) )
            {
                NDTLogErr(_T("NDTOpenAndBindDriver failed."));
                rc = TPR_FAIL;
            }
            // Unblock stress test thread.
            SetEvent(g_hEventUnblockStress);

            NDTLogMsg(_T("Waiting %d mili sec"), dwWait);
            Sleep(dwWait);
        }

        //Stop the stress test.
        SetEvent(g_hEventStopStress);
        NDTLogVbs(_T("Stress threads signaled to Stop"));

    }

    //If we have ever set the power of device then let PM know that we don't want to control
    //it any more.
    if (bControlledPower)
    {
        DWORD dw = SetMiniportPower(PwrDeviceUnspecified);
        if (dw != ERROR_SUCCESS)
        {
            NDTLogWrn(_T("Failed to Set Power 'PwrDeviceUnspecified' to %s adapter. Error=%d"), g_szTestAdapter, dw);
        }
    }

    // If we have come here as a result of error then Set events for Start & Stop
    SetEvent(g_hEventStartStress);
    SetEvent(g_hEventStopStress);

    DWORD rc_OIDs = TPR_FAIL; DWORD rc_SendRecv = TPR_FAIL;


    NDTLogMsg(_T("Waiting for Stress OIDs thread to terminate"));

    //Wait for threads to terminate
    // Get the exit code from the threads to decide if the test has passed or not.
    SYSTEMTIME SystemTime;

    GetSystemTime(&SystemTime);
    //We have to pause the send receive thread, so increasing the timeout
    NDTLogMsg(_T("Waiting for oid thread at %d:%d:%d.%d for %d"), 
        SystemTime.wHour,
        SystemTime.wMinute,
        SystemTime.wSecond,
        SystemTime.wMilliseconds,
        _1_Min * 5);
    if (WAIT_OBJECT_0 == WaitForSingleObject(hThOIDs, _1_Min * 5))
    {
        GetSystemTime(&SystemTime);
        NDTLogMsg(_T("Oid thread returned at %d:%d:%d.%d"), 
            SystemTime.wHour,
            SystemTime.wMinute,
            SystemTime.wSecond,
            SystemTime.wMilliseconds);
        GetExitCodeThread(hThOIDs, &rc_OIDs);
        NDTLogMsg(_T("Stress OIDs thread exited with exit value : %s"), 
            (rc_OIDs == TPR_PASS)? _T("PASS"):_T("FAIL"));
    }
    else
    {
        TerminateThread(hThOIDs,rc_OIDs);
        NDTLogMsg(_T("Stress OIDs thread terminated...."));
    }


    //We have to pause the send receive thread, so increasing the timeout
    NDTLogMsg(_T("Waiting for StressSendRecv thread to terminate"));
    //Wait for threads to terminate
    GetSystemTime(&SystemTime);
    NDTLogMsg(_T("Waiting for StressSendRecv thread at %d:%d:%d.%d for %d"), 
        SystemTime.wHour,
        SystemTime.wMinute,
        SystemTime.wSecond,
        SystemTime.wMilliseconds,
        _1_Min * 5);
    if (WAIT_OBJECT_0 == WaitForSingleObject(hThSendRecv, _1_Min * 5))
    {
        GetSystemTime(&SystemTime);
        NDTLogMsg(_T("StressSendRecv thread returned at %d:%d:%d.%d"), 
            SystemTime.wHour,
            SystemTime.wMinute,
            SystemTime.wSecond,
            SystemTime.wMilliseconds);
        GetExitCodeThread(hThSendRecv, &rc_SendRecv);
        NDTLogMsg(_T("Stress SendRecv thread exited with exit value : %s"), 
            (rc_SendRecv == TPR_PASS)? _T("PASS"):_T("FAIL"));
    }
    else
    {
        TerminateThread(hThSendRecv,rc_SendRecv);
        NDTLogMsg(_T("Stress SendRecv thread terminated...."));
    }

    // Set the rc accordingly.
    if ( (rc_SendRecv == TPR_FAIL) || (rc_OIDs == TPR_FAIL) || (rc == TPR_FAIL) )
    {
        NDTLogErr(_T("1c_StressSusRes Failed"));
        rc = TPR_FAIL;
    }
    else
        rc = TPR_PASS;

    //Clean up
    hr = CloseAllProtocolDrivers(ahDriverList);
    if (FAILED(hr))
    {
        rc = TPR_FAIL;
        NDTLogErr(_T("Failed to unbind and close protocol drivers."));
    }

    return rc;
}

HRESULT IsMiniportPowerManagabled(BOOL *pbPMAware)
{
    HRESULT hr = S_OK;
    HANDLE hAdapter = NULL;
    BOOL bBound = FALSE;
    NDIS_PNP_CAPABILITIES sPNP = {{0}};
    DWORD dwSize=sizeof(sPNP);
    UINT cbUsed = 0; UINT cbRequired = 0;

    NDTLogMsg(_T("Checking if miniport is PM aware."));

    hr = NDTOpen(g_szTestAdapter, &hAdapter);
    if (FAILED(hr)) {
        hAdapter = NULL;
        NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
        goto cleanUp;
    }

    // Bind
    hr = NDTBind(hAdapter, FALSE, g_ndisMedium);
    if (FAILED(hr)) {
        NDTLogErr(g_szFailBind, hr);
        goto cleanUp;
    }
    bBound = TRUE;

    NDTLogVbs(_T("Querying PNP Capabilities"));
    hr = NDTQueryInfo(
        hAdapter, OID_PNP_CAPABILITIES, PBYTE(&sPNP), dwSize,
        &cbUsed, &cbRequired
        );

    if (FAILED(hr)) {
        NDTLogMsg(_T("%s does not support Power Management, Status: 0x%x"), g_szTestAdapter, NDIS_FROM_HRESULT(hr));
        *pbPMAware = FALSE;
    }
    else
    {
        *pbPMAware = TRUE;
        if (sPNP.Flags)
        {
            NDTLogMsg(_T("%s supports Power Management and")
                _T(" one or more wake-up capabilities"), g_szTestAdapter);
        }
        else
            NDTLogMsg(_T("%s supports only Power Management, but")
            _T(" does not have wake-up capabilities"), g_szTestAdapter);
    }

cleanUp:
    if (bBound)
    {
        hr = NDTUnbind(hAdapter);
        if (FAILED(hr)) {
            NDTLogErr(g_szFailUnbind, hr);
        }
    }
    if (hAdapter != NULL)
    {
        hr = NDTClose(&hAdapter);
        if (FAILED(hr)) {
            NDTLogErr(g_szFailClose, hr);
        }        
    }
    return hr;
}

// returns ERROR_SUCCESS when succeeds.
DWORD SetMiniportPower(CEDEVICE_POWER_STATE Dx)
{
    //CEDEVICE_POWER_STATE Dx = PwrDeviceUnspecified;
    TCHAR                szName[MAX_PATH];        
    int                  nChars;        
    PTCHAR pszAdapter = g_szTestAdapter;
    DWORD dwRet = ERROR_WRITE_FAULT;

    nChars = _sntprintf_s(
        szName, 
        MAX_PATH-1, 
        _TRUNCATE,
        _T("%s\\%s"), 
        PMCLASS_NDIS_MINIPORT, 
        pszAdapter);

    if(nChars != (-1)) 
    {
        dwRet = SetDevicePower(szName, POWER_NAME, Dx);
    }

    return dwRet;
}


DWORD WINAPI StressSendRecv(LPVOID pParam)
{
    DWORD rc = TPR_FAIL;
    HRESULT hr = S_OK;
    NDIS_MEDIUM ndisMedium = g_ndisMedium;
    ULONG ulConversationId = 0;

    HANDLE ahAdapter = *((HANDLE *) pParam);
    HANDLE ahSend = NULL;
    ULONG aulPacketsSent = 0;
    ULONG aulPacketsCompleted = 0;
    ULONG aulPacketsCanceled = 0; 
    ULONG aulPacketsUncanceled = 0;
    ULONG aulPacketsReplied = 0;
#ifndef NDT6   
    ULONG aulTime = 0;
#else
    ULONGLONG aulTime = 0;
#endif
    ULONG aulBytesSend = 0;
    ULONG aulBytesRecv = 0;

    UINT  cbAddr = 0;
    UINT  cbHeader = 0;

    BYTE* pucRandomAddr = NULL;

    BYTE* apucDestAddr[8];

    UINT  uiMaximumFrameSize = 0;
    UINT  uiMaximumTotalSize = 0;
    UINT  uiMinimumPacketSize = 64;
    UINT  uiMaximumPacketSize = 0;

    UINT  uiPacketSize = 64;
    UINT  uiPacketsToSend = 1000;

    NDTLogVbs(_T("StressSendRecv: thread started"));

    if (rand_s((unsigned int*)&ulConversationId) != 0)   {
        NDTLogErr(g_szFailGetConversationId, 0);
        return TPR_FAIL;
    }


    for(int i = 0; i < 1; i++)
    {
        // Zero structures
        memset(apucDestAddr, 0, sizeof(apucDestAddr));

        // Get some information about a media
        NDTGetMediumInfo(ndisMedium, &cbAddr, &cbHeader);

        // Get maximum frame size
        hr = NDTGetMaximumFrameSize(ahAdapter, &uiMaximumFrameSize);
        if (FAILED(hr)) {
            NDTLogErr(g_szFailGetMaximumFrameSize, hr);
            break;
        }

        // Get maximum total size
        hr = NDTGetMaximumTotalSize(ahAdapter, &uiMaximumTotalSize);
        if (FAILED(hr)) {
            NDTLogErr(g_szFailGetMaximumTotalSize, hr);
            break;
        }

        if (uiMaximumTotalSize - uiMaximumFrameSize != cbHeader) {
            hr = E_FAIL;
            NDTLogErr(
                _T("Difference between values above should be %d"), cbHeader
                );
            break;
        }

        NDTLogVbs(_T("StressSendRecv: NDT bound MaximumFrameSize:%d MaximumTotalSize:%d"),uiMaximumFrameSize,uiMaximumTotalSize);
        uiMaximumPacketSize = uiMaximumFrameSize;

        // Generate address used later
        pucRandomAddr = NDTGetRandomAddr(ndisMedium);
        apucDestAddr[0] = pucRandomAddr;

        NDTLogVbs(_T("StressSendRecv: waiting for stress to start"));

        //Wait for 10 sec till StartStress is flaged.
        if (WAIT_OBJECT_0 != WaitForSingleObject(g_hEventStartStress, 10000))
            break;

        NDTLogMsg(_T("StressSendRecv: stress started"));
        uiPacketSize = uiMinimumPacketSize;

        HRESULT HrStress = S_OK;
        DWORD dwRet = 0;
        for(;;)
        {
            HrStress = S_OK;

            dwRet = WaitForSingleObject(g_hEventStopStress, 1);
            if ((WAIT_OBJECT_0==dwRet) || (dwRet == WAIT_FAILED))
            {
                NDTLogMsg(_T("StressSendRecv: signaled to stop"));
                break;
            }
            
            // Check whether master thread call ResetEvent()
            dwRet = WaitForSingleObject(g_hEventUnblockStress, 1);
            if ( WAIT_OBJECT_0 != dwRet )
            {
                NDTLogMsg(_T("StressSendRecv: block for Power D4"));
                if( WaitForSingleObject(g_hEventUnblockStress, 300000) == WAIT_OBJECT_0)
                {
                    NDTLogMsg(_T("StressSendRecv: signaled to unblock"));
                    // renew ahAdapter 
                    ahAdapter = *((HANDLE *) pParam);
                } 
                // Wait timeout or funtion failed
                else
                {
                    // go to wait Stopstress event
                    continue;
                }
            }

            NDTLogMsg(_T("StressSendRecv: Sending %d packets each of %d size to a random address"),uiPacketsToSend,uiPacketSize);

            // Set Power D4 will close the adapter, must let master thread wait untill 
            // send complete. 
            ResetEvent(g_hEventWaitOnceTestCompleted[1]);

            // Send on first instance
            HrStress = NDTSend(
                ahAdapter, cbAddr, NULL, 1, apucDestAddr, NDT_RESPONSE_NONE, 
                NDT_PACKET_TYPE_FIXED, uiPacketSize, uiPacketsToSend, ulConversationId, 0, 0, 
                &ahSend
                );

            if (FAILED(HrStress)) {
                hr = HrStress;
                NDTLogErr(g_szFailSend, hr);
                break;
            }

            // Wait to finish on first
            HrStress = NDTSendWait(
                ahAdapter, ahSend, INFINITE, &aulPacketsSent,
                &aulPacketsCompleted, &aulPacketsCanceled, 
                &aulPacketsUncanceled, &aulPacketsReplied, &aulTime, 
                &aulBytesSend, &aulBytesRecv
                );

            if (FAILED(HrStress)) {
                hr = HrStress;
                NDTLogErr(g_szFailSendWait, hr);
                break;
            }

            NDTLogMsg(_T("StressSendRecv: %d packets each of %d size were SendCompleted"),
                aulPacketsCompleted,uiPacketSize);

            // Notify master thread the test completed.
            SetEvent(g_hEventWaitOnceTestCompleted[1]);

            // Update the packet size
            uiPacketSize += 64;

            if (uiPacketSize > uiMaximumPacketSize)
                uiPacketSize = uiMinimumPacketSize;
            else
                --uiPacketSize; //This just helps to have odd/even packet size alternately.
        }
    }

    // If we have come here as a result of error then Set events 
    SetEvent(g_hEventWaitOnceTestCompleted[1]);

    // We have deside about test pass/fail there
    rc = FAILED(hr) ? TPR_FAIL : TPR_PASS;

    delete pucRandomAddr;

    NDTLogMsg(_T("StressSendRecv: Cleaned up & exiting."));
    return rc;
}

DWORD WINAPI StressOIDs(LPVOID pParam)
{
    DWORD rc = TPR_PASS;
    HRESULT hr = S_OK;
    NDIS_MEDIUM ndisMedium = g_ndisMedium;
    UINT uiPhysicalMedium = 0;
    HANDLE hAdapter = *((HANDLE *) pParam);
    UCHAR * pacBuffer = NULL;
    UINT  cbAddr = 0;
    UINT  cbHeader = 0;
    UINT cbUsed = 0;
    UINT cbRequired = 0;
    NDIS_OID *aSupportedOids = NULL;
    UINT cbSupportedOids = 0;
    NDIS_OID aRequiredOids[256];
    INT cRequiredOids = 0;

    NDTLogVbs(_T("StressOIDs: thread started"));
    for(int i = 0; i < 1; i++)
    {
        // Get some information about a media
        NDTGetMediumInfo(ndisMedium, &cbAddr, &cbHeader);


        hr = NDTGetPhysicalMedium(hAdapter, &uiPhysicalMedium);

        NDTLogVbs(_T("StressOIDs: Get Supported OIDs List"));

        hr = NDTQueryInfo(
            hAdapter, OID_GEN_SUPPORTED_LIST, aSupportedOids, 0,
            &cbUsed, &cbRequired
            );
        if ((NDIS_FROM_HRESULT(hr)) != NDIS_STATUS_INVALID_LENGTH) {
            NDTLogErr(
                _T("Querying OID_GEN_SUPPORTED_LIST returned 0x%08x"),
                hr
                );
            goto cleanUp;
        }

        cbSupportedOids = cbRequired;
        aSupportedOids = new NDIS_OID[cbRequired/sizeof(NDIS_OID)];
        if (!aSupportedOids)
        {
            hr = E_OUTOFMEMORY;
            goto cleanUp;
        }

        hr = NDTQueryInfo(
            hAdapter, OID_GEN_SUPPORTED_LIST, aSupportedOids, cbSupportedOids,
            &cbUsed, &cbRequired
            );
        if (FAILED(hr)) {
            NDTLogErr(
                _T("Querying OID_GEN_SUPPORTED_LIST returned 0x%08x"),
                hr
                );
            goto cleanUp;
        }

        INT cbSupported = cbUsed/sizeof(NDIS_OID);
        NDTLogMsg(_T("StressOIDs: %s supports ** %d ** OIDs"),g_szTestAdapter,cbSupported);

        aRequiredOids[cRequiredOids++] = OID_GEN_SUPPORTED_LIST;
        aRequiredOids[cRequiredOids++] = OID_GEN_HARDWARE_STATUS;
        aRequiredOids[cRequiredOids++] = OID_GEN_MEDIA_SUPPORTED;
        aRequiredOids[cRequiredOids++] = OID_GEN_MEDIA_IN_USE;
        aRequiredOids[cRequiredOids++] = OID_GEN_MAXIMUM_LOOKAHEAD;
        aRequiredOids[cRequiredOids++] = OID_GEN_MAXIMUM_FRAME_SIZE;
        aRequiredOids[cRequiredOids++] = OID_GEN_LINK_SPEED;
        aRequiredOids[cRequiredOids++] = OID_GEN_TRANSMIT_BUFFER_SPACE;
        aRequiredOids[cRequiredOids++] = OID_GEN_RECEIVE_BUFFER_SPACE;
        aRequiredOids[cRequiredOids++] = OID_GEN_TRANSMIT_BLOCK_SIZE;
        aRequiredOids[cRequiredOids++] = OID_GEN_RECEIVE_BLOCK_SIZE;
        aRequiredOids[cRequiredOids++] = OID_GEN_VENDOR_ID;
        aRequiredOids[cRequiredOids++] = OID_GEN_VENDOR_DESCRIPTION;
        aRequiredOids[cRequiredOids++] = OID_GEN_CURRENT_PACKET_FILTER;
        aRequiredOids[cRequiredOids++] = OID_GEN_CURRENT_LOOKAHEAD;
        aRequiredOids[cRequiredOids++] = OID_GEN_DRIVER_VERSION;
        aRequiredOids[cRequiredOids++] = OID_GEN_MAXIMUM_TOTAL_SIZE;
        aRequiredOids[cRequiredOids++] = OID_GEN_MAC_OPTIONS;
        aRequiredOids[cRequiredOids++] = OID_GEN_MEDIA_CONNECT_STATUS;
        aRequiredOids[cRequiredOids++] = OID_GEN_MAXIMUM_SEND_PACKETS;
        aRequiredOids[cRequiredOids++] = OID_GEN_VENDOR_DRIVER_VERSION;

        aRequiredOids[cRequiredOids++] = OID_GEN_XMIT_OK;
        aRequiredOids[cRequiredOids++] = OID_GEN_RCV_OK;
        aRequiredOids[cRequiredOids++] = OID_GEN_XMIT_ERROR;
        aRequiredOids[cRequiredOids++] = OID_GEN_RCV_ERROR;
        aRequiredOids[cRequiredOids++] = OID_GEN_RCV_NO_BUFFER;

        aRequiredOids[cRequiredOids++] = OID_802_3_PERMANENT_ADDRESS;
        aRequiredOids[cRequiredOids++] = OID_802_3_CURRENT_ADDRESS;
        aRequiredOids[cRequiredOids++] = OID_802_3_MULTICAST_LIST;
        aRequiredOids[cRequiredOids++] = OID_802_3_MAXIMUM_LIST_SIZE;
        aRequiredOids[cRequiredOids++] = OID_802_3_RCV_ERROR_ALIGNMENT;
        aRequiredOids[cRequiredOids++] = OID_802_3_XMIT_ONE_COLLISION;
        aRequiredOids[cRequiredOids++] = OID_802_3_XMIT_MORE_COLLISIONS;

        DWORD cbacBuffSize = 8192;
        pacBuffer = (UCHAR *) new UCHAR[cbacBuffSize];
        if (!pacBuffer)
        {
            hr = E_OUTOFMEMORY;
            break;
        }

        NDTLogVbs(_T("StressOIDs: waiting for stress to start"));
        //Wait for 10 sec till StartStress is flaged.
        if (WAIT_OBJECT_0 != WaitForSingleObject(g_hEventStartStress, 10000))
            break;

        NDTLogMsg(_T("StressOIDs: stress started"));

        INT ix=0;
        DWORD dwRet = 0;
        HRESULT HrStress = S_OK;
        for(;;)
        {
            HrStress = S_OK;
            if ((ix % 10) == 0)
            {
                //Check for Stopstress event periodically after n numbers of Queries/Sets
                dwRet = WaitForSingleObject(g_hEventStopStress, 1);
                if ((WAIT_OBJECT_0==dwRet) || (dwRet == WAIT_FAILED)) 
                {
                    NDTLogMsg(_T("StressOIDs: signaled to stop"));
                    break;
                }        
            }
            
            // Check whether master thread call ResetEvent()
            dwRet = WaitForSingleObject(g_hEventUnblockStress, 1);
            if ( WAIT_OBJECT_0 != dwRet )
            {
                NDTLogMsg(_T("StressOIDs: block for Power D4"));
                if( WaitForSingleObject(g_hEventUnblockStress, 300000) == WAIT_OBJECT_0)
                {
                    NDTLogMsg(_T("StressOIDs: signaled to unblock"));
                    // renew hAdapter 
                    hAdapter = *((HANDLE *) pParam);
                } 
                // Wait timeout or funtion failed
                else
                {
                    // go to wait Stopstress event
                    continue;
                }                           
            }
            
            // Set Power D4 will close the adapter, must let master thread wait untill 
            // send complete. 
            ResetEvent(g_hEventWaitOnceTestCompleted[0]);

            HrStress = NDTQueryInfo(hAdapter, aRequiredOids[ix], pacBuffer, cbacBuffSize,
                &cbUsed,&cbRequired);

            if (FAILED(HrStress)) {
                NDTLogErr(_T("StressOIDs: Querying OID failed returned 0x%08x"), HrStress);
                hr = HrStress;
                break;
            }
            // Notify master thread the test completed.
            SetEvent(g_hEventWaitOnceTestCompleted[0]);
        
            ++ix;
            if (ix >= cRequiredOids)
            {
                Sleep(5000);
                ix =0;
            }


        }

    }

cleanUp:
    // If we have come here as a result of error then Set events 
    SetEvent(g_hEventWaitOnceTestCompleted[0]);

    // We have deside about test pass/fail there
    rc = FAILED(hr) ? TPR_FAIL : TPR_PASS;

    if (pacBuffer)
        delete [] pacBuffer;

    if (aSupportedOids)
        delete [] aSupportedOids;

    NDTLogMsg(_T("StressOIDs: Cleaned up & exiting."));
    return rc;
}

HRESULT CloseAllProtocolDrivers(HANDLE *ahDriverList)
{
    HRESULT hr = S_OK;
    int i;
    for (i = 0; i < 3; i++) {
        if (ahDriverList[i] != NULL)
        {
            hr = NDTUnbind(ahDriverList[i]);
            if (FAILED(hr)) {
                NDTLogErr(g_szFailUnbind, hr);
                goto cleanUp;
            }

            hr = NDTClose(&ahDriverList[i]);
            if (FAILED(hr)) {
                NDTLogErr(g_szFailClose, hr);
                goto cleanUp;
            }                 
        }
    }

cleanUp:
    return hr;
}

HRESULT OpenAndBindDriver(HANDLE *ahAdapter)
{
    HRESULT hr = S_OK;
    BOOL bForce30 = FALSE;

    // Open
    hr = NDTOpen(g_szTestAdapter, ahAdapter);
    if (FAILED(hr)) {
        *ahAdapter = NULL;
        NDTLogErr(g_szFailOpen, g_szTestAdapter, hr);
    }

    // Bind
    hr = NDTBind(*ahAdapter, bForce30, g_ndisMedium);
    if (FAILED(hr)) {
        NDTLogErr(g_szFailBind, hr);
    }

    return hr;
}
