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

#include "AccApiFunctional.h"
#include "AccApiFuncWrap.h"
#include "AccHelper.h"
#include <math.h>

extern AccTestParam g_AccParam;


#define ACC_SAMPLE_UPPER_CUTOFF 0.75
#define ACC_SAMPLE_LOWER_CUTOFF 0.25


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
// tst_AccelerometerSetMode
//
void tst_AccelerometerSetMode( CClientManager::ClientPayload* pPayload ) 
{
    LOG_START();    
    BOOL bResult = TRUE;
    BOOL bValidQ = FALSE;
    BOOL bValidA = FALSE;
    BOOL bRdRes = FALSE;
    
    DWORD dwResult = ERROR_SUCCESS;
    DWORD dwBytesRead = 0;
    DWORD dwFlags = 0;
    DWORD dwSampleCount = 0;
    DWORD dwWait = WAIT_FAILED;
    DWORD dwRxSampleCount = 0;
    
    ACC_DATA accData = {0};
    ACC_ORIENT accOrient = {0};

    //Do we have what we need....
    VERIFY_STEP("Varifying Client Payload", (pPayload!=NULL), TRUE );
    VERIFY_STEP("Varifying Client Payload Data", (pPayload->pBufIn!=NULL), TRUE );
    VERIFY_STEP("Varifying Client Payload Size", (pPayload->dwLenIn==sizeof(AccSetMode_t)), TRUE );

    //Make life easier
    AccSetMode_t* pData = (AccSetMode_t*)pPayload->pBufIn;
   
    //What are we dealing with
    LOG( "tst_AccelerometerSetMode:" );
    LOG( "Accelerometer Handle..(0x%08X)", pData->hAcc );
    LOG( "Message Queue Handle..(0x%08X)", pData->hQueue );
    LOG( "New Mode..............(0x%08X)", pData->configMode );
    LOG( "Reserved..............(0x%08X)", pData->reserved?pData->reserved:0x00000000);
    LOG( "Expecting O-Samples...(%s)", pData->expOSamples?_T("TRUE"):_T("FALSE"));
    LOG( "Expecting S-Samples...(%s)", pData->expSSamples?_T("TRUE"):_T("FALSE"));
    LOG( "Expected Error........(%d)", pData->expError );


    ((pData->hQueue != INVALID_HANDLE_VALUE)&&(pData->hQueue != NULL))?bValidQ=TRUE:bValidQ=FALSE;
    ((pData->hAcc != INVALID_HANDLE_VALUE)&&(pData->hAcc != NULL))?bValidA=TRUE:bValidA=FALSE;

    //At this time this test only runs in automation using the stub PDD. 
    if( bValidA )
    {
        if( !EnableSimStubRotation( pData->hAcc  ) )
        {
            LOG_ERROR( "Could not initiate rotation simulation in stub PDD" );
        }
    }

    //Mode was allegedly set as expected. Wait to ensure new samples are being
    //received, empty the message queue, and take several readings to make
    //sure we are getting the data we want.     
    for( int phase = 0; phase < 3; phase++ )
    {
        LOG( "Running Phase %d of test", phase );

        if( phase == 1 )
        {
            //Try setting the mode
            dwResult = f_AccelerometerSetMode(pData->hAcc, pData->configMode, pData->reserved);

            if( pData->expError != dwResult )
            {
                LOG_ERROR("AccelerometerSetMode() - Expected:(%d) Received:(%d)", pData->expError, dwResult );
            }
        }
        else 
        {
            if( bValidA )
            {
                //Reset to default
                dwResult = f_AccelerometerSetMode(pData->hAcc, ACC_CONFIG_STREAMING_DATA_MODE, pData->reserved);
            }
        }

        dwSampleCount = 0;
        dwRxSampleCount = 0;
        
        do
        {
            dwSampleCount++;
        
            bRdRes = ReadMsgQueue(
                pData->hQueue,
                (LPVOID) &accData,
                sizeof(accData),
                &dwBytesRead,
                10 * ONE_SECOND,
                &dwFlags);
            dwResult = GetLastError();

            LOG( "MESSAGE TYPE (%d) Sample Count (%d)", accData.hdr.msgType, dwSampleCount );

            
            switch( dwResult )
            {
                case ERROR_PIPE_NOT_CONNECTED:
                    LOG_WARN( "Read Result: ERROR_PIPE_NOT_CONNECTED" );                        
                    break;
        
                case ERROR_INSUFFICIENT_BUFFER:
                    LOG_WARN( "Read Result: ERROR_INSUFFICIENT_BUFFER" );
                    break;
        
                case ERROR_INVALID_HANDLE:
                    LOG( "Read Result: ERROR_INVALID_HANDLE" );
                    if( bValidQ )
                    {
                        LOG_ERROR( "Queue Handle Corruption" );
                    }
                    break;
                      
                case ERROR_TIMEOUT:
                    LOG( "Read Result: ERROR_TIMEOUT" );
                    if( bValidQ )
                    {
                        if( pData->expOSamples || pData->expSSamples )
                        {
                            LOG_ERROR( "Sampling Failed (WAIT_TIMEOUT)" );
                        }
                        else
                        {
                            LOG( "No Unexpected Samples Received" );
                        }
                    }
                    else
                    {
                        if( pData->expOSamples || pData->expSSamples )
                        {
                            LOG_ERROR( "Test Condition Error" );
                        }
                        else
                        {
                            LOG( "No Samples Expected, No Valid Queue Provided" );                    
                        }
                    }
                      
                    //If no valid Accelerometer handle, this should time out.
                    //After the first check exit.
                    if( !bValidA && dwSampleCount > 2 )
                    {
                        //goto DONE;
                        continue;
                    }                       
                    break;
              
                case ERROR_SUCCESS:
                    LOG( "Read Result: ERROR_SUCCESS" );
                    if( !pData->expOSamples && !pData->expSSamples )
                    {                    
                        LOG_WARN( "Unexpected Samples Received - Pre-existing Sample" );
                    }
                    else
                    {                   
                        if( phase != 1 )
                        {                            
                            if( accData.hdr.msgType == MESSAGE_ACCELEROMETER_3D_DATA || accData.hdr.msgType == MESSAGE_ACCELEROMETER_3D_STATE )
                            {
                                if( pData->expSSamples )
                                {
                                    dwRxSampleCount++;
                                    LOG( "Received: MESSAGE_ACCELEROMETER_3D_DATA @ phase %d", phase); 
                                    AccDumpSample( accData );
                                }
                                else
                                {
                                    LOG_ERROR( "Unexpected Sample Received" );
                                }
                            }
                            else if( accData.hdr.msgType == MESSAGE_ACCELEROMETER_3D_DEVICE_ORIENTATION_CHANGE )
                            {
                                AccDumpOrientSample(*(ACC_ORIENT*)(&accData));
                                LOG( "Received Unexpected: MESSAGE_ACCELEROMETER_3D_DEVICE_ORIENTATION_CHANGE @ phase %d", phase); 
                            }
                            else
                            {
                                LOG_ERROR( "Unkown Sample Type Received (0x%08X)", accData.hdr.msgType);
                            }
                        }
                        else
                        {
                            if( pData->expOSamples )
                            {
                                if( accData.hdr.msgType == MESSAGE_ACCELEROMETER_3D_DEVICE_ORIENTATION_CHANGE )
                                {
                                    dwRxSampleCount++;
                                    AccDumpOrientSample(*(ACC_ORIENT*)(&accData));
                                    LOG( "Received: MESSAGE_ACCELEROMETER_3D_DEVICE_ORIENTATION_CHANGE @ phase %d", phase); 
                                }
                                else
                                {
                                    AccDumpSample(accData);
                                    LOG( "Received Unexpected: MESSAGE_ACCELEROMETER_3D_DATA @ phase %d", phase);                                 
                                }                                   
                            }
                            else if( pData->expSSamples )
                            {
                                if( accData.hdr.msgType == MESSAGE_ACCELEROMETER_3D_DATA || accData.hdr.msgType == MESSAGE_ACCELEROMETER_3D_STATE )
                                {
                                    dwRxSampleCount++;
                                    AccDumpSample(accData);
                                    LOG( "Received: MESSAGE_ACCELEROMETER_3D_DATA @ phase %d", phase); 
                                }
                                else
                                {                                
                                    AccDumpSample(accData);
                                    LOG_WARN( "Received Unexpected: MESSAGE_ACCELEROMETER_3D_DEVICE_ORIENTATION_CHANGE @ phase %d", phase);                                 
                                }                                   
                            }
                        }
                    }
                    break;
           
                default:
                    LOG_ERROR( "Read Result: %d", dwResult);
                    break;
            }

            //flush queue
            while( ReadMsgQueue(pData->hQueue,(LPVOID)&accData,sizeof(accData), &dwBytesRead,0,&dwFlags) );


        } 
        while( dwSampleCount < ACC_TEST_SAMPLE_SIZE );
        
        //Rough check to make sure we stopped/started getting samples as expected
        LOG( "Total Samples Rx'd: (%d)   Iteration Count: (%d)", dwRxSampleCount, dwSampleCount);

        if( pData->expOSamples || pData->expSSamples )
        {
            if( dwRxSampleCount == 0 )
            {
//                LOG_ERROR( "No Valid Samples Received" );
            }
        }
              
    }    

DONE:
    pPayload->dwOutVal = bResult;
    
    LOG_END();
}//tst_AccelerometerSetMode



