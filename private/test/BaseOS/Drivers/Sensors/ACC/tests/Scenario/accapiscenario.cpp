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

#include "AccApiScenario.h"
#include "AccApiFuncWrap.h"
#include "AccHelper.h"

#define ACC_EVENT_NAME1 _T("ACC_TEST_EVENT_1")
#define ACC_EVENT_NAME2 _T("ACC_TEST_EVENT_2")
#define ACC_EVENT_NAME_BLK _T("ACC_TEST_EVENT_BLK")


AccSampleCollection_t g_SampleReference1 = {0, 0, NULL};
AccSampleCollection_t g_SampleReference2 = {0, 0, NULL};


extern AccTestParam g_AccParam;
extern HANDLE g_hPwrReq;


static BOOL bLargeDelta = FALSE;


//------------------------------------------------------------------------------
// There's rarely much need to modify the remaining two functions
// (DllMain and ShellProc) unless you need to debug some very
// strange behavior, or if you are doing other customizations.
//
BOOL WINAPI DllMain( HANDLE hInstance, ULONG dwReason, LPVOID lpReserved )
{
    LOG_START();
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(lpReserved);

    BOOL bResult = TRUE;
    switch( dwReason )
    {
        case DLL_PROCESS_ATTACH:
            LOG( "DLL_PROCESS_ATTACH(%d)", dwReason );
            break;
        case DLL_PROCESS_DETACH:
            LOG( "DLL_PROCESS_DETACH(%d)", dwReason );
            break;
        case DLL_THREAD_ATTACH:
            LOG( "DLL_THREAD_ATTACH(%d)", dwReason );
            break;
        case DLL_THREAD_DETACH:
            LOG( "DLL_THREAD_DETACH(%d)", dwReason );
            break;
        default:
            LOG_WARN( "Unknown Dll Call Reason - Reason: %d", dwReason );
            bResult = FALSE;
            break;
    }
    LOG_END();
    return bResult;

}//DllMain


//------------------------------------------------------------------------------
// tst_Callback1
//
void tst_Callback1( ACC_DATA* acc_data )
{
    LOG_START();

    if( acc_data )
    {
        AccDumpSample(*acc_data);
    }
    else
    {
        LOG_WARN( "Callback called with invalid data pointer" );
        goto DONE;
    }

    if( g_SampleReference1.pSamples == NULL )
    {
        g_SampleReference1.dwMaxSamples = 200;
        g_SampleReference1.pSamples = (ACC_DATA*)malloc( g_SampleReference1.dwMaxSamples  * sizeof( ACC_DATA ) );
    }

    //Add current sample to our collection
    DWORD idx = g_SampleReference1.dwSampleCount;
    if( idx < g_SampleReference1.dwMaxSamples )
    {

        if( memcpy_s( &(g_SampleReference1.pSamples[idx]), g_SampleReference1.dwMaxSamples - idx , acc_data, sizeof(ACC_DATA) ) )
        {
            LOG_WARN( "Could not copy sample to result collection" );
        }
        else
        {
            g_SampleReference1.dwSampleCount++;
        }
    }
    else
    {
        LOG_WARN( "Could not copy sample to result collection - test buffer is full with %d items", (idx+1) );
    }


    HANDLE hEvent = CreateEvent( NULL, FALSE, FALSE, ACC_EVENT_NAME1 );
    if( hEvent != NULL )
    {
        SetEvent( hEvent );
    }
    else
    {
        LOG_WARN( "Could not open event" );
    }

DONE:
    LOG_END();
}//tst_Callback1


//------------------------------------------------------------------------------
// tst_Callback2
//
void tst_Callback2( ACC_DATA* acc_data )
{
    LOG_START();

    if( acc_data )
    {
        AccDumpSample(*acc_data);
    }
    else
    {
        LOG_WARN( "Callback called with invalid data pointer" );
        goto DONE;
    }

    if( g_SampleReference2.pSamples == NULL )
    {
        g_SampleReference2.dwMaxSamples = 200;
        g_SampleReference2.pSamples = (ACC_DATA*)malloc( g_SampleReference2.dwMaxSamples  * sizeof( ACC_DATA ) );
    }

    //Add current sample to our collection
    DWORD idx = g_SampleReference2.dwSampleCount;
    if( idx < g_SampleReference2.dwMaxSamples )
    {
        if( memcpy_s( &(g_SampleReference2.pSamples[idx]), g_SampleReference2.dwMaxSamples - idx , acc_data, sizeof(ACC_DATA) ) )
        {
            LOG_WARN( "Could not copy sample to result collection" );
        }
        else
        {
            g_SampleReference2.dwSampleCount++;
        }
    }
    else
    {
        LOG_WARN( "Could not copy sample to result collection - test buffer is full with %d items", (idx+1) );
    }


    HANDLE hEvent = CreateEvent( NULL, FALSE, FALSE, ACC_EVENT_NAME2 );
    if( hEvent != NULL )
    {
        SetEvent( hEvent );
    }
    else
    {
        LOG_WARN( "Could not open event" );
    }
DONE:

    LOG_END();

}//tst_Callback2


