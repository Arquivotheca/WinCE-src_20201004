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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//

#include "Dshow.h"
#include "tux.h"
#include "kato.h"
#include "logger.h"
#include "globals.h"
#include <initguid.h>
#include "SourceFilterGraphEx.h"
#include "SinkFilterGraphEx.h"
#include "dvrinterfaces.h"
#include "Grabber.h"
#include "helper.h"
#include "eventhandler.h"

TCHAR * pszSinkFilterGraphFileName   = NULL;
TCHAR * pszSourceFilterGraphFileName = NULL;
FILE *    g_fp = NULL;    // used for file, otherwise open file in callback is costly.
bool    g_flag = false;    // used for callback to tell system if test passed
int        g_iCount = 0;    // used to count how many sample pass, detect stream stall.

HRESULT g_hr;
#define _CHK(x, y) if(FAILED(g_hr = x)) {LogError(__LINE__, pszFnName, TEXT("Failed at %s : 0x%x."), y, g_hr); g_flag = false; } else LogText(__LINE__, pszFnName, TEXT("Succeed at %s."), y);

BOOL TestCases_Init()
{
    return TRUE;
}

BOOL TestCases_Cleanup()
{
    return TRUE;
}

TESTPROCAPI ConvertLongPermPartOutBuffer(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // This test is to try to set Permanent file size really small,
    // Then have following distribution:
    //   |------------------------|    pause buffer
    // P - P - T - T - T - ... LIVE

    // When delete the Perm recording, since some of the Perm in pause buffer,
    // It will be converted to Temp Rec first, then delete the outmost set...
    // This test is difficult. we are using 141kb/s. Set Perm size as 50 K.
    // Perm recording last 30 seconds

    const TCHAR * pszFnName = TEXT("ConvertLongPermPartOutBuffer");

    CSinkFilterGraph mySink;
    CSourceFilterGraph mySourceEx;
    LPOLESTR lpFileName = NULL;
    LPOLESTR lpLiveToken = NULL;
    INT32 TempBufferSize = 60;    // 60 sec buffer
    INT32 i32LP = 5;    // try construct and destruct 3 times the DVREngine graph
    DWORD dwSize = 5000000;        // 5M on MPEG, about 12 seconds to fill a file.
    LONGLONG llEarly1 = 0;
    LONGLONG llEarly2 = 0;
    LONGLONG llTemp = 0;

    DWORD dwError = Helper_DvrRegSetValueDWORD(MAX_PERMANENT_FILE_SIZE_KEY, &dwSize);
    if(ERROR_SUCCESS != dwError)
    {
        LogError(__LINE__, pszFnName, TEXT("Failed to set Permanent Rec File size as %d."), dwSize);
        return TPR_ABORT;
    }
    g_flag = true;

    mySourceEx.LogToConsole(true);
    mySink.LogToConsole(true);

    mySourceEx.SetCommandLine(g_pShellInfo->szDllCmdLine);
    mySink.SetCommandLine(g_pShellInfo->szDllCmdLine);

    _CHK(mySink.Initialize(), TEXT("Initialize Sink Graph"));
    _CHK(mySourceEx.Initialize(), TEXT("Initialize mySourceEx Graph"));

    _CHK(mySink.SetupFilters(), TEXT("Setting up Sink Graph"));

    LogText(__LINE__, pszFnName, TEXT("Temp buffer size is %d."), TempBufferSize);

    LPOLESTR pRecordingPath = NULL;
    _CHK(mySink.SetRecordingPath(), TEXT("SetRecording Path"));
    _CHK(mySink.GetRecordingPath(&pRecordingPath), TEXT("SetRecording Path"));
    LogText(TEXT("Set recording Path as %s."), pRecordingPath);
    CoTaskMemFree(pRecordingPath);
    pRecordingPath = NULL;

    _CHK(mySink.BeginTemporaryRecording(TempBufferSize * 1000), TEXT("BeginTemporaryRecording with 60 buffer"));
    _CHK(mySink.GetBoundToLiveToken(&lpLiveToken), TEXT("GetBoundToLiveToken"));
    LogText(__LINE__, pszFnName, TEXT("live token is %s."), lpLiveToken);
    _CHK(mySink.Run(), TEXT("Run Graph"));
    Sleep(10*1000);
    _CHK(mySourceEx.SetupFilters(lpLiveToken), TEXT("Source setup filters"));
    _CHK(mySourceEx.Run(), TEXT("Source run"));
    _CHK(mySourceEx.GetAvailable(&llEarly1, &llTemp), TEXT("Get Available now at Live"));
    LogText(__LINE__, pszFnName, TEXT("Early is %I64d, Late is %I64d"), llEarly1, llTemp);    // trial get, nonsense here.

    _CHK(mySink.BeginPermanentRecording(0), TEXT("BeginPermanentRecording"));
    _CHK(mySink.GetCurFile(&lpFileName, NULL), TEXT("Get Perm rec filename"));
    LogText(__LINE__, pszFnName, TEXT("FileName is %s."), lpFileName);

    LogText(__LINE__, pszFnName, TEXT("Going to sleep %d seconds to fill Pause Buffer."), TempBufferSize);
    Sleep((TempBufferSize + 5) * 1000);    // Sleep for buffer filled.
    _CHK(mySink.BeginTemporaryRecording(TempBufferSize * 1000), TEXT("BeginTemporaryRecording with 60 buffer"));
    LogText(__LINE__, pszFnName, TEXT("Sleep %d seconds."), TempBufferSize/2);
    Sleep(TempBufferSize*500);        // Buffer half Temp Half Perm
    _CHK(mySourceEx.GetAvailable(&llEarly1, &llTemp), TEXT("Get Available now before delete Perm recording"));
    LogText(__LINE__, pszFnName, TEXT("Early is %I64d, Late is %I64d, length is %I64d."), llEarly1, llTemp, llTemp - llEarly1);
    LogText(__LINE__, pszFnName, TEXT("Now will delete Perm Rec."));
    _CHK(mySink.DeleteRecording(lpFileName), TEXT("delete Perm rec"));
    Sleep(5*1000);
    _CHK(mySourceEx.GetAvailable(&llEarly2, &llTemp), TEXT("Get Available now after delete Perm rec"));
    LogText(__LINE__, pszFnName, TEXT("Early is %I64d, Late is %I64d, Length is %I64d."), llEarly2, llTemp, llTemp-llEarly2);

    if(llEarly2 - llEarly1 < 10*1000*10000)    // means perm does not run out of buffer
    {
        LogText(__LINE__, pszFnName, TEXT("The pause buffer start before and after permanent recording deletion are %I64d, %I64d."),
            llEarly1, llEarly2);
        LogError(__LINE__, pszFnName, TEXT("They should differ about 30s, in fact they differ %I64d seconds."), (llEarly2-llEarly1)/1000/10000);
        g_flag = false;
    }
    // The source graph must be uninitialized first if you're bound to live.
    // otherwise the sink graph can't delete the outstanding temporary files.
    mySourceEx.UnInitialize();
    mySink.UnInitialize();

    if(g_flag == false)
    {
        return TPR_FAIL;
    }
    else
    {
        return TPR_PASS;
    }
}