//------------------------------------------------------------------------------
// tst_AccelerometerCancelCallback
//
void tst_AccelerometerCancelCallback( CClientManager::ClientPayload* pPayload ) 
{
    LOG_START();
    BOOL bResult = TRUE;
    HANDLE hEvent = NULL;
    ACCELEROMETER_CALLBACK* pFunc = NULL;    
    DWORD dwResult = ERROR_SUCCESS;
    DWORD dwLastErr = ERROR_SUCCESS;
    DWORD dwWait = WAIT_FAILED;
    DWORD dwSampleCount = 0;
    DWORD dwRxSampleCount = 0;
        
    //Do we have what we need....
    VERIFY_STEP("Varifying Client Payload", (pPayload!=NULL), TRUE );
    VERIFY_STEP("Varifying Client Payload Data", (pPayload->pBufIn!=NULL), TRUE );
    VERIFY_STEP("Varifying Client Payload Size", (pPayload->dwLenIn==sizeof(AccCancelCallTstData_t)), TRUE );

    //Make life easier
    AccCancelCallTstData_t* pData = (AccCancelCallTstData_t*)pPayload->pBufIn;

    //What are we dealing with
    LOG( "tst_AccelerometerCancelCallback:" );
    LOG( "Valid Callback........(%s)", pData->bValidCallback?_T("TRUE"):_T("FALSE"));
    LOG( "Expecting Samples.....(%s)", pData->expSamples?_T("TRUE"):_T("FALSE"));
    LOG( "Accelerometer Handle..(0x%08X)", pData->hAcc );
    LOG( "Expected Error........(%d)", pData->expError );

    if( pData->bValidCallback )
    {

        //Create the event to verify that it is successfully connected
        hEvent = CreateEvent( NULL, FALSE, FALSE, ACC_EVENT_NAME );
        if( hEvent == NULL )
        {
            LOG_ERROR( "Could not create test event" );
        }

        //See if we get signaled that the callback is being called
        dwWait = WaitForSingleObject( hEvent, ACC_MIN_SAMPLE_INTERVAL * 10 );
        if( dwWait == WAIT_TIMEOUT || dwWait == WAIT_FAILED )
        {
            LOG_ERROR("Test Callback not being called as expected (%d)", dwWait);
        }
        else
        {
            LOG("Verified functional callback" );
        }
    }
   
    // Attempt to Cancel it
    dwResult = f_AccelerometerCancelCallback(pData->hAcc );

    if( pData->expError != dwResult )
    {
        LOG_ERROR("tst_AccelerometerCancelCallback() - Expected:(%d) Received:(%d)", pData->expError, dwResult );
    }


    //If we didn't have a valid callback to begin with
    if( !pData->bValidCallback )
    {    
        goto DONE;
    }
    

    //Check if we actually stopped getting the callback
    do
    {
        dwWait = WaitForSingleObject( hEvent, ACC_MIN_SAMPLE_INTERVAL * 10 );
        dwSampleCount++;

        if( dwWait == WAIT_FAILED )
        {
            LOG_ERROR( "Callback Notification: WAIT_FAILED" );
        }
        else if( dwWait == WAIT_TIMEOUT )
        {
            if( pData->expSamples )
            {
                LOG_WARN( "Sampling Failed (WAIT_TIMEOUT)" );
            }
            else
            {
                LOG( "No Sampling as Expected");
            }
        }
        else
        {
            dwRxSampleCount++;
            if( !pData->expSamples )
            {
                LOG_WARN( "Unexpected Samples Received" );
            }
            else
            {
                LOG( "tst_AccelerometerCancelCallback() - Samples received");
            }
        }
    }
    while( dwSampleCount < ACC_TEST_SAMPLE_SIZE );

    //Rough check to make sure we stopped/started getting samples as expected
    LOG( "Total Samples Rx'd: (%d)   Iteration Count: (%d)", dwRxSampleCount, dwSampleCount );
    
    if( pData->expSamples )
    {
        //If we want them, make sure we got at least some of them
        if( dwRxSampleCount < ceil((FLOAT)dwRxSampleCount * ACC_SAMPLE_UPPER_CUTOFF) )
        {
            LOG_ERROR("Cutoff:(%d)" ,ceil((FLOAT)dwRxSampleCount * ACC_SAMPLE_UPPER_CUTOFF) );
        }
    }
    else
    {
        //if we didn't want any make sure we didn't get much
        if( dwRxSampleCount > ceil((FLOAT)dwRxSampleCount * ACC_SAMPLE_LOWER_CUTOFF) )
        {
            LOG_ERROR("Cutoff:(%d)" ,ceil((FLOAT)dwRxSampleCount * ACC_SAMPLE_LOWER_CUTOFF) );
        }
    }
    
    
DONE:
    pPayload->dwOutVal = bResult;
    LOG_END();
}//tst_AccelerometerCancelCallback





