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

#include "TUXMAIN.H"
#include "TEST_WAVETEST.H"
#include "TEST_WAVEIN.H"
#include "ERRORMAP.H"

bool  g_bNeedCSVFileCaptureHeader = TRUE;
BOOL g_formatNotSupported = FALSE;           //To skip different duration of a wave format if it is not supported on the platform
timer_type g_ttRecordEndTime;   // for CaptureInitialLatency()
bool  g_bLatencyTest = FALSE;

/*
  * Function Name: GetUserEvaluation
  *
  * Purpose: Solicit input from the user
  *
*/

DWORD GetUserEvaluation( const TCHAR *szPrompt )
{
    BOOL    bRerunTest = FALSE;
    int     iResponse;
    DWORD   tr         = TPR_PASS;

    LOG( TEXT( "Running in Interactive Mode." ) );
    HMODULE hCoreDLL = GetCoreDLLHandle();
    if( NULL != hCoreDLL )
    {
        PFNMESSAGEBOX pfnMessageBox
            = (PFNMESSAGEBOX)GetProcAddress( hCoreDLL, TEXT( "MessageBoxW" ) );
        if( NULL  != pfnMessageBox )
        {
            iResponse = pfnMessageBox( NULL, szPrompt, TEXT(
                "Interactive Response" ), MB_YESNOCANCEL | MB_ICONQUESTION );
            switch( iResponse )
            {
            case IDYES:
            break;
            case IDNO:
                LOG( TEXT("ERROR:   The user said that the audio file didn't playback correctly." ) );
                tr = GetReturnCode( tr, TPR_FAIL );
            break;
            case IDCANCEL:
                 LOG(TEXT( "INFORMATION:   The user said rerun test." ) );
                bRerunTest = TRUE;
            break;
            default:
                  LOG( TEXT( "ERROR in %s @ line %u:" ),TEXT( __FILE__ ), __LINE__ );
                 LOG( TEXT( "\tInvalid response from MessageBox." ) );
                tr = GetReturnCode( tr, TPR_FAIL );
                break;
            } // switch iResponse
        } // if MessageBoxW
         else
        {  //  !MessageBoxW
            LOG( TEXT( "ERROR in %s @ line %u:" ),TEXT( __FILE__ ), __LINE__ );
            LOG( TEXT( "\tpfnMessageBox is NULL, GetProcAddress failed." ) );
            tr = GetReturnCode( tr, TPR_ABORT );
        } // if-else MessageBoxW
        if( FALSE == FreeCoreDLLHandle( hCoreDLL ) ) // Check FreeCoreDLL.
            tr = GetReturnCode( tr, TPR_FAIL );
        } // if hCoreDLL
    else
    { // !hCoreDLL
        // Could not get a handle to coredll
        LOG( TEXT( "ERROR in %s @ line %u:" ), TEXT( __FILE__ ), __LINE__ );
        LOG( TEXT( "\tFailed to get a handle to coredll.dll\n" ) );
        tr = GetReturnCode( tr, TPR_ABORT );
    } // else hCoreDLL

    if( bRerunTest )
    {
        LOG( TEXT( "INFORMATION:   This test case is being rerun per user request." ) );
        return TPR_RERUN_TEST;
    } // if bRerunTest
    return tr;
} // GetUserEvaluation

/*
  * Function Name: LaunchCaptureSoundThread
  *
  * Purpose: to launch an audio capture thread.
  *
*/

DWORD LaunchCaptureSoundThread( LPVOID lpParm )
{
    DWORD                          dwBufferLength;
    DWORD                          dwExpectedCaptureTime = 2000; // in ms
    DWORD                          dwReturn              = TPR_PASS;
    DWORD                          dwWaitForSingleObjectResult;
    HANDLE                         hCompletion           = NULL;
    HWAVEIN                        hwi;
    int                            iCaptureTimeTolerance = 200; // in ms
    MMRESULT                       mmRtn;
    char*                          pcData                = NULL;
    LAUNCHCAPTURESOUNDTHREADPARMS* pLaunchCaptureSoundThreadParms;
    timer_type                     ttCaptureEndTime;            // in ticks
    timer_type                     ttCaptureStartTime;          // in ticks
    timer_type                     ttLatency;                   // in ms
    timer_type                     ttObservedCaptureTime;       // in ms
    timer_type                     ttResolution;                // in ticks/sec
    WAVEFORMATEX                   wfx;
    WAVEHDR                        wh;

    // check for capture device
    if( !waveInGetNumDevs() )
    {
                LOG( TEXT("ERROR: waveInGetNumDevs reported zero devices, we need at least one.") );
                return TPR_SKIP;
    }

    // Valididate lpParm.
    if( lpParm )
    {
        pLaunchCaptureSoundThreadParms= (LAUNCHCAPTURESOUNDTHREADPARMS*)lpParm;
        hCompletion = pLaunchCaptureSoundThreadParms->hCompletion;
        hwi = pLaunchCaptureSoundThreadParms->hwi;
        wfx = pLaunchCaptureSoundThreadParms->wfx;
        LOG( TEXT("in LaunchCaptureSoundThread waveIn handle = 0x%08x, event handle = 0x%08x."), hwi, hCompletion );
    } // if lpParm
    else // !lpParm
    {
        LOG(TEXT( "FAIL:\tLaunchCaptureSoundThread received NULL, lpParm." ) );
        return TPR_FAIL;
    } // else !lpParm

    // Valididate hCompletion.
    if( NULL == hCompletion )
    {
        LOG( TEXT("FAIL:\tLaunchCaptureSoundThread received NULL hCompletion." ) );
        return TPR_FAIL;
    } // if hCompletion

    // Valididate hwi.
    if( NULL == hwi )
    {
        LOG( TEXT( "FAIL:\tLaunchCaptureSoundThread received NULL handle." ) );
        return TPR_FAIL;
    } // if hwi

    // Set up an audio buffer for test.
    dwBufferLength = dwExpectedCaptureTime * wfx.nAvgBytesPerSec / 1000;

    // verify buffer length.
    if( dwExpectedCaptureTime >= (ULONG_MAX / wfx.nAvgBytesPerSec ) )
    {
                LOG( TEXT (
            "WARNING:\tPotential overflow, dwExpectedCaptureTime = %d ms." ),
            dwExpectedCaptureTime );
        return TPR_ABORT;
    } // if ExpectedCaptureTime
        PREFAST_SUPPRESS(419, "The above line is checking for overflow. This seems to be prefast noise, since the path/trace Prefast lists is not possible. (Prefast team concurs so far.)");

    pcData = new char[ dwBufferLength ];
        if( !pcData )
    {
                LOG( TEXT( "ERROR in %s @ line %u:" ), TEXT( __FILE__ ), __LINE__ );
        LOG( TEXT( "\tNew failed for pcData, possibly out of memory." ) );
        return TPR_FAIL;
    } // if !pcData

    ZeroMemory( pcData, dwBufferLength );
    ZeroMemory( &wh, sizeof( WAVEHDR ) );
    wh.lpData         = pcData;
        wh.dwBufferLength = dwBufferLength;
        wh.dwLoops        = 0;
        wh.dwFlags        = 0;

    // Prepare header
        mmRtn = waveInPrepareHeader( hwi, &wh, sizeof( wh ) );
        if( MMSYSERR_NOERROR != mmRtn )
    {
        LOG( TEXT( "FAIL:\twaveInPrepareHeader failed with return code %d." ),
            mmRtn );
        char* pszText;
        pszText = new char( MaxErrorText );
        waveInGetErrorText( mmRtn, (LPTSTR)pszText, MaxErrorText );
        LOG( TEXT( "FAIL:\t%s" ), pszText );
        delete pszText;
        dwReturn = TPR_FAIL;
    } // if !MMSYSERR_NOERROR

    // Retrieve capture start time.
    if( !QueryPerformanceCounter(
        reinterpret_cast<LARGE_INTEGER*>( &ttCaptureStartTime ) ) )
    {
        LOG( TEXT(
            "FAIL:\tQueryPerformanceCounter failed to get ttCaptureStartTime."
            ) );
        dwReturn = TPR_FAIL;
    } // if !QueryPerformanceCounter

    // Send buffer to the waveform-audio input device.
    mmRtn = waveInAddBuffer( hwi, &wh, sizeof( wh ) );
        if( MMSYSERR_NOERROR != mmRtn )
    {
        LOG(
        TEXT( "FAIL:\twaveInAddBuffer failed with return code %d." ), mmRtn );
        dwReturn = TPR_FAIL;
    } // if waveInAddBuffer

    // Capture wh.
    mmRtn = waveInStart( hwi );
        if( MMSYSERR_NOERROR != mmRtn )
    {
        LOG( TEXT( "FAIL:\twaveInStart failed with return code %d." ), mmRtn );
        dwReturn = TPR_FAIL;
    } // if MMSYSERR_NOERROR

    // Wait for wh to finish capturing.
    dwWaitForSingleObjectResult = WaitForSingleObject(
        hCompletion, dwExpectedCaptureTime + (DWORD)iCaptureTimeTolerance );

    // Retrieve capture end time.
    if( !QueryPerformanceCounter(
        reinterpret_cast<LARGE_INTEGER*>( &ttCaptureEndTime ) ) )
    {
        LOG( TEXT(
            "FAIL:\tQueryPerformanceCounter failed to get ttCaptureEndTime."
            ) );
        dwReturn = TPR_FAIL;
    } // if !QueryPerformanceCounter

    LOG( TEXT( "dwExpectedCaptureTime = %u." ), dwExpectedCaptureTime );
    LOG( TEXT( "iCaptureTimeTolerance = %d." ), iCaptureTimeTolerance );

    if( WAIT_OBJECT_0 != dwWaitForSingleObjectResult )
    {
        if( WAIT_TIMEOUT == dwWaitForSingleObjectResult )
        {
            LOG( TEXT( "FAIL:\tWave in didn't signal completion." ) );
            dwReturn = TPR_FAIL;
        } // if WAIT_TIMEOUT
        else // !WAIT_TIMEOUT
        {
            if( WAIT_FAILED == dwWaitForSingleObjectResult )
            {
                LOG( TEXT( "FAIL:\tWaitForSingleObject failed with error %d" ),
                    GetLastError() );
                dwReturn = TPR_FAIL;
            } // if WAIT_FAILED
            else // !WAIT_FAILED
            {
                LOG( TEXT(
                    "FAIL:\tWaitForSingleObject unknown error. Last error %d"
                    ), GetLastError() );
                dwReturn = TPR_FAIL;
            } // else  !WAIT_FAILED
        } // else !WAIT_TIMEOUT
    } // if !WAIT_OBJECT_0

    if( wh.dwFlags & WHDR_INQUEUE )
    {
        LOG( TEXT( "FAIL:\tCapture finished without releasing queue" ) );
        dwReturn = TPR_FAIL;
    } // if wh.dwFlags & WHDR_INQUEUE

    if( !( wh.dwFlags & WHDR_DONE ) )
    {
        LOG( TEXT( "FAIL:\tHeader not done." ) );
        dwReturn = TPR_FAIL;
    } // if( !(whdr.dwFlags & WHDR_DONE ) )

    // Obtain Performance Frequency resolution and calculate latency.
    QueryPerformanceFrequency(
        reinterpret_cast<LARGE_INTEGER*>(&ttResolution) );
    LOG( TEXT( "Performance frequency resolution = %I64i ticks/sec." ),
        ttResolution );

    ttObservedCaptureTime
        = ( ttCaptureEndTime - ttCaptureStartTime ) * 1000 / ttResolution;
    LOG( TEXT( "ttObservedCaptureTime = %I64i." ), ttObservedCaptureTime );

    ttLatency = ttObservedCaptureTime - (__int64)dwExpectedCaptureTime;
    LOG( TEXT( "Latency = %I64i ms." ), ttLatency );
    // Test complete, clean up and finish.

    // reset the device
        mmRtn = waveInReset( hwi );
        if( MMSYSERR_NOERROR != mmRtn )
        {
                LOG( TEXT( "FAIL:\twaveInReset failed with return code %d."),
                    mmRtn );
                dwReturn = TPR_FAIL;
        }

    // unprepare header
        mmRtn = waveInUnprepareHeader( hwi, &wh, sizeof( wh ) );
        if( MMSYSERR_NOERROR != mmRtn )
    {
        LOG( TEXT( "FAIL:\twaveInUnprepareHeader failed with return code %d."
            ), mmRtn );
        dwReturn = TPR_FAIL;
    } // if !MMSYSERR_NOERROR

    delete [] pcData;
    pcData = NULL;

    // close/delete handle to event
    if( !CloseHandle( hCompletion ) )
    {
        LOG( TEXT( "FAIL:\tCloseHandle failed to close event 0x%08x." ),
            hCompletion );
        dwReturn = TPR_FAIL;
    } // if !CloseHandle
    hCompletion = NULL;

    LOG( TEXT( "Thread 0x%08x Exiting, Return Code Should be %s." ),
        hwi, TPR_CODE_TO_TEXT( dwReturn ) );
    return dwReturn;
} // LaunchCaptureSoundThread