TESTPROCAPI PauseBufferLenAccurate(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // This test straight test pause buffer length should keep somehow accurate    

    const TCHAR * pszFnName = TEXT("PauseBufferLenAccurate");

    CSinkFilterGraph mySink;
    CSourceFilterGraphEx mySourceEx;
    LPOLESTR lpFileName = NULL;
    LPOLESTR lpLiveToken = NULL;
    INT32 TempBufferSize = 30;    // 60 sec buffer
    INT32 i32LP = 5;    // try construct and destruct 3 times the DVREngine graph
    DWORD dwSize = 50000;    // 50 K to make a lot small Temp file chunks
    LONGLONG llEarly = 0;
    LONGLONG llLate = 0;
    LONGLONG llTemp = 0;

    DWORD dwError = Helper_DvrRegSetValueDWORD(MAX_TEMPORARY_FILE_SIZE_KEY, &dwSize);
    if(ERROR_SUCCESS != dwError)
    {
        LogError(__LINE__, pszFnName, TEXT("Failed to set Temp Rec File size as %d."), dwSize);
        return TPR_ABORT;
    }
    g_flag = true;

    mySourceEx.LogToConsole(true);
    mySink.LogToConsole(true);

    mySourceEx.SetCommandLine(g_pShellInfo->szDllCmdLine);
    mySink.SetCommandLine(g_pShellInfo->szDllCmdLine);

    _CHK(mySink.Initialize(), TEXT("Initialize Sink Graph"));
    _CHK(mySourceEx.Initialize(), TEXT("Initialize mySourceEx Graph"));

    _CHK(mySink.SetupFilters(), TEXT("Setting up Sink Graph"));

    LogText(__LINE__, pszFnName, TEXT("Temp buffer size is %d."), TempBufferSize);

    LPOLESTR pRecordingPath = NULL;
    _CHK(mySink.SetRecordingPath(), TEXT("SetRecording Path"));
    _CHK(mySink.GetRecordingPath(&pRecordingPath), TEXT("SetRecording Path"));
    LogText(TEXT("Set recording Path as %s."), pRecordingPath);
    CoTaskMemFree(pRecordingPath);
    pRecordingPath = NULL;

    _CHK(mySink.BeginTemporaryRecording(TempBufferSize * 1000), TEXT("BeginTemporaryRecording with 60 buffer"));
    _CHK(mySink.GetBoundToLiveToken(&lpLiveToken), TEXT("GetBoundToLiveToken"));
    LogText(__LINE__, pszFnName, TEXT("live token is %s."), lpLiveToken);
    _CHK(mySink.Run(), TEXT("Run Graph"));
    Sleep(TempBufferSize*1000);
    _CHK(mySourceEx.SetupFilters(lpLiveToken), TEXT("Source setup filters"));
    _CHK(mySourceEx.Run(), TEXT("Source run"));
    while(i32LP-- > 0)
    {
        _CHK(mySourceEx.GetAvailable(&llEarly, &llLate), TEXT("Get Available now at Live"));
        LogText(__LINE__, pszFnName, TEXT("Early is %I64d, Late is %I64d, Length is %I64d."), llEarly, llLate, llLate-llEarly);
        if( (llLate-llEarly)/10000 < (TempBufferSize-10)*1000
            || (llLate-llEarly)/10000 > (TempBufferSize+10)*1000 )     //too small or large
        {
            LogError(__LINE__, pszFnName, TEXT("The pause buffer is %I64d sec, compare TempBufferSize become too large."), (llLate-llEarly)/1000/10000);
            g_flag = false;
            break;
        }
        Sleep(10*1000);

    }

    // The source graph must be uninitialized first if you're bound to live.
    // otherwise the sink graph can't delete the outstanding temporary files.
    mySourceEx.UnInitialize();
    mySink.UnInitialize();

    if(g_flag == false)
    {
        return TPR_FAIL;
    }
    else
    {
        return TPR_PASS;
    }
}