//------------------------------------------------------------------------------
// tst_AccelerometerCreateCallback
//
void tst_AccelerometerCreateCallback( CClientManager::ClientPayload* pPayload )
{
    LOG_START();
    BOOL bResult = TRUE;
    HANDLE hEvent = NULL;
    ACCELEROMETER_CALLBACK* pFunc = NULL;
    DWORD dwResult = ERROR_SUCCESS;
    DWORD dwLastErr = ERROR_SUCCESS;
    DWORD dwWait = WAIT_FAILED;
    DWORD dwSampleCount = 0;
    DWORD dwRxSampleCount = 0;
    DWORD dwData = 0;
        
    //Do we have what we need....
    VERIFY_STEP("Varifying Client Payload", (pPayload!=NULL), TRUE );
    VERIFY_STEP("Varifying Client Payload Data", (pPayload->pBufIn!=NULL), TRUE );
    VERIFY_STEP("Varifying Client Payload Size", (pPayload->dwLenIn==sizeof(AccCreateCallTstData_t)), TRUE );

    //Make life easier
    AccCreateCallTstData_t* pData = (AccCreateCallTstData_t*)pPayload->pBufIn;

    //What are we dealing with
    LOG( "tst_AccelerometerCreateCallback:" );
    LOG( "Fake Callback.........(%s)", pData->bFakeCallback?_T("TRUE"):_T("FALSE"));
    LOG( "Valid Callback........(%s)", pData->bValidCallback?_T("TRUE"):_T("FALSE"));
    LOG( "Has Context...........(%s)", pData->bHasContext?_T("TRUE"):_T("FALSE"));
    LOG( "Context Value.........(0x%x)", pData->lpvContext);

    LOG( "Expecting Samples.....(%s)", pData->expSamples?_T("TRUE"):_T("FALSE"));
    LOG( "Accelerometer Handle..(0x%08X)", pData->hAcc );
    LOG( "Expected Error........(%d)", pData->expError );

    if( pData->bValidCallback )
    {
        hEvent = CreateEvent( NULL, FALSE, FALSE, ACC_EVENT_NAME );
        if( hEvent == NULL )
        {
            LOG_ERROR( "Could not create test event" );
        }
        if( pData->bFakeCallback )
        {
            pFunc = (ACCELEROMETER_CALLBACK*)&tst_FakeCallback;
        }
        else
        {
            pFunc = (ACCELEROMETER_CALLBACK*)&tst_Callback;
        }
    }


    //Make the call
    dwResult = f_AccelerometerCreateCallback(pData->hAcc, pFunc, pData->lpvContext );

    if( pData->expError != dwResult )
    {
        LOG_ERROR("Expected:(%d) Received:(%d)", pData->expError, dwResult );
    }

    //If we didn't want a callback.... 
    if( !pData->bValidCallback )
    {    
        goto DONE;
    }
    
    //Check if we are actually getting the callback
    do
    {
        dwWait = WaitForSingleObject( hEvent, ACC_MIN_SAMPLE_INTERVAL * 10 );

        dwSampleCount++;

        if(pData->bHasContext) {
            dwData = GetEventData( hEvent );
            LOG( "Context from event: 0x%x\r\n", dwData );
            if( dwData != (DWORD)CALLBACK_CONTEXT_SIG ) {
                LOG_ERROR( "Callback did not receive expected context\r\n");
            }
        }

        if( dwWait == WAIT_FAILED )
        {
            LOG_ERROR( "Message Queue Notification: WAIT_FAILED" );
        }
        else if( dwWait == WAIT_TIMEOUT )
        {
            if( pData->expSamples )
            {
                LOG_WARN( "Sampling Failed (WAIT_TIMEOUT)" );
            }
            else
            {
                LOG( "No Sampling as Expected");
            }
        }
        else
        {
            dwRxSampleCount++;
            if( !pData->expSamples )
            {
                LOG_WARN( "Unexpected Samples Received" );
            }
            else
            {
                LOG( "Samples received");
            }
        }
    }
    while( dwSampleCount < ACC_TEST_SAMPLE_SIZE );

    //Rough check to make sure we stopped/started getting samples as expected
    LOG( "Total Samples Rx'd: (%d)   Iteration Count: (%d)", dwSampleCount, dwSampleCount );
    
    if( pData->expSamples )
    {
        //If we want them, make sure we got at least some of them
        if( dwRxSampleCount < (dwRxSampleCount * ACC_SAMPLE_UPPER_CUTOFF) )
        {
            LOG_ERROR("Cutoff:(%d)" ,(dwRxSampleCount * ACC_SAMPLE_UPPER_CUTOFF) );
        }
    }
    else
    {
        //if we didn't want any make sure we didn't get much
        if( dwRxSampleCount > (dwRxSampleCount * ACC_SAMPLE_LOWER_CUTOFF) )
        {
            LOG_ERROR("Cutoff:(%d)" ,(dwRxSampleCount * ACC_SAMPLE_LOWER_CUTOFF) );
        }
    }

    
DONE:
    pPayload->dwOutVal = bResult;
    LOG_END();
}//tst_AccelerometerCreateCallback