void LogWaveInDevCaps(UINT uDeviceID,WAVEINCAPS *pWIC) {
        DWORD i;

        LOG(TEXT("Capabilities for Wave In Device:  %u"),        uDeviceID);
        LOG(TEXT("    Product:  %s"),                        pWIC->szPname);
        LOG(TEXT("    Manufacture ID:  %u"),                pWIC->wMid);
        LOG(TEXT("    Product ID:  %u"),                        pWIC->wPid);
        LOG(TEXT("    Driver Version:  %u"),                pWIC->vDriverVersion);
        LOG(TEXT("    Channels:  %u"),                        pWIC->wChannels);
        LOG(TEXT("    Audio Formats:"));
        for(i=0;lpFormats[i].szName;i++)
                LOG(TEXT("        %s %s"),lpFormats[i].szName,(pWIC->dwFormats&lpFormats[i].value?TEXT(" Supported"):TEXT("*****Not Supported*****")));
        LOG(TEXT("    Extended functionality supported:"));
}

void CALLBACK waveInProcCallback(HWAVEIN hwo2, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2) {
        switch(uMsg) {
        case WIM_CLOSE:
                SetEvent(hEvent);
                if(state==WIM_DATA) {
                        state=WIM_CLOSE;
                }
                else LOG(TEXT("ERROR:  WIM_CLOSE received when not done"));
                break;
        case WIM_OPEN:
                if (!g_bLatencyTest)
                    SetEvent(hEvent);
                if(state==WIM_CLOSE) {
                        state=WIM_OPEN;
                }
                else LOG(TEXT("ERROR:  WIM_OPEN received when not closed"));
                break;
        case WIM_DATA:
                SetEvent(hEvent);
                if (g_bLatencyTest)
                { // for CaptureInitialLatency()
                    QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&g_ttRecordEndTime));
                    g_bLatencyTest = FALSE;
                }
                if(state==WIM_OPEN) {
                        state=WIM_DATA;
                }
                else LOG(TEXT("ERROR:  WIM_DATA received when not opened"));
                break;
        default:
                LOG(TEXT("ERROR:  Unknown Message received (%u)"),uMsg);
        }
}

LRESULT CALLBACK windowInProcCallback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch(uMsg) {
        case MM_WIM_CLOSE:
                waveInProcCallback((HWAVEIN)wParam,WIM_CLOSE,(DWORD)hEvent,0,0);
                return 0;
        case MM_WIM_OPEN:
                waveInProcCallback((HWAVEIN)wParam,WIM_OPEN,(DWORD)hEvent,0,0);
                return 0;
        case MM_WIM_DATA:
                waveInProcCallback((HWAVEIN)wParam,WIM_DATA,(DWORD)hEvent,lParam,0);
                return 0;
        case WM_DESTROY:
                HMODULE hCoreDLL = GetCoreDLLHandle();
                if(NULL != hCoreDLL) {
                        PFNPOSTQUITMESSAGE pfnPostQuitMessage = (PFNPOSTQUITMESSAGE)GetProcAddress(hCoreDLL, TEXT("PostQuitMessage"));
                        if(NULL != pfnPostQuitMessage)
                                pfnPostQuitMessage (0);
                        else
                                LOG(TEXT("ERROR: GetProcAddress Failed for PostQuitMessage in Coredll.dll\n"));
                        FreeCoreDLLHandle(hCoreDLL);
                }
                else
                        LOG(TEXT("ERROR: Could not get a handle to CoreDll.dll, unable to call PostQuitMessage."));
                return 0;
        }

        int retVal = 0;
        HMODULE hCoreDLL = GetCoreDLLHandle();
        if(NULL != hCoreDLL) {
                PFNDEFWINDOWPROC pfnDefWindowProc = (PFNDEFWINDOWPROC)GetProcAddress(hCoreDLL, TEXT("DefWindowProcW"));
                if(NULL != pfnDefWindowProc)
                        retVal = pfnDefWindowProc(hwnd,uMsg,wParam,lParam);
                else {
                        LOG(TEXT("ERROR: GetProcAddress Failed for DefWindowProc in Coredll.dll\n"));
                        retVal = 1;
                }
                FreeCoreDLLHandle(hCoreDLL);
        }
        return retVal;
}

DWORD WINAPI WindowInMessageThreadProc(LPVOID lpParameter) {
        MSG msg;

        // only if on a window enabled system
        WNDCLASS wndclass;
        if(lpParameter) {
                  wndclass.style          =   CS_HREDRAW | CS_VREDRAW;
                  wndclass.lpfnWndProc    =   windowInProcCallback;
                  wndclass.cbClsExtra     =   0;
                  wndclass.cbWndExtra     =   0;
                  wndclass.hInstance      =   g_hInst;
                  wndclass.hIcon          =   NULL;
                  wndclass.hCursor        =   NULL;
                  wndclass.hbrBackground  =   NULL;
                  wndclass.lpszMenuName   =   NULL;
                  wndclass.lpszClassName  =   TEXT("WaveTest");

                HMODULE hCoreDLL = GetCoreDLLHandle();
                if(NULL != hCoreDLL) {
                        PFNREGISTERCLASS pfnRegisterClass = (PFNREGISTERCLASS)GetProcAddress(hCoreDLL, TEXT("RegisterClassW"));
                        if(NULL != pfnRegisterClass) {
                                if (0 == pfnRegisterClass(&wndclass)){
                                        LOG(TEXT("ERROR: Register Class FAILED"));
                                        return TPR_ABORT;
                                }
                        }
                        else {
                                LOG(TEXT("ERROR: Unable to get proc address for RegisterClass"));
                                return TPR_ABORT;
                        }

                        PFNCREATEWINDOWEX pfnCreateWindowEx = ( PFNCREATEWINDOWEX)GetProcAddress(hCoreDLL, TEXT("CreateWindowExW"));
                        if(NULL != pfnCreateWindowEx) {
                                hwnd = pfnCreateWindowEx( 0, //extended style
                                  TEXT("WaveTest"),
                                  TEXT("WaveTest"),
                                  WS_DISABLED,
                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                  NULL, NULL,
                                  g_hInst,
                                    &g_hInst );

                                if(hwnd==NULL)  {
                                          LOG(TEXT("ERROR: CreateWindow Failed"));
                                        return TPR_ABORT;
                                }
                        }
                        else {
                                LOG(TEXT("ERROR: Unable to get proc address for CreateWindow"));
                                return TPR_ABORT;
                        }


                        if(0 == SetEvent(hEvent)) {
                                int iLastErr = GetLastError();
                                LOG(TEXT("ERROR: Unable to SetEvent. GetLastError was %d\n"), iLastErr);
                                return TPR_ABORT;
                        }


                FreeCoreDLLHandle(hCoreDLL);
                }
        }

        HMODULE hCoreDll = GetCoreDLLHandle();
        if(NULL != hCoreDll) {
                PFNGETMESSAGE pfnGetMessage = (PFNGETMESSAGE)GetProcAddress(hCoreDll, TEXT("GetMessageW"));
                PFNTRANSLATEMESSAGE pfnTranslateMessage = (PFNTRANSLATEMESSAGE)GetProcAddress(hCoreDll, TEXT("TranslateMessage"));
                PFNDISPATCHMESSAGE pfnDispatchMessage = (PFNDISPATCHMESSAGE)GetProcAddress(hCoreDll, TEXT("DispatchMessageW"));
                if( (NULL != pfnGetMessage) && (NULL != pfnTranslateMessage) && (NULL != pfnDispatchMessage) ){
                        while(pfnGetMessage(&msg,NULL,0,0)) {
                                if(NULL==msg.hwnd) {
                                        windowInProcCallback(msg.hwnd,msg.message,msg.wParam,msg.lParam);
                                }
                                else {
                                        pfnTranslateMessage(&msg);
                                        pfnDispatchMessage(&msg);
                                }
                        } // end While
                }
                else {
                        LOG(TEXT("ERROR: Message Processing Function(s) not found in coredll.dll"));
                        return TPR_ABORT;
                }

                FreeCoreDLLHandle(hCoreDll);
        }

        if(lpParameter) {
                HMODULE hCoreDLL = GetCoreDLLHandle();
                if(NULL != hCoreDLL) {
                        PFNDESTROYWINDOW pfnDestroyWindow = (PFNDESTROYWINDOW)GetProcAddress(hCoreDLL, TEXT("DestroyWindow"));
                        PFNUNREGISTERCLASS pfnUnregisterClass = (PFNUNREGISTERCLASS)GetProcAddress(hCoreDLL, TEXT("UnregisterClassW"));
                        if((NULL != pfnDestroyWindow) && (NULL != pfnUnregisterClass)) {
                                pfnDestroyWindow(hwnd);
                                pfnUnregisterClass(wndclass.lpszClassName,g_hInst);
                        }
                        else {
                                LOG(TEXT("ERROR: Destroy Window and UnRegisterClass Failed"));
                                return TPR_ABORT;
                        }
                }
                else {
                        LOG(TEXT("ERROR: Unable to get a handle to CoreDLL"));
                        return TPR_ABORT;
                }
        }

        return TPR_PASS;
}

TESTPROCAPI RecordWaveFormat(TCHAR *szWaveFormat,DWORD dwSeconds,DWORD dwNotification,DWORD *hrReturn) {
        HRESULT hr;
        HWAVEIN hwi;
        HWAVEOUT hwo;
        int iResponse = 0;
        WAVEFORMATEX wfx;
        WAVEHDR wh,wh_out;
        HANDLE hThread=NULL;
        char *data=NULL,*data2=NULL;
        DWORD dwExpectedPlaytime,dwTime=0,dwCallback;
        MMRESULT mmRtn = TPR_PASS;
        timer_type start_count,finish_count=0,duration,expected,m_Resolution;

        state=WIM_CLOSE;
        hwo=NULL;
        hwi=NULL;
        QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&m_Resolution));
       ValidateResolution( m_Resolution );

        if(StringFormatToWaveFormatEx( &wfx, szWaveFormat )!=TPR_PASS) {
               mmRtn = GetReturnCode(mmRtn, TPR_FAIL);
                goto Error;
        }
        //data=new char[dwSeconds*wfx.nAvgBytesPerSec];

        // This check added so that we don't overflow the DWORD and wind up with
        // a buffer that is smaller than we expected.
        if( dwSeconds < (ULONG_MAX / wfx.nAvgBytesPerSec) ){
                PREFAST_SUPPRESS(419, "The above line is checking for overflow. This seems to be prefast noise, since the path/trace Prefast lists is not possible. (Prefast team concurs so far.)");
                data=new char[dwSeconds*wfx.nAvgBytesPerSec];
                }
        else
                {
                        LOG(TEXT("ERROR: Line %d, DWORD Overflow."), __LINE__);
                     mmRtn = GetReturnCode(mmRtn, TPR_FAIL);
                        goto Error;
                }

        CheckCondition_mmRtn(!data,"ERROR:  New Failed for Data",TPR_ABORT,"Out of Memory");
        ZeroMemory(data,dwSeconds*wfx.nAvgBytesPerSec);
        data2=new char[dwSeconds*wfx.nAvgBytesPerSec];
        CheckCondition_mmRtn(!data2,"ERROR:  New Failed for Data",TPR_ABORT,"Out of Memory");
        hEvent=CreateEvent(NULL,FALSE,FALSE,NULL);
        CheckCondition_mmRtn(hEvent==NULL,"ERROR:  Unable to CreateEvent",TPR_ABORT,"Refer to GetLastError for a Possible Cause");
        switch(dwNotification) {
                case CALLBACK_NULL:
                        dwCallback=NULL;
                break;
                case CALLBACK_EVENT:
                        dwCallback=(DWORD)hEvent;
                break;
                case CALLBACK_FUNCTION:
                        dwCallback=(DWORD)waveInProcCallback;
                break;
                case CALLBACK_THREAD:
                        hThread=CreateThread(NULL,0,WindowInMessageThreadProc,(void*)FALSE,0,&dwCallback);
                        CheckCondition_mmRtn(hThread==NULL,"ERROR:  Unable to CreateThread",TPR_ABORT,"Refer to GetLastError for a Possible Cause");
                break;
                case CALLBACK_WINDOW:
                        hThread=CreateThread(NULL,0,WindowInMessageThreadProc,(void*)TRUE,0,NULL);
                        CheckCondition_mmRtn(hThread==NULL,"ERROR:  Unable to CreateThread",TPR_ABORT,"Refer to GetLastError for a Possible Cause");
                         hr=WaitForSingleObject(hEvent,10000);
                        CheckCondition_mmRtn(hr==WAIT_TIMEOUT,"ERROR:  Window didn't notify its creation within ten seconds",TPR_ABORT,"Unknown");
                        CheckCondition_mmRtn(hr!=WAIT_OBJECT_0,"ERROR:  Unknown Error waiting for Window Creation to notify its creation",TPR_ABORT,"Unknown");
                        dwCallback=(DWORD)hwnd;
                        break;
        }
        hr=waveInOpen(&hwi,g_dwInDeviceID,&wfx,dwCallback,(DWORD)hEvent,dwNotification);
        if((hr!=MMSYSERR_NOERROR)&&(hrReturn)) {
                 *hrReturn=hr;
               mmRtn = GetReturnCode(mmRtn, TPR_FAIL);
                 goto Error;
        }
        CheckMMRESULT_mmRtn(hr,"ERROR:  Failed to open.  waveInOpen",TPR_ABORT,"Driver doesn't really support this format");
        switch(dwNotification) {
                 case CALLBACK_FUNCTION:
                case CALLBACK_THREAD:
                case CALLBACK_WINDOW:
                         hr=WaitForSingleObject(hEvent,1000);
                         QueryCondition_mmRtn(hr==WAIT_TIMEOUT,"ERROR:  Function, Thread or Window didn't notify that it received the open message within one second",TPR_ABORT,"Open message wasn't sent to handler");
                        QueryCondition_mmRtn(hr!=WAIT_OBJECT_0,"ERROR:  Unknown error while waiting for driver to open",TPR_ABORT,"Unknown");
                break;
        }