#if 0
// Below are just helper tests
TESTPROCAPI AsfSourceTunningTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    //    This is just for Tom's request as test ASF Source Tunning.

    const TCHAR * pszFnName = TEXT("AsfSourceTunningTest");

    DWORD result = TPR_FAIL;
    HRESULT hr = E_FAIL;
    long lFoundSignal = 0;
    long lchannel = 0;

    CSourceFilterGraphEx mySource;

    //CComPtr<IBaseFilter> pEm10 = NULL;
    //CComPtr<IBaseFilter> pGrabber = NULL;
    //CComPtr<IBaseFilter> pDVRSink = NULL;
    CComPtr<IAMTVTuner> pTvTuner = NULL;
    //CComPtr<IBaseFilter> pDemux = NULL;
    //CComPtr<IGrabberSample> pGrabberSample = NULL;
    //CComPtr<IStreamBufferCapture> pIStreamBufferCapture = NULL;

    g_flag = true;

    mySource.LogToConsole(true);
    _CHK(mySource.Initialize(), TEXT("init Source"));

    _CHK(mySource.SetupDemuxDirectPlayBackFromFile(), TEXT("Setup DirectPlayback"));
    _CHK(mySource.FindInterface(IID_IAMTVTuner, (void **)&pTvTuner), TEXT("Find IAMTvTuner Interface"));

    lchannel = 1;
    _CHK(pTvTuner->put_Channel(lchannel,AMTUNER_SUBCHAN_DEFAULT,AMTUNER_SUBCHAN_DEFAULT), TEXT("IAMTvTuner putchannel to channel lchannel"));
    LogText(__LINE__, pszFnName, TEXT("Tune to channel %d."), lchannel);
    _CHK(mySource.Run(), TEXT("source Run"));
    // Now building graph, grabber and DVRSink;

    while(true)
    {
        lchannel = 2;
        _CHK(pTvTuner->put_Channel(lchannel,AMTUNER_SUBCHAN_DEFAULT,AMTUNER_SUBCHAN_DEFAULT), TEXT("IAMTvTuner tune to channel lchannel"));
        LogText(__LINE__, pszFnName, TEXT("Tune to channel %d"), lchannel);
        Sleep(20*1000);
        lchannel = 1;
        _CHK(pTvTuner->put_Channel(lchannel,AMTUNER_SUBCHAN_DEFAULT,AMTUNER_SUBCHAN_DEFAULT), TEXT("IAMTvTuner putchannel to channel lchannel"));
        LogText(__LINE__, pszFnName, TEXT("Tune to channel %d."), lchannel);
        Sleep(20*1000);
    }

    mySource.UnInitialize();

    if(g_flag == false)
    {
        return TPR_FAIL;
    }
    else
    {
        return TPR_PASS;
    }
}
#endif