//------------------------------------------------------------------------------
// tst_AccelerometerStop
//
void tst_AccelerometerStop( CClientManager::ClientPayload* pPayload ) 
{
    LOG_START();    
    BOOL bResult = TRUE;
    DWORD dwResult = ERROR_SUCCESS;
    DWORD dwSampleCount = 0;
    DWORD dwWait = WAIT_FAILED;
    DWORD dwRxSampleCount = 0;
    ACC_DATA accData = {0};
    BOOL bRdRes = FALSE;
    DWORD dwFlags = 0;
    DWORD dwBytesRead = 0;
    BOOL bValidQ = FALSE;
    BOOL bValidA = FALSE;



    //Do we have what we need....
    VERIFY_STEP("Varifying Client Payload", (pPayload!=NULL), TRUE );
    VERIFY_STEP("Varifying Client Payload Data", (pPayload->pBufIn!=NULL), TRUE );
    VERIFY_STEP("Varifying Client Payload Size", (pPayload->dwLenIn==sizeof(AccOpenAccStopTstData_t)), TRUE );

    //Make life easier
    AccOpenAccStopTstData_t* pData = (AccOpenAccStopTstData_t*)pPayload->pBufIn;
    ((pData->hQueue != INVALID_HANDLE_VALUE)&&(pData->hQueue != NULL))?bValidQ=TRUE:bValidQ=FALSE;
    ((pData->hAcc != INVALID_HANDLE_VALUE)&&(pData->hAcc != NULL))?bValidA=TRUE:bValidA=FALSE;
   
    //What are we dealing with
    LOG( "tst_AccelerometerStop:" );
    LOG( "Expecting Samples.....(%s)", pData->expSamples?_T("TRUE"):_T("FALSE"));
    LOG( "Accelerometer Handle..(0x%08X)", pData->hAcc );
    LOG( "Message Queue Handle..(0x%08X)", pData->hQueue );
    LOG( "Expected Error........(%d)", pData->expError );

    //Verify we're getting something
    if(  bValidQ )
    {
        LOG( "Verifying Accelerometer is ON" );
        bRdRes = ReadMsgQueue(
            pData->hQueue,
            (LPVOID) &accData,
            sizeof(accData),
            &dwBytesRead,
            15 * ONE_SECOND,
            &dwFlags);
        if( GetLastError() != ERROR_SUCCESS )
        {
            LOG_ERROR( "No Samples Being Received - Check Device Start" );
        }
    }
        
    //Try stoping it
    dwResult = f_AccelerometerStop(pData->hAcc);

    if( pData->expError != dwResult )
    {
        LOG_ERROR("AccelerometerStop() - Expected:(%d) Received:(%d)", pData->expError, dwResult );
    }
 
    do
    {
        dwSampleCount++;

        bRdRes = ReadMsgQueue(
            pData->hQueue,
            (LPVOID) &accData,
            sizeof(accData),
            &dwBytesRead,
            5 * ONE_SECOND,
            &dwFlags);
        dwResult = GetLastError();

        switch( dwResult )
        {
            case ERROR_PIPE_NOT_CONNECTED:
                LOG_WARN( "Read Result: ERROR_PIPE_NOT_CONNECTED" );                        
                break;

            case ERROR_INSUFFICIENT_BUFFER:
                LOG_WARN( "Read Result: ERROR_INSUFFICIENT_BUFFER" );
                break;

            case ERROR_INVALID_HANDLE:
                LOG( "Read Result: ERROR_INVALID_HANDLE" );
                if( bValidQ )
                {
                    LOG_ERROR( "Queue Handle Corruption" );
                }
                break;
                
            case ERROR_TIMEOUT:
                LOG( "Read Result: ERROR_TIMEOUT" );
                if( bValidQ )
                {
                    if( pData->expSamples )
                    {
                        LOG_WARN( "Sampling Failed (WAIT_TIMEOUT)" );
                    }
                    else
                    {
                        LOG( "No Unexpected Samples Received" );
                    }
                }
                else
                {
                    if( pData->expSamples )
                    {
                        LOG_ERROR( "Test Condition Error" );
                    }
                    else
                    {
                        LOG( "No Samples Expected, No Valid Queue Provided" );                    
                    }
                }
                
                //If no valid Accelerometer handle, this should time out.
                //After the first check exit.
                if( !bValidA && dwSampleCount > 2 )
                {
                    goto DONE;
                }                       
                
                break;
        
            case ERROR_SUCCESS:
                LOG( "Read Result: ERROR_SUCCESS" );
                dwRxSampleCount++;
                
                if( !pData->expSamples )
                {                    
                    LOG_WARN( "Unexpected Samples Received - Pre-existing Sample" );
                }
                else
                {
                    LOG( "Sample Received"); 
                }
                
                AccDumpSample( accData );

                break;


            default:
                LOG_ERROR( "Read Result: %d", dwResult);
                break;
        } 
    }
    while( dwSampleCount < ACC_TEST_SAMPLE_SIZE );

    //Rough check to make sure we stopped/started getting samples as expected
    LOG( "Total Samples Rx'd: (%d)   Iteration Count: (%d) Samples Expected: (%s)", dwRxSampleCount, dwSampleCount,( pData->expSamples)?_T("TRUE"):_T("FALSE")   );
    
    if( pData->expSamples )
    {
        //If we want them, make sure we got at least some of them
        if( dwRxSampleCount < (dwSampleCount * ACC_SAMPLE_UPPER_CUTOFF) )
        {
            LOG_ERROR("tst_AccelerometerStop() - Cutoff:(%d)" ,(dwRxSampleCount * ACC_SAMPLE_UPPER_CUTOFF) );
        }
    }
    else
    {
        //if we didn't want any make sure we didn't get much
        if( dwRxSampleCount > (dwSampleCount * ACC_SAMPLE_LOWER_CUTOFF) )
        {
            LOG_ERROR("tst_AccelerometerStop() - Cutoff:(%d)" ,(dwRxSampleCount * ACC_SAMPLE_LOWER_CUTOFF) );
        }
    }

DONE:

    pPayload->dwOutVal = bResult;    
    LOG_END();
}//tst_AccelerometerStop