StartRecording:
        ZeroMemory(data2,dwSeconds*wfx.nAvgBytesPerSec);
        ZeroMemory(&wh,sizeof(WAVEHDR));
        wh.lpData=data;
        wh.dwBufferLength=dwSeconds*wfx.nAvgBytesPerSec;
        wh.dwLoops=1;
        wh.dwFlags=WHDR_BEGINLOOP|WHDR_ENDLOOP;
        memcpy(&wh_out,&wh,sizeof(wh));
        wh.lpData=data2;
        dwExpectedPlaytime=wh.dwBufferLength*1000/wfx.nAvgBytesPerSec;
        if(g_useSound) {
                hr=waveOutOpen(&hwo,g_dwOutDeviceID,&wfx,NULL,NULL,CALLBACK_NULL);
                if((hr!=MMSYSERR_NOERROR)&&(hrReturn)) {
                        *hrReturn=hr;
                     mmRtn = GetReturnCode(mmRtn, TPR_FAIL);
                        goto Error;
                }
                CheckMMRESULT_mmRtn(hr,"ERROR:  Failed to open driver.  waveOutOpen",TPR_ABORT,"Driver doesn't really support this format");
                SineWave(wh_out.lpData,wh_out.dwBufferLength,&wfx);
                hr=waveOutPrepareHeader(hwo,&wh_out,sizeof(WAVEHDR));
                CheckMMRESULT_mmRtn(hr,"ERROR:  Failed to prepare header.  waveOutPrepareHeader",TPR_ABORT,"Driver doesn't really support this format");
        }
        hr=waveInPrepareHeader(hwi,&wh,sizeof(WAVEHDR));
        CheckMMRESULT_mmRtn(hr,"ERROR:  Failed to prepare header.  waveInPrepareHeader",TPR_ABORT,"Driver doesn't really support this format");
        hr=waveInAddBuffer(hwi,&wh,sizeof(WAVEHDR));
        CheckMMRESULT_mmRtn(hr,"ERROR:  Failed to add buffer.  waveInAddBuffer",TPR_ABORT,"Driver doesn't really support this format");
        if(g_interactive)  // Dynamically load coredll.dll to use the MessageBox
                {
                if(g_headless) {
                                LOG(TEXT("Running in Interactive Headless Mode."));

                                TCHAR tchC;
                                GotoStartRecordPrompt:
                                DWORD retVal = GetHeadlessUserInput(TEXT("INTERACTIVE PROMPT: OK to start recording?\n(Press 'Y' for Yes, 'N' for No or 'R' for Retry/Prompt Again)"), &tchC);
                                if(retVal != TPR_PASS ) {
                                        LOG(TEXT("ERROR: Failed to get headless interactive user input."));
                                        if(retVal == TPR_SKIP) { // Then Keyboard is not present on device
                                                mmRtn = TPR_SKIP;
                                                goto Error;
                                        }
                                        else {
                                                 mmRtn = GetReturnCode(retVal, TPR_ABORT);
                                                 goto Error;
                                        }
                                }

                                if(tchC == (TCHAR)'N' || tchC == (TCHAR)'n') {
                                        goto GotoStartRecordPrompt;
                                }
                                else if(tchC == (TCHAR)'R' || tchC == (TCHAR)'r') {

                                        goto GotoStartRecordPrompt;
                                }
                                // else user entered 'Y', thus they heard the audio, and thus we can move on with the test
                }
                else{
                        LOG(TEXT("Running in Interactive Mode."));
                        HMODULE hCoreDLL = GetCoreDLLHandle();
                        if(NULL != hCoreDLL) {
                                PFNMESSAGEBOX pfnMessageBox = (PFNMESSAGEBOX)GetProcAddress(hCoreDLL, TEXT("MessageBoxW"));
                                if(NULL  != pfnMessageBox)
                                        iResponse = pfnMessageBox(NULL,TEXT("Click OK to start recording"),TEXT("Interactive Response"),MB_OK|MB_ICONQUESTION);
                                else {
                                        LOG(TEXT("ERROR: pfnMessageBox is NULL, GetProcAddress failed."));
                                   mmRtn = GetReturnCode(mmRtn, TPR_ABORT);
                                        goto Error;
                                }
                                if(FALSE == FreeCoreDLLHandle(hCoreDLL)) {
                                   mmRtn = GetReturnCode(mmRtn, TPR_ABORT); // FreeCoreDLL Handle Failed, aborting
                                        goto Error;
                                }
                        }
                        else {
                                // Could not get a handle to coredll
                                LOG(TEXT("ERROR:Failed to get a handle to coredll.dll\n"));
                            mmRtn = GetReturnCode(mmRtn, TPR_ABORT);
                                goto Error;
                        }

                }
        }
        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&start_count));
        hr=waveInStart(hwi);
        CheckMMRESULT_mmRtn(hr,"ERROR:  Failed to start.  waveInStart",TPR_ABORT,"Driver doesn't really support this format");
        if(g_useSound) {
                hr=waveOutWrite(hwo,&wh_out,sizeof(WAVEHDR));
                CheckMMRESULT_mmRtn(hr,"ERROR:  Failed to write.  waveOutWrite",TPR_ABORT,"Driver doesn't really support this format");
        }
        switch(dwNotification) {
                case CALLBACK_NULL:
                        dwTime=0;
                        while((!(wh.dwFlags&WHDR_DONE))&&(dwTime<dwExpectedPlaytime+1000)) {
                                Sleep(g_dwSleepInterval);
                                dwTime+=g_dwSleepInterval;
                        }
                        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&finish_count));
                break;
                case CALLBACK_FUNCTION:
                case CALLBACK_THREAD:
                case CALLBACK_WINDOW:
                case CALLBACK_EVENT:
                        hr=WaitForSingleObject(hEvent,dwExpectedPlaytime+1000);
                        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&finish_count));
                break;
        }
    duration = ( finish_count-start_count ) * 1000 / m_Resolution;
    expected=dwExpectedPlaytime;
    LOG( TEXT(  "Resolution:  %I64i ticks per second, Start:  %I64i ticks, Finish:  %I64i ticks,  Duration:  %I64i ms, Expected duration:  %I64i ms.\n" ),
                m_Resolution,start_count,finish_count,duration,expected);
    expected=duration-expected;
    if(expected<0)        {
            QueryCondition_mmRtn((-expected>g_dwInAllowance),"ERROR:  expected capturetime is too short",TPR_ABORT,"Driver notifying the end of capture too early");
    }
    else {
            QueryCondition_mmRtn((expected>g_dwInAllowance),"ERROR:  expected capturetime is too long",TPR_ABORT,"Driver notifying the end of capture too late");
            QueryCondition_mmRtn((expected>=1000),"ERROR:  notification not received within one second after expected capturetime",TPR_ABORT,"Driver is not signalling notification at all");
    }
    if(g_interactive) {
            if(g_headless) {
                    TCHAR tchC;
                    DWORD retVal = GetHeadlessUserInput(TEXT("INTERACTIVE PROMPT: Press 'Y' to playback recording, or press 'N' or 'R' to Re-Record Recording."),
                                                                                            &tchC);
                    if(retVal != TPR_PASS ) {
                            LOG(TEXT("ERROR: Failed to get headless interactive user input."));
                            if(retVal == TPR_SKIP) // Then Keyboard is not present on device
                                    return TPR_SKIP;
                            else
                                    return GetReturnCode(retVal, TPR_ABORT);
                    }

                if(tchC == (TCHAR)'N' || tchC == (TCHAR)'n' || tchC == (TCHAR)'R' || tchC == (TCHAR)'r') {
                        hr=waveOutReset(hwo);
                        QueryMMRESULT_mmRtn(hr,"ERROR:  Failed to reset.  waveOutReset",TPR_ABORT,"Driver didn't reset device properly");
                        hr=waveOutUnprepareHeader(hwo,&wh_out,sizeof(WAVEHDR));
                        CheckMMRESULT_mmRtn(hr,"ERROR:  Failed unprepare header.  waveOutPrepareHeader",TPR_ABORT,"Driver doesn't really support this format");
                        hr=waveInStop(hwi);
                        QueryMMRESULT_mmRtn(hr,"ERROR:  Failed to stop.  waveInStop",TPR_ABORT,"Driver didn't stop properly");
                        hr=waveInReset(hwi);
                        QueryMMRESULT_mmRtn(hr,"ERROR:  Failed to reset.  waveInReset",TPR_ABORT,"Driver didn't reset properly");
                        hr=waveInUnprepareHeader(hwi,&wh,sizeof(WAVEHDR));
                        QueryMMRESULT_mmRtn(hr,"ERROR:  Failed to unprepare.  waveInUnprepare",TPR_ABORT,"Driver doesn't really support this format");
                        goto StartRecording;
                }

                        // else user entered 'Y', thus they heard the audio, and thus we can move on with the test
        }
        else{
                HMODULE hCoreDLL = GetCoreDLLHandle();
                if(NULL != hCoreDLL) {
                        PFNMESSAGEBOX pfnMessageBox = (PFNMESSAGEBOX)GetProcAddress(hCoreDLL, TEXT("MessageBoxW"));
                        if(NULL  != pfnMessageBox)
                                iResponse = pfnMessageBox(NULL,TEXT("Click Yes to playback recording or No to re-record recording"),TEXT("Interactive Response"),MB_YESNO|MB_ICONQUESTION);
                        else {
                                LOG(TEXT("ERROR: pfnMessageBox is NULL, GetProcAddress failed."));
                            mmRtn = GetReturnCode(mmRtn, TPR_ABORT);
                        }
                        if(FALSE == FreeCoreDLLHandle(hCoreDLL))
                                mmRtn = GetReturnCode(mmRtn, TPR_ABORT); // FreeCoreDLL Handle Failed, aborting
                }
                else {
                        // Could not get a handle to coredll
                        mmRtn = GetReturnCode(mmRtn, TPR_ABORT);
                        iResponse=IDYES; // Set to Yes to skip switch statement below
                }

                switch(iResponse) {
                        case IDNO:
                                hr=waveOutReset(hwo);
                                QueryMMRESULT_mmRtn(hr,"ERROR:  Failed to reset.  waveOutReset",TPR_ABORT,"Driver didn't reset device properly");
                                hr=waveOutUnprepareHeader(hwo,&wh_out,sizeof(WAVEHDR));
                                CheckMMRESULT_mmRtn(hr,"ERROR:  Failed unprepare header.  waveOutPrepareHeader",TPR_ABORT,"Driver doesn't really support this format");
                                hr=waveInStop(hwi);
                                QueryMMRESULT_mmRtn(hr,"ERROR:  Failed to stop.  waveInStop",TPR_ABORT,"Driver didn't stop properly");
                                hr=waveInReset(hwi);
                                QueryMMRESULT_mmRtn(hr,"ERROR:  Failed to reset.  waveInReset",TPR_ABORT,"Driver didn't reset properly");
                                hr=waveInUnprepareHeader(hwi,&wh,sizeof(WAVEHDR));
                                QueryMMRESULT_mmRtn(hr,"ERROR:  Failed to unprepare.  waveInUnprepare",TPR_ABORT,"Driver doesn't really support this format");
                                goto StartRecording;
                        } // End Switch(iResponse)
        }
    }


    if(g_useSound) {
            LOG(TEXT("Playing back what has been recorded"));
            hr=waveOutReset(hwo);
            QueryMMRESULT_mmRtn(hr,"ERROR:  Failed to reset.  waveOutReset",TPR_ABORT,"Driver didn't reset device properly");
            hr=waveOutUnprepareHeader(hwo,&wh_out,sizeof(WAVEHDR));
            CheckMMRESULT_mmRtn(hr,"ERROR:  Failed unprepare header.  waveOutPrepareHeader",TPR_ABORT,"Driver doesn't really support this format");
    }
    else {
        if(hwi) {
                hr=waveInStop(hwi);
                QueryMMRESULT_mmRtn(hr,"ERROR:  Failed to stop.  waveInStop",TPR_ABORT,"Driver didn't stop properly");
                hr=waveInReset(hwi);
                QueryMMRESULT_mmRtn(hr,"ERROR:  Failed to reset.  waveInReset",TPR_ABORT,"Driver didn't reset properly");
                hr=waveInUnprepareHeader(hwi,&wh,sizeof(WAVEHDR));
                QueryMMRESULT_mmRtn(hr,"ERROR:  Failed to unprepare header.  waveInUnprepareHeader",TPR_ABORT,"Driver didn't unprepare properly");
                hr=waveInClose(hwi);
                QueryMMRESULT_mmRtn(hr,"ERROR:  Failed to close.  waveInClose",TPR_ABORT,"Driver didn't close device properly");
                switch(dwNotification) {
                         case CALLBACK_FUNCTION:
                        case CALLBACK_THREAD:
                        case CALLBACK_WINDOW:
                                 hr=WaitForSingleObject(hEvent,1000);
                                  QueryCondition_mmRtn(hr==WAIT_TIMEOUT,"ERROR:  Function, Thread or Window didn't notify that it received the close message within one second",TPR_ABORT,"Close message wasn't sent to handler");
                                QueryCondition_mmRtn(hr!=WAIT_OBJECT_0,"ERROR:  Unknown Error while waiting for driver to close",TPR_ABORT,"Unknown");

                        break;
                }
                hwi=NULL;
        }

        hr=waveOutOpen(&hwo,0,&wfx,NULL,NULL,CALLBACK_NULL);
        if((hr!=MMSYSERR_NOERROR)&&(hrReturn)) {
                *hrReturn=hr;
                mmRtn = GetReturnCode(mmRtn, TPR_FAIL);
                goto Error;
        }
        CheckMMRESULT_mmRtn(hr,"ERROR:  Failed to open driver.  waveOutOpen",TPR_ABORT,"Driver doesn't really support this format");
     }
     memcpy(wh_out.lpData,wh.lpData,wh.dwBytesRecorded);
     hr=waveOutPrepareHeader(hwo,&wh_out,sizeof(WAVEHDR));
     CheckMMRESULT_mmRtn(hr,"ERROR:  Failed to prepare header.  waveOutPrepareHeader",TPR_ABORT,"Driver doesn't really support this format");