void EventCallBack(long lEventCode, long lParam1, long lParam2, PVOID pUserData)
{
            switch (lEventCode)
                {
                case EC_COMPLETE:
                 LogText(__LINE__, TEXT("EventCallback"), TEXT("Get event EC_COMPLETE %d."), lEventCode);
                break;
                case EC_USERABORT: 
                LogText(__LINE__, TEXT("EventCallback"), TEXT("Get event EC_USERABORT %d."), lEventCode);
                break;
                case EC_ERRORABORT:
                LogText(__LINE__, TEXT("EventCallback"), TEXT("Get event EC_ERRORABORT %d."), lEventCode);
                break;
            default:
                LogText(__LINE__, TEXT("EventCallback"), TEXT("Get unknown event %d."), lEventCode);
                }
}
TESTPROCAPI EndEventTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    /*    
    At NSL level when hit end_of_media there are two end_media fired. Check at
    DVREngine level to see what the problem is
    */


    const TCHAR * pszFnName = TEXT("EndEventTest");
    CSinkFilterGraphEx mySinkGraph;
    CSourceFilterGraph mySourceGraph;
    CComPtr<IFileSourceFilter> pFileSource;        // For DVR source
    CComPtr<IFileSinkFilter> pFileSink;        // For DVR source
    CComPtr<IStreamBufferCapture> pSBC;    // Start Temp Recording

    CComPtr<IBaseFilter> pBaseFilterDVRSource;
    CComPtr<IBaseFilter> pBaseFilterDemux;
    CComPtr<IBaseFilter> pBaseFilterEM10;
    CComPtr<IBaseFilter> pBaseFilterGrabber;
    CComPtr<IMediaSeeking> pMediaSeek;

    CComPtr<IPin> pPinAudio;
    CComPtr<IPin> pPinVideo;

    UINT32 ui32StartCount = 0;
    UINT32 ui32EndCount = 0;
    // BUGBUG: make the number of samples a command line parameter
    int iNumOfSample = 300;    // Default send 1000 Samples
    UINT32 ui32Time = 20;
    LPOLESTR wszFileName = NULL;
    LONGLONG llStop = 0;

    g_flag = true;        //set default as true;

    mySourceGraph.LogToConsole(true);
    mySinkGraph.LogToConsole(true);

    mySourceGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);
    mySinkGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);

    //Set up the Sink Graph with EM10 + Grabber + DVR sink
    mySinkGraph.Initialize();

    _CHK(mySinkGraph.SetupFilters(), TEXT("Setting up sink graph"));

    LPOLESTR pRecordingPath = NULL;
    _CHK(mySinkGraph.SetRecordingPath(), TEXT("SetRecording Path"));
    _CHK(mySinkGraph.GetRecordingPath(&pRecordingPath), TEXT("SetRecording Path"));
    LogText(TEXT("Set recording Path as %s."), pRecordingPath);
    CoTaskMemFree(pRecordingPath);
    pRecordingPath = NULL;

    _CHK(mySinkGraph.BeginPermanentRecording(0), TEXT("Sink BeginPermanentRecording"));
    mySinkGraph.Run();

    LogText(__LINE__, pszFnName, TEXT("Sleep 5 sec."));
    Sleep(10 * 1000);

    // Get Current Perm File Name
    _CHK(mySinkGraph.GetCurFile(&wszFileName ,NULL), TEXT("Get Perm Rec File Name"));
    _CHK(mySinkGraph.BeginTemporaryRecording(0), TEXT("Sink BeginPermanentRecording"));

    LogText(__LINE__, pszFnName, TEXT("Recording to file : %s."), wszFileName);
    // Now set up playback filter! DVR source + Grabber + Demux;
    _CHK(mySourceGraph.Initialize(), TEXT("Init Source FilterGraph"));

    _CHK(mySourceGraph.SetupFilters(wszFileName), TEXT("Setup Source Filter Graph"));

    LogText(__LINE__, pszFnName, TEXT("Sleep 3 second."));
    Sleep(3 * 1000);

    // register event callback
    mySourceGraph.RegisterOnEventCallback(EventCallBack, NULL);
    // Run source
    _CHK(mySourceGraph.Run() ,TEXT("Run Source Graph"));
    _CHK(mySourceGraph.FindInterface(__uuidof(IStreamBufferMediaSeeking), (void * *) &pMediaSeek), TEXT("Get MediaSeek Interface"));
    if(pMediaSeek->GetStopPosition(&llStop) != S_OK)
    {
        LogError(__LINE__, pszFnName, TEXT("Failed to get stop time"));
        g_flag = FALSE;
    }
    LogText(__LINE__, pszFnName, TEXT("Stop position is %I64d."), llStop);    
    LogText(__LINE__, pszFnName, TEXT("Sleep %d seconds."), ui32Time);
    Sleep(ui32Time * 1000);
    // Stop graph
    _CHK(mySourceGraph.Stop(), TEXT("Stop source Graph"));
    _CHK(mySinkGraph.Stop(), TEXT("Stop sink Graph"));    

    mySinkGraph.UnInitialize();
    mySourceGraph.UnInitialize();

    if(g_flag == false)
        return TPR_FAIL;
    return TPR_PASS;
}