//------------------------------------------------------------------------------
// tst_CallbackBlock
//
void tst_CallbackBlock( ACC_DATA* acc_data )
{
    LOG_START();
    static DWORD dwSampleCount = 0;
    static DWORD dwLastTime = 0;
    static DWORD dwPrevTime = 0;
    static DOUBLE dwAvgDelta = 0;

    static const DWORD MAX_SAMPLES = 500;


    if( acc_data )
    {
        dwSampleCount++;
    }
    else
    {
        LOG_WARN( "Callback called with invalid data pointer" );
        goto DONE;
    }

    if( dwSampleCount == 2 )
    {
        //Initialize our average timestamp delta...
        dwAvgDelta = ( acc_data->hdr.dwTimeStampMs - dwLastTime );
    }
    else if( dwSampleCount <=  10 )
    {

        dwAvgDelta = ( ( acc_data->hdr.dwTimeStampMs - dwLastTime ) + dwAvgDelta ) * 0.5;

        if( dwSampleCount == 10 )
        {
            //We got enough good samples... save the time-stamp and go to
            //sleep for a while... hoping to back up the queue...
            dwLastTime = acc_data->hdr.dwTimeStampMs;

            DWORD dwSleepInterval = 100 * (DWORD)dwAvgDelta;

            LOG( "Sleeping for roughly 100 sample intervals (%d)", dwSleepInterval );
            Sleep( dwSleepInterval );
            LOG( "Yawn! Resuming...." );
        }
    }
    else if( dwSampleCount > 10 && dwSampleCount < MAX_SAMPLES )
    {
        //After waking up lets collect several hundred samples
        //and verify their time stamps, the expectation is that
        //we will see samples whose timestamp follows the original last
        //sample at delta intervalas followed by a very large delta jump.

        DWORD dwDelta = acc_data->hdr.dwTimeStampMs - dwLastTime;
        dwLastTime = acc_data->hdr.dwTimeStampMs;


        LOG( "[DELTA] Last: %d, Goal: %f", dwDelta, dwAvgDelta);

        if( dwDelta > (5 * dwAvgDelta) )
        {
            bLargeDelta = TRUE;
            LOG( "[DELTA] Large Delta Detected" );
            dwSampleCount = MAX_SAMPLES + 1;
        }
    }
    else if( dwSampleCount > MAX_SAMPLES )
    {
        LOG( "Enough samples received, signaling main test thread..." );

        HANDLE hEvent = CreateEvent( NULL, FALSE, FALSE, ACC_EVENT_NAME_BLK );
        if( hEvent != NULL )
        {
            SetEvent( hEvent );
        }
        else
        {
            LOG_WARN( "Could not open event" );
        }
    }

    dwLastTime = acc_data->hdr.dwTimeStampMs;
DONE:
    LOG_END();
}//tst_CallbackBlock




//------------------------------------------------------------------------------
// tst_CallbackStats
//
void tst_CallbackStats( ACC_DATA* acc_data )
{

    static DWORD dwSampleCount = 0;

    if( acc_data )
    {
        dwSampleCount++;

        if( dwSampleCount < 1000 )
        {
            AccDumpSampleToFile( acc_data );
        }
        else
        {
            AccDumpSampleToFile( acc_data , TRUE );

            LOG( "Enough samples received, signaling main test thread..." );

            HANDLE hEvent = CreateEvent( NULL, FALSE, FALSE, ACC_EVENT_NAME_BLK );
            if( hEvent != NULL )
            {
                SetEvent( hEvent );
            }
            else
            {
                LOG_WARN( "Could not open event" );
            }
        }
    }
    else
    {
        LOG_WARN( "Callback called with invalid data pointer" );
        return;
    }
}//tst_CallbackStats



//------------------------------------------------------------------------------
// tst_StartScenario01
//
void tst_StartScenario01( CClientManager::ClientPayload* pPayload )
{
    LOG_START();
    BOOL bResult = TRUE;
    BOOL bStarted = FALSE;

    LUID sensorLuid = {0};
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    HANDLE hQ1 = INVALID_HANDLE_VALUE;
    HANDLE hQ2 = INVALID_HANDLE_VALUE;

    //Create two nominal queues
    if( !CreateNominalMessageQueue(hQ1) )
    {
        LOG_ERROR( "Could not create sensor message queue 1" );
    }

    if( !CreateNominalMessageQueue(hQ2) )
    {
        LOG_ERROR( "Could not create sensor message queue 2" );
    }

    //Open Sensor
    if( !AccGetHandle(hAcc, sensorLuid))
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }

    //Start sensor with Queue 1
    if( ERROR_SUCCESS != f_AccelerometerStart(hAcc, hQ1) )
    {
        LOG_ERROR( "Could not Start Accelerometer" );
    }
    bStarted = TRUE;

    //Verify Samples are being received
    if( !AccSampleVerify( hQ1, sensorLuid ) )
    {
        LOG_ERROR( "Could not verify samples for Q1. Check log for details." );
    }

    //Call Start with same sensor handle and Queue 2
    if( ERROR_SUCCESS == f_AccelerometerStart(hAcc, hQ2) )
    {
        if( ERROR_SUCCESS != f_AccelerometerStop(hAcc) )
        {
             LOG_WARN( "Could not Stop Accelerometer" );
        }
        LOG_ERROR( "Second Q associated with same sensor handle" );
    }

    //Verify Queue 2 is not receiving samples
    if( !AccSampleVerify( hQ2, sensorLuid, FALSE ) )
    {
        LOG_ERROR( "Unexpected Samples Received in Q2." );
    }


    //Verify Queue 1 is still receiving samples
    if( !AccSampleVerify( hQ1, sensorLuid ) )
    {
        LOG_ERROR( "Could not verify samples for Q1. Check log for details." );
    }

    //Cleanup
    //Start sensor with Queue 1
    if( ERROR_SUCCESS != f_AccelerometerStop(hAcc) )
    {
        LOG_ERROR( "Could not Stop Accelerometer" );
    }
    bStarted = FALSE;

    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQ1);
    CHECK_CLOSE_Q_HANDLE(hQ2);

    //Do smoke test
    if( !AccNominalScenarioCheck( ) )
    {
        LOG_ERROR( "Could not pass nominal scenario check" );
    }

DONE:

    if( bStarted )
    {
        if( ERROR_SUCCESS != f_AccelerometerStop(hAcc) )
        {
            LOG_ERROR( "Could not Stop Accelerometer" );
        }
    }

    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQ1);
    CHECK_CLOSE_Q_HANDLE(hQ2);
    pPayload->dwOutVal = bResult;
    LOG_END();
}//tst_StartScenario01