StartPlayback:
     wh_out.dwLoops=1;
     hr=waveOutWrite(hwo,&wh_out,sizeof(WAVEHDR));
     CheckMMRESULT_mmRtn(hr,"ERROR:  Failed to write.  waveOutWrite",TPR_ABORT,"Driver doesn't really support this format");
     Sleep((dwSeconds+1)*1000);
        if(g_interactive) {
                if(g_headless) {
                        TCHAR tchC;
                        DWORD retVal = GetHeadlessUserInput(TEXT("INTERACTIVE PROMPT:  Did you hear the recording played back? Press 'Y' to continue, or press 'N' or 'R' to Playback the Recording again."),
                                                                                                &tchC);
                        if(retVal != TPR_PASS ) {
                                LOG(TEXT("ERROR: Failed to get headless interactive user input."));
                                if(retVal == TPR_SKIP) // Then Keyboard is not present on device
                                        return TPR_SKIP;
                                else
                                        return GetReturnCode(retVal, TPR_ABORT);
                        }

                        if(tchC == (TCHAR)'N' || tchC == (TCHAR)'n' || tchC == (TCHAR)'R' || tchC == (TCHAR)'r') {
                                goto StartPlayback;
                        }
                        // else user entered 'Y', thus they heard the audio, and thus we can move on with the test
                }
                else { // interactive and not headless
                        HMODULE hCoreDLL = GetCoreDLLHandle();
                        if(NULL != hCoreDLL) {
                                PFNMESSAGEBOX pfnMessageBox = (PFNMESSAGEBOX)GetProcAddress(hCoreDLL, TEXT("MessageBoxW"));
                                if(NULL  != pfnMessageBox)
                                        iResponse = pfnMessageBox(NULL,TEXT("Click Yes to continue, click no to playback recording again"),TEXT("Interactive Response"),MB_YESNO|MB_ICONQUESTION);
                                else {
                                        LOG(TEXT("ERROR: pfnMessageBox is NULL, GetProcAddress failed."));
                                        mmRtn = GetReturnCode(mmRtn, TPR_ABORT);
                                }
                                if(FALSE == FreeCoreDLLHandle(hCoreDLL))
                                        mmRtn = GetReturnCode(mmRtn, TPR_ABORT); // FreeCoreDLL Handle Failed, aborting
                        }
                        else {
                                // Could not get a handle to coredll
                                mmRtn = GetReturnCode(mmRtn, TPR_ABORT);
                                iResponse=IDYES; // Set to Yes to if statement below
                        }
                        if(iResponse==IDNO)         goto StartPlayback;
                }
        }
Error:
        if(hwo) {
                hr=waveOutReset(hwo);
                QueryMMRESULT_mmRtn(hr,"ERROR:  Failed to reset.  waveOutReset",TPR_ABORT,"Driver didn't reset device properly");
                hr=waveOutUnprepareHeader(hwo,&wh_out,sizeof(WAVEHDR));
                QueryMMRESULT_mmRtn(hr,"ERROR:  Failed unprepare header.  waveOutPrepareHeader",TPR_ABORT,"Driver doesn't really support this format");
                hr=waveOutClose(hwo);
                QueryMMRESULT_mmRtn(hr,"ERROR:  Failed to close.  waveOutClose",TPR_ABORT,"Driver didn't close device properly");
        }
        if(hwi) {
                hr=waveInStop(hwi);
                QueryMMRESULT_mmRtn(hr,"ERROR:  Failed to stop.  waveInStop",TPR_ABORT,"Driver didn't stop properly");
                hr=waveInReset(hwi);
                QueryMMRESULT_mmRtn(hr,"ERROR:  Failed to reset.  waveInReset",TPR_ABORT,"Driver didn't reset properly");
                hr=waveInUnprepareHeader(hwi,&wh,sizeof(WAVEHDR));
                QueryMMRESULT_mmRtn(hr,"ERROR:  Failed to unprepare header.  waveInUnprepareHeader",TPR_ABORT,"Driver didn't unprepare properly");
                hr=waveInClose(hwi);
                QueryMMRESULT_mmRtn(hr,"ERROR:  Failed to close.  waveInClose",TPR_ABORT,"Driver didn't close device properly");
                switch(dwNotification) {
                         case CALLBACK_FUNCTION:
                        case CALLBACK_THREAD:
                        case CALLBACK_WINDOW:
                         hr=WaitForSingleObject(hEvent,1000);
                          QueryCondition_mmRtn(hr==WAIT_TIMEOUT,"ERROR:  Function, Thread or Window didn't notify that it received the close message within one second",TPR_ABORT,"Close message wasn't sent to handler");
                        QueryCondition_mmRtn(hr!=WAIT_OBJECT_0,"ERROR:  Unknown Error while waiting for driver to close",TPR_ABORT,"Unknown");
                        break;
                }
        }

        HMODULE hCoreDLL = GetCoreDLLHandle();
        switch(dwNotification) {
                case CALLBACK_WINDOW:
                        if(NULL != hCoreDLL) {
                                PFNPOSTMESSAGE pfnPostMessage = (PFNPOSTMESSAGE)GetProcAddress(hCoreDLL, TEXT("PostMessageW"));
                                if(NULL != pfnPostMessage)
                                        pfnPostMessage(hwnd,WM_QUIT,0,0);
                                else {
                                        LOG(TEXT("ERROR: GetProcAddress failed to find PostMessage in CoreDLL"));
                                        FreeCoreDLLHandle(hCoreDLL);
                                        return TPR_ABORT;
                                }
                        }
                        else {
                                LOG(TEXT("ERROR: Unable to get a handle to CoreDLL"));
                                FreeCoreDLLHandle(hCoreDLL);
                                return TPR_ABORT;
                        }
                break;
                case CALLBACK_THREAD:
                        if(FALSE == PostThreadMessage(dwCallback,WM_QUIT,0,0)) {
                                int ret = GetLastError();
                                LOG(TEXT("ERROR: PostThreadMessage WM_QUIT Failed."));
                                if( ERROR_INVALID_THREAD_ID == ret)
                                        LOG(TEXT("ERROR: INVALID_THREAD_ID"));
                                mmRtn = GetReturnCode(mmRtn, TPR_ABORT);
                        }


                break;
        }
        FreeCoreDLLHandle(hCoreDLL);

        switch(dwNotification) {
                case CALLBACK_WINDOW:
                case CALLBACK_THREAD:
                        hr=WaitForSingleObject(hThread,1000);
                         QueryCondition_mmRtn(hr==WAIT_TIMEOUT,"ERROR:  Thread didn't release within one second",TPR_ABORT,"Unknown");
                        QueryCondition_mmRtn(hr!=WAIT_OBJECT_0,"ERROR:  Unknown Error while waiting for thread to release",TPR_ABORT,"Unknown");
        }
        if(data) {
                delete[] data;
        }
        if(data2) {
                delete[] data2;
        }
        if(hEvent)
                CloseHandle(hEvent);
        if(hThread)
                CloseHandle(hThread);
        return mmRtn;
}

TESTPROCAPI CaptureCapabilities(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) {
        BEGINTESTPROC
        MMRESULT mmRtn;
        WAVEINCAPS wic;
        DWORD tr=TPR_PASS;

        if(g_bSkipIn) return TPR_SKIP;
        LOG(TEXT("INSTRUCTIONS:  This test case displays driver capabilities."));
         LOG(TEXT("INSTRUCTIONS:  Driver capabilies need to be confirmed manually"));
        LOG(TEXT("INSTRUCTIONS:  Please confirm that your driver is capable of performing the functionality that it reports"));
         mmRtn=waveInGetDevCaps(g_dwInDeviceID,&wic,sizeof(wic));
        CheckMMRESULT(mmRtn,"ERROR:  Failed to get device caps.  waveInGetDevCaps",TPR_FAIL,"Unknown");
        LogWaveInDevCaps(g_dwInDeviceID,&wic);
Error:
        return tr;
}

TESTPROCAPI Capture(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) {
        BEGINTESTPROC
        int iResponse = 0;
        WAVEINCAPS wic;
        MMRESULT mmRtn;
        DWORD hrReturn,i,itr,tr=TPR_PASS;

        if(g_bSkipIn) return TPR_SKIP;
         LOG(TEXT("INSTRUCTIONS:  This test case will attempt to record and playback a tone for all common formats"));
         LOG(TEXT("INSTRUCTIONS:  Please listen to playback to ensure that a tone is played"));
        mmRtn=waveInGetDevCaps(g_dwInDeviceID,&wic,sizeof(wic));
        CheckMMRESULT(mmRtn,"ERROR:  Failed to get device caps.  waveInGetDevCaps",TPR_FAIL,"Driver responded incorrectly");
        for(i=0;lpFormats[i].szName;i++) {
                        hrReturn=MMSYSERR_NOERROR;
                    LOG(TEXT("Attempting to record %s"),lpFormats[i].szName);
                itr=RecordWaveFormat(lpFormats[i].szName,g_duration,CALLBACK_NULL,&hrReturn);
                if(wic.dwFormats&lpFormats[i].value) {
                        if(itr==TPR_FAIL)
                                LOG(TEXT("ERROR:  waveInGetDevCaps reports %s is supported, but %s was returned."),lpFormats[i].szName,g_ErrorMap[hrReturn]);
                        else if(itr==TPR_ABORT)
                                {
                                        LOG(TEXT("ERROR:  RecordWaveFormat Returned An ABORT. See above Debug Info For Explanation."));
                                        itr = TPR_FAIL;
                                }
                }
                else if(itr==TPR_PASS) {
                        LOG(TEXT("ERROR:  waveInGetDevCaps reports %s is unsupported, but WAVERR_BADFORMAT was not returned."),lpFormats[i].szName);
                        itr=TPR_FAIL;
                }
                else {
                        if(itr == TPR_ABORT)
                                {
                                        LOG(TEXT("ERROR:  RecordWaveFormat returned an ABORT. See above Debug Info For Explanation."));
                                        itr=TPR_FAIL;
                                }
                        else if(itr == TPR_FAIL)
                                itr = TPR_FAIL;
                        else
                                itr=TPR_PASS;
                }
                if(g_interactive) {
                        if(g_headless) {
                                LOG(TEXT("Running in Interactive Headless Mode."));

                                TCHAR tchC;
                                DWORD retVal = GetHeadlessUserInput(TEXT("INTERACTIVE PROMPT: Did you hear the recorded sound?\n(Press 'Y' for yes, 'N' for No or 'R' for Retry)"),
                                                                                                        &tchC);
                                if(retVal != TPR_PASS ) {
                                        LOG(TEXT("ERROR: Failed to get headless interactive user input."));
                                        if(retVal == TPR_SKIP) // Then Keyboard is not present on device
                                                return TPR_SKIP;
                                        else
                                                return GetReturnCode(retVal, TPR_ABORT);
                                }

                                if(tchC == (TCHAR)'N' || tchC == (TCHAR)'n') {
                                        LOG(TEXT("ERROR:  User said there was no tone produced for %s"),lpFormats[i].szName);
                                        tr|=TPR_FAIL;
                                }
                                else if(tchC == (TCHAR)'R' || tchC == (TCHAR)'r') {
                                        // Try Again
                                        i--;
                                }
                                // else user entered 'Y', thus they heard the audio, and thus we can move on with the test
                        }
                        else{
                                LOG(TEXT("Running in Interactive Mode."));
                                HMODULE hCoreDLL = GetCoreDLLHandle();
                                if(NULL != hCoreDLL) {
                                        PFNMESSAGEBOX pfnMessageBox = (PFNMESSAGEBOX)GetProcAddress(hCoreDLL, TEXT("MessageBoxW"));
                                        if(NULL  != pfnMessageBox)
                                                iResponse = pfnMessageBox(NULL,TEXT("Did you hear the recorded sound? (cancel to retry)"),TEXT("Interactive Response"),MB_YESNOCANCEL|MB_ICONQUESTION);
                                        else {
                                                LOG(TEXT("ERROR: pfnMessageBox is NULL, GetProcAddress failed."));
                                                tr|=TPR_ABORT;
                                        }
                                        if(FALSE == FreeCoreDLLHandle(hCoreDLL))
                                                tr|=TPR_ABORT; // FreeCoreDLL Handle Failed, aborting
                                        }
                                else {
                                        // Could not get a handle to coredll
                                        LOG(TEXT("ERROR: Could not get a handle to CoreDLL"));
                                        tr|=TPR_ABORT;
                                        iResponse=IDYES; // Set to Yes to skip switch statement below
                                }

                                switch(iResponse) {
                                        case IDNO:
                                                LOG(TEXT("ERROR:  User said there was no tone produced for %s"),lpFormats[i].szName);
                                                tr|=TPR_FAIL;
                                        break;
                                        case IDCANCEL:
                                                i--;
                                } // end switch(iResponse)
                        }
                }
                 else tr|=itr;
        }
Error:
        return tr;
}