//------------------------------------------------------------------------------
// tst_AccelerometerStart
//
void tst_AccelerometerStart( CClientManager::ClientPayload* pPayload ) 
{
    LOG_START();    
    BOOL bResult = TRUE;
    DWORD dwResult = ERROR_SUCCESS;
    DWORD dwSampleCount = 0;
    DWORD dwWait = WAIT_FAILED;
    DWORD dwRxSampleCount = 0;
    ACC_DATA accData = {0};
    BOOL bRdRes = FALSE;
    DWORD dwFlags = 0;
    DWORD dwBytesRead = 0;
    BOOL bValidQ = FALSE;
    BOOL bValidA = FALSE;

    //Do we have what we need....
    VERIFY_STEP("Varifying Client Payload", (pPayload!=NULL), TRUE );
    VERIFY_STEP("Varifying Client Payload Data", (pPayload->pBufIn!=NULL), TRUE );
    VERIFY_STEP("Varifying Client Payload Size", (pPayload->dwLenIn==sizeof(AccOpenAccStartTstData_t)), TRUE );

    //Make life easier
    AccOpenAccStartTstData_t* pData = (AccOpenAccStartTstData_t*)pPayload->pBufIn;
    ((pData->hQueue != INVALID_HANDLE_VALUE)&&(pData->hQueue != NULL))?bValidQ=TRUE:bValidQ=FALSE;
    ((pData->hAcc != INVALID_HANDLE_VALUE)&&(pData->hAcc != NULL))?bValidA=TRUE:bValidA=FALSE;
 
    //What are we dealing with
    LOG( "tst_AccelerometerStart:" );
    LOG( "Expecting Samples.....(%s)", pData->expSamples?_T("TRUE"):_T("FALSE"));
    LOG( "Accelerometer Handle..(0x%08X)", pData->hAcc );
    LOG( "Message Queue Handle..(0x%08X)", pData->hQueue );
    LOG( "Expected Error........(%d)", pData->expError );
    
    //Try starting it
    dwResult = f_AccelerometerStart(pData->hAcc, pData->hQueue);

    if( pData->expError != dwResult )
    {
        LOG_ERROR("AccelerometerStart() - Expected:(%d) Received:(%d)", pData->expError, dwResult );
    }


    
    //Make sure we are getting something, if we were supposed to
    LOG( "Attempting to read samples....." );
    do
    {
        dwSampleCount++;

        bRdRes = ReadMsgQueue(
            pData->hQueue,
            (LPVOID) &accData,
            sizeof(accData),
            &dwBytesRead,
            5 * ONE_SECOND,
            &dwFlags);
        dwResult = GetLastError();

        switch( dwResult )
        {
            case ERROR_PIPE_NOT_CONNECTED:
                LOG_WARN( "Read Result: ERROR_PIPE_NOT_CONNECTED" );                        
                break;

            case ERROR_INSUFFICIENT_BUFFER:
                LOG_WARN( "Read Result: ERROR_INSUFFICIENT_BUFFER" );
                break;

            case ERROR_INVALID_HANDLE:
                LOG( "Read Result: ERROR_INVALID_HANDLE" );
                if( bValidQ )
                {
                    LOG_ERROR( "Queue Handle Corruption" );
                }
                break;
                
            case ERROR_TIMEOUT:
                LOG( "Read Result: ERROR_TIMEOUT" );
                if( bValidQ )
                {
                    if( pData->expSamples )
                    {
                        LOG_WARN( "Sampling Failed (WAIT_TIMEOUT)" );
                    }
                    else
                    {
                        LOG( "No Unexpected Samples Received" );
                    }
                }
                else
                {
                    if( pData->expSamples )
                    {
                        LOG_ERROR( "Test Condition Error" );
                    }
                    else
                    {
                        LOG( "No Samples Expected, No Valid Queue Provided" );                    
                    }
                }
                
                //If no valid Accelerometer handle, this should time out.
                //After the first check exit.
                if( !bValidA && dwSampleCount > 2 )
                {
                    goto DONE;
                }                       
                
                break;
        
            case ERROR_SUCCESS:
                LOG( "Read Result: ERROR_SUCCESS" );
                dwRxSampleCount++;
                
                if( !pData->expSamples )
                {
                    LOG_WARN( "Unexpected Samples Received" );
                }
                else
                {
                    LOG( "Sample Received"); 
                }
                
                AccDumpSample( accData );

                break;


            default:
                LOG_ERROR( "Read Result: %d", dwResult);
                break;
        } 
    }
    while( dwSampleCount < ACC_TEST_SAMPLE_SIZE );


    //Rough check to make sure we stopped/started getting samples as expected
    LOG( "Total Samples Rx'd: (%d)   Iteration Count: (%d) Samples Expected: (%s)", dwRxSampleCount, dwSampleCount,( pData->expSamples)?_T("TRUE"):_T("FALSE")   );
    
    if( pData->expSamples )
    {
        //If we want them, make sure we got at least some of them
        if( dwRxSampleCount < (dwSampleCount * ACC_SAMPLE_UPPER_CUTOFF) )
        {
            LOG_ERROR("tst_AccelerometerStart() - Cutoff:(%d)" ,(dwRxSampleCount * ACC_SAMPLE_UPPER_CUTOFF) );
        }
    }
    else
    {
        //if we didn't want any make sure we didn't get much
        if( dwRxSampleCount > (dwSampleCount * ACC_SAMPLE_LOWER_CUTOFF) )
        {
            LOG_ERROR("tst_AccelerometerStart() - Cutoff:(%d)" ,(dwRxSampleCount * ACC_SAMPLE_LOWER_CUTOFF) );
        }
    }

DONE:
    pPayload->dwOutVal = bResult;    
    LOG_END();
}//tst_AccelerometerStart