//------------------------------------------------------------------------------
// tst_StartScenario02
//
void tst_StartScenario02( CClientManager::ClientPayload* pPayload )
{
    LOG_START();
    BOOL bResult = TRUE;
    BOOL bStarted = FALSE;
    LUID sensorLuid1 = {0};
    LUID sensorLuid2 = {0};
    HSENSOR hAcc1 = ACC_INVALID_HANDLE_VALUE;
    HSENSOR hAcc2 = ACC_INVALID_HANDLE_VALUE;
    HANDLE hQ = INVALID_HANDLE_VALUE;

    //Create nominal queues
    if( !CreateNominalMessageQueue(hQ) )
    {
        LOG_ERROR( "Could not create sensor message queue 1" );
    }

    //Open Sensor
    if( !AccGetHandle(hAcc1, sensorLuid1))
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }

    //Start sensor with Queue 1
    if( ERROR_SUCCESS != f_AccelerometerStart(hAcc1, hQ) )
    {
        LOG_ERROR( "Could not Start Accelerometer" );
    }
    bStarted = TRUE;

    //Verify Samples are being received
    if( !AccSampleVerify( hQ, sensorLuid1 ) )
    {
        LOG_ERROR( "Could not verify samples for Sensor 1. Check log for details." );
    }

    //Open second handle to Sensor
    if( !AccGetHandle(hAcc2, sensorLuid2))
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }

    //Start sensor with same Queue - Ensure success
    if( ERROR_SUCCESS == f_AccelerometerStart(hAcc2, hQ) )
    {
         if( ERROR_SUCCESS != f_AccelerometerStop(hAcc1) )
         {
             LOG_WARN( "Could not Stop Accelerometer" );
         }

         LOG_ERROR( "Invalid Accelerometer Start" );
    }

    //Verify Samples are being received for only the first instance
    if( !AccSampleVerify( hQ, sensorLuid1 ) )
    {
        LOG_ERROR( "Could not verify samples for Sensor 1. Check log for details." );
    }

    //Cleanup
    //Stop sensor 1
    if( ERROR_SUCCESS != f_AccelerometerStop(hAcc1) )
    {
        LOG_ERROR( "Could not Stop Accelerometer" );
    }
    bStarted = FALSE;
    CHECK_CLOSE_ACC_HANDLE(hAcc1);
    CHECK_CLOSE_ACC_HANDLE(hAcc2);
    CHECK_CLOSE_Q_HANDLE(hQ);

    //Do smoke test
    if( !AccNominalScenarioCheck( ) )
    {
        LOG_ERROR( "Could not pass nominal scenario check" );
    }

DONE:
    if( bStarted )
    {
        if( ERROR_SUCCESS != f_AccelerometerStop(hAcc1) )
        {
            LOG_ERROR( "Could not Stop Accelerometer" );
        }
    }

    CHECK_CLOSE_ACC_HANDLE(hAcc1);
    CHECK_CLOSE_ACC_HANDLE(hAcc2);
    CHECK_CLOSE_Q_HANDLE(hQ);
    pPayload->dwOutVal = bResult;
    LOG_END();
}//tst_StartScenario02



//------------------------------------------------------------------------------
// tst_QueueScenario01
//
void tst_QueueScenario01( CClientManager::ClientPayload* pPayload )
{
    LOG_START();
    BOOL bResult = TRUE;
    BOOL bStarted = FALSE;
    LUID sensorLuid = {0};
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    HANDLE hQ = INVALID_HANDLE_VALUE;

    MSGQUEUEOPTIONS testQOpt =
    {
        Q_NOM_SIZE,
        0, //Don't allow broken
        10,
        Q_NOM_MAXMSG_SIZE,
        Q_NOM_ACCESS,
    };


    //Special Queue case
    hQ = CreateMsgQueue( _T("TSTQ"), &testQOpt );
    if( hQ == INVALID_HANDLE_VALUE )
    {
        LOG_ERROR( "Could not create message queue" );
    }

    //Nominal handle
    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }

     //Start sensor with Queue 1
    if( ERROR_SUCCESS != f_AccelerometerStart(hAcc, hQ) )
    {
        LOG_ERROR( "Could not Start Accelerometer" );
    }
    bStarted = TRUE;

    //Verify Samples are being received
    if( !AccSampleVerify( hQ, sensorLuid ) )
    {
        LOG_ERROR( "Could not verify samples for Sensor 1. Check log for details." );
    }

    //Attempt to Close Message Queue
    if( !CloseMsgQueue(hQ) )
    {
        LOG_ERROR( "Could not close message queue" );
    }

    //Take a nap
    Sleep( ONE_SECOND );

    //Attempt Stop sensor
    if( ERROR_SUCCESS != f_AccelerometerStop(hAcc) )
    {
        LOG_ERROR( "Could not Stop Accelerometer" );
    }
    bStarted = FALSE;

    //Attempt to close handle
    if( !CloseHandle( (HANDLE)hAcc ) )
    {
        LOG_ERROR( "Could no close handle" );
    }
    hAcc = ACC_INVALID_HANDLE_VALUE;

    //Do smoke test
    if( !AccNominalScenarioCheck( ) )
    {
        LOG_ERROR( "Could not pass nominal scenario check" );
    }

DONE:
    if( bStarted )
    {
        if( ERROR_SUCCESS != f_AccelerometerStop(hAcc) )
        {
            LOG_ERROR( "Could not Stop Accelerometer" );
        }
    }

    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQ);
    pPayload->dwOutVal = bResult;
    LOG_END();
}//tst_QueueScenario01



//------------------------------------------------------------------------------
// tst_QueueScenario02
//
void tst_QueueScenario02( CClientManager::ClientPayload* pPayload )
{
    LOG_START();
    BOOL bResult = TRUE;
    BOOL bStarted = FALSE;
    LUID sensorLuid = {0};
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    HANDLE hQ = INVALID_HANDLE_VALUE;

    MSGQUEUEOPTIONS testQOpt =
    {
        Q_NOM_SIZE,
        Q_NOM_FLAG,
        Q_NOM_MAXMSG,
        (Q_NOM_MAXMSG_SIZE - sizeof(DWORD)),
        Q_NOM_ACCESS,
    };


    //Special Queue case
    hQ = CreateMsgQueue( _T("TSTQ"), &testQOpt );
    if( hQ == INVALID_HANDLE_VALUE )
    {
        LOG_ERROR( "Could not create message queue" );
    }

    //Nominal handle
    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }

    //Start sensor with Queue
    if( ERROR_SUCCESS != f_AccelerometerStart(hAcc, hQ) )
    {
        LOG_ERROR( "Could not Start Accelerometer" );
    }
    bStarted = TRUE;

    //Take a nap
    Sleep( ONE_SECOND );

    //Verify Samples are NOT being received
    if( !AccSampleVerify( hQ, sensorLuid, FALSE ) )
    {
        LOG_ERROR( "Unexpected samples received" );
    }

    //Attempt Stop sensor
    if( ERROR_SUCCESS != f_AccelerometerStop(hAcc) )
    {
        LOG_ERROR( "Could not Stop Accelerometer" );
    }
    bStarted = FALSE;

    //Attempt to close handle
    if( !CloseHandle( (HANDLE)hAcc ) )
    {
        LOG_ERROR( "Could no close handle" );
    }
    hAcc = ACC_INVALID_HANDLE_VALUE;

    //Attempt to Close Message Queue
    if( !CloseMsgQueue(hQ) )
    {
        LOG_ERROR( "Could not close message queue" );
    }

    //Do smoke test
    if( !AccNominalScenarioCheck( ) )
    {
        LOG_ERROR( "Could not pass nominal scenario check" );
    }