TESTPROCAPI CaptureNotifications(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) {
        BEGINTESTPROC
        int iResponse = 0;
        WAVEINCAPS wic;
        MMRESULT mmRtn;
        DWORD itr,tr=TPR_PASS,i,j;

        if(g_bSkipIn) return TPR_SKIP;
        LOG(TEXT("INSTRUCTIONS:  This test case will attempt to playback a tone for all types of callbacks"));
        LOG(TEXT("INSTRUCTIONS:  Please listen to playback to ensure that a tone is played"));
         mmRtn=waveInGetDevCaps(g_dwInDeviceID,&wic,sizeof(wic));
        CheckMMRESULT(mmRtn,"ERROR:  Failed to get device caps.  waveInGetDevCaps",TPR_FAIL,"Driver responded incorrectly");
        for(i=0;lpFormats[i].szName;i++)
                if(wic.dwFormats&lpFormats[i].value) break;
        if(!lpFormats[i].szName) {
                LOG(TEXT("ERROR:  There are no supported formats"));
                    return TPR_SKIP;
            }
        for(j=0;lpCallbacks[j].szName;j++) {
                    LOG(TEXT("Attempting to record with %s"),lpCallbacks[j].szName);
                itr=RecordWaveFormat(lpFormats[i].szName,g_duration,lpCallbacks[j].value,NULL);
                if(g_interactive) {
                        if(g_headless) {
                                LOG(TEXT("Running in Interactive Headless Mode."));

                                TCHAR tchC;
                                DWORD retVal = GetHeadlessUserInput(TEXT("INTERACTIVE PROMPT: Did you hear the recorded Sound?\n(Press 'Y' for yes, 'N' for No or 'R' for Retry)"),
                                                                                                        &tchC);
                                if(retVal != TPR_PASS ) {
                                        LOG(TEXT("ERROR: Failed to get headless interactive user input."));
                                        if(retVal == TPR_SKIP) // Then Keyboard is not present on device
                                                return TPR_SKIP;
                                        else
                                                return GetReturnCode(retVal, TPR_ABORT);
                                }

                                if(tchC == (TCHAR)'N' || tchC == (TCHAR)'n') {
                                        LOG(TEXT("ERROR:  User said there was no tone produced for %s"),lpFormats[i].szName);
                                        tr|=TPR_FAIL;
                                }
                                else if(tchC == (TCHAR)'R' || tchC == (TCHAR)'r') {
                                        // Try Again
                                        j--;
                                }
                                // else user entered 'Y', thus they heard the audio, and thus we can move on with the test
                        }
                        else{
                                LOG(TEXT("Running in Interactive Mode."));
                                HMODULE hCoreDLL = GetCoreDLLHandle();
                                if(NULL != hCoreDLL) {
                                        PFNMESSAGEBOX pfnMessageBox = (PFNMESSAGEBOX)GetProcAddress(hCoreDLL, TEXT("MessageBoxW"));
                                        if(NULL  != pfnMessageBox)
                                                iResponse = pfnMessageBox(NULL,TEXT("Did you hear the recorded sound? (cancel to retry)"),TEXT("Interactive Response"),MB_YESNOCANCEL|MB_ICONQUESTION);
                                        else {
                                                LOG(TEXT("ERROR: pfnMessageBox is NULL, GetProcAddress failed."));
                                                tr|=TPR_ABORT;
                                        }
                                        if(FALSE == FreeCoreDLLHandle(hCoreDLL))
                                                tr|=TPR_ABORT; // FreeCoreDLL Handle Failed, aborting
                                        }
                                else {
                                        // Could not get a handle to coredll
                                        tr|=TPR_ABORT;
                                        iResponse=IDYES; // Set to Yes to skip switch statement below
                                }

                                switch(iResponse) {
                                        case IDNO:
                                                LOG(TEXT("ERROR:  User said there was no tone produced for %s"),lpFormats[i].szName);
                                                tr|=TPR_FAIL;
                                        break;
                                        case IDCANCEL:
                                                j--;
                                } // end switch(iResponse)
                        }
                }
                else tr|=itr;
        }
Error:
        return tr;
}