//------------------------------------------------------------------------------
// tst_OpenSensor
//
void tst_OpenSensor( CClientManager::ClientPayload* pPayload ) 
{
    LOG_START();    
    BOOL bResult = TRUE;
    DEVMGR_DEVICE_INFORMATION di = {0};   
    di.dwSize = sizeof( di );
    HSENSOR hAcc = ACC_INVALID_HANDLE_VALUE;
    LUID blankLuid = {0xFF};


    //Do we have what we need....
    VERIFY_STEP("Varifying Client Payload", (pPayload!=NULL), TRUE );
    VERIFY_STEP("Varifying Client Payload Data", (pPayload->pBufIn!=NULL), TRUE );
    VERIFY_STEP("Varifying Client Payload Size", (pPayload->dwLenIn==sizeof(AccOpenSensorTstData_t)), TRUE );
    VERIFY_STEP("Acquiring Accelerometer Information", (AccAvailable( di )), TRUE );

    //Make life easier
    AccOpenSensorTstData_t* pData = (AccOpenSensorTstData_t*)pPayload->pBufIn;

    //What are we dealing with
    LOG( "tst_OpenSensor:" );
    LOG( "Device Name...........(%s)", (pData->in_name)?(pData->in_name):_T("NULL"));
    LOG( "Sensor LUID...........(0x%08X%08X)", (pData->in_pSensorLuid)?pData->in_pSensorLuid->HighPart:0xFFFFFFFF, (pData->in_pSensorLuid)?pData->in_pSensorLuid->LowPart:0xFFFFFFFF);
    LOG( "Expected Handle.......(0x%08X)", pData->expHandle);
    LOG( "Expected Error........(%d)", pData->expError );

    //Set a marker to make sure it changes when it's supposed. 
    if( pData->in_pSensorLuid )
    {
        memset(pData->in_pSensorLuid, 0xFF, sizeof(LUID));
    }

    hAcc = f_OpenSensor( pData->in_name, pData->in_pSensorLuid);

    //We got the error we wanted, was this supposed to be a good case?
    if( pData->expError == ERROR_SUCCESS )
    {
        //Verify good handle
        if( hAcc == ACC_INVALID_HANDLE_VALUE )
        {
            LOG_ERROR( "Function Succeeded with Invalid Handle Return" );
        }

        //Make sure we got an updated sensor id    
        if(pData->in_pSensorLuid)
        {
            //if (RtlEqualLuid(*pData->in_pSensorLuid, blankLuid))
            if (memcmp(pData->in_pSensorLuid, &blankLuid, sizeof(LUID)) == 0)
            {  
                LOG_ERROR( "Function Succeeded with Invalid Sensor ID" );
            }        
        }
    }
    
    // Got what we wanted, but it was an error case to check
    if( pData->expError != ERROR_SUCCESS )
    {
        //Is the handle not what we expected
        if( pData->expHandle != hAcc )
        {
            LOG_ERROR("Handle - Expected:(0x%08X) Received:(0x%08X)", pData->expHandle, hAcc );
        }
       
        //Make sure sensor id was left alone
        //if( !RtlEqualLuid(*pData->in_pSensorLuid, blankLuid) )
        if( memcmp(pData->in_pSensorLuid, &blankLuid, sizeof(LUID)) != 0)
        {  
            LOG_WARN( "Function Modified Sensor ID In Failure Scenario" );
        }        
    }

DONE:

    CHECK_CLOSE_ACC_HANDLE( hAcc );
    pPayload->dwOutVal = bResult;
    
    LOG_END();
}//tst_OpenSensor