DONE:
    if( bStarted )
    {
        if( ERROR_SUCCESS != f_AccelerometerStop(hAcc) )
        {
            LOG_ERROR( "Could not Stop Accelerometer" );
        }
    }

    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQ);
    pPayload->dwOutVal = bResult;
    LOG_END();
}//tst_QueueScenario02



//------------------------------------------------------------------------------
// tst_EndScenario01
//
void tst_EndScenario01( CClientManager::ClientPayload* pPayload )
{
    LOG_START();
    BOOL bResult = TRUE;
    BOOL bStarted = FALSE;
    LUID sensorLuid = {0};
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    HANDLE hQ = INVALID_HANDLE_VALUE;


    //Create two nominal queues
    if( !CreateNominalMessageQueue(hQ) )
    {
        LOG_ERROR( "Could not create sensor message queue 1" );
    }

    //Nominal handle
    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }

    //Start sensor with Queue
    if( ERROR_SUCCESS != f_AccelerometerStart(hAcc, hQ) )
    {
        LOG_ERROR( "Could not Start Accelerometer" );
    }
    bStarted = TRUE;

    //Verify Samples are being received
    if( !AccSampleVerify( hQ, sensorLuid ) )
    {
        LOG_ERROR( "Could not verify samples for Sensor. Check log for details." );
    }

    //Attempt to Close Message Queue
    if( !CloseMsgQueue(hQ) )
    {
        LOG_ERROR( "Could not close message queue" );
    }

    //Take a nap
    Sleep( ONE_SECOND );

    //Attempt Stop sensor
    if( ERROR_SUCCESS != f_AccelerometerStop(hAcc) )
    {
        LOG_ERROR( "Could not Stop Accelerometer" );
    }
    bStarted = FALSE;

    //Attempt to close handle
    if( !CloseHandle( (HANDLE)hAcc ) )
    {
        LOG_ERROR( "Could no close handle" );
    }
    hAcc = ACC_INVALID_HANDLE_VALUE;

    //Cleanup
    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQ);

    //Do smoke test
    if( !AccNominalScenarioCheck( ) )
    {
        LOG_ERROR( "Could not pass nominal scenario check" );
    }

DONE:
    if( bStarted )
    {
        if( ERROR_SUCCESS != f_AccelerometerStop(hAcc) )
        {
            LOG_ERROR( "Could not Stop Accelerometer" );
        }
    }

    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQ);
    pPayload->dwOutVal = bResult;
    LOG_END();
}//tst_EndScenario01

//------------------------------------------------------------------------------
// tst_EndScenario02
//
void tst_EndScenario02( CClientManager::ClientPayload* pPayload )
{
    LOG_START();
    BOOL bResult = TRUE;
    BOOL bStarted = FALSE;
    LUID sensorLuid = {0};
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    HANDLE hQ = INVALID_HANDLE_VALUE;


    //Create two nominal queues
    if( !CreateNominalMessageQueue(hQ) )
    {
        LOG_ERROR( "Could not create sensor message queue 1" );
    }

    //Nominal handle
    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }

    //Start sensor with Queue
    if( ERROR_SUCCESS != f_AccelerometerStart(hAcc, hQ) )
    {
        LOG_ERROR( "Could not Start Accelerometer" );
    }
    bStarted = TRUE;

    //Verify Samples are being received
    if( !AccSampleVerify( hQ, sensorLuid ) )
    {
        LOG_ERROR( "Could not verify samples for Sensor. Check log for details." );
    }

    //Attempt Stop sensor
    if( ERROR_SUCCESS != f_AccelerometerStop(hAcc) )
    {
        LOG_ERROR( "Could not Stop Accelerometer" );
    }
    bStarted = FALSE;

    //Attempt to Close Message Queue
    if( !CloseMsgQueue(hQ) )
    {
        LOG_ERROR( "Could not close message queue" );
    }

    //Start sensor with Queue
    if( ERROR_SUCCESS == f_AccelerometerStart(hAcc, hQ) )
    {
        bStarted = TRUE;
        LOG_ERROR( "Unexpected Start Accelerometer success" );
    }

    //Attempt to close handle
    if( !CloseHandle( (HANDLE)hAcc ) )
    {
        LOG_ERROR( "Could no close handle" );
    }
    hAcc = ACC_INVALID_HANDLE_VALUE;

     //Cleanup
    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQ);

    //Do smoke test
    if( !AccNominalScenarioCheck( ) )
    {
        LOG_ERROR( "Could not pass nominal scenario check" );
    }

DONE:
    if( bStarted )
    {
        if( ERROR_SUCCESS != f_AccelerometerStop(hAcc) )
        {
            LOG_ERROR( "Could not Stop Accelerometer" );
        }
    }

    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQ);
    pPayload->dwOutVal = bResult;
    LOG_END();
}//tst_EndScenario02