TESTPROCAPI RepeatBuildPlaybackGraphLive(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // This test just construct DVR sink and then 
    // repeatedly construct and destruct playback filter to see if graph Freeze.

    const TCHAR * pszFnName = TEXT("RepeatBuildPlaybackGraphLive");

    CSinkFilterGraph mySink;
    CSourceFilterGraphEx mySourceEx;
    LPOLESTR pseLiveToken = NULL;
    INT32 TempBufferSize = 30;    // 30 sec buffer
    INT32 i32LP = 5;    // try construct and destruct 3 times the DVREngine graph

    g_flag = true;

    mySourceEx.LogToConsole(true);
    mySink.LogToConsole(true);

    mySourceEx.SetCommandLine(g_pShellInfo->szDllCmdLine);
    mySink.SetCommandLine(g_pShellInfo->szDllCmdLine);

    _CHK(mySink.Initialize(), TEXT("Initialize Sink Graph"));

    _CHK(mySink.SetupFilters(), TEXT("Setting up Sink Graph"));

    LogText(__LINE__, pszFnName, TEXT("Temp buffer size is %d."), TempBufferSize);

    LPOLESTR pRecordingPath = NULL;
    _CHK(mySink.SetRecordingPath(), TEXT("SetRecording Path"));
    _CHK(mySink.GetRecordingPath(&pRecordingPath), TEXT("SetRecording Path"));
    LogText(TEXT("Set recording Path as %s."), pRecordingPath);
    CoTaskMemFree(pRecordingPath);
    pRecordingPath = NULL;

    _CHK(mySink.BeginTemporaryRecording(TempBufferSize), TEXT("BeginTemporaryRecording"));
    _CHK(mySink.GetBoundToLiveToken(&pseLiveToken), TEXT("Get bound to live token"));
    LogText(__LINE__, pszFnName, TEXT("Bound to Live Token is %s."), pseLiveToken);
    _CHK(mySink.Run(), TEXT("Run Graph"));
    LogText(__LINE__, pszFnName, TEXT("Going to sleep %d Milli seconds to fill Temp Buffer."), TempBufferSize);
    Sleep((TempBufferSize + 5) * 1000);    // Sleep for buffer filled.

    while(i32LP-- > 0)
    {
        _CHK(mySourceEx.Initialize(), TEXT("Initialize Source Graph"));
        _CHK(mySourceEx.SetupFilters(pseLiveToken), TEXT("Setup Temp live playback graph"));
        _CHK(mySourceEx.Run(), TEXT("Run Source Graph"));
        Sleep(15*1000);
        _CHK(mySourceEx.Stop(), TEXT("Stop Source Graph"));
        mySourceEx.UnInitialize();
    }

    mySink.UnInitialize();

    if(g_flag == false)
    {
        return TPR_FAIL;
    }
    else
    {
        return TPR_PASS;
    }
}