#define MRCHECK(mr,str)\
    if ((mr != MMSYSERR_NOERROR)) { LOG(TEXT(#str) TEXT(" failed. mr=%08x\r\n"), mr); dwRet = TPR_FAIL; goto errorInVirtFreeCapture;}


TESTPROCAPI CaptureVirtualFree(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) {
        BEGINTESTPROC
        PBYTE pBufferBits;
        DWORD dwBufferSize;
        DWORD dwRet = TPR_PASS;
        const DWORD dwTolerance = 1000;
        DWORD dwWait;
        HANDLE hevDone;
        HWAVEIN hwi;

        // Set up defaults
        PTSTR pszFilename = TEXT("wavrec.wav");
        DWORD dwDuration = 5 * 1000;    // record for 5 seconds
        DWORD dwChannels = 1;           // default to mono
        DWORD dwBitsPerSample = 16;     // default to 16-bit samples
        DWORD dwSampleRate = 11025;     // default to 11.025KHz sample rate
        DWORD dwDeviceId = 0;           // capture from device 0

        // check for capture device
        UINT n=waveInGetNumDevs();

        if(0 == n) {
                LOG(TEXT( "ERROR: waveInGetNumDevs reported zero devices, we need at least one."));
                dwRet = TPR_SKIP;
                goto exitFunction;
        }

        // set up the wave format structure
        WAVEFORMATEX wfx;
        wfx.cbSize = 0;
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.wBitsPerSample = (WORD) dwBitsPerSample;
        wfx.nSamplesPerSec = dwSampleRate;
        wfx.nChannels = (WORD) dwChannels;
        wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
        wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;

        // compute the size of the capture buffer
        // compute total # of samples & multiply by blocksize to get sample-aligned buffer size
        dwBufferSize = (dwDuration * wfx.nSamplesPerSec / 1000) * wfx.nBlockAlign;

        // let user know what's going on
        LOG(TEXT("Recording %5d ms to \"%s\": %c%02d %5dHz (%8d bytes)\r\n")
            , dwDuration
            , pszFilename
            , wfx.nChannels == 2 ? L'S' : L'M'
            , wfx.wBitsPerSample
            , wfx.nSamplesPerSec
            , dwBufferSize
            );

        // try to allocate the capture buffer using VirtualAlloc
        if(dwBufferSize < UINT_MAX) {
                // Allocate space for the buffer
                pBufferBits = (BYTE *)VirtualAlloc(NULL, dwBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
                if(pBufferBits == NULL) {
                        LOG(TEXT("Unable to allocate %d bytes for %d ms of audio data\r\n"), dwBufferSize, dwDuration);
                        dwRet = TPR_FAIL;
                        goto exitFunction;
                }
        }
        else {
                LOG(TEXT("ERROR: dwBufferSize >= UINT_MAX "));
                dwRet = TPR_FAIL;
                goto exitFunction;
        }

        // set up the WAVEHDR structure
        WAVEHDR hdr;
        memset(&hdr, 0, sizeof(WAVEHDR));
        hdr.dwBufferLength = dwBufferSize;
        hdr.lpData = (char *) pBufferBits;

        // create an event so we know when capture is completed
        hevDone = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (hevDone == NULL) {
                LOG(TEXT("Unable to create completion event\r\n"));
                dwRet = TPR_ABORT;
                goto exitFunction;
        }

        // open the wave capture device
        MMRESULT mr = waveInOpen(&hwi, g_dwInDeviceID, &wfx, (DWORD) hevDone, NULL, CALLBACK_EVENT);
        if (mr != MMSYSERR_NOERROR) {
                LOG(TEXT("waveInOpen failed. mr=%08x\r\n"), mr);
                dwRet =  TPR_FAIL;
                goto exitFunction;
        }

        // prepare the buffer for capture
        mr = waveInPrepareHeader(hwi, &hdr, sizeof(WAVEHDR));
        MRCHECK(mr, waveInPrepareHeader);

        // submit the buffer for capture
        mr = waveInAddBuffer(hwi, &hdr, sizeof(WAVEHDR));
        MRCHECK(mr, waveInAddBuffer);

        // start capturing
        LOG(TEXT("Starting capture...\r\n"));
        mr = waveInStart(hwi);
        MRCHECK(mr, waveInStart);

        // free the payload buffer here
        // Now attempt to VirtualFree the buffer that is currently being recorded to
        BOOL res = VirtualFree(pBufferBits, 0, MEM_RELEASE);
        if(0 == res) {
                int ngle = GetLastError();
                LOG(TEXT("ERROR: VirtualFree failed. Last Error was %d\n"), ngle);
                dwRet = TPR_FAIL;
                goto errorInVirtFreeCapture;
        }
        else
                LOG(TEXT("VirtualFree Succeeded in freeing the buffer."));

        LOG(TEXT("Waiting for completion event.\r\n"));
        // wait for completion + 1 second tolerance
        dwWait= WaitForSingleObject(hevDone, dwDuration + dwTolerance);
        if (dwWait != WAIT_OBJECT_0) {
            LOG(TEXT("Timeout waiting for capture to complete.\r\n"));
            dwRet = TPR_FAIL;
            mr = waveInReset(hwi);
            if (mr != MMSYSERR_NOERROR) {
                LOG(TEXT("warning: waveInReset failed. mr=%08x\r\n"), mr);
            }
        }



errorInVirtFreeCapture:
        // now clean up: unprepare the buffer
        mr = waveInUnprepareHeader(hwi, &hdr, sizeof(WAVEHDR));
        MRCHECK(mr, waveInUnprepareHeader);

        // close the input stream from capture device & free the event handle
        mr = waveInClose(hwi);
        if (mr != MMSYSERR_NOERROR) {
            LOG(TEXT("Error: waveInClose failed. mr=%08x\r\n"), mr);
            dwRet = TPR_FAIL;
        }

        // free the event
        CloseHandle(hevDone);

exitFunction:
        return dwRet;
}
#undef MRCHECK

/*
  * Function Name: CaptureInitialLatency
  *
  * Purpose: tests the average latency of a single buffer (aka initial latency).
  *
*/

TESTPROCAPI CaptureInitialLatency( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    BEGINTESTPROC

    char*         data                   = NULL;
    DWORD         dwBufferLength         = 0;
    DWORD         dwCSVFileBufferLength;
    DWORD         dwCSVFileBytesWritten  = 0;
    DWORD         dwExpectedRecordTime;         // in milliseconds
    DWORD         dwRetVal               = TPR_PASS;
    HANDLE        hCSVFile;
    HWAVEIN       hwi;
    int           iLatency;                     // in milliseconds
    int           iRecordTimeTolerance   = g_dwInAllowance; // in milliseconds
    LPLATENCYINFO lpLI;
    TCHAR         lpszCSVColumnHeaders[] = TEXT(
        "Capture Time in MS, Sample Frequency (Samples/Sec.), Number of Channels, Bits/Sample, Latency in ms, Status\r\n" );
    static const int iBUFSIZE = 121;
    TCHAR         lpszCSVOutputBuffer[ iBUFSIZE ];
    TCHAR         lpszCSVTestCaseTitle[] = TEXT(
        "Audio Capture Latency Test Results for Device # " );
    MMRESULT      mmRtn;
    TCHAR*        szWaveFormat = NULL; // example TEXT( "500 WAVE_FORMAT_1M08" )
    timer_type    ttObservedRecordTime;         // in milliseconds
    //timer_type    ttRecordEndTime;
    timer_type    ttRecordStartTime;
    timer_type    ttResolution;
    WAVEHDR       wh;
    WAVEFORMATEX  wfx;
    //HANDLE        hevDone;
    DWORD         dwWait;

    // The shell doesn't necessarily want us to execute the test. Make sure first.
    if( uMsg != TPM_EXECUTE )
    {
        LOG(TEXT( "In CaptureInitialLatency: Parameter uMsg != TPM_EXECUTE. uMsg passed in was: %d"), uMsg );
        return TPR_NOT_HANDLED;
    } // if !uMsg

    // check for capture device
        if( !waveInGetNumDevs() )
    {
                LOG( TEXT(
            "ERROR: waveInGetNumDevs reported zero devices, we need at least one."
            ) );
                return TPR_SKIP;
        }

    if( g_pszWaveCharacteristics )
    {
        // Obtain test duration and wave characteristics from the command line.
        if( TPR_PASS != CommandLineToWaveFormatEx( &wfx ) )
        {
                    LOG( TEXT( "FAIL in %s @ line %u:" ), TEXT( __FILE__ ), __LINE__ );
            LOG(
                TEXT( "\tUnable to get command line wave characteristics." ) );
            return TPR_ABORT;
        } // if !TPR_PASS
        wfx.cbSize          = 0;
        dwExpectedRecordTime = g_iLatencyTestDuration;
    } // if g_pszWaveCharacteristics
    else
    {  // !g_pszWaveCharacteristics
        if( lpFTE ) lpLI = (LPLATENCYINFO)lpFTE->dwUserData;
        else
        { // !lpFTE
                    LOG( TEXT( "FAIL in %s @ line %u:" ), TEXT( __FILE__ ), __LINE__ );
            LOG( TEXT( "\tUnable to get wave characteristics." ) );
            return TPR_ABORT;
        } // else !lpFTE

        if( lpLI ) szWaveFormat = (TCHAR*)lpLI->szWaveFormat;
        else
        { // !lpLI
                    LOG( TEXT( "FAIL in %s @ line %u:"), TEXT( __FILE__ ), __LINE__ );
            LOG( TEXT( "\tlpLI NULL.") );
            return TPR_ABORT;
        } // else !lpLI

        if( !szWaveFormat )
        {
                    LOG( TEXT( "FAIL in %s @ line %u: Wave Format NULL." ),
                TEXT(__FILE__), __LINE__ );
            LOG( TEXT( "\tszWaveFormat NULL.") );
            return TPR_ABORT;
        } // if !szWaveFormat

        dwExpectedRecordTime = (int)lpLI->iTime;
        if( dwExpectedRecordTime < 0 ) {
                    LOG( TEXT ("FAIL in %s @ line %u: Expected record time < 0."),
                TEXT(__FILE__),__LINE__ );
            return TPR_ABORT;
        }

        // Extract expected record time and wave format information from
        // szWaveFormat and populate wfx.
            mmRtn = StringFormatToWaveFormatEx( &wfx, szWaveFormat);
        wfx.cbSize = 0;
        if( TPR_PASS != mmRtn )
        {
                    LOG( TEXT( "FAIL in %s @ line %u:"), TEXT(__FILE__),__LINE__ );
                    LOG( TEXT(
                " \tStringFormatToWaveFormatEx returned error code, #%d." ),
                mmRtn );
                    return TPR_ABORT;
        } // if !TPR_PASS
    } // else !g_pszWaveCharacteristics

    // create an event so we know when capture is completed
    hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (hEvent == NULL)
    {
         LOG(TEXT("Unable to create completion event\r\n"));
         return TPR_ABORT;
    }

    g_bLatencyTest = TRUE;
    state = WIM_CLOSE;
    // Open the Wave Input Stream
    mmRtn = waveInOpen( &hwi, g_dwInDeviceID, &wfx, (DWORD)waveInProcCallback, 0, CALLBACK_FUNCTION );
    if( MMSYSERR_NOERROR != mmRtn )
    {
           //if format is not supported, return PASS for error # 32 (WAVERR_BADFORMAT)
          if(mmRtn == WAVERR_BADFORMAT)
          {
              LOG( TEXT ( "\tWave format %u_%u_%u is not supported."), wfx.nSamplesPerSec, wfx.nChannels, wfx.wBitsPerSample  );
              g_formatNotSupported = TRUE;
              return TPR_PASS;
          }
          LOG( TEXT ( "FAIL in %s @ line %u:" ), TEXT(__FILE__),__LINE__ );
          LOG( TEXT ( "\twaveInOpen returned error code, #%d" ), mmRtn );
          LOG( TEXT ( "\tattempting to open for wave format %s."), lpLI->szWaveFormat );
          return TPR_FAIL;
    } // if !MMSYSERR_NOERROR
    // Display test details.
    LOG( TEXT ( "" ) );
    LOG( TEXT ("Capture time = %d ms, Wave Format = %s, Device # %d." ),
        dwExpectedRecordTime,
        szWaveFormat ? szWaveFormat : g_pszWaveCharacteristics,
        g_dwInDeviceID );

    // Set up an audio buffer for test.
    dwBufferLength = dwExpectedRecordTime * wfx.nAvgBytesPerSec / 1000;
    BlockAlignBuffer( &dwBufferLength, wfx.nBlockAlign );
        data          = new char[ dwBufferLength ];
        if( !data )
    {
                LOG(TEXT("ERROR:  \r\n\tNew failed for data [%s:%u]\n"),
            TEXT(__FILE__),__LINE__);
                LOG(TEXT("Possible Cause:  Out of Memory\n"));
        return TPR_ABORT;
    } // if !data

    ZeroMemory( data, dwBufferLength );
    ZeroMemory( &wh,sizeof( WAVEHDR ) );

        // a buffer that is smaller than we expected.
    if( dwExpectedRecordTime >= (ULONG_MAX / wfx.nAvgBytesPerSec) )
    {
                LOG( TEXT( "FAIL in %s @ line %u:" ), TEXT(__FILE__), __LINE__ );
                LOG( TEXT(
            "\tPotential overflow, dwExpectedRecordTime = %d milliseconds."),
            dwExpectedRecordTime );
        delete [] data;
        return TPR_ABORT;
    } // if dwExpectedRecordTime
        PREFAST_SUPPRESS(419, "The above line is checking for overflow. This seems to be prefast noise, since the path/trace Prefast lists is not possible. (Prefast team concurs so far.)");

    wh.lpData         = data;
        wh.dwBufferLength = dwBufferLength;
        wh.dwLoops        = 1;
        wh.dwFlags        = 0;

        // Prepare header
        mmRtn = waveInPrepareHeader( hwi, &wh, sizeof( wh ) );
        if( MMSYSERR_NOERROR != mmRtn )
    {
        LOG( TEXT( "FAIL in %s @ line %u:" ), TEXT(__FILE__), __LINE__ );
        LOG( TEXT( "\twaveInPrepareHeader failed with return code %d." ),
            mmRtn );
        delete [] data;
        return TPR_FAIL;
    } // if !MMSYSERR_NOERROR

    // Retrieve record start time.
    if( !QueryPerformanceCounter(
        reinterpret_cast<LARGE_INTEGER*>( &ttRecordStartTime ) ) )
    {
        LOG( TEXT( "FAIL in %s @ line %u:." ), TEXT(__FILE__), __LINE__ );
        LOG( TEXT( "\tQueryPerformanceCounter failed." ) );
        delete [] data;
        return TPR_ABORT;
    } // if !QueryPerformanceCounter

        // record wh.
    mmRtn=waveInAddBuffer( hwi, &wh, sizeof( wh ) );
        if( MMSYSERR_NOERROR != mmRtn )
    {
        LOG( TEXT( "FAIL in %s @ line %u:" ), TEXT(__FILE__), __LINE__ );
        LOG( TEXT( "\twaveInWrite failed with return code %d." ), mmRtn );
        delete [] data;
        return TPR_FAIL;
    } // if MMSYSERR_NOERROR

        mmRtn=waveInStart( hwi);
    if( MMSYSERR_NOERROR != mmRtn )
    {
        LOG( TEXT( "FAIL in %s @ line %u:" ), TEXT(__FILE__), __LINE__ );
        LOG( TEXT( "\twaveInStart failed." ) );
        delete [] data;
        return TPR_ABORT;
    } // if MMSYSERR_NOERROR

    LOG(TEXT("Waiting for capture completion event...\r\n"));
    // wait for completion + tolerance second
    dwWait= WaitForSingleObject(hEvent, dwExpectedRecordTime + iRecordTimeTolerance);
    if (dwWait != WAIT_OBJECT_0)
    {
         LOG(TEXT("Timeout waiting for capture to complete.\r\n"));
         mmRtn = waveInReset(hwi);
         if (mmRtn != MMSYSERR_NOERROR)
         {
              LOG( TEXT( "FAIL in %s @ line %u:" ), TEXT(__FILE__), __LINE__ );
              LOG( TEXT( "\twaveInReset failed with return code %d." ), mmRtn );
              delete [] data;
              return TPR_FAIL;
         }
    }

    // Test complete, clean up and finish.

    // unprepare header
        mmRtn = waveInUnprepareHeader( hwi, &wh, sizeof( wh ) );
        if( MMSYSERR_NOERROR != mmRtn )
    {
        LOG( TEXT( "FAIL in %s @ line %u:" ), TEXT(__FILE__), __LINE__ );
        LOG( TEXT( "\twaveInUnprepareHeader failed with return code %d." ),
            mmRtn );
        delete [] data;
        return TPR_FAIL;
    } // if !MMSYSERR_NOERROR

    mmRtn = waveInClose( hwi );
    if( MMSYSERR_NOERROR != mmRtn )
    {
                LOG( TEXT( "FAIL in %s @ line %u:"), TEXT(__FILE__), __LINE__ );
                LOG( TEXT( "\twaveInClose returned error code, #%d."), mmRtn);
        delete [] data;
        return TPR_FAIL;
    } // if !MMSYSERR_NOERROR
    delete [] data;
    data = NULL;
    if (hEvent != NULL)
    {
        CloseHandle(hEvent);
        hEvent = NULL;
    }

    // Obtain Performance frequency resolution and calculate latency.
    QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&ttResolution));
        LOG( TEXT ( "Performance frequency resolution = %d ticks/sec." ),
        ttResolution );
    ValidateResolution( ttResolution );

    ttObservedRecordTime
        = ( g_ttRecordEndTime - ttRecordStartTime ) * 1000 / ttResolution;
    iLatency = int( ttObservedRecordTime - dwExpectedRecordTime );
        LOG( TEXT ( "Latency = %d ms." ), iLatency );

        // If Latency exceeds tolerance fail the test.
    if(    ( iLatency < -iRecordTimeTolerance )
        || ( iLatency >  iRecordTimeTolerance ) )
    { // if iLatency
                LOG( TEXT( "FAIL in %s @ line %u:"), TEXT(__FILE__), __LINE__ );
                LOG( TEXT( "Latency = %d: must be >= -%d and <= %d."),
            iLatency, iRecordTimeTolerance, iRecordTimeTolerance );
        dwRetVal = TPR_FAIL;
    } // if iLatency

    if( g_pszCSVFileName )
    {
        hCSVFile = CreateFile( g_pszCSVFileName, GENERIC_WRITE, 0, NULL,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );

        if( INVALID_HANDLE_VALUE == hCSVFile )
                    LOG( TEXT( "Unable to open file %s. Proceeding with test." ),
                g_pszCSVFileName );
        else
        { // !INVALID_HANDLE_VALUE
                //Set file pointer to end of file, to append data.
            if( 0xFFFFFFFF == SetFilePointer( hCSVFile, 0, NULL,FILE_END ) )
                LOG( TEXT( "Unable to set file pointer to end of file file %s. Proceeding with test." ),
                    g_pszCSVFileName );
            else
            { // !0xFFFFFFFF
                int nChars =0;
                if( g_bNeedCSVFileCaptureHeader )
                {
                    // display test case description
                    dwCSVFileBufferLength
                        = wcslen( ( wchar_t * ) lpszCSVTestCaseTitle ) * 2; // since each char is 2 bytes

                    if( 0 == WriteFile( hCSVFile, lpszCSVTestCaseTitle,
                        dwCSVFileBufferLength, &dwCSVFileBytesWritten, NULL ) )
                        LOG( TEXT(
                            "Unable to write audio capture latency test results to file file %s. Error = %d. Proceeding with test." ),
                            g_pszCSVFileName, GetLastError() ) ;

                    nChars = swprintf_s ( lpszCSVOutputBuffer,_countof(lpszCSVOutputBuffer), TEXT( " %d\r\n" ),
                        g_dwInDeviceID );
                    if(nChars < 0)
                    {
                        LOG( TEXT("FAIL: TESTPROCAPI CaptureInitialLatency --swprintf_s Error." ));
                        
                    }
                    dwCSVFileBufferLength
                        = wcslen( ( wchar_t  * ) lpszCSVOutputBuffer ) * 2; // since each char is 2 bytes

                    if( 0 == WriteFile( hCSVFile, lpszCSVOutputBuffer,
                        dwCSVFileBufferLength, &dwCSVFileBytesWritten, NULL ) )
                        LOG( TEXT(
                            "Unable to write audio capture latency test results to file file %s. Error = %d. Proceeding with test." ),
                            g_pszCSVFileName, GetLastError() ) ;

                    // display column headers
                    dwCSVFileBufferLength
                        = wcslen( ( wchar_t  * ) lpszCSVColumnHeaders ) * 2; // since each char is 2 bytes

                    if( 0 == WriteFile( hCSVFile, lpszCSVColumnHeaders,
                        dwCSVFileBufferLength, &dwCSVFileBytesWritten, NULL ) )
                        LOG( TEXT(
                            "Unable to write audio capture latency test results to file file %s. Error = %d. Proceeding with test." ),
                            g_pszCSVFileName, GetLastError() ) ;

                    g_bNeedCSVFileCaptureHeader = FALSE;
                } // if g_bNeedCSVFileCaptureHeader
                // display test results
                nChars = swprintf_s ( lpszCSVOutputBuffer,_countof(lpszCSVOutputBuffer), TEXT( " %d, %d, %d, %d, %d, %s\r\n" ),
                    dwExpectedRecordTime,
                    wfx.nSamplesPerSec,
                    wfx.nChannels,
                    wfx.wBitsPerSample,
                    iLatency,
                    TPR_CODE_TO_TEXT( dwRetVal )
                    );
                if(nChars < 0)
                {
                    LOG( TEXT("FAIL :TESTPROCAPI TCaptureInitialLatency- swprintf_s Error." ));
                }
                dwCSVFileBufferLength = wcslen( ( wchar_t  * ) lpszCSVOutputBuffer ) * 2 ; // since each char is 2 bytes

                if( 0 == WriteFile( hCSVFile, lpszCSVOutputBuffer,
                    dwCSVFileBufferLength, &dwCSVFileBytesWritten, NULL ) )
                    LOG( TEXT(
                        "Unable to write audio capture latency test results to file file %s. Error = %d. Proceeding with test." ),
                        g_pszCSVFileName, GetLastError() ) ;

                FlushFileBuffers( hCSVFile );
            } // else !0xFFFFFFFF
            CloseHandle( hCSVFile );
        } // else !INVALID_HANDLE_VALUE
    } // if g_pszCSVFileName
    return dwRetVal;
} // CaptureInitialLatency()

/*
  * Function Name: CaptureMultipleStreams
  *
  * Purpose: tests the ability to capture multiple audio streams
  * simultaneously.
  *
*/

TESTPROCAPI CaptureMultipleStreams(
    UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    BEGINTESTPROC
/////////////////////////////////////////////////////////////////////////////////////

    BOOL         bExitCodeThreadStatus                         = 0;
    DWORD        dwClosedWaveInHandles                         = 0;
    DWORD        dwDuration                                    = 0;
    DWORD        dwExitCodeThread                              = 0;
    DWORD        dwHandleCount                                 = 0;
    DWORD        dwHandleIndex                                 = 0;
    DWORD        dwLastError;
    DWORD        dwWaitVal;
    HANDLE       hCompletion[ HANDLE_LIMIT ];
    HANDLE       hLaunchCaptureSoundThread[ HANDLE_LIMIT ]     = { NULL };
    HWAVEIN      hwi[ HANDLE_LIMIT ]                           = { NULL };
    LAUNCHCAPTURESOUNDTHREADPARMS
                 LaunchCaptureSoundThreadParms[ HANDLE_LIMIT ] = { NULL };
    MMRESULT     mmRtn;
    UINT         uRet                                          = TPR_PASS;
    WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, 11025, 11025, 1, 8, 0 };

    LOG( TEXT( "In CaptureMultipleStreams." ) );

        // check for capture device
        if( !waveInGetNumDevs() )
    {
                LOG( TEXT(
            "ERROR: waveInGetNumDevs reported zero devices, we need at least one."
            ) );
                return TPR_SKIP;
        }

    do
    {
        // Create an event handle for the wave input stream.
        hCompletion[ dwHandleIndex ] = CreateEvent( NULL, FALSE, FALSE, NULL );
        if( !hCompletion[dwHandleIndex] )
        {
                    LOG( TEXT( "FAIL:\tCouldn't create thread completion event." ) );
                    return TPR_FAIL;
        }

        // Try to open a wave input stream.
        mmRtn = waveInOpen( &( hwi[ dwHandleIndex ] ), WAVE_MAPPER, &wfx,
            (DWORD)( hCompletion[ dwHandleIndex ] ), 0, CALLBACK_EVENT );

        switch( mmRtn )
        {

        case MMSYSERR_NOERROR:
            LOG( TEXT(
                "CaptureMultipleStreams() spawned waveIn handle # %u (0x%08x), and event handle (0x%08x)."
                ), dwHandleIndex, hwi[ dwHandleIndex ],
                hCompletion[ dwHandleIndex ] );

            // Create a thread and pass it the waveIn handle.
            LaunchCaptureSoundThreadParms[ dwHandleIndex ].hCompletion
                = hCompletion[ dwHandleIndex ];
            LaunchCaptureSoundThreadParms[ dwHandleIndex ].hwi
                = hwi[ dwHandleIndex ];
            LaunchCaptureSoundThreadParms[ dwHandleIndex ].wfx = wfx;

            hLaunchCaptureSoundThread[ dwHandleIndex ]
                = CreateThread( NULL, 0, LaunchCaptureSoundThread,
                (PVOID)&(LaunchCaptureSoundThreadParms[ dwHandleIndex ]),
                0, NULL );

            if( NULL == hLaunchCaptureSoundThread[ dwHandleIndex ] )
            {
                dwLastError = GetLastError();
                if( ERROR_MAX_THRDS_REACHED == dwLastError ) LOG( TEXT(
                        "unable to spawn CaptureSoundThread #%u, because the maximum limit has been reached"
                        ), dwHandleIndex + 1 );
                else // !ERROR_MAX_THRDS_REACHED
                {
                    LOG( TEXT(
                        "FAIL:\tError #%u occurred wile attempting to spawn CaptureSoundThread #%u."
                        ), dwLastError, dwHandleIndex );
                    uRet = TPR_FAIL;
                } // !ERROR_MAX_THRDS_REACHED
            } // if NULL

            // Increment dwHandleIndex for the next attempt to open a waveIn
            // handle.
            dwHandleIndex++;
        break;

        //        case MMSYSERR_INVALHANDLE: // Handle quota exceeded
        case MMSYSERR_NOMEM: // The system apparently sets MMSYSERR_NOMEM when
                             // there are no more handles available.  We think
                             // that this is inappropriate, but for now, we're
                             // using it.
        LOG( TEXT( "Out of handles.  index = %u." ), dwHandleIndex );
        break;

        case MMSYSERR_ALLOCATED:
                if( 0 == dwHandleIndex )
            {
                LOG( TEXT(
                    "FAIL:\twaveInOpen couldn't open any audio capture streams.  Return code = %d." ),
                    mmRtn );
                uRet = TPR_FAIL;
            } // if dwHandleIndex
            else // dwHandleIndex > 0
            {
                LOG( TEXT(
                    "WARNING: SKIPPING TEST: Unable to open multiple capture streams."
                    ) );
                LOG( TEXT(
                    "\tMake sure that your driver model supports multiple capture streams. (e.g. WaveDev2 Based Drivers)"
                    ) );
                uRet = TPR_SKIP;
            } // dwHandleIndex > 0
        break;

        default:
            LOG( TEXT( "FAIL:\twaveInOpen() returned error code = #%d." ),
                mmRtn );
            uRet = TPR_FAIL;
        break;
        } // switch
    } while( ( MMSYSERR_NOERROR == mmRtn )
      &&   ( dwHandleIndex < HANDLE_LIMIT ) );

    switch( mmRtn )
    {

    case MMSYSERR_NOERROR:
        LOG( TEXT(
            "ABORT:\tOpened %u waveIn handles. This exceeds the presumed limit."
            ), dwHandleIndex );
        uRet = TPR_ABORT;
    break;

    //        case MMSYSERR_INVALHANDLE: // Handle quota exceeded
    case MMSYSERR_NOMEM: // The system apparently sets MMSYSERR_NOMEM when
                         // there are no more handles available.  We think
                         // that this is inappropriate, but for now, we're
                         // using it.
        LOG( TEXT( "%u waveIn Handles opened." ), dwHandleIndex );
        break;

    case MMSYSERR_ALLOCATED:
    default:
        // An error occurred and was processed, in the previous do-while loop.
    break;

    } // switch

    // Close all the handles.
    // dwHandleIndex was never used as an index, but it gives the correct
    // number of handles opened.
    dwHandleCount = dwHandleIndex;
    for( dwHandleIndex = 0; dwHandleIndex < dwHandleCount; dwHandleIndex++ )
    {
        dwWaitVal = WaitForSingleObject(
            hLaunchCaptureSoundThread[ dwHandleIndex ], 3000 );
        switch( dwWaitVal )
        {
        case WAIT_OBJECT_0:
            LOG( TEXT( "Thread 0x%08x finished." ),
                hLaunchCaptureSoundThread[ dwHandleIndex ] );
        break;
        case WAIT_TIMEOUT:
            LOG( TEXT(
                "FAIL:\tThread 0x%08x didn't finish in expected time." ),
                hLaunchCaptureSoundThread[ dwHandleIndex ] );
            uRet = TPR_FAIL;
        break;
        default:
            LOG( TEXT(
                "FAIL:\tThread 0x%08x failed to finish in expected time. WaitForSingleObject() returned %d."
                ), hLaunchCaptureSoundThread[ dwHandleIndex ], dwWaitVal );
            uRet = TPR_FAIL;
        break;
        } // switch dwWaitVal

        // break loop after 6 seconds
        bExitCodeThreadStatus = 0;
        dwExitCodeThread      = STILL_ACTIVE;
        dwDuration            = 0;
        while( STILL_ACTIVE == dwExitCodeThread )
        {
            bExitCodeThreadStatus = GetExitCodeThread(
                hLaunchCaptureSoundThread[ dwHandleIndex ],
                &dwExitCodeThread );
            Sleep(100);
            dwDuration += 100;

            if( dwDuration > 5000)
            {
                dwExitCodeThread = 0;
                LOG( TEXT(
                    "FAIL:\tTimed out while waiting for status of thread #%u." ),
                    dwHandleIndex );
                uRet = TPR_FAIL;
            } // if dwDuration
        } // while STILL_ACTIVE

        if( 0 == bExitCodeThreadStatus )
        {
            dwLastError = GetLastError();
            LOG( TEXT(
                "FAIL:\tError #%u occurred wile invoking GetExitCodeThread() for handle #%u."
                ), dwLastError, dwHandleIndex );
            uRet = TPR_FAIL;
        } // if bExitCodeThreadStatus
        else // !bExitCodeThreadStatus
        {
            LOG( TEXT( "GetExitCodeThread() succeeded for handle #%i." ),
                dwHandleIndex );
        } // else !bExitCodeThreadStatus

        if( TPR_PASS != dwExitCodeThread )
        {
            LOG( TEXT(
                "FAIL:\tLaunchCaptureSoundThread() thread handle %u failed, with return code = %d."
                ), dwHandleIndex, dwExitCodeThread );
            uRet = TPR_FAIL;
        } // if !TPR_PASS

        if( 0 == CloseHandle( hLaunchCaptureSoundThread[ dwHandleIndex ] ) )
        {
            LOG( TEXT( "FAIL:\tCouldn't close thread handle #%u (ox%08x)." ),
                dwHandleIndex, hLaunchCaptureSoundThread[ dwHandleIndex ] );
            uRet = TPR_FAIL;
        } // if !CloseHandle
        else // CloseHandle
        {
            hLaunchCaptureSoundThread[ dwHandleIndex ] = NULL;
            dwClosedWaveInHandles++;
        } // else CloseHandle

        mmRtn = waveInClose( hwi[ dwHandleIndex ] );
        if( MMSYSERR_NOERROR != mmRtn )
        {
            LOG( TEXT( "FAIL:\tCouldn't close handle #%u." ), dwHandleIndex );
            LOG( TEXT(
                "\twaveInClose() failed:  Returned error code = #%d." ),
                mmRtn );
            uRet = TPR_FAIL;
        } // if !MMSYSERR_NOERROR
        else // MMSYSERR_NOERROR
            hwi[ dwHandleIndex ] = 0;

    } // for dwHandleIndex

    LOG( TEXT( "%d waveIn Handles closed." ), dwClosedWaveInHandles );
    return uRet;
} // CaptureMultipleStreams()

/*
  * Function Name: CaptureInitialLatencySeries
  *
  * Purpose: tests the average latency of a single buffer (aka initial latency)
  * for varying record times.  The entire series of tests will be repeated for
  * every wave format specified in the latency test table.
  *
*/

TESTPROCAPI CaptureInitialLatencySeries( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
    BEGINTESTPROC

    DWORD         dwSeriesTestResult = TPR_PASS;
    DWORD         dwTestResult;
    int           iOriginalTime;
    int           iRet;
    LPLATENCYINFO lpLI;
    BOOL          bContinue;


    // The shell doesn't necessarily want us to execute the test. Make sure
    // first.
        if( uMsg != TPM_EXECUTE )
    {
            LOG( TEXT(
            "In CaptureInitialLatencySeries: Parameter uMsg != TPM_EXECUTE. uMsg passed in was: %d" ),
            uMsg );
                return TPR_NOT_HANDLED;
    } // if !uMsg

    // check for capture device
        if( !waveInGetNumDevs() )
    {
                LOG( TEXT(
            "ERROR: waveInGetNumDevs reported zero devices, we need at least one."
            ) );
                return TPR_SKIP;
        }

    if( g_pszWaveCharacteristics )
    {
        iRet = _stscanf_s( g_pszWaveCharacteristics, TEXT( "%u" ),
            &g_iLatencyTestDuration );
            if( iRet != 1 )
        {
                    LOG( TEXT( "ERROR in %s @ line %u:" ),
                TEXT( __FILE__ ), __LINE__ );
                    LOG( TEXT( "\t%s not recognized\n" ), g_pszWaveCharacteristics );
                    LOG( TEXT(
                "\tPossible Cause:  Command line wave characteristic format not in the form: d_f_c_b\n"
                ) );
                    return TPR_FAIL;
        } // if iRet
        // Run a series of tests using the wave format from the command line,
        // and a series of decreasing record times.
        do{
               dwTestResult = CaptureInitialLatency( uMsg, tpParam, lpFTE );
               dwSeriesTestResult = GetReturnCode( dwTestResult, dwSeriesTestResult );

               //if the format is not supported, we do not need to execute the test for different durations
               if(g_formatNotSupported == TRUE)
               {
                       g_formatNotSupported = FALSE;
                       break;
               }
               bContinue = ( g_iLatencyTestDuration > 0 ) ? true : false; // add empty sample case
               g_iLatencyTestDuration /= 2;
        } while( bContinue );
        g_iLatencyTestDuration = 0;
    } // if g_pszWaveCharacteristics
    else
    {
        if( lpFTE ) lpLI = (LPLATENCYINFO)lpFTE->dwUserData;
        else // !lpFTE
        {
                    LOG( TEXT( "ERROR in %s @ line %u:" ),
                TEXT( __FILE__ ), __LINE__ );
                    LOG( TEXT( "\tlpFTE NULL." ) );
                    return TPR_FAIL;
        } // else !lpFTE

        // Run a series of tests for each line in the latency test table.
        while( lpLI->szWaveFormat )
        {
            // Save the original record time, so that it can be restored for any
            // future runs.
            iOriginalTime = lpLI->iTime;

             // Run a series of tests with the same wave format, but decreasing
             // record times.
            do
            {
                    dwTestResult = CaptureInitialLatency( uMsg, tpParam, lpFTE );
                    dwSeriesTestResult = GetReturnCode( dwTestResult, dwSeriesTestResult );

                    //if the format is not supported, we do not need to execute the test for different durations
                    if(g_formatNotSupported == TRUE)
                    {
                            g_formatNotSupported = FALSE;
                            break;
                    }
                    bContinue = ( lpLI->iTime > 0 ) ? true: false; // add empty sample case
                    lpLI->iTime /= 2;
            } while( bContinue );

            // Restored the rigional record time, for any later runs.
            lpLI->iTime = iOriginalTime;

            lpLI++;
            lpFTE->dwUserData = (DWORD)lpLI;
        } // while lpLI->szWaveFormat
    } // else !g_pszWaveCharacteristics
    return dwSeriesTestResult;
} // CaptureInitialLatencySeries




extern BOOL FullDuplex();

/*
  * Function Name: CaptureMixing
  *
  * Purpose: This test stresses the system by initiating a capture and than
  * invoking PlaybackMixing to launch a series of sound threads. It supports an
  * "interactive" command line option, which allows the user to monitor the
  * test and fail it, if the sound quality is unacceptable.
  *
*/

TESTPROCAPI CaptureMixing( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
     BEGINTESTPROC

          if( g_bSkipOut )
               return TPR_SKIP;

          char*        data                  = NULL;
          DWORD        dwBufferLength        = 0;    // in bytes
          DWORD        dwExpectedRecordTime  = ( g_dwNoOfThreads + 3 ) * PLAY_GAP; // milliseconds
          DWORD        dwRecordCounter;
          DWORD        dwRecordTimeTolerance = 200; // in milliseconds
          DWORD        dwUserResponse;
          HWAVEIN      hwi;
          MMRESULT     mmRtn;
          timer_type   RecordStartTime;      // in ticks
          TCHAR*       szWaveFormat          = NULL; // example TEXT( "500 WAVE_FORMAT_1M08" )
          DWORD        tr                    = TPR_PASS;
          timer_type   ttRecordEndTime;      // in ticks
          timer_type   ttResolution;         // in ticks/sec
          WAVEFORMATEX wfx;
          WAVEHDR      wh;

          //---- check for capture device
          if( !waveInGetNumDevs() )
          {
               LOG( TEXT( "ERROR: waveInGetNumDevs reported zero devices, we need at least one." ) );
               return TPR_SKIP;
          }

          //---- If either the command line or the driver reports half-duplex operation,
          //---- we should skip this case because it requires input and output handles open at the same time.
          LOG( TEXT( "Checking Driver's Ability to Handle Full-Duplex Operation" ) );

          if( !FullDuplex() )
          {
               //---- driver reports half-duplex
               if( !g_useSound )
               {
                    //---- command line agrees, this is a half duplex device, skip this test
                    LOG( TEXT( "Command line reports half-duplex, driver agrees, skipping this test." ) );
                    return TPR_SKIP;
               }
               else
               {
                    //---- command line disagrees
                    //---- turn off full duplex operation, log this and abort this test
                    LOG( TEXT( "Warning:  Unable to open waveIn and waveOut at the same time" ) );
                    LOG( TEXT( "          Your driver claims Half-Duplex Operation" ) );
                    LOG( TEXT( "          Your commandline claims Full-Duplex Operation" ) );
                    LOG( TEXT( "          Turning off Full-Duplex Operation and aborting this test." ) );
                    g_useSound = FALSE;
                    return TPR_ABORT;
               }
          }
          else if( !g_useSound )
          {
               //---- driver reports full duplex, command line disagrees (has -e switch)
               //---- log this and abort the test
               LOG(TEXT("Failure:  Able to open waveIn and waveOut at the same time"));
               LOG(TEXT("          Your driver claims Full-Duplex Operation"));
               LOG(TEXT("          Your commandline claims Half-Duplex Operation (-c \"-e\")"));
               LOG(TEXT("          Fix your driver to work in Half-Duplex by making sure that" ));
               LOG(TEXT("          waveIn and waveOut cannot be opened at the same time"));
               LOG(TEXT("          or test your driver as a Full-Duplex driver"));
               LOG(TEXT("                without commandline -c \"-e\" options."));

               return TPR_ABORT;
          }




     Begin:
          LOG( TEXT( "Attempting to capture a sound." ) );

          //---- Specify format of waveform-audio data.
          ZeroMemory( &wfx, sizeof( wfx ) );

          wfx.wFormatTag           = WAVE_FORMAT_PCM;
          wfx.nChannels            = 1;
          wfx.nSamplesPerSec       = 11025;
          wfx.wBitsPerSample       = 8;
          wfx.nBlockAlign          = wfx.nChannels * wfx.wBitsPerSample / 8;
          wfx.nAvgBytesPerSec      = wfx.nSamplesPerSec * wfx.nBlockAlign;
          wfx.cbSize               = 0;

          //---- Open the Wave Input Stream
          mmRtn = waveInOpen( &hwi, g_dwInDeviceID, &wfx, NULL, 0, NULL );
          if( MMSYSERR_NOERROR != mmRtn )
          {
               LOG( TEXT( "FAIL in %s @ line %u:" ), TEXT( __FILE__ ), __LINE__ );
               LOG( TEXT( "\twaveInOpen returned error code, #%d." ), mmRtn );
               return TPR_FAIL;
          }

          dwBufferLength = dwExpectedRecordTime * wfx.nAvgBytesPerSec / 1000;
          BlockAlignBuffer( &dwBufferLength, wfx.nBlockAlign );
          data = new char[ dwBufferLength ];
          if( !data )
          {
               LOG( TEXT( "ERROR:  New failed for data [%s:%u]\n" ), TEXT( __FILE__ ), __LINE__ );
               LOG(TEXT("\tPossible Cause:  Out of Memory\n"));
               return TPR_ABORT;
          }

          ZeroMemory( data, dwBufferLength );
          ZeroMemory( &wh, sizeof( WAVEHDR ) );

          wh.lpData         = data;
          wh.dwBufferLength = dwBufferLength;
          wh.dwLoops        = 0;
          wh.dwFlags        = 0;

          //---- Prepare header
          mmRtn = waveInPrepareHeader( hwi, &wh, sizeof( wh ) );
          if( MMSYSERR_NOERROR != mmRtn )
          {
               LOG(      TEXT( "FAIL in %s @ line %u:\r\n\twaveInPrepareHeader failed with return code %d." ),
                         TEXT(__FILE__), __LINE__, mmRtn );
               delete[] data;
               data = 0;
               return TPR_FAIL;
          }

          if( MMSYSERR_NOERROR != waveInStart( hwi ) )
          {
               LOG(      TEXT( "FAIL in %s @ line %u:\r\n\twaveInStart failed." ),
                         TEXT(__FILE__), __LINE__ );
               delete[] data;
               data = 0;
               return TPR_ABORT;
          }

          //---- Retrieve record start time.
          if( !QueryPerformanceCounter( reinterpret_cast<LARGE_INTEGER*>( &RecordStartTime ) ) )
          {
               LOG( TEXT( "FAIL in %s @ line %u:." ), TEXT(__FILE__), __LINE__ );
               LOG( TEXT("\tQueryPerformanceCounter failed to record start time." ) );
               tr = GetReturnCode( tr, TPR_ABORT );
          }

          //---- Send input buffer, wh, to the waveform-audio input device, to start capture.
          mmRtn = waveInAddBuffer( hwi, &wh, sizeof( wh ) );
          if( MMSYSERR_NOERROR != mmRtn )
          {
               LOG( TEXT( "FAIL in %s @ line %u:." ), TEXT( __FILE__ ), __LINE__ );
               LOG( TEXT( "\twaveInAddBuffer failed with return code %d." ), mmRtn );
               tr = GetReturnCode( tr, TPR_FAIL );
          }

          tr = GetReturnCode( tr, LaunchMultipleAudioPlaybackThreads( uMsg ) );

          //---- Wait for wh to finish capture.
          for(
               dwRecordCounter = 0;
               ( dwRecordCounter < ( dwExpectedRecordTime + dwRecordTimeTolerance ) ) && !( wh.dwFlags & WHDR_DONE );
               dwRecordCounter++
             )
               Sleep( 1 );

          //---- Fail the test if capture isn't done.
          if( !( wh.dwFlags & WHDR_DONE ) )
          {
               LOG(      TEXT ( "FAIL in %s @ line %u:\r\n\tCapture time too long." ),
                         TEXT(__FILE__), __LINE__ );
               tr = GetReturnCode( tr, TPR_FAIL );
          }

          //---- Retrieve record end time.
          if( !QueryPerformanceCounter( reinterpret_cast<LARGE_INTEGER*>( &ttRecordEndTime ) ) )
          {
               LOG( TEXT( "FAIL in %s @ line %u:." ), TEXT(__FILE__), __LINE__ );
               LOG( TEXT("\tQueryPerformanceCounter failed to record end time." ) );
               tr = GetReturnCode( tr, TPR_ABORT );
          }

          //---- Obtain Performance frequency resolution.
          QueryPerformanceFrequency( reinterpret_cast<LARGE_INTEGER*>( &ttResolution ) );
          LOG( TEXT( "Performance frequency resolution = %d ticks/sec." ), ttResolution );
          ValidateResolution( ttResolution );

          //---- Test complete, clean up and finish
          mmRtn = waveInUnprepareHeader( hwi, &wh, sizeof( wh ) );
          if( MMSYSERR_NOERROR != mmRtn )
          {
               LOG( TEXT( "FAIL in %s @ line %u:." ), TEXT(__FILE__), __LINE__ );
               LOG( TEXT( "\twaveInUnprepareHeader failed with return code %d." ), mmRtn );
               tr = GetReturnCode( tr, TPR_FAIL );
          }

          mmRtn = waveInClose( hwi );
          if( MMSYSERR_NOERROR != mmRtn )
          {
               LOG( TEXT( "FAIL in %s @ line %u:."), TEXT( __FILE__ ), __LINE__ );
               LOG( TEXT( "\twaveInClose returned error code, #%d."), mmRtn );
               tr = GetReturnCode( tr, TPR_FAIL );
          }
          delete[] data;
          data = NULL;

          if( g_interactive )
          {
               //---- Let user monitor sound quality.
               dwUserResponse = AskUserAboutSoundQuality();
               if( TPR_RERUN_TEST == dwUserResponse )
                    goto Begin;
               else
                    tr = GetReturnCode( tr, dwUserResponse );
          }
          return tr;
}