//------------------------------------------------------------------------------
// tst_EndScenario03
//
void tst_EndScenario03( CClientManager::ClientPayload* pPayload )
{
    LOG_START();
    BOOL bResult = TRUE;
    BOOL bStarted = FALSE;
    LUID sensorLuid = {0};
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    HANDLE hQ = INVALID_HANDLE_VALUE;


    //Create two nominal queues
    if( !CreateNominalMessageQueue(hQ) )
    {
        LOG_ERROR( "Could not create sensor message queue 1" );
    }

    //Nominal handle
    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }

    //Start sensor with Queue
    if( ERROR_SUCCESS != f_AccelerometerStart(hAcc, hQ) )
    {
        LOG_ERROR( "Could not Start Accelerometer" );
    }
    bStarted = TRUE;

    //Verify Samples are being received
    if( !AccSampleVerify( hQ, sensorLuid ) )
    {
        LOG_ERROR( "Could not verify samples for Sensor. Check log for details." );
    }

    //Attempt to close handle
    if( !CloseHandle( (HANDLE)hAcc ) )
    {
        LOG_ERROR( "Could no close handle" );
    }
    hAcc = ACC_INVALID_HANDLE_VALUE;

    //Take a nap
    Sleep( ONE_SECOND );

    //Attempt Stop sensor
    if( ERROR_SUCCESS != f_AccelerometerStop(hAcc) )
    {
        LOG( "Could not Stop Accelerometer" );
    }
    bStarted = FALSE;

    //Attempt to Close Message Queue
    if( !CloseMsgQueue(hQ) )
    {
        LOG_ERROR( "Could not close message queue" );
    }

    //Cleanup
    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQ);

    //Do smoke test
    if( !AccNominalScenarioCheck( ) )
    {
        LOG_ERROR( "Could not pass nominal scenario check" );
    }

DONE:
    if( bStarted )
    {
        if( ERROR_SUCCESS != f_AccelerometerStop(hAcc) )
        {
            LOG_ERROR( "Could not Stop Accelerometer" );
        }
    }

    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQ);
    pPayload->dwOutVal = bResult;
    LOG_END();
}//tst_EndScenario03


//------------------------------------------------------------------------------
// tst_EndScenario04
//
void tst_EndScenario04( CClientManager::ClientPayload* pPayload )
{
    LOG_START();
    BOOL bResult = TRUE;
    BOOL bStarted = FALSE;
    LUID sensorLuid = {0};
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    HANDLE hQ = INVALID_HANDLE_VALUE;

    //Create two nominal queues
    if( !CreateNominalMessageQueue(hQ) )
    {
        LOG_ERROR( "Could not create sensor message queue 1" );
    }

    //Nominal handle
    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }


    //Attempt Stop sensor
    if( ERROR_SUCCESS == f_AccelerometerStop(hAcc) )
    {
        LOG_ERROR( "Stop Accelerometer Succeeded without Start" );
    }

    //Take a nap
    Sleep( ONE_SECOND );

    //Cleanup
    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQ);

    //Do smoke test
    if( !AccNominalScenarioCheck( ) )
    {
        LOG_ERROR( "Could not pass nominal scenario check" );
    }

DONE:
    if( bStarted )
    {
        if( ERROR_SUCCESS != f_AccelerometerStop(hAcc) )
        {
            LOG_ERROR( "Could not Stop Accelerometer" );
        }
    }

    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQ);
    pPayload->dwOutVal = bResult;
    LOG_END();
}//tst_EndScenario04