TESTPROCAPI OrphanedRecording(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    const TCHAR * pszFnName = TEXT("OrphanedRecording");

    CSinkFilterGraph mySink;
    CSourceFilterGraph mySource;
    LPOLESTR pseLiveToken = NULL;

    // N minutes * 60000 milliseconds per minute
    int TempBufferSize = 1 * 60000;
    int PermRecordLength = 1 * 60000;
    int TenSeconds = 10000;

    g_flag = true;

    mySource.LogToConsole(true);
    mySink.LogToConsole(true);

    mySource.SetCommandLine(g_pShellInfo->szDllCmdLine);
    mySink.SetCommandLine(g_pShellInfo->szDllCmdLine);

    _CHK(mySink.Initialize(), TEXT("Initialize Sink Graph"));
    _CHK(mySource.Initialize(), TEXT("Initialize Source Graph"));

    mySink.LogToConsole(true);
    mySource.LogToConsole(true);

    _CHK(mySink.SetupFilters(), TEXT("Setting up Sink Graph"));
    _CHK(mySink.SetRecordingPath(), TEXT("SetRecording Path"));

    // change the temp buffer size to TempBufferSize
    _CHK(mySink.BeginTemporaryRecording(TempBufferSize), TEXT("BeginTemporaryRecording"));
    _CHK(mySink.BeginPermanentRecording(0), TEXT("BeginPermanentRecording"));

    _CHK(mySink.GetBoundToLiveToken(&pseLiveToken), TEXT("Get bound to live token"));
    _CHK(mySource.SetupFilters(pseLiveToken), TEXT("Setup Temp live playback graph"));

    _CHK(mySink.Run(), TEXT("Run Graph"));
    _CHK(mySource.Run(), TEXT("Run Graph"));

    Sleep(TenSeconds);

    _CHK(mySource.Pause(), TEXT("Pause the source graph"));

    // record for an amount of time
    Sleep(PermRecordLength);

    // end the permanent recording and begin a new temporary recording
    _CHK(mySink.BeginTemporaryRecording(TempBufferSize), TEXT("BeginTemporaryRecording"));

    // now sleep until the permanent recording is orphaned past the end of the pause buffer
    Sleep(TempBufferSize + 500);

    _CHK(mySource.Run(), TEXT("Run the source graph"));

    // fast forward to the end.
    //_CHK(mySource.SetRate(2), TEXT("Set the rate to 2x"));
    Sleep(2*PermRecordLength + TenSeconds);

    _CHK(mySink.Stop(), TEXT("Stop the sink graph"));
    _CHK(mySource.Stop(), TEXT("Stop the source graph"));

    // When bound to live, the source must be released before the sink.
    mySource.UnInitialize();
    mySink.UnInitialize();

    if(g_flag == false)
    {
        return TPR_FAIL;
    }
    else
    {
        return TPR_PASS;
    }
}