//------------------------------------------------------------------------------
// tst_CallbackScenario01
//
void tst_CallbackScenario01( CClientManager::ClientPayload* pPayload )
{
    LOG_START();
    BOOL bResult = TRUE;
    BOOL bStarted = FALSE;
    BOOL bFailed = FALSE;
    DWORD dwWait = 0;
    DWORD dwEventRxCount = 0;
    DWORD dwAttempts = 0;
    LUID sensorLuid = {0};
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    HANDLE hEvent1 = INVALID_HANDLE_VALUE;


    //Nominal handle
    if( !AccGetHandle( hAcc, sensorLuid ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }

    hEvent1 = CreateEvent( NULL, FALSE, FALSE, ACC_EVENT_NAME1 );
    if( hEvent1 == NULL )
    {
        LOG_ERROR( "Could not create test event" );
    }

    if( ERROR_SUCCESS != f_AccelerometerCreateCallback(hAcc, (ACCELEROMETER_CALLBACK*)&tst_Callback1, NULL))
    {
        LOG_ERROR( "Could not attach accelerometer sample callback" );
    }

    //Check if we are actually getting the callback
    do
    {
        dwAttempts++;
        dwWait = WaitForSingleObject( hEvent1, 30 * ONE_SECOND );

        if( dwWait == WAIT_FAILED )
        {
            bFailed = TRUE;
            LOG_ERROR( "Callback Notification: WAIT_FAILED" );
        }
        else if( dwWait == WAIT_TIMEOUT )
        {
            bFailed = TRUE;
            LOG_WARN( "Callback  Sampling Failed (WAIT_TIMEOUT)" );
        }
        else
        {
            LOG( "Accelerometer Callback Received");
        }
    }
    while( dwAttempts < 5 );

    if( bFailed )
    {
        LOG_ERROR( "Callback Notification Failed" );
    }

    if( !f_AccelerometerCreateCallback(hAcc, (ACCELEROMETER_CALLBACK*)&tst_Callback1, NULL))
    {
        LOG_ERROR( "UNKNOWN BEHAVIOR CASE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" );
    }

    //Check if we are actually getting the callback
    bFailed = FALSE;
    dwAttempts = 0;
    do
    {
        dwAttempts++;
        dwWait = WaitForSingleObject( hEvent1, 30 * ONE_SECOND );

        if( dwWait == WAIT_FAILED )
        {
            bFailed = TRUE;
            LOG_ERROR( "Callback Notification: WAIT_FAILED" );
        }
        else if( dwWait == WAIT_TIMEOUT )
        {
            bFailed = TRUE;
            LOG_WARN( "Callback  Sampling Failed (WAIT_TIMEOUT)" );
        }
        else
        {
            LOG( "Accelerometer Callback Received");
        }
    }
    while( dwAttempts < 5 );

    if( bFailed )
    {
        LOG_ERROR( "Callback Notification Failed" );
    }

DONE:

    CHECK_CLOSE_ACC_HANDLE(hAcc);

    pPayload->dwOutVal = bResult;
    LOG_END();
}//tst_CallbackScenario01



//------------------------------------------------------------------------------
// tst_CallbackScenario02
//
void tst_CallbackScenario02( CClientManager::ClientPayload* pPayload )
{
    LOG_START();
    static const DWORD dwInst = 2;
    BOOL bResult = TRUE;
    BOOL bStarted = FALSE;
    BOOL bFailed = FALSE;
    BOOL bDifferent = FALSE;
    DWORD dwWait = 0;
    DWORD dwFailures = 0;
    DWORD dwAttempts = 0;

    LUID sensorLuids[dwInst];
    HSENSOR hAccs[dwInst];
    HANDLE hEvent;

    hEvent = INVALID_HANDLE_VALUE;

    for( DWORD i = 0; i< dwInst; i++ )
    {
        hAccs[i] = ACC_INVALID_HANDLE_VALUE;
    }


    //Nominal handle
    if( !AccGetHandle( hAccs[0], sensorLuids[0] ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle 1" );
    }

    if( !AccGetHandle( hAccs[1], sensorLuids[1] ) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle 2" );
    }

    //callback verification event
    hEvent = CreateEvent( NULL, FALSE, FALSE, ACC_EVENT_NAME1 );
    if( hEvent == NULL )
    {
        LOG_ERROR( "Could not create test event" );
    }

    //Assign callbacks
    if( ERROR_SUCCESS != f_AccelerometerCreateCallback(hAccs[0], (ACCELEROMETER_CALLBACK*)&tst_Callback1, NULL))
    {
        LOG_ERROR( "Could not attach accelerometer sample callback 1" );
    }

    if( ERROR_SUCCESS != f_AccelerometerCreateCallback(hAccs[1], (ACCELEROMETER_CALLBACK*)&tst_Callback1, NULL))
    {
        LOG_ERROR( "Could not attach accelerometer sample callback 1" );
    }

    //Passed the threshold
    bStarted = TRUE;

    //Check if we are actually getting the callback
    do
    {
        LOG( "Callback 1 Loop> Attempts(%d) Failed(%d) Sample( %d of %d ) - Waiting.........",
            dwAttempts, dwFailures, g_SampleReference1.dwSampleCount, g_SampleReference1.dwMaxSamples);

        dwAttempts++;
        dwWait = WaitForSingleObject( hEvent, 30 * ONE_SECOND );

        if( dwWait == WAIT_FAILED )
        {
            dwFailures++;
            LOG_ERROR( "Callback Notification: WAIT_FAILED" );
        }
        else if( dwWait == WAIT_TIMEOUT )
        {
            dwFailures++;
            LOG_WARN( "Callback  Sampling Failed (WAIT_TIMEOUT)" );
        }
        else
        {
            LOG( "Accelerometer Callback Received from event");
        }
    }
    while( dwAttempts < g_SampleReference1.dwMaxSamples );

    if( dwFailures > 0 )
    {
        LOG_ERROR( "Callback Notification Failed" );
    }

    if( g_SampleReference1.dwSampleCount < 2 )
    {
        LOG_ERROR( "Not enough samples were collected to continue test" );
    }

    //At this point the sample array should have been populated from two different
    //sensor handles. Verify by ensuring different LUIDs exist for samples in the
    //sample collection.
    for( DWORD i = 0; (i < g_SampleReference1.dwSampleCount - 1  && !bDifferent ); i++ )
    {
        LOG( "SAMPLE REFERENCE 1: (%l,%l,%l) TS:(%d) LHi:(%l) LLo:(%l)",
            g_SampleReference1.pSamples[i].x,
            g_SampleReference1.pSamples[i].y,
            g_SampleReference1.pSamples[i].z,
            g_SampleReference1.pSamples[i].hdr.dwTimeStampMs,
            g_SampleReference1.pSamples[i].hdr.sensorLuid );

        LOG( "SAMPLE REFERENCE 2: (%l,%l,%l) TS:(%d) LHi:(%l) LLo:(%l)",
            g_SampleReference1.pSamples[i+1].x,
            g_SampleReference1.pSamples[i+1].y,
            g_SampleReference1.pSamples[i+1].z,
            g_SampleReference1.pSamples[i+1].hdr.dwTimeStampMs,
            g_SampleReference1.pSamples[i+1].hdr.sensorLuid );

    //bDifferent = !RtlEqualLuid( &(g_SampleReference1.pSamples[i].luid), &(g_SampleReference1.pSamples[i+1].luid));
    bDifferent = memcmp(&g_SampleReference1.pSamples[i+1].hdr.sensorLuid,
                        &g_SampleReference1.pSamples[i].hdr.sensorLuid,
                        sizeof(LUID)) != 0;

    }

    if( !bDifferent )
    {
        LOG_ERROR( "Did not receive notifications from one of the accelerometer handles" );
    }

DONE:

    if( bStarted )
    {

        //Assign callbacks
        if( ERROR_SUCCESS != f_AccelerometerCancelCallback(hAccs[0]) )
        {
            LOG_ERROR( "Could not cancel accelerometer sample callback 1" );
        }

        if( ERROR_SUCCESS != f_AccelerometerCancelCallback(hAccs[1]))
        {
            LOG_ERROR( "Could not cancel accelerometer sample callback 1" );
        }

        bStarted = FALSE;
    }

    CHECK_CLOSE_ACC_HANDLE(hAccs[0]);
    CHECK_CLOSE_ACC_HANDLE(hAccs[1]);
    CHECK_CLOSE_HANDLE(hEvent);
    pPayload->dwOutVal = bResult;
    LOG_END();
}//tst_CallbackScenario02

//------------------------------------------------------------------------------
// tst_CallbackScenario03
//
void tst_CallbackScenario03( CClientManager::ClientPayload* pPayload )
{
    LOG_START();
    static const DWORD dwInst = 2;
    BOOL bResult = TRUE;
    BOOL bStarted = FALSE;
    BOOL bFailed = FALSE;
    BOOL bDifferent = FALSE;
    DWORD dwWait = 0;
    DWORD dwFailures = 0;
    DWORD dwAttempts = 0;

    LUID sensorLuid = {0};
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    HANDLE hEvents[dwInst];

    for( DWORD i = 0; i< dwInst; i++ )
    {
        hEvents[i] = INVALID_HANDLE_VALUE;
    }

    //Nominal handle
    if( !AccGetHandle( hAcc, sensorLuid) )
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }

    //callback verification event
    hEvents[0] = CreateEvent( NULL, FALSE, FALSE, ACC_EVENT_NAME1 );
    if( hEvents[0] == NULL )
    {
        LOG_ERROR( "Could not create test event 1" );
    }

    hEvents[1] = CreateEvent( NULL, FALSE, FALSE, ACC_EVENT_NAME2 );
    if( hEvents[1] == NULL )
    {
        LOG_ERROR( "Could not create test event 2" );
    }

    //Assign callbacks
    if( ERROR_SUCCESS != f_AccelerometerCreateCallback(hAcc, (ACCELEROMETER_CALLBACK*)&tst_Callback1, NULL))
    {
        LOG_ERROR( "Could not attach accelerometer sample callback 1" );
    }

    if( ERROR_SUCCESS == f_AccelerometerCreateCallback(hAcc, (ACCELEROMETER_CALLBACK*)&tst_Callback2, NULL))
    {
        LOG_ERROR( "Second Callback Creation should have Failed!" );
    }

    //Passed the threshold
    bStarted = TRUE;

    //Check if we are actually getting the callback
    do
    {
        LOG( "Callback 1 Loop> Attempts(%d) Failed(%d) Sample( %d of %d ) - Waiting.........",
            dwAttempts, dwFailures, g_SampleReference1.dwSampleCount, g_SampleReference1.dwMaxSamples);

        dwAttempts++;
        dwWait = WaitForMultipleObjects( dwInst,hEvents,FALSE, 30 * ONE_SECOND );

        if( dwWait == WAIT_FAILED )
        {
            dwFailures++;
            LOG_ERROR( "Callback Notification: WAIT_FAILED" );
        }
        else if( dwWait == WAIT_TIMEOUT )
        {
            dwFailures++;
            LOG_WARN( "Callback  Sampling Failed (WAIT_TIMEOUT)" );
        }
        else
        {
            //Should only be getting events from the first object
            LOG( "Accelerometer Callback Received from event %d", (dwWait-WAIT_OBJECT_0));
            if( (dwWait-WAIT_OBJECT_0) > 1 )
            {
                LOG_ERROR( "Unexpected Callback received" );
            }
        }
    }
    while( dwAttempts < g_SampleReference1.dwMaxSamples );

    if( dwFailures > 0 )
    {
        LOG_ERROR( "Callback Notification Failed" );
    }

    if( g_SampleReference1.dwSampleCount < 2 )
    {
        LOG_ERROR( "Not enough samples were collected to continue test" );
    }

    //At this point the sample array should have been populated from a single
    //sensor handle. Verify by ensuring that only one LUID exist for samples in the
    //sample collection.
    for( DWORD i = 0; (i < g_SampleReference1.dwSampleCount - 1  && !bDifferent ); i++ )
    {
                LOG( "SAMPLE REFERENCE 1: (%l,%l,%l) TS:(%d) LHi:(%l) LLo:(%l)",
                    g_SampleReference1.pSamples[i].x,
                    g_SampleReference1.pSamples[i].y,
                    g_SampleReference1.pSamples[i].z,
                    g_SampleReference1.pSamples[i].hdr.dwTimeStampMs,
                    g_SampleReference1.pSamples[i].hdr.sensorLuid );

                LOG( "SAMPLE REFERENCE 2: (%l,%l,%l) TS:(%d) LHi:(%l) LLo:(%l)",
                    g_SampleReference1.pSamples[i+1].x,
                    g_SampleReference1.pSamples[i+1].y,
                    g_SampleReference1.pSamples[i+1].z,
                    g_SampleReference1.pSamples[i+1].hdr.dwTimeStampMs,
                    g_SampleReference1.pSamples[i+1].hdr.sensorLuid );

            //bDifferent = !RtlEqualLuid( &(g_SampleReference1.pSamples[i].luid), &(g_SampleReference1.pSamples[i+1].luid));
            bDifferent = memcmp(&g_SampleReference1.pSamples[i+1].hdr.sensorLuid,
                                &g_SampleReference1.pSamples[i].hdr.sensorLuid,
                                sizeof(LUID)) != 0;

            }

    if( bDifferent )
    {
        LOG_ERROR( "Received notifications from unexpected callback" );
    }

DONE:

    if( bStarted )
    {

        //Assign callbacks
        if( !f_AccelerometerCancelCallback(hAcc) )
        {
            LOG_ERROR( "Could not cancel accelerometer sample callback 1" );
        }


        bStarted = FALSE;
    }

    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_HANDLE(hEvents[0]);
    CHECK_CLOSE_HANDLE(hEvents[1]);

    pPayload->dwOutVal = bResult;
    LOG_END();
}//tst_CallbackScenario03




//------------------------------------------------------------------------------
// tst_CallbackScenario04
//
void tst_CallbackScenario04( CClientManager::ClientPayload* pPayload )
{
    LOG_START();
    BOOL bResult = TRUE;

//DONE:
    LOG_END();
}//tst_CallbackScenario04


//------------------------------------------------------------------------------
// tst_PowerScenario01
//
void tst_PowerScenario01( CClientManager::ClientPayload* pPayload )
{
    LOG_START();
    BOOL bResult = TRUE;
    BOOL bStarted = FALSE;

    LUID sensorLuid = {0};
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    HANDLE hQ1 = INVALID_HANDLE_VALUE;


    if( !AccSetPowerRequirement(D0) )
    {
        LOG_ERROR( "Could not re-request power requirement" );
    }

    if( !CreateNominalMessageQueue(hQ1) )
    {
        LOG_ERROR( "Could not create sensor message queue 1" );
    }

    if( !AccGetHandle(hAcc, sensorLuid))
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }

    if( ERROR_SUCCESS != f_AccelerometerStart(hAcc, hQ1) )
    {
        LOG_ERROR( "Could not Start Accelerometer" );
    }
    bStarted = TRUE;

    if( !AccSampleVerify( hQ1, sensorLuid ) )
    {
        LOG_ERROR( "Could not verify samples for Q1. Check log for details." );
    }

    //At this point we have an active subscriber receiving samples. Forcibly
    //put the device into D4 power state. But we must first release the TUX
    //level setup of the D0 Power requirement (and reset it later)

    if( !AccReleasePowerRequirement() )
    {
        LOG_ERROR( "Test Setup Failure" );
    }

    Sleep( 500 );

    if( !AccSetSystemPowerState(POWER_STATE_USERIDLE) )
    {
        LOG_ERROR( "Test Setup Failure" );
    }

    Sleep( 500 );

    if( !AccSampleVerify( hQ1, sensorLuid, FALSE ) )
    {
        LOG_ERROR( "Samples Still being received while device is in idle state." );
    }

    if( !AccSetPowerRequirement(D0) )
    {
        LOG_ERROR( "Could not re-request power requirement" );
    }


    if( !AccSetSystemPowerState(POWER_STATE_ON) )
    {
        LOG_ERROR( "Could not enable power" );
    }
        Sleep( 500 );
    if( !AccSampleVerify( hQ1, sensorLuid ) )
    {
        LOG_ERROR( "Could not verify samples for Q1 post-power re-apply. Check log for details." );
    }

    if( ERROR_SUCCESS != f_AccelerometerStop(hAcc) )
    {
        LOG_ERROR( "Could not Stop Accelerometer" );
    }
    bStarted = FALSE;

    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQ1);

    //Do smoke test
    if( !AccNominalScenarioCheck( ) )
    {
        LOG_ERROR( "Could not pass nominal scenario check" );
    }

DONE:

    if( bStarted )
    {
        if( ERROR_SUCCESS != f_AccelerometerStop(hAcc) )
        {
            LOG_ERROR( "Could not Stop Accelerometer" );
        }
    }


    CHECK_CLOSE_ACC_HANDLE(hAcc);
    CHECK_CLOSE_Q_HANDLE(hQ1);

    pPayload->dwOutVal = bResult;
    LOG_END();
}//tst_PowerScenario01


//------------------------------------------------------------------------------
// tst_QueueScenario03
//
void tst_QueueScenario03( CClientManager::ClientPayload* pPayload )
{
    LOG_START();
    BOOL bResult = TRUE;
    HANDLE hEvent = NULL;
    HSENSOR hAcc = NULL;
    LUID sensorLuid = {0};
    ACCELEROMETER_CALLBACK* pFunc = NULL;
    DWORD dwResult = ERROR_SUCCESS;
    DWORD dwLastErr = ERROR_SUCCESS;
    DWORD dwWait = WAIT_FAILED;

    //Create an event to manage our blocking callback
    hEvent = CreateEvent( NULL, FALSE, FALSE, ACC_EVENT_NAME_BLK );
    if( hEvent == NULL )
    {
        LOG_ERROR( "Could not create test event" );
    }
    else
    {
        pFunc = (ACCELEROMETER_CALLBACK*)tst_CallbackBlock;
    }

    //Get an accelerometer handle
    if( !AccGetHandle(hAcc, sensorLuid))
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }


    //Create the callback
    if( f_AccelerometerCreateCallback(hAcc, pFunc, NULL ) != ERROR_SUCCESS)
    {
        LOG_ERROR( "Could not create accelerometer callback" );
    }

    //Wait for callback to tell us that the callback finished the test
    dwWait = WaitForSingleObject( hEvent, 2 * ONE_MINUTE );

    if( dwWait == WAIT_FAILED )
    {
        LOG_ERROR( "Message Queue Notification: WAIT_FAILED" );
    }
    else if( dwWait == WAIT_TIMEOUT )
    {
        LOG_WARN( "Sampling Failed (WAIT_TIMEOUT)" );
    }
    else
    {
        LOG( "Callback Returned" );
        if( !bLargeDelta )
        {
            LOG_ERROR( "Did not detect a large delta indicative of queue sample drop" );
        }
    }