TESTPROCAPI TempOrphanedRecording(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    const TCHAR * pszFnName = TEXT("TempOrphanedRecording");

    CSinkFilterGraph mySink;
    CSourceFilterGraph mySource;
    LPOLESTR pseLiveToken = NULL;

    // N minutes * 60000 milliseconds per minute
    int TempBufferSize = 5 * 60000;
    int TenSeconds = 10000;

    g_flag = true;

    mySource.LogToConsole(true);
    mySink.LogToConsole(true);

    mySource.SetCommandLine(g_pShellInfo->szDllCmdLine);
    mySink.SetCommandLine(g_pShellInfo->szDllCmdLine);

    _CHK(mySink.Initialize(), TEXT("Initialize Sink Graph"));
    _CHK(mySource.Initialize(), TEXT("Initialize Source Graph"));

    mySink.LogToConsole(true);
    mySource.LogToConsole(true);

    _CHK(mySink.SetupFilters(), TEXT("Setting up Sink Graph"));
    _CHK(mySink.SetRecordingPath(), TEXT("SetRecording Path"));

    // change the temp buffer size to TempBufferSize
    _CHK(mySink.BeginTemporaryRecording(TempBufferSize), TEXT("BeginTemporaryRecording"));

    _CHK(mySink.GetBoundToLiveToken(&pseLiveToken), TEXT("Get bound to live token"));
    _CHK(mySource.SetupFilters(pseLiveToken), TEXT("Setup Temp live playback graph"));

    _CHK(mySink.Run(), TEXT("Run Graph"));
    _CHK(mySource.Run(), TEXT("Run Graph"));

    Sleep(TenSeconds);

    _CHK(mySource.Pause(), TEXT("Pause the source graph"));

    // record for an amount of time
    Sleep(TempBufferSize + TenSeconds);

    _CHK(mySource.Run(), TEXT("Run the source graph"));

    // fast forward to the end.
    //_CHK(mySource.SetRate(2), TEXT("Set the rate to 2x"));
    Sleep(TempBufferSize + TenSeconds);

    _CHK(mySink.Stop(), TEXT("Stop the sink graph"));
    _CHK(mySource.Stop(), TEXT("Stop the source graph"));

    // When bound to live, the source must be released before the sink.
    mySource.UnInitialize();
    mySink.UnInitialize();

    if(g_flag == false)
    {
        return TPR_FAIL;
    }
    else
    {
        return TPR_PASS;
    }
}