DONE:
    pPayload->dwOutVal = bResult;
    LOG_END();
}//tst_QueueScenario03



//------------------------------------------------------------------------------
// tst_UtilityScenario01
//
void tst_UtilityScenario01( CClientManager::ClientPayload* pPayload )
{
    LOG_START();
    BOOL bResult = TRUE;
    HANDLE hEvent = NULL;
    HSENSOR hAcc = NULL;
    LUID sensorLuid = {0};
    ACCELEROMETER_CALLBACK* pFunc = NULL;
    DWORD dwResult = ERROR_SUCCESS;
    DWORD dwLastErr = ERROR_SUCCESS;
    DWORD dwWait = WAIT_FAILED;

    //Create an event to manage our blocking callback
    hEvent = CreateEvent( NULL, FALSE, FALSE, ACC_EVENT_NAME_BLK );
    if( hEvent == NULL )
    {
        LOG_ERROR( "Could not create test event" );
    }
    else
    {
        pFunc = (ACCELEROMETER_CALLBACK*)&tst_CallbackStats;
    }

    //Get an accelerometer handle
    if( !AccGetHandle(hAcc, sensorLuid))
    {
        LOG_ERROR( "Could not acquire accelerometer handle" );
    }


    //Create the callback
    if( f_AccelerometerCreateCallback(hAcc, pFunc, NULL ) != ERROR_SUCCESS)
    {
        LOG_ERROR( "Could not create accelerometer callback" );
    }

    //Wait for callback to tell us that the callback finished the test
    dwWait = WaitForSingleObject( hEvent, 60 * ONE_SECOND );

    if( dwWait == WAIT_FAILED )
    {
        LOG_ERROR( "Message Queue Notification: WAIT_FAILED" );
    }
    else if( dwWait == WAIT_TIMEOUT )
    {
        LOG_WARN( "Sampling Failed (WAIT_TIMEOUT)" );
    }
    else
    {
        LOG( "Callback Returned" );
    }

DONE:
    pPayload->dwOutVal = bResult;
    LOG_END();
}//tst_UtilityScenario01

