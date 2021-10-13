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
#include <Streams.h>
#include <Dshow.h>
#include <initguid.h>
#include "logger.h"
#include "globals.h"
#include "SourceFilterGraphEx.h"
#include "SinkFilterGraphEx.h"
#include "dvrinterfaces.h"
#include "Grabber.h"
#include "helper.h"
#include "eventhandler.h"

typedef enum
{
// uses the iamstreamselect interfaces
#if 0
    TEST_SELECTION_IAMStreamSelect_COUNT,
    TEST_SELECTION_IAMStreamSelect_COUNT_NEGATIVE,
    TEST_SELECTION_IAMStreamSelect_ENABLE,
    TEST_SELECTION_IAMStreamSelect_ENABLE_NEGATIVE,
    TEST_SELECTION_IAMStreamSelect_INFO,
    TEST_SELECTION_IAMStreamSelect_INFO_NEGATIVE,
#endif
    TEST_SELECTION_SourceGraph_StopAndRun,
    TEST_SELECTION_SourceGraph_RewindStopAndRun,
    TEST_SELECTION_SourceGraph_PauseAndRun,
    TEST_SELECTION_BoundToLiveBeginMultiplePermanentRecording,
    TEST_SELECTION_BoundToRecordingBeginMultiplePermanentRecording,
    TEST_SELECTION_Position_SeekBackward,
    TEST_SELECTION_Position_SeekForward,
    TEST_SELECTION_RateChangedEvent,
    TEST_SELECTION_RateAndPositioning,
    TEST_SELECTION_RewindToBeginning,
    TEST_SELECTION_PauseAtBeginning,
    TEST_SELECTION_SlowAtBeginning,
    TEST_SELECTION_FastForwardToEnd,
    TEST_SELECTION_StopSinkDuringSource,
} TEST_SELECTION;

typedef enum
{
    BOUND_TO_LIVE_TEMPORARY,
    BOUND_TO_LIVE_TEMPORARY_FULL_PAUSEBUFFER,
    BOUND_TO_LIVE_PERMANENT,
    BOUND_TO_RECORDING
} GRAPH_TYPE;

TCHAR * g_pszSinkFilterGraphFileName   = NULL;
TCHAR * g_pszSourceFilterGraphFileName = NULL;

bool    g_flag = false; // used for callback to tell system if test passed
LONGLONG g_llDefaultRecordingBufferSizeInMilliseconds = 20000;

HRESULT g_hr;
#define _CHK(x, y) if(FAILED(g_hr = x)) {LogError(__LINE__, pszFnName, TEXT("Failed at %s : 0x%x."), y, g_hr); g_flag = false; } else LogText(__LINE__, pszFnName, TEXT("Succeed at %s."), y);

TESTPROCAPI GetBufferBeforeStartTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // BUGBUG: This is repro for bug 97492
    // * Create a capture graph but do not start it
    // * Create a playback graph in the same process by giving the live tv token for the capture graph 
    // (e.g. for a debug build "livetv0" for your first ever capture graph -- or ask the capture graph for its live tv token).
    // * Load fails with E_FAIL

    const TCHAR * pszFnName = TEXT("GetBufferBeforeStartTest");
    CSinkFilterGraph mySink;
    CSourceFilterGraphEx mySource;
    LPOLESTR wszLiveToken = NULL;

    g_flag = true;//default pass unless not

    mySource.LogToConsole(true);
    mySink.LogToConsole(true);

    mySource.SetCommandLine(g_pShellInfo->szDllCmdLine);
    mySink.SetCommandLine(g_pShellInfo->szDllCmdLine);

    mySink.Initialize();
    mySource.Initialize();

    _CHK(mySink.SetupFilters(), TEXT("Setting up Sink Graph"));

    _CHK(mySink.GetBoundToLiveToken(&wszLiveToken), TEXT("Sink GetBoundToLiveToken"));
    LogText(__LINE__, pszFnName, TEXT("LiveToken is : %s."), wszLiveToken);
    if(SUCCEEDED(mySource.SetupFilters(wszLiveToken, TRUE)))
    {
        LogError(__LINE__, pszFnName, TEXT("Succeeded building the playback graph using a live token before the sink graph is running."));
        g_flag = false;
    }
    else
    {
        LogText(__LINE__, pszFnName, TEXT("Failed to build the graph using live before the sink graph is running; succeed."));
        g_flag = true;
    }

    mySink.UnInitialize();
    mySource.UnInitialize();


    if(g_flag)
    {
        return TPR_PASS;
    }
    else
    {
        return TPR_FAIL;
    }
}

TESTPROCAPI GetCurFilePermenantRecStopTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    const TCHAR * pszFnName = TEXT("GetCurFilePermenantRecStopTest");

    CSinkFilterGraphEx mySink;
    LPOLESTR pName = NULL;
    HRESULT hr = E_FAIL;
    UINT32 uiSleepTime = 5*1000;
    UINT32 iCountHolder = 0;

    g_flag = true;//default pass unless not

    mySink.LogToConsole(true);

    mySink.SetCommandLine(g_pShellInfo->szDllCmdLine);

    _CHK(mySink.Initialize(), TEXT("Initialize the Sink FilterGraph"));

    _CHK(mySink.SetupFilters(TRUE), TEXT("Setup the Sink FilterGraph"));

    _CHK(mySink.SetGrabberCallback(mySink.SequentialStamper), TEXT("Setup the Sink FilterGraph Sequentilal Stamper"));
    // Start Perm Rec for 5 seconds
    _CHK(mySink.SetRecordingPath(), TEXT("Setup the recording Path"));
    _CHK(mySink.BeginPermanentRecording(0), TEXT("Begin Perm Recording"));
    LogText(__LINE__, pszFnName, TEXT("Stamp mySink.Ex_iCount start as : %d."), mySink.Ex_iCount);
    _CHK(mySink.Run(), TEXT("Run Graph"));
    Sleep(uiSleepTime);

    LogText(__LINE__, pszFnName, TEXT("Stamp mySink.Ex_iCount end as : %d."), mySink.Ex_iCount);
    iCountHolder = mySink.Ex_iCount;
    // Stop the Graph now
    _CHK(mySink.Stop(), TEXT("Stop the Graph"));
    Sleep(1000);
    // now try to Get filename
    hr = mySink.GetCurFile(&pName, NULL);
    LogText(__LINE__, pszFnName, TEXT("Filename is %s; return value is 0x%x."), pName, hr);

    mySink.UnInitialize();

    if(pName == NULL && SUCCEEDED(hr))
    {
        LogError(__LINE__, pszFnName, TEXT("Get FileName is NULL; error."));
        return TPR_FAIL;
    }
    else
    {
        CoTaskMemFree(pName);
        return TPR_PASS;
    }
}

TESTPROCAPI LoadNoExistFileTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    const TCHAR * pszFnName = TEXT("LoadNoExistFileTest");

    CSinkFilterGraphEx mySink;
    LPOLESTR pName = TEXT("ThisIsNoneExistFileNameHGFD");// Some None exist file.
    HRESULT hr = E_FAIL;

    g_flag = true;//default pass unless not

    mySink.LogToConsole(true);

    mySink.SetCommandLine(g_pShellInfo->szDllCmdLine);

    _CHK(mySink.Initialize(), TEXT("Initialize the Sink FilterGraph"));

    hr = mySink.SetupFilters(pName, TRUE);

    mySink.UnInitialize();

    if(SUCCEEDED(hr))
    {
        LogText(__LINE__, pszFnName, TEXT("Setup Filter succeeded with a Non-exist file : %s."), pName);
        return TPR_FAIL;
    }
    else
    {
        LogText(__LINE__, pszFnName, TEXT("Failed to load bad file; succeed."));
        return TPR_PASS;
    }
}

TESTPROCAPI LoadMissingIndexFileTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // This test is for Test Load missing index file set.
    // Create a valid permanent recording set then delete its .idx file.
    // Load shall fail.
    const TCHAR * pszFnName = TEXT("LoadMissingIndexFileTest");

    CSinkFilterGraphEx mySink;
    CSourceFilterGraphEx mySource;
    LPTSTR pName = NULL;
    LPTSTR pmyName = TEXT("\\Hard disk4\\TempRec\\a34525h");// Some Random invalid name
    LPTSTR pNameTemp = new TCHAR[128];
    LPTSTR pNameNew = new TCHAR[128];
    DWORD iDur = 10*1000;
    HRESULT hr;

    g_flag = true;

    if(pNameTemp && pNameNew)
    {
        mySource.LogToConsole(true);
        mySink.LogToConsole(true);

        mySource.SetCommandLine(g_pShellInfo->szDllCmdLine);
        mySink.SetCommandLine(g_pShellInfo->szDllCmdLine);

        _CHK(mySink.Initialize(), TEXT("Initialize Sink Filter"));

        _CHK(mySink.SetupFilters(TRUE), TEXT("Initialize Sink Filter"));

        _CHK(mySink.SetRecordingPath(), TEXT("Setup the recording Path"));
        _CHK(mySink.BeginPermanentRecording(0), TEXT("Start Perm recording"));
        _CHK(mySink.Run(), TEXT("Run Sink Graph"));
        LogText(__LINE__, pszFnName, TEXT("Sleep %d milliseconds for Recording."), iDur);
        Sleep(iDur);
        //Get Filename
        _CHK(mySink.GetCurFile(&pName, NULL), TEXT("Get Cur filename"));
        if(pName == NULL)
        {
            LogError(__LINE__, pszFnName, TEXT("Get NUll name; error."));
            g_flag = false;
        }
        // Destroy the Sink Graph
        _CHK(mySink.Stop(), TEXT("Stop Sink Graph"));
        mySink.UnInitialize();
        // Copy the file to myName. Old file can not be deleted.
        _tcscpy(pNameTemp, pName);
        _tcscat(pNameTemp, _T("_0.dvr-dat"));
        //Generate new filename
        _tcscpy(pNameNew, pmyName);
        _tcscat(pNameNew, _T("_0.dvr-dat"));
        LogText(__LINE__, pszFnName, TEXT("Start to copy the Dat file %s to New name %s."), pNameTemp, pNameNew);
        CopyFile(pNameTemp, pNameNew, false);
        // Start Source and try to load file
        _CHK(mySource.Initialize(), TEXT("Initialize the Source Graph"));
        hr = mySource.SetupFilters(pmyName, TRUE); 
        if(SUCCEEDED(hr))
        {
            LogError(__LINE__, pszFnName, TEXT("This is wrong, succeeded load recording without IDX data file."));
            g_flag = false;
        }
        else
        {
            LogText(__LINE__, pszFnName, TEXT("Failed to load bad recording; succeed."));
        }
    }
    else
    {
        LogError(__LINE__, pszFnName, TEXT("Failed to allocate a pointer for the string."));
        g_flag = false;
    }

    mySource.UnInitialize();

    if(pNameTemp)
        delete [] pNameTemp;
    if(pNameNew)
        delete [] pNameNew;

    if(g_flag)
    {
        return TPR_PASS;
    }
    else
    {
        return TPR_FAIL;
    }
}

TESTPROCAPI PermStartFromOriginTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    //    This test ensure Permanent recording start from 1st several frame.
    const TCHAR * pszFnName = TEXT("PermStartFromOriginTest");


    CSinkFilterGraphEx mySinkGraph;
    CSourceFilterGraphEx mySourceGraph;

    UINT32 ui32StartCount = 0;
    UINT32 ui32EndCount = 0;
    int iNumOfSample = 300;// Default send 1000 Samples
    UINT32 iRecTime = 5*1000;// 5 seconds between Sink and Source
    LPOLESTR wszFileName = NULL;

    g_flag = true;//set default as true;

    mySourceGraph.LogToConsole(true);
    mySinkGraph.LogToConsole(true);

    mySourceGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);
    mySinkGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);

    //Set up the Sink Graph with EM10 + Grabber + DVR sink
    mySinkGraph.Initialize();

    _CHK(mySinkGraph.SetupFilters(TRUE), TEXT("Setting up sink graph"));

    _CHK(mySinkGraph.SetGrabberCallback(CSinkFilterGraphEx::SequentialStamper), TEXT("Setting up sink graph Stamper"));

    LPOLESTR pRecordingPath = NULL;
    _CHK(mySinkGraph.SetRecordingPath(), TEXT("SetRecording Path"));
    _CHK(mySinkGraph.GetRecordingPath(&pRecordingPath), TEXT("SetRecording Path"));
    LogText(TEXT("Set recording Path as %s."), pRecordingPath);
    CoTaskMemFree(pRecordingPath);
    pRecordingPath = NULL;

    _CHK(mySinkGraph.BeginPermanentRecording(0), TEXT("Sink BeginPermanentRecording"));
    mySinkGraph.Run();
    LogText(__LINE__, pszFnName, TEXT("Sleep %d milliseconds."), iRecTime);
    Sleep(iRecTime);

    // Get Current Perm File Name
    _CHK(mySinkGraph.GetCurFile(&wszFileName ,NULL), TEXT("Get Perm Rec File Name"));
    LogText(__LINE__, pszFnName, TEXT("Recording to file : %s."), wszFileName);
    // Now set up playback filter! DVR source + Grabber + Demux;
    _CHK(mySourceGraph.Initialize(), TEXT("Initialize Source FilterGraph"));
    _CHK(mySourceGraph.SetupFilters(wszFileName, TRUE), TEXT("Setup Source FilterGraph"));
    CoTaskMemFree(wszFileName);
    wszFileName = NULL;
    LogText(__LINE__, pszFnName, TEXT("Sleep 3 seconds."));
    Sleep(3 * 1000);
    // hookup callback!
    _CHK(mySourceGraph.SetGrabberCallback(mySourceGraph.SequentialPermStampChecker), TEXT("hook up SequentialPermStampChecker"));

    _CHK(mySourceGraph.Run() ,TEXT("Run Source Graph"));
    LogText(__LINE__, pszFnName, TEXT("Sleep %d milliseconds."), iRecTime);
    Sleep(iRecTime);
    _CHK(mySourceGraph.SetGrabberCallback(NULL), TEXT("Unhook SequentialPermStampChecker"));
    if(CSinkFilterGraphEx::Ex_flag == false)
    {
        LogError(__LINE__, pszFnName, TEXT("The Perm Recording Does not start from beginning or stamp break somewhere."));
        g_flag = false;
    }
    else
    {
        LogText(__LINE__, pszFnName, TEXT("The Perm Recording start from beginning and stamp conserved; succeed."));
    }
    // Stop graph
    _CHK(mySourceGraph.Stop(), TEXT("Stop source Graph"));
    _CHK(mySinkGraph.Stop(), TEXT("Stop sink Graph"))

    mySourceGraph.UnInitialize();
    mySinkGraph.UnInitialize();

    if(g_flag == false)
        return TPR_FAIL;
    return TPR_PASS;
}

TESTPROCAPI LiveKeepLiveTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    /*    
    This test will send 1000 asf sample to DVR engine (app 65 MB). Use a GrabberSample to detect if stream is
    going and read back correctly. Stream are stamped with a unique number from 1 to UINT32.MAX. Just make sure
    we read back sequentially to whatever we write to disk.
    */
    const TCHAR * pszFnName = TEXT("LiveKeepLiveTest");

    CSinkFilterGraphEx mySinkGraph;
    CSourceFilterGraphEx mySourceGraph;
    UINT32 iDelay = 5*1000;
    UINT32 iTestDur = 30;// 30 times
    INT32 Delta = 15;// Max Frame Lag between Sink and Source
    LPOLESTR wszLiveToken = NULL;

    g_flag = true;//set default as true;

    mySourceGraph.LogToConsole(true);
    mySinkGraph.LogToConsole(true);

    mySourceGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);
    mySinkGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);

    //Set up the Sink Graph with Em10 + Grabber + DVR sink
    mySinkGraph.Initialize();

    _CHK(mySinkGraph.SetupFilters(TRUE), TEXT("Setting up sink graph"));

    _CHK(mySinkGraph.SetGrabberCallback(CSinkFilterGraphEx::SequentialStamper), TEXT("Setting up sink graph callback stamper"));

    _CHK(mySinkGraph.SetRecordingPath(), TEXT("SetRecording Path"));
    _CHK(mySinkGraph.BeginTemporaryRecording(30*1000), TEXT("Sink BeginTemporaryRecording"));
    mySinkGraph.Run();
    Sleep(3*1000);
    _CHK(mySinkGraph.GetBoundToLiveToken(&wszLiveToken), TEXT("Sink GetBoundToLiveToken"));
    LogText(__LINE__, pszFnName, TEXT("LiveToken is : %s."), wszLiveToken);
    LogText(__LINE__, pszFnName, TEXT("Sleep %d milliseconds."), iDelay);
    Sleep(iDelay);

    // Now set up playback filter! DVR source + Grabber + Demux;
    _CHK(mySourceGraph.Initialize(), TEXT("Initialize Source FilterGraph"));
    _CHK(mySourceGraph.SetupFilters(wszLiveToken, TRUE), TEXT("Setup Source live playback graph"));
    // hookup callback!
    _CHK(mySourceGraph.SetGrabberCallback(mySourceGraph.SequentialTempStampChecker), TEXT("SetCallback Temp Checker of Source"));

    _CHK(mySourceGraph.Run() ,TEXT("Run Source Graph"));
    LogText(__LINE__, pszFnName, TEXT("Sleep %d milliseconds."), iTestDur);
    // sleep 20 seconds and then run for 20 seconds additional in order to fill up the temporary buffer
    Sleep(20*1000);
    while(iTestDur-- > 1)
    {
        if(iTestDur % 20 == 0)
        {
            LogText(__LINE__, pszFnName, TEXT("Gap between Sink and Source, Sink %d -- Source %d."), mySinkGraph.Ex_iCount, mySourceGraph.Ex_iCount);
        }
        INT32 sinkMinusSource = mySinkGraph.Ex_iCount - mySourceGraph.Ex_iCount;
        LogText(__LINE__, pszFnName, TEXT("Sink - Source is %d frame."), sinkMinusSource);

        if(abs(sinkMinusSource) > Delta)        // Delta is Max frame lag
        {
            LogError(__LINE__, pszFnName, TEXT("Lag too large between Sink and Source."));
            LogError(__LINE__, pszFnName, TEXT("Sink iCount %d, source iCount %d."), mySinkGraph.Ex_iCount, mySourceGraph.Ex_iCount);
            g_flag = false;
        }
        Sleep(1000);
    }
    _CHK(mySourceGraph.SetGrabberCallback(NULL), TEXT("SetCallback source of GrabberSample as NULL"));
    if(mySourceGraph.Ex_iCount < iTestDur * 10)
    {
        LogError(__LINE__, pszFnName, TEXT("The number of samples are %d. less than iTestDur * 10 = %d."), mySourceGraph.Ex_iCount, iTestDur * 10);
        g_flag = false;
    }
    if(mySourceGraph.Ex_flag == false)
    {
        LogError(__LINE__, pszFnName, TEXT("The Stamp break at playback."));
        g_flag = false;
    }

    // Stop graph
    _CHK(mySourceGraph.Stop(), TEXT("Stop source Graph"));
    _CHK(mySinkGraph.Stop(), TEXT("Stop sink Graph"))

    mySourceGraph.UnInitialize();
    mySinkGraph.UnInitialize();

    LogText(__LINE__, pszFnName, TEXT("Sink Graph decomposed."));

    if(g_flag == false)
        return TPR_FAIL;
    return TPR_PASS;
}

TESTPROCAPI BufferSizeAccurateTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Repeatedly set different Buffer size recording and check if the size are true.
    // Use IMediaSeeking to Test specified Buffersize are all reachable!
    // Start Sink with 40 sec Buffer and soon Playback. 
    // Play for total 60 seconds. At 25th second record the position and Stamped frame Number.
    // then seek back to 25 seconds use ABS seeking, make sure
    // 1. playback graph Ex_iCount keep going
    // 2. Source::Ex_iCount around the recorded value.
    const TCHAR * pszFnName = TEXT("BufferSizeAccurateTest");

    CSinkFilterGraphEx mySink;
    CSourceFilterGraphEx mySource;

    LONGLONG uiCurPos = 0;// Source graph Current pos
    LONGLONG uiRecPos = 0;// Sink Graph Recorded position at 25 seconds
    UINT32 uiRecFrame = 0;// Sink Graph recorded frame position at 25 seconds
    UINT32 uiPlayLength = 60 * 1000;// PLay length 60 seconds
    UINT32 uiOffset = 25*1000;// Wait 25 seconds then record pos and frame number
    LONGLONG uiBuffer = 40*1000;
    LONGLONG llSeekBackDur = uiOffset - uiPlayLength;// negative 35 seconds
    UINT32 uiDelta = 30;// Suppose the seek return stream within 20 frames of speficied position.
    LPOLESTR wszLiveToken = NULL;
    // Start real fun
    g_flag = true;// default pass unless wrong somewhere

    mySource.LogToConsole(true);
    mySink.LogToConsole(true);

    mySource.SetCommandLine(g_pShellInfo->szDllCmdLine);
    mySink.SetCommandLine(g_pShellInfo->szDllCmdLine);

    _CHK(mySink.Initialize(), TEXT("mySink initialize"));

    _CHK(mySink.SetupFilters(TRUE), TEXT("mySink setup graph"));

    _CHK(mySink.SetGrabberCallback(mySink.SequentialStamper), TEXT("Set Sequantial Stamper on Sink"));
    _CHK(mySink.SetRecordingPath(), TEXT("mySink setup Recording Path"));
    _CHK(mySink.BeginTemporaryRecording(uiBuffer), TEXT("mySink Start Temp rec with uiBuffer length"));
    _CHK(mySink.GetBoundToLiveToken(&wszLiveToken), TEXT("Sink GetBoundToLiveToken"));
    LogText(__LINE__, pszFnName, TEXT("LiveToken is : %s."), wszLiveToken);
    // Setup playback then start both
    _CHK(mySource.Initialize(), TEXT("mySource initialize"));
    _CHK(mySource.SetupFilters(wszLiveToken, TRUE), TEXT("setup Graph"));
    _CHK(mySource.SetGrabberCallback(mySource.SequentialTempStampChecker), TEXT("Set Temp Checker for Sink"));
    _CHK(mySink.Run(), TEXT("Run Sink!"));

    _CHK(mySource.Run(), TEXT("Run Source"));

    // Now sleep uiOffset milli sec and rec and Pos and FrameNum
    LogText(__LINE__, pszFnName, TEXT("Will sleep %d milliseconds."), uiOffset);
    Sleep(uiOffset);
    uiRecFrame = mySource.Ex_iCount;// Source Frame Number, should be later seeked source frame
    _CHK(mySource.GetCurrentPosition(&uiRecPos), TEXT("Get Source Pos at uiOffset seconds"));
    LogText(__LINE__, pszFnName, TEXT("The pos now is %d, and Frame number is %d."),(UINT32)uiRecPos, uiRecFrame);
    // Sleep rest time
    LogText(__LINE__, pszFnName, TEXT("Will sleep again %d milliseconds."), uiPlayLength - uiOffset);
    Sleep(uiPlayLength - uiOffset);
    // Now Seek back 35 seconds (at 25th sec). which are still inside buffer!
    // llSeekBackDur = (uiPlayLength - uiOffset) * 10000;
    //_CHK(mySource.SetPositions(&llSeekBackDur, AM_SEEKING_RelativePositioning, NULL, AM_SEEKING_NoPositioning), TEXT("Seek on Source now"));
    LogText(__LINE__, pszFnName, TEXT("Now seekback to %d milliseconds."),(INT32)uiRecPos);
    _CHK(mySource.SetPositions(&uiRecPos, AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning), TEXT("Seek on Source now to uiRecPos"));
    // Now Position should back to near end of buffer and  check real meat here
    Sleep(50);// This is ugly but the DVR engine perf make consistent repro very hard.

    LogWarning(__LINE__, pszFnName, TEXT("Ignore this stamp break since Seeking always break stamp."));
    _CHK(mySource.GetCurrentPosition(&uiCurPos), TEXT("Get Source Pos After seek!"));
    LogText(__LINE__, pszFnName, TEXT("Playback pos now is %d, should seek back around %d."), (UINT32)uiCurPos, (UINT32)uiRecPos);
    LogText(__LINE__, pszFnName, TEXT("Playback frame now is %d, should seek back around %d."), mySource.Ex_iCount, uiRecFrame);
    if((uint) abs((int)(uiRecFrame - mySource.Ex_iCount)) > uiDelta)
    {
        LogError(__LINE__, pszFnName, TEXT("The seeking does not return to Buffer starting; error."));
        g_flag = false;
    }
    Sleep(10*1000);

    mySource.Stop();
    mySink.Stop();

    mySource.UnInitialize();
    mySink.UnInitialize();

    if(g_flag == true)
        return TPR_PASS;
    else
        return TPR_FAIL;
}    

TESTPROCAPI RelativeSeekingTemp(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Repeatedly set different Buffer size recording and check if the size are true.
    // Use IMediaSeeking to Test specified Buffersize are all reachable!
    // Start Sink with 40 sec Buffer and soon Playback. 
    // Play for total 60 seconds. At 25th second record the position and Stamped frame Number.
    // then seek back to 25 seconds use REL seeking, make sure
    // 1. playback graph Ex_iCount keep going
    // 2. Source::Ex_iCount around the recorded value.
    const TCHAR * pszFnName = TEXT("RelativeSeekingTemp");

    CSinkFilterGraphEx mySink;
    CSourceFilterGraphEx mySource;

    LONGLONG uiCurPos = 0;// Source graph Current pos
    LONGLONG uiStop = 0;// Source graph Current pos
    LONGLONG uiRecPos = 0;// Sink Graph Recorded position at 25 seconds
    UINT32 uiRecFrame = 0;// Sink Graph recorded frame position at 25 seconds
    UINT32 uiPlayLength = 60 * 1000;// PLay length 60 seconds
    UINT32 uiOffset = 25*1000;// Wait 25 seconds then record pos and frame number
    LONGLONG uiBuffer = 40*1000;// Set Buffer Size; Better large than any pause length otherwise pasue pos hit buffer window low end
    LONGLONG llSeekBackDur = uiOffset - uiPlayLength;// negative 35 seconds
    UINT32 uiDelta = 20;// Suppose the seek return stream within 50 frames of speficied position.
    LPOLESTR wszLiveToken = NULL;
    // Start real fun

    g_flag = true;//set default as true;

    mySource.LogToConsole(true);
    mySink.LogToConsole(true);

    mySource.SetCommandLine(g_pShellInfo->szDllCmdLine);
    mySink.SetCommandLine(g_pShellInfo->szDllCmdLine);

    _CHK(mySink.Initialize(), TEXT("mySink initialize"));

    _CHK(mySink.SetupFilters(TRUE), TEXT("mySink setup graph"));

    _CHK(mySink.SetGrabberCallback(mySink.SequentialStamper), TEXT("Set Sequantial Stamper on Sink"));
    _CHK(mySink.SetRecordingPath(), TEXT("mySink setup Recording Path"));
    _CHK(mySink.BeginTemporaryRecording(uiBuffer), TEXT("mySink Start Temp rec with uiBuffer length"));
    _CHK(mySink.GetBoundToLiveToken(&wszLiveToken), TEXT("Sink GetBoundToLiveToken"));
    LogText(__LINE__, pszFnName, TEXT("LiveToken is : %s."), wszLiveToken);
    // Setup playback then start both
    _CHK(mySource.Initialize(), TEXT("mySource initialize"));
    _CHK(mySource.SetupFilters(wszLiveToken, TRUE), TEXT("setup Graph"));
    _CHK(mySource.SetGrabberCallback(mySource.SequentialTempStampChecker), TEXT("Set Temp Checker for Sink"));
    _CHK(mySink.Run(), TEXT("Run Sink"));

    _CHK(mySource.Run(), TEXT("Run Source"));

    // Now sleep uiOffset milli sec and rec and Pos and FrameNum
    LogText(__LINE__, pszFnName, TEXT("Will sleep %d milliseconds."), uiOffset);
    Sleep(uiOffset);
    uiRecFrame = mySource.Ex_iCount;// Source Frame Number, should be later seeked source frame
    _CHK(mySource.GetCurrentPosition(&uiRecPos), TEXT("Get Source Pos at uiOffset seconds"));
    LogText(__LINE__, pszFnName, TEXT("The pos now is %d, and Frame number is %d."),(UINT32)uiRecPos, uiRecFrame);
    // Sleep rest time
    LogText(__LINE__, pszFnName, TEXT("Will sleep again %d milliseconds."), uiPlayLength - uiOffset);
    Sleep(uiPlayLength - uiOffset);
    // Get end Pos
    _CHK(mySource.GetPositions(&llSeekBackDur, &uiStop), TEXT("Get Positions for Cur and End"));
    // Now Seek back to starting position.
    llSeekBackDur = llSeekBackDur - uiRecPos;
    llSeekBackDur *= -1;
    LogText(__LINE__, pszFnName, TEXT("Now seekback %d milliseconds."),(INT32)llSeekBackDur);
    _CHK(mySource.SetPositions(&llSeekBackDur, AM_SEEKING_RelativePositioning, NULL, AM_SEEKING_NoPositioning), TEXT("Seek on Source now"));
    //_CHK(mySource.SetPositions(&uiRecPos, AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning), TEXT("Seek on Source now to uiRecPos"));
    // Now Position should back to near end of buffer and  check real meat here
    LogWarning(__LINE__, pszFnName, TEXT("Ignore this stamp break since Seeking always break stamp."));
    Sleep(50);
    _CHK(mySource.GetCurrentPosition(&uiCurPos), TEXT("Get Source Pos After seek"));
    LogText(__LINE__, pszFnName, TEXT("Playback pos now is %d, should seek back around %d."), (UINT32)uiCurPos, (UINT32)uiRecPos);
    LogText(__LINE__, pszFnName, TEXT("Playback frame now is %d, should seek back around %d."), mySource.Ex_iCount, uiRecFrame);
    if((uint) abs((int)(uiRecFrame - mySource.Ex_iCount)) > uiDelta)
    {
        LogError(__LINE__, pszFnName, TEXT("The seeking does not return to Buffer starting; error."));
        g_flag = false;
    }
    Sleep(10*1000);

    mySource.Stop();
    mySink.Stop();

    mySource.UnInitialize();
    mySink.UnInitialize();

    if(g_flag == true)
        return TPR_PASS;
    else
        return TPR_FAIL;
}

TESTPROCAPI NotAlwaysFlushAndSeektoNow(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // This test ensure after a flush, the source graph will seek to now only after set
    // IStreamPlayback interface with single method SeekToLiveOnFlush(boolean), 
    // when True and there was a flush, then it will seek to live. 
    // When set to False nothing is done, the dvr will ignore the flush, 
    // it won't flush and it won't move. Default mode if never called is False.
    // The flush is gained by putting a tunning on Sink.
    // Stamp will be used here to check if the source do seek to NOW
    // After the flush
    const TCHAR * pszFnName = TEXT("NotAlwaysFlushAndSeektoNow");

    DWORD result = TPR_FAIL;
    HRESULT hr = E_FAIL;
    long lFoundSignal = 0;
    long lchannel = 0;
    UINT32 uiSourceStamp = 0;
    INT64 i64Pso = 0;
    INT64 i64Psolast = 0;
    UINT32 uiSinkStamp = 0;
    UINT32 ui32Delta = 5;//    10 sec
    LONGLONG early = 0;
    LONGLONG late = 0;

    CSinkFilterGraph mySink;
    CSourceFilterGraph mySource;

    CComPtr<IBaseFilter> pASFSource = NULL;
    CComPtr<IBaseFilter> pGrabber = NULL;
    CComPtr<IBaseFilter> pDVRSink = NULL;
    CComPtr<IBaseFilter> pDemux = NULL;
    CComPtr<IStreamBufferCapture> pIStreamBufferCapture = NULL;
    CComPtr<IStreamBufferPlayback> pIStreamBufferPlayback = NULL;

    LPOLESTR lpLiveToken = NULL;

    g_flag = true;

    mySink.LogToConsole(true);// no popup window;
    mySource.LogToConsole(true);

    mySource.SetCommandLine(g_pShellInfo->szDllCmdLine);
    mySink.SetCommandLine(g_pShellInfo->szDllCmdLine);

    mySink.Initialize();

    _CHK(mySink.SetupFilters(), TEXT("Setup sink graph"));

    lchannel = 5;
    _CHK(mySink.TuneTo(lchannel,AMTUNER_SUBCHAN_DEFAULT,AMTUNER_SUBCHAN_DEFAULT), TEXT("IAMTvTuner putchannel to channel lchannel"));
    LogText(__LINE__, pszFnName, TEXT("Tune to channel %d."), lchannel);
    // Now building graph, DVRSink;
    _CHK(mySink.SetRecordingPath(), TEXT("Setup recording path"));
    _CHK(mySink.BeginTemporaryRecording(300*1000), TEXT("SBC Beging Temp Rec"));

    Sleep(3*1000);
    // Start Sink 
    _CHK(mySink.Run(), TEXT("Start Sink"));
    _CHK(pIStreamBufferCapture->GetBoundToLiveToken(&lpLiveToken), TEXT("get live token now"));
    // Setup Source
    LogText(__LINE__, pszFnName, TEXT("Sleep 2 seconds."));
    _CHK(mySource.Initialize(), TEXT("Initialize Source"));
    Sleep(3*1000);
    _CHK(mySource.SetupFilters(lpLiveToken), TEXT("source setup filters with live token"));
    _CHK(mySource.Run(), TEXT("source Run"));
    Sleep(10*1000);// Let system rest a while
    _CHK(mySource.GetCurrentPosition(&i64Psolast), TEXT("Get Cur pos before Pause"));
    LogText(__LINE__, pszFnName, TEXT("The pos is %I64d."), i64Psolast);
    _CHK(mySource.Pause(), TEXT("Pause Cur pos"));
    Sleep(ui32Delta * 2 *1000);
    _CHK(mySource.Run(), TEXT("Run Again"));
    _CHK(mySource.GetCurrentPosition(&i64Pso), TEXT("Get Cur pos after Pause"));
    LogText(__LINE__, pszFnName, TEXT("The pos is %I64d."), i64Pso);
    LogText(__LINE__, pszFnName, TEXT("The Shift now is %I64d."), i64Pso - i64Psolast );
    _CHK(mySource.GetAvailable(&early, &late), TEXT("Get available seek"));
    LogText(__LINE__, pszFnName, TEXT("The early is %I64d, late is %I64d."), early, late);
    Sleep(20*1000);
    // Default policy is no seek
    _CHK(mySource.GetCurrentPosition(&i64Psolast), TEXT("Get Cur pos again before tune"));
    LogText(__LINE__, pszFnName, TEXT("The pos is %I64d."), i64Psolast);
    lchannel = 7;
    _CHK(mySource.FindInterface(__uuidof(IStreamBufferPlayback), (void **)&pIStreamBufferPlayback), TEXT("Find pIStreamBufferPlayback Interface"));
    _CHK(pIStreamBufferPlayback->SetTunePolicy(STRMBUF_PLAYBACK_TUNE_IGNORED), TEXT("Set Tune Policy as STRMBUF_PLAYBACK_TUNE_IGNORED"));
    LogText(__LINE__, pszFnName, TEXT("Tune to channel %d."), lchannel);
    _CHK(mySink.TuneTo(lchannel,AMTUNER_SUBCHAN_DEFAULT,AMTUNER_SUBCHAN_DEFAULT), TEXT("IAMTvTuner tune to channel lchannel"));
    Sleep(3*1000);
    _CHK(mySource.GetCurrentPosition(&i64Pso), TEXT("Get Cur pos again after tune"));
    LogText(__LINE__, pszFnName, TEXT("The pos is %I64d, diff is %I64d."), i64Pso, i64Pso - i64Psolast);
    if(i64Pso - i64Psolast > ui32Delta * 1000 * 10000)
    {
        LogError(__LINE__, pszFnName, TEXT("Wrong, Source jump after tune."));
        g_flag = false;
    }
    else
    {
        LogText(__LINE__, pszFnName, TEXT("Stream keep pace after tune, no jump."));
    }
    _CHK(mySource.GetAvailable(&early, &late), TEXT("Get available seek"));
    LogText(__LINE__, pszFnName, TEXT("The early is %I64d, late is %I64d."), early, late);
    
    // Now set policy as Seek!
    Sleep(5*1000);
    /*
    _CHK(mySource.GetCurrentPosition(&i64Psolast), TEXT("Get Cur pos again before tune"));
    LogText(__LINE__, pszFnName, TEXT("The pos is %I64d."), i64Psolast);
    */
    lchannel = 9;
    _CHK(pIStreamBufferPlayback->SetTunePolicy(STRMBUF_PLAYBACK_TUNE_FLUSH_AND_GO_TO_LIVE), TEXT("Set Tune Policy as Tune-Flush"));
    _CHK(mySink.TuneTo(lchannel,AMTUNER_SUBCHAN_DEFAULT,AMTUNER_SUBCHAN_DEFAULT), TEXT("IAMTvTuner tune to channel lchannel"));
    LogText(__LINE__, pszFnName, TEXT("Tune to channel %d."), lchannel);
    Sleep(3*1000);
    _CHK(mySource.GetCurrentPosition(&i64Pso), TEXT("Get Cur pos again after tune"));
    LogText(__LINE__, pszFnName, TEXT("The Cur pos is %I64d."), i64Pso);
    _CHK(mySource.GetAvailable(&early, &late), TEXT("Get available seek"));
    LogText(__LINE__, pszFnName, TEXT("The early is %I64d, late is %I64d."), early, late);
    if(i64Pso - i64Psolast < ui32Delta * 1000 * 10000)
    {
        LogError(__LINE__, pszFnName, TEXT("Wrong, Source does not jump after tune."));
        g_flag = false;
    }
    else
    {
        LogText(__LINE__, pszFnName, TEXT("Stream jumped to live after tune."));
    }

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

TESTPROCAPI StopPositionMatch(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // 1.  Query the stop position of a Just Complete Perm Rec with Sink on.
    // 2.  Query again the Stop Pos on the same Perm Rec with a new standalone Source.
    // They should match. By obervation is they differ 30' in a 3 min recording.

    const TCHAR * pszFnName = TEXT("StopPositionMatch");

    CSinkFilterGraph mySink;
    CSourceFilterGraph mySource;
    CSourceFilterGraph mySourceNew;
    LONGLONG llStopPos1 = 0;
    LONGLONG llStopPosNew1 = 0;
    LONGLONG llStopPosNew2 = 0;
    LPOLESTR lpRecName = NULL;
    UINT32 uiDur = 100*1000;// 100 sec recording length

    g_flag = true; 
    mySink.LogToConsole(true);
    mySource.LogToConsole(true);

    mySource.SetCommandLine(g_pShellInfo->szDllCmdLine);
    mySink.SetCommandLine(g_pShellInfo->szDllCmdLine);

    _CHK(mySink.Initialize(), TEXT("Initialize my Sink"));

    _CHK(mySink.SetupFilters(), TEXT("Setup Filters"));

    _CHK(mySink.SetRecordingPath(), TEXT("Setup recording Path"));
    _CHK(mySink.BeginPermanentRecording(0), TEXT("Begin Perm Recording with 0 buffer converted"));
    _CHK(mySink.GetCurFile(&lpRecName, NULL), TEXT("Get Curfile Name"));
    LogText(__LINE__, pszFnName, TEXT("The recorded filename is %s."), lpRecName);
    LogText(__LINE__, pszFnName, TEXT("Going to rec for %d milliseconds."), uiDur);
    _CHK(mySink.Run(), TEXT("Run Sink"));
    Sleep(uiDur);
    _CHK(mySource.Initialize(), TEXT("Initizlize my Source"));
    mySource.SetupFilters(lpRecName);
    _CHK(mySource.Run(), TEXT("Run Source"));
    LogText(__LINE__, pszFnName, TEXT("Start another Temp recording in Sink."));
    _CHK(mySink.BeginTemporaryRecording(60*1000), TEXT("Begin Temp Recording with 60 second buffer"));
    Sleep(10*1000);// Let system rest a while
    _CHK(mySource.GetStopPosition(&llStopPos1), TEXT("Get Stop position first time"));
    //_CHK(mySource.GetDuration(&llStopPos1), TEXT("Get Dur first time"));
    LogText(__LINE__, pszFnName, TEXT("The stop position is %I64d."), llStopPos1);
    LogText(__LINE__, pszFnName, TEXT("Destroy Sink and Source now."));
    _CHK(mySource.Stop(), TEXT("Stop source now")); 
    mySource.UnInitialize();
    _CHK(mySink.Stop(), TEXT("Stop sink now"))
    mySink.UnInitialize();
    mySourceNew.LogToConsole(true);
    _CHK(mySourceNew.Initialize(), TEXT("Initialize new Source"));
    mySourceNew.SetupFilters(lpRecName);
    _CHK(mySourceNew.GetStopPosition(&llStopPosNew1), TEXT("Get Stop position of new Source - should be same as last time value"));
    //_CHK(mySourceNew.GetDuration(&llStopPosNew1), TEXT("Get Dur of new Source - should be same as last time value"));
    LogText(__LINE__, pszFnName, TEXT("New Stop position 1 is %I64d."), llStopPosNew1);
    _CHK(mySourceNew.Run(), TEXT("Run New source"));
    Sleep(5*1000);
    _CHK(mySourceNew.GetStopPosition(&llStopPosNew2), TEXT("Get Stop position of new Source - should be same as last time value"));
    //_CHK(mySourceNew.GetDuration(&llStopPosNew2), TEXT("Get Dur of new Source - should be same as last time value"));
    LogText(__LINE__, pszFnName, TEXT("New standalone Stop position 2 is %I64d."), llStopPosNew2);
    LogText(__LINE__, pszFnName, TEXT("The original stop position with Sink is %I64d."), llStopPos1);
    if(llStopPosNew2 != llStopPosNew1)
    {
        LogError(__LINE__, pszFnName, TEXT("Stop position before and after Run differs."));
        g_flag = false;
    }
    if(llStopPos1 != llStopPosNew1)
    {
        LogError(__LINE__, pszFnName, TEXT("The stop positions are different when get with Sink and without Sink."));
        g_flag = false;
    }

    _CHK(mySourceNew.Stop(), TEXT("Stop New source"));
    mySourceNew.UnInitialize();
    mySource.UnInitialize();
    mySink.UnInitialize();

    if(g_flag == true)
        return TPR_PASS;
    else
        return TPR_FAIL;
}    

TESTPROCAPI BeginTemporaryRecDiffBufTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // This test will loop N times on different Temp Buffer Size.
    // The rule is, temp buffer is always larger or equal than it current size.

    const TCHAR * pszFnName = TEXT("BeginTemporaryRecDiffBufTest");

    LONGLONG InitTempBufferSize = 100 * 1000;// set 100 second buffer init size;
    LONGLONG LastTempBufferSize = 0;
    LONGLONG CurrTempBufferSize = 0;
    LONGLONG SetTempBufferSize[5] = {200*1000, 300*1000, 250*1000, 830*1000, 400*1000};
    STRMBUF_CAPTURE_MODE Mode;
    // When later if the temp file size are large, this number has to change.
    int N = 5;// Looper iTmp time;
    int iInterval = 10*1000;//interval as 10 seconds

    CSinkFilterGraphEx mySinkGraph;

    g_flag = true;//set default as true;

    mySinkGraph.LogToConsole(true);

    mySinkGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);

    _CHK(mySinkGraph.Initialize(), TEXT("Initialize graph"));

    _CHK(mySinkGraph.SetupFilters(TRUE), TEXT("Setting up Sink Graph"));

    // Search Interfaces
    _CHK(mySinkGraph.SetGrabberCallback(mySinkGraph.SimpleCountDisp), TEXT("SetCallback of GrabberSample"));
    //Setting recording path
    _CHK(mySinkGraph.SetRecordingPath(), TEXT("SetRecording Path"));
    _CHK(mySinkGraph.BeginTemporaryRecording(InitTempBufferSize), TEXT("SBC BeginTemporaryRecording"));

    _CHK(mySinkGraph.Run(), TEXT("Run Graph"));
    Sleep(iInterval);//let graph full functional
    _CHK(mySinkGraph.GetCaptureMode(&Mode,&LastTempBufferSize), TEXT("Get Capture Mode initialize"));

    while(N-- > 0)
    {
        LPOLESTR pTMP = NULL;
        _CHK(mySinkGraph.BeginTemporaryRecording(SetTempBufferSize[N]), TEXT("BeginTemporaryRecording"));
        LogText(__LINE__, pszFnName, TEXT("Temp Buffer is set as : %d."), SetTempBufferSize[N]);
        Sleep(iInterval);
        CurrTempBufferSize = 0;
        _CHK(mySinkGraph.GetCaptureMode(&Mode,&CurrTempBufferSize), TEXT("Get Capture Mode"));
        LogText(__LINE__, pszFnName, TEXT("Get Mode:%d,\tLength:%d."), Mode, (int)CurrTempBufferSize);
        if(Mode != STRMBUF_TEMPORARY_RECORDING || CurrTempBufferSize < SetTempBufferSize[N])
        {
            LogError(__LINE__, pszFnName, TEXT("Mode or BufferSize wrong. Mode has to be STRMBUF_TEMPORARY_RECORDING and buffer size larger than last time :%d."), LastTempBufferSize);
            g_flag = false;
            break;
        }
        LogText(__LINE__, pszFnName, TEXT("------------------------------------"));
        LastTempBufferSize = CurrTempBufferSize;
    }
    _CHK(mySinkGraph.Stop(), TEXT("Stop Graph"));
    mySinkGraph.UnInitialize();

    if(g_flag == false)
        return TPR_FAIL;
    return TPR_PASS;
}

TESTPROCAPI BeginTempPermTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // This test make sure the Perm Rec successfully convert the previous
    // Temp Rec into itself as specified.
    // 1. Create 30 sec Temp Rec, Remember Sink stamp1 Count
    // 2. Create 30 Sec Perm Rec (with 30 sec conversion), remember Stamp2 Count.
    // 3. Destroy Sink, build Source, playback perm rec item for 80 sec.
    // Check. (A), stamp conserved (B) stamp = stamp1+stamp2.
    const TCHAR * pszFnName = TEXT("BeginTempPermTest");

    CSinkFilterGraphEx mySink;
    CSourceFilterGraphEx mySource;

    INT32 uiStampTemp;// Temp count
    INT32 uiStampPerm;// Perm Count
    INT32 uiStampTotal;// total count, should equal Source
    INT32 uiStampSource;// Final source playback sount
    INT32 ierror = 30;// Max Error Tolerence

    LONGLONG llTempBuffer = 30*1000;// 30 sec Temp Rec
    LONGLONG llBuffConv = 30*1000;// Perm conserve 30 sec
    UINT32 ui32PlayDur = (UINT32)(llTempBuffer + llBuffConv) + 10*1000;// PlayLength

    LPOLESTR wszFileName = NULL;// Perm Filename
    g_flag = true;//set default as true;

    mySource.LogToConsole(true);
    mySink.LogToConsole(true);

    mySource.SetCommandLine(g_pShellInfo->szDllCmdLine);
    mySink.SetCommandLine(g_pShellInfo->szDllCmdLine);

    // ===============================================================
    // 1. Create 30 sec Temp Rec, Remember Sink stamp1 Count
    //Set up the Sink Graph with EM10 + Grabber + DVR sink
    _CHK(mySink.Initialize(), TEXT("Sink Inititialize"));

    _CHK(mySink.SetupFilters(TRUE), TEXT("Setting up sink graph"));

    _CHK(mySink.SetGrabberCallback(mySink.SequentialStamper), TEXT("Setting up sink graph Stamper"));
    mySink.Ex_iCount = 0;

    LPOLESTR pRecordingPath = NULL;
    _CHK(mySink.SetRecordingPath(), TEXT("SetRecording Path"));
    _CHK(mySink.GetRecordingPath(&pRecordingPath), TEXT("SetRecording Path"));
    LogText(TEXT("Set recording Path as %s."), pRecordingPath);
    CoTaskMemFree(pRecordingPath);
    pRecordingPath = NULL;

    _CHK(mySink.BeginTemporaryRecording(llTempBuffer), TEXT("Sink BeginTemporaryRecording"));
    _CHK(mySink.Run(), TEXT("Run Sink"));
    LogText(__LINE__, pszFnName, TEXT("Temp Recording last for %d seconds."), (UINT32)llTempBuffer/1000);
    Sleep((UINT32)llTempBuffer);
    uiStampTemp = mySink.Ex_iCount;
    LogText(__LINE__, pszFnName, TEXT("Temp Recording has count stamp %d."), uiStampTemp);
        // 2. Create 30 Sec Perm Rec (with 30 sec conversion), remember Stamp2 Count.
    _CHK(mySink.BeginPermanentRecording(llBuffConv), TEXT("Sink BeginPermanentRecording"));
    _CHK(mySink.GetCurFile(&wszFileName, NULL), TEXT("get recording Filename"));
    LogText(__LINE__, pszFnName, TEXT("The recorded filename is %s."), wszFileName);
    LogText(__LINE__, pszFnName, TEXT("Permanent Recording last for %d seconds."), (UINT32)llTempBuffer/1000);
    Sleep((UINT32)llTempBuffer);
    uiStampTotal = mySink.Ex_iCount;
    uiStampPerm = uiStampTotal - uiStampTemp;
    LogText(__LINE__, pszFnName, TEXT("Permanent Recording has count stamp %d."), uiStampPerm);

    // we must stop and uninitalize the sink graph prior to setting up the source for playback. If we leave the sink graph
    // set up but stopped it will retain connections to the permanent recording it just did, resulting in the source graph attempting
    // to use the sink graph's live bypass when it nears the end of the recording.
    _CHK(mySink.Stop(), TEXT("Stop Sink"));
    mySink.UnInitialize();

    // 3. Stop Sink, build Source, playback perm rec item for 80 sec.
    _CHK(mySource.Initialize(), TEXT("my Source Initialize"));
    _CHK(mySource.SetupFilters(wszFileName, TRUE), TEXT("my Source Setup graph"));
    _CHK(mySource.SetGrabberCallback(mySource.SequentialPermStampChecker), TEXT("hookup Stamp Checker"));
    LogText(__LINE__, pszFnName, TEXT("Source will play for %d seconds."), ui32PlayDur);
    _CHK(mySource.Run(), TEXT("my Source Run"));
    Sleep(ui32PlayDur);
    uiStampSource = mySource.Ex_iCount;
    // Check. (A), stamp conserved (B) stamp = stamp1+stamp2.
    LogText(__LINE__, pszFnName, TEXT("Source playlength Stamp (uiStampSource) is %d."), uiStampSource);
    LogText(__LINE__, pszFnName, TEXT("Sink Temp Stamp is %d."), uiStampTemp);
    LogText(__LINE__, pszFnName, TEXT("sink Perm Stamp is %d."), uiStampPerm);
    LogText(__LINE__, pszFnName, TEXT("sink Total Stamp (uiStampTotal) is %d."), uiStampTotal);
    if( abs(uiStampSource - uiStampTotal) > ierror)
    {
        g_flag = false;
        LogError(__LINE__, pszFnName, TEXT("The uiStampSource length differs from uiStampTotal too much."));
    }

    mySource.UnInitialize();

    if(g_flag == true)
        return TPR_PASS;
    else
        return TPR_FAIL;
}

TESTPROCAPI BeginPermRecShortTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    /*    
    IStreamBufferCapture::BeginPermanentRecording will execute properly when execute 
    a length very short. This correspond to press recording then stop immediately.
    1. Start Perm Rec.
    2. Get filename and Stop immediately. ( like 1 sec later )
    3. Load the file and play.
    see see what will happen
    */

    const TCHAR * pszFnName = TEXT("BeginPermRecShortTest");

    CSinkFilterGraphEx mySinkGraph;
    CSourceFilterGraphEx mySourceGraph;
    LPOLESTR wszFileName = NULL;

    g_flag = true;//set default as true;

    mySourceGraph.LogToConsole(true);
    mySinkGraph.LogToConsole(true);

    mySourceGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);
    mySinkGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);

    //Set up the Sink Graph with EM10 + Grabber + DVR sink
    mySinkGraph.Initialize();

    _CHK(mySinkGraph.SetupFilters(TRUE), TEXT("Setting up sink graph"));

    _CHK(mySinkGraph.SetGrabberCallback(CSinkFilterGraphEx::SequentialStamper), TEXT("Setting up sink graph Stamper"));
    //_CHK(mySinkGraph.SetGrabberCallback(NULL), TEXT("Setting up sink graph Stamper"));

    LPOLESTR pRecordingPath = NULL;
    _CHK(mySinkGraph.SetRecordingPath(), TEXT("SetRecording Path"));
    _CHK(mySinkGraph.GetRecordingPath(&pRecordingPath), TEXT("SetRecording Path"));
    LogText(TEXT("Set recording Path as %s."), pRecordingPath);
    CoTaskMemFree(pRecordingPath);
    pRecordingPath = NULL;

    _CHK(mySinkGraph.BeginPermanentRecording(0), TEXT("Sink BeginPermanentRecording"));
    _CHK(mySinkGraph.Run(), TEXT("Run Sink"));
    // Get Current Perm File Name
    _CHK(mySinkGraph.GetCurFile(&wszFileName ,NULL), TEXT("Get Perm Rec File Name"));
    LogText(__LINE__, pszFnName, TEXT("Recording to file : %s."), wszFileName);
    LogText(__LINE__, pszFnName, TEXT("Sleep 1 second."));
    Sleep(1000);
    _CHK(mySinkGraph.Stop(), TEXT("Stop sink Graph"));
    mySinkGraph.UnInitialize();
    //    Short recording is done
    // Now set up playback filter! DVR source + Grabber + Demux;
    _CHK(mySourceGraph.Initialize(), TEXT("Initialize Source FilterGraph"));
    _CHK(mySourceGraph.SetupFilters(wszFileName, TRUE), TEXT("Setup Perm Rec Playback graph"));
    _CHK(mySourceGraph.SetGrabberCallback(mySourceGraph.SequentialStampDisp), TEXT("Setup Callback func"));
    _CHK(mySourceGraph.Run(), TEXT("Run Source"));
    Sleep(3 * 1000);
    // hookup callback!
    _CHK(mySourceGraph.SetGrabberCallback(NULL), TEXT("SetCallback of GrabberSample as NULL"));
    // Stop graph
    _CHK(mySourceGraph.Stop(), TEXT("Stop source Graph"));
    LogText(__LINE__, pszFnName, TEXT("Source played %d frame."), mySinkGraph.Ex_iCount);

    mySourceGraph.UnInitialize();
    mySinkGraph.UnInitialize();

    if(g_flag == false)
        return TPR_FAIL;
    return TPR_PASS;
}

TESTPROCAPI BeginPermPermTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // This test make sure the Perm Rec discard conversion if the previous
    // Rec is a Permanent Rec.
    // 1. Create 30 sec Perm Rec1, Remember Sink stamp1 Count
    // 2. Create 30 Sec Perm Rec2 (with 30 sec conversion), remember Stamp2 Count.
    // 3. Destroy Sink, build Source, playback perm rec item for 80 sec.
    // Check. (A), stamp conserved (B) Sourcestamp = stamp2.

    const TCHAR * pszFnName = TEXT("BeginPermPermTest");

    CSinkFilterGraphEx mySink;
    CSourceFilterGraphEx mySource;

    UINT32 uiStampPerm1;// Perm1 count
    UINT32 uiStampPerm2;// Perm2 Count
    UINT32 uiStampTotal;// total count, should equal Source
    UINT32 uiStampSource1;// Final source playback sount
    UINT32 uiStampSource2;// Final source playback sount
    INT32 ierror = 30;// Max Error Tolerence

    LONGLONG llPermBuffer = 30*1000;// 30 sec Perm1 Rec
    LONGLONG llBuffConv = 30*1000;// Perm conserve 30 sec
    UINT32 ui32PlayDur = (UINT32)(llBuffConv) + 10*1000;// PlayLength

    LPOLESTR wszFileName1 = NULL;// Perm Filename
    LPOLESTR wszFileName2 = NULL;// Perm Filename
    g_flag = true;//set default as true;

    mySource.LogToConsole(true);
    mySink.LogToConsole(true);

    mySource.SetCommandLine(g_pShellInfo->szDllCmdLine);
    mySink.SetCommandLine(g_pShellInfo->szDllCmdLine);

    // ===============================================================
    // 1. Create 30 sec Perm1 Rec, Remember Sink stamp1 Count
    //Set up the Sink Graph with EM10 + Grabber + DVR sink
    _CHK(mySink.Initialize(), TEXT("Sink Initialize"));

    _CHK(mySink.SetupFilters(TRUE), TEXT("Setting up sink graph"));

    _CHK(mySink.SetGrabberCallback(mySink.SequentialStamper), TEXT("Setting up sink graph Stamper"));
    mySink.Ex_iCount = 0;

    LPOLESTR pRecordingPath = NULL;
    _CHK(mySink.SetRecordingPath(), TEXT("SetRecording Path"));
    _CHK(mySink.GetRecordingPath(&pRecordingPath), TEXT("SetRecording Path"));
    LogText(TEXT("Set recording Path as %s."), pRecordingPath);
    CoTaskMemFree(pRecordingPath);
    pRecordingPath = NULL;

    _CHK(mySink.BeginPermanentRecording(llPermBuffer), TEXT("Sink BeginPermanentRecording 1"));
    _CHK(mySink.GetCurFile(&wszFileName1, NULL), TEXT("get recording 1 Filename"));
    LogText(__LINE__, pszFnName, TEXT("The recorded 1 filename is %s."), wszFileName1);
    _CHK(mySink.Run(), TEXT("Run Sink"));
    LogText(__LINE__, pszFnName, TEXT("Perm Recording last for %d seconds."), (UINT32)llPermBuffer/1000);
    Sleep((UINT32)llPermBuffer);
    uiStampPerm1 = mySink.Ex_iCount;
    LogText(__LINE__, pszFnName, TEXT("Perm Recording 1 has count stamp %d."), uiStampPerm1);
    // 2. Create 30 Sec Perm Rec (with 30 sec conversion), remember Stamp2 Count.
    _CHK(mySink.BeginPermanentRecording(2*llBuffConv), TEXT("Sink BeginPermanentRecording 2"));
    _CHK(mySink.GetCurFile(&wszFileName2, NULL), TEXT("get recording Filename"));
    LogText(__LINE__, pszFnName, TEXT("The recorded 2 filename is %s."), wszFileName2);
    LogText(__LINE__, pszFnName, TEXT("Permanent Recording last for %d seconds."), (UINT32)llPermBuffer/1000);
    Sleep((UINT32)llPermBuffer);
    uiStampTotal = mySink.Ex_iCount;
    uiStampPerm2 = uiStampTotal - uiStampPerm1;
    LogText(__LINE__, pszFnName, TEXT("Permanent Recording 2 has count stamp %d."), uiStampPerm2);
    _CHK(mySink.Stop(), TEXT("Stop Sink"));
    // 3. Stop Sink, build Source, playback perm rec item for 80 sec.
    _CHK(mySource.Initialize(), TEXT("my Source Initialize"));
    _CHK(mySource.SetupFilters(wszFileName1, TRUE), TEXT("my Source Setup graph"));
    _CHK(mySource.SetGrabberCallback(mySource.SequentialPermStampChecker), TEXT("hookup Stamp Checker"));
    LogText(__LINE__, pszFnName, TEXT("Source will play for %d seconds."), ui32PlayDur);
    _CHK(mySource.Run(), TEXT("my Source Run"));
    Sleep(ui32PlayDur);
    uiStampSource1 = mySource.Ex_iCount;
    _CHK(mySource.Stop(), TEXT("my Source Stop"));
    _CHK(mySource.Load(wszFileName2, NULL), TEXT("my Source load Perm 2"));
    _CHK(mySource.Run(), TEXT("my Source Run Again"));
    Sleep(ui32PlayDur);
    uiStampSource2 = mySource.Ex_iCount - uiStampSource1;
    _CHK(mySource.Stop(), TEXT("my Source Stop"));
    // Check. (A), stamp conserved (B) Source stamp1 = stamp2 = 30 sec
    LogText(__LINE__, pszFnName, TEXT("Source 1 playlength Stamp is %d."), uiStampSource1);
    LogText(__LINE__, pszFnName, TEXT("Source 2 playlength Stamp is %d."), uiStampSource2);
    LogText(__LINE__, pszFnName, TEXT("Sink Perm1 Stamp is %d."), uiStampPerm1);
    LogText(__LINE__, pszFnName, TEXT("sink Perm2 Stamp is %d."), uiStampPerm2);
    LogText(__LINE__, pszFnName, TEXT("sink Total Stamp is %d."), uiStampTotal);
    if(uiStampSource1 < 100 || uiStampSource2 < 100)
    {
        LogError(__LINE__, pszFnName, TEXT("Recorded item is wrong."));
        g_flag = false;
    }

    mySource.UnInitialize();
    mySink.UnInitialize();

    if(g_flag == true)
        return TPR_PASS;
    else
        return TPR_FAIL;
}

TESTPROCAPI SetRecordingPathDeep(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // This is simple : make a 1000 character long Dir ans
    // Set it as recording Path. Start a Temp rec,
    // Get filename and see if we can properly play it.

    // 1. Generate a deep.....deep dir.
    //        use loop to do the job
    // 2. set recording path and carry on
    // As long as the code finish successfully. Test pass.

    const TCHAR * pszFnName = TEXT("SetRecordingPathDeep");

    LPTSTR lpDirFullName = new TCHAR[MAX_PATH];
    LPOLESTR lpFileName = NULL;
    size_t iMaxDirLength = MAX_PATH;
    HRESULT hr = S_OK;
    LPCTSTR DirName = TEXT("CTVDeepDirTest");
    int iDepth = 16;
    int i = 0;
    DWORD dwError = 0;
    UINT32 iRecDur = 5;
    CSinkFilterGraphEx mySink;
    g_flag = true;

    if(lpDirFullName)
    {
        memset(lpDirFullName, NULL, iMaxDirLength);

        _CHK(mySink.GetDefaultRecordingPath(lpDirFullName), TEXT("SetRecording Path"));

        while(i++ < iDepth)
        {
            _tcscat(lpDirFullName, TEXT("\\"));
            _tcscat(lpDirFullName, DirName);
            if(CreateDirectory(lpDirFullName, NULL) == NULL)    // Zero indicate fail;
            {
                dwError = GetLastError();
                if(dwError != 183)
                {
                    LogWarning(__LINE__, pszFnName, TEXT("Failed to create deep dir. GetlastError as %d."), dwError);
                    return TPR_ABORT;
                }
            }
            LogText(__LINE__, pszFnName, TEXT("Creating %dth Dir."), i);
        }
        LogText(__LINE__, pszFnName, TEXT("The filename is :  %s."), lpDirFullName);
        LogText(__LINE__, pszFnName, TEXT("Max path is %d, deepdir path is %d."), MAX_PATH, _tcslen(lpDirFullName));

        mySink.LogToConsole(true);

        mySink.SetCommandLine(g_pShellInfo->szDllCmdLine);

        // Start recording to that dir!

        _CHK(mySink.Initialize(), TEXT("Initialize Sink Graph"));

        _CHK(mySink.SetupFilters(TRUE), TEXT("setup Sink Graph"));

        hr = mySink.SetRecordingPath(lpDirFullName), TEXT("Set recording Path");
        if(FAILED(hr))
        {
            LogText(__LINE__, pszFnName, TEXT("Failed to set deep path; succeed."));
        }
        else
        {
            LogError(__LINE__, pszFnName, TEXT("Succeeded in setting a path deeper than what should be functional."));
            g_flag = false;
        }
    }
    else
    {
        LogError(__LINE__, pszFnName, TEXT("Failed to allocate a pointer for the string."));
        g_flag = false;
    }

    if(!g_flag)
        return TPR_FAIL;
    return TPR_PASS;
}

TESTPROCAPI SetRecordingPathNoExistTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // This is simple : make a 1000 character long Dir ans
    // Set it as recording Path. Start a Temp rec,
    // Get filename and see if we can properly play it.

    // 1. Generate a deep.....deep dir.
    //        use loop to do the job
    // 2. set recording path and carry on
    // As long as the code finish successfully. Test pass.

    const TCHAR * pszFnName = TEXT("SetRecordingPathNoExistTest");

    LPTSTR lpDirFullName = new TCHAR[MAX_PATH];
    LPOLESTR lpFileName = NULL;
    size_t iMaxDirLength = MAX_PATH;
    memset(lpDirFullName, NULL, iMaxDirLength);
    HRESULT hr = S_OK;
    GUID myGuid;
    LPOLESTR lpGuidStr = new WCHAR[40];//32 Guid + null

    CSinkFilterGraphEx mySink;

    g_flag = true;//set default as true;

    if(lpDirFullName && lpGuidStr)
    {
        // Create unique Dir
        Helper_CreateRandomGuid(&myGuid);
        // CoCreateGuid not supported on all configs
        //_CHK(CoCreateGuid(&myGuid), TEXT("CoCreateGuid for Recording Path"));
        if(StringFromGUID2(myGuid,lpGuidStr, 40) == 0)
        {
            LogError(__LINE__, pszFnName, TEXT("Failed to convert GUID to wstring; abort."));
            LogText(__LINE__, pszFnName, TEXT("The gotten GUID is %s."), lpGuidStr);
            LogText(__LINE__, pszFnName, TEXT("GetLastError is %d."), GetLastError());
            return TPR_ABORT;
        }
        LogText(__LINE__, pszFnName, TEXT("The generated string is %s."), lpGuidStr);
        // generate unique dir

        _CHK(mySink.GetDefaultRecordingPath(lpDirFullName), TEXT("SetRecording Path"));
        _tcscat(lpDirFullName, TEXT("\\"));
        _tcscat(lpDirFullName, lpGuidStr);
        LogText(__LINE__, pszFnName, TEXT("The Final generated recording path is :  %s."), lpDirFullName);

        // Start recording to that dir!

        mySink.LogToConsole(true);

        mySink.SetCommandLine(g_pShellInfo->szDllCmdLine);

        _CHK(mySink.Initialize(), TEXT("Initialize Sink Graph"));

        _CHK(mySink.SetupFilters(TRUE), TEXT("setup Sink Graph"));

        hr = mySink.SetRecordingPath(lpDirFullName), TEXT("Set recording Path");
    }
    else
    {
        LogError(__LINE__, pszFnName, TEXT("Failed to allocate a pointer for the string."));
        hr = E_FAIL;
    }

    mySink.UnInitialize();

    if(lpDirFullName)
        delete [] lpDirFullName;

    if(lpGuidStr)
        delete [] lpGuidStr;

    if(FAILED(hr))
    {
        LogText(__LINE__, pszFnName, TEXT("Failed to set non-exist path; succeed"));
        return TPR_PASS;
    }
    else
    {
        LogText(__LINE__, pszFnName, TEXT("succeed to set non exist path; error"));
        return TPR_FAIL;
    }
}

TESTPROCAPI SetRecordingPathNULLTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // This is simple : Make a recording Path
    //    from GUID , so it is guranteed non-exist
    // Then check if DVREngine return error.

    const TCHAR * pszFnName = TEXT("SetRecordingPathNULLTest");

    CSinkFilterGraphEx mySink;
    LPOLESTR lpNULLPATH = NULL;
    HRESULT hr = S_OK;

    g_flag = true;//set default as true;

    mySink.LogToConsole(true);

    mySink.SetCommandLine(g_pShellInfo->szDllCmdLine);

    // Start recording to NULL dir!

    _CHK(mySink.Initialize(), TEXT("Initialize Sink Graph"));

    _CHK(mySink.SetupFilters(TRUE), TEXT("setup Sink Graph"));

    hr = mySink.SetRecordingPath(lpNULLPATH), TEXT("Set recording Path as NULL");

    mySink.UnInitialize();

    if(FAILED(hr))
    {
        LogText(__LINE__, pszFnName, TEXT("Failed to set NULL path. Expected. hr = 0x%x."), hr);
        return TPR_PASS;
    }
    else
    {
        LogText(__LINE__, pszFnName, TEXT("succeed to set non exist path; error."));
        return TPR_FAIL;
    }
}

TESTPROCAPI GetRecordingPathUninitializedTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // This is simple : Make a recording Path
    //    from GUID , so it is guranteed non-exist
    // Then check if DVREngine return error.

    const TCHAR * pszFnName = TEXT("GetRecordingPathUninitializedTest");

    CSinkFilterGraphEx mySink;
    LPOLESTR lpGetRecPATH = NULL;
    HRESULT hr = S_OK;

    g_flag = true;//set default as true;

    mySink.LogToConsole(true);

    mySink.SetCommandLine(g_pShellInfo->szDllCmdLine);

    // Start recording to NULL dir!

    _CHK(mySink.Initialize(), TEXT("Initialize Sink Graph"));

    _CHK(mySink.SetupFilters(TRUE), TEXT("setup Sink Graph"));

    hr = mySink.GetRecordingPath(&lpGetRecPATH), TEXT("Get recording Path before set the Path");

    mySink.UnInitialize();

    if(!FAILED(hr) && lpGetRecPATH != NULL)
    {
        LogText(__LINE__, pszFnName, TEXT("Succeed to get rec path before set path, retrieved default path as expected."));
        LogText(__LINE__, pszFnName, TEXT("The gotten path is : %s."), lpGetRecPATH);
        return TPR_PASS;
    }
    else
    {
        LogText(__LINE__, pszFnName, TEXT("Failed to get rec path or get NULL path before set path; succeed. hr = 0x%x."), hr);
        return TPR_FAIL;
    }
}

DWORD CleanupOrphanedBrokenRecTest_ThreadProc(LPVOID lpParameter)
{
    const TCHAR * pszFnName = TEXT("CleanupOrphanedBrokenRecTest_ThreadProc");

    LONGLONG InitTempBufferSize = 9999*1000;
    LPOLESTR fn = new WCHAR[256];
    CSinkFilterGraph mySinkGraph;

    g_flag = true;//set default as true;

    if(!fn)
    {
        LogWarning(__LINE__, pszFnName, TEXT("Allocating an array to hold the file name failed."));
        return -1;
    }

    if(lpParameter == NULL)
    {
        LogWarning(__LINE__, pszFnName, TEXT("Must pass in a >=  256 WCHAR pointer to hold the return filename."));
        delete [] fn;
        return -1;
    }

    _CHK(CoInitialize(NULL), TEXT("Initialize COM"));

    mySinkGraph.LogToConsole(true);

    mySinkGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);

    _CHK(mySinkGraph.Initialize(), TEXT("Thread initialize Graph"));

    _CHK(mySinkGraph.SetupFilters(), TEXT("Thread Setting up Sink Graph"));

    //Setting recording path
    _CHK(mySinkGraph.SetRecordingPath(), TEXT("Thread SetRecording Path"));
    _CHK(mySinkGraph.BeginTemporaryRecording(InitTempBufferSize), TEXT("Thread SBC BeginTemporaryRecording"));
    _CHK(mySinkGraph.GetCurFile(&fn, NULL), TEXT("Thread GetCurFile"));
    LogText(__LINE__, pszFnName, TEXT("Thread get filename as : %s."), fn);
    _tcscpy((LPOLESTR)lpParameter,fn);

    _CHK(mySinkGraph.Run(), TEXT("Run Graph"));

    mySinkGraph.UnInitialize();

    delete [] fn;

    Sleep(9999*1000);// Stop here basically.

    return 0;
}

TESTPROCAPI CleanupOrphanedBrokenRecTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    const TCHAR * pszFnName = TEXT("CleanupOrphanedBrokenRecTest");

    LPOLESTR lpFileName = new WCHAR[256];
    CSinkFilterGraph mySink;
    WIN32_FIND_DATA myFind;
    TCHAR tszRecordingPath[MAX_PATH];

    HANDLE hd = CreateThread( NULL, NULL, CleanupOrphanedBrokenRecTest_ThreadProc, (LPVOID)lpFileName, NULL, NULL);
    if(hd == NULL)
    {
        LogWarning(__LINE__, pszFnName, TEXT("Can not create Child Thread; abort."));
        return TPR_ABORT;
    }
    LogText(__LINE__, pszFnName, TEXT("Spwan child process as 0x%x."), hd);
    Sleep(25*1000);
    LogText(__LINE__, pszFnName, TEXT("The filename in Parents is %s."), (LPOLESTR)lpFileName);
    LogText(__LINE__, pszFnName, TEXT("Going to terminate the child process."));
    TerminateThread(hd, 0);
    Sleep(3*1000);// Let system rest

    g_flag = true;//set default as true;

    mySink.LogToConsole(true);

    mySink.SetCommandLine(g_pShellInfo->szDllCmdLine);

    _CHK(mySink.Initialize(), TEXT("Initialize Sink in Parent"));

    _CHK(mySink.SetupFilters(), TEXT("Setup WMV file"));

    _CHK(mySink.GetDefaultRecordingPath(tszRecordingPath), TEXT("Retrieve the default recording path"));

    _CHK(mySink.CleanupOrphanedRecordings(tszRecordingPath), TEXT("Sink Clean Orphans"));
    // Generate the filename

    mySink.UnInitialize();

    _tcscat(lpFileName, TEXT("*.*"));
    if(FindFirstFile(lpFileName, &myFind) != INVALID_HANDLE_VALUE)
    {
        LogError(__LINE__, pszFnName, TEXT("Find recording file after cleanup; error."));
        return TPR_FAIL;
    }
    else
    {
        return TPR_PASS;
    }
}

#if 0
TESTPROCAPI PlayBrokenRecTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    /*    
    This test will send 1000 asf sample to DVR engine (app 65 MB). Use a GrabberSample to detect if stream is
    going and read back correctly. Stream are stamped with a unique number from 1 to UINT32.MAX. Just make sure
    we read back from 1 to whatever we write to disk.
    */


    const TCHAR * pszFnName = TEXT("PlayBrokenRecTest");

    CSinkFilterGraphEx mySinkGraph;
    CSourceFilterGraphEx mySourceGraph;

    UINT32 ui32StartCount = 0;
    UINT32 ui32EndCount = 0;
    int iNumOfSample = 400// Default send 1000 Samples
    UINT32 ui32Time = iNumOfSample / 20;
    LPOLESTR wszFileName = NULL;
    LPOLESTR wszFileName1st = new TCHAR[256];
    FILE * fp = NULL;
    fpos_t flen = 0;

    if(ts->iNOS != 0)
    {
        LogText(__LINE__, pszFnName, TEXT("Setting number of sample as %d."), ts->iNOS);
        iNumOfSample = ts->iNOS//set costomered sample number.
        ui32Time = iNumOfSample / 20//reset sleep time
    }
    else
    {
        LogText(__LINE__, pszFnName, TEXT("Use /cNOS### to specify # of samples to send. One sample around 65K. ### is 0-9 digits."));
        LogText(__LINE__, pszFnName, TEXT("Default is around 1300 Samples, ~50 seconds."));
    }


    g_flag = true    //set default as true;
    mySinkGraph.LogToConsole(true);
    mySourceGraph.LogToConsole(true);
    //Set up the Sink Graph with EM10 + Grabber + DVR sink
    mySinkGraph.Initialize();
    _CHK(mySinkGraph.SetupFilters(TRUE), TEXT("Setting up sink graph"));
    //_CHK(mySinkGraph.SetGrabberCallback(NULL), TEXT("Setting up sink graph Stamper"));


    LPOLESTR pRecordingPath = NULL;
    _CHK(mySinkGraph.SetRecordingPath(), TEXT("SetRecording Path"));
    _CHK(mySinkGraph.GetRecordingPath(&pRecordingPath), TEXT("SetRecording Path"));
    LogText(TEXT("Set recording Path as %s."), pRecordingPath);
    CoTaskMemFree(pRecordingPath);
    pRecordingPath = NULL;
    _CHK(mySinkGraph.BeginPermanentRecording(0), TEXT("Sink BeginPermanentRecording"));
    mySinkGraph.Run();
    LogText(__LINE__, pszFnName, TEXT("Sleep ui32Time seconds."));
    Sleep(ui32Time * 1000);
    // Get Current Perm File Name
    _CHK(mySinkGraph.GetCurFile(&wszFileName ,NULL), TEXT("Get Perm Rec File Name"));
    LogText(__LINE__, pszFnName, TEXT("Recording to file : %s."), wszFileName);
    Sleep(10 * 1000)// Let rec last a little several files
    _CHK(mySinkGraph.Stop(), TEXT("Stop the Recording"));
    mySinkGraph.UnInitialize()    //release file so can mess it up
    // Now try to mess up the file
#if 1
    LogText(__LINE__, pszFnName, TEXT("Now start to mess up the recorded file."));
    _tcscpy(wszFileName1st, wszFileName);
    _tcscat(wszFileName1st, _T("_0.dvr-dat"));
    LogText(__LINE__, pszFnName, TEXT("Will mess up file %s."), wszFileName1st);
    fp = _tfopen(wszFileName1st, _T("rw"));
    if(fp == NULL)
    {
        LogWarning(_T("Failed to open the perm recorded file; abort."));
        return TPR_ABORT;
    }
    if(fseek(fp, 0, SEEK_END) != 0)
    {
        LogWarning(_T("Failed to seek in recorded file; abort."));
        return TPR_ABORT;
    }
    if(fgetpos(fp, &flen) != 0)
    {
        LogWarning(_T("Failed to Get recorded file length; abort."));
        return TPR_ABORT;
    }
    LogText(__LINE__, pszFnName, TEXT("File len is %d."), (UINT32)flen);
    flen /= 100    // 1/10 from start of file.
    if(fsetpos(fp, &flen) != 0)
    {
        LogWarning(_T("Failed to Get recorded file length; abort."));
        return TPR_ABORT;
    }
    flen *= 50;
    srand(54654676)// random number
    while(flen-- > 0)
    {
        _fputtc((unsigned)rand(), fp);
    }
    fclose(fp);
    LogText(__LINE__, pszFnName, TEXT("finished messing file up."));
    // should mess up file well
#endif
    // Now set up playback filter! DVR source + Grabber + Demux;
    _CHK(mySourceGraph.Initialize(), TEXT("Initialize Source FilterGraph"));
    _CHK(mySourceGraph.SetupFiltersWithGrabber(wszFileName), TEXT("Setup Source filter graph"));
    _CHK(mySourceGraph.SetGrabberCallback(mySourceGraph.NullCounterNoStamp), TEXT("Setup grabber callback"))
    LogText(__LINE__, pszFnName, TEXT("Sleep 3 seconds."));
    Sleep(3 * 1000);
    mySourceGraph.Ex_iCount = 0;

    _CHK(mySourceGraph.Run() ,TEXT("Run Source Graph"));
    LogText(__LINE__, pszFnName, TEXT("Sleep %d seconds."), ui32Time*2);
    Sleep(ui32Time *2 * 1000);
    LogText(__LINE__, pszFnName, TEXT("if you see this then we may be good! See when we stop the graph."));
    LogText(__LINE__, pszFnName, TEXT("The number of samples are %d."), mySourceGraph.Ex_iCount, ui32Time * 10);
    if(mySourceGraph.Ex_iCount < ui32Time * 10)
    {
        LogError(__LINE__, pszFnName, TEXT("The number of samples are too small."));
        g_flag = false;
    }
    // Stop graph
    _CHK(mySourceGraph.Stop(), TEXT("Stop source Graph"));
    //_CHK(mySinkGraph.Stop(), TEXT("Stop sink Graph"));

    if(g_flag == false)
        return TPR_FAIL;
    return TPR_PASS;
}
#endif

TESTPROCAPI FlushAndSeektoNow(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // This test ensure after a flush, the source graph will seek to now.
    // The flush is gained by putting a tunning on Sink.
    // Stamp will be used here to check if the source do seek to NOW
    // After the flush

    const TCHAR * pszFnName = TEXT("FlushAndSeektoNow");

    DWORD result = TPR_FAIL;
    HRESULT hr = E_FAIL;
    long lFoundSignal = 0;
    long lchannel = 0;
    UINT32 uiSourceStamp = 0;
    UINT32 uiSinkStamp = 0;

    CSinkFilterGraph mySink;
    CSourceFilterGraphEx mySource;

    CComPtr<IBaseFilter> pEm10 = NULL;
    CComPtr<IBaseFilter> pGrabber = NULL;
    CComPtr<IBaseFilter> pDVRSink = NULL;
    CComPtr<IBaseFilter> pDemux = NULL;
    CComPtr<IGrabberSample> pGrabberSample = NULL;
    CComPtr<IStreamBufferCapture> pIStreamBufferCapture = NULL;

    LPOLESTR lpLiveToken = NULL;

    g_flag = true;

    mySink.LogToConsole(true);    // no popup window;
    mySource.LogToConsole(true);

    mySource.SetCommandLine(g_pShellInfo->szDllCmdLine);
    mySink.SetCommandLine(g_pShellInfo->szDllCmdLine);

    mySink.Initialize();

    _CHK(mySink.SetupFilters(TRUE), TEXT("mySink.SetupFilters"));

    lchannel = 1;
    _CHK(mySink.TuneTo(lchannel,AMTUNER_SUBCHAN_DEFAULT,AMTUNER_SUBCHAN_DEFAULT), TEXT("IAMTvTuner putchannel to channel lchannel"));
    LogText(__LINE__, pszFnName, TEXT("Tune to channel %d."), lchannel);
    //setup callback
    _CHK(mySink.FindInterface(__uuidof(IGrabberSample), (void **)&pGrabberSample), TEXT("Get IGrabberSample interface"));
    _CHK(pGrabberSample->SetCallback(CSinkFilterGraphEx::SequentialStamper, NULL), TEXT("set SequentialStamper callback"));

    _CHK(mySink.SetRecordingPath(), TEXT("Setup recording path"));
    _CHK(mySink.BeginTemporaryRecording(300*1000), TEXT("SBC Begin Temp Rec"));

    Sleep(3*1000);

    lchannel = 1;
    //    _CHK(pTvTuner->AutoTune(lchannel, &lFoundSignal), TEXT("IAMTvTuner autotune to channel lchannel"));
    //    _CHK(pTvTuner->StoreAutoTune(), TEXT("IAMTvTuner store autotune"));
    // Start Sink 
    _CHK(mySink.Run(), TEXT("Start Sink"));
    _CHK(mySink.GetBoundToLiveToken(&lpLiveToken), TEXT("get live token now"));
    // Setup Source
    LogText(__LINE__, pszFnName, TEXT("Sleep 2 seconds."));
    Sleep(2*1000);    // Let system rest a while
    _CHK(mySource.Initialize(), TEXT("Initialize source"));
    _CHK(mySource.SetupFilters(lpLiveToken, TRUE), TEXT("source setup filters with live token"));
    _CHK(mySource.Run(), TEXT("source Run"));
    Sleep(2*1000);
    _CHK(mySource.Pause(), TEXT("source Pause 10 seconds"));
    Sleep(10*1000);
    _CHK(mySource.SetGrabberCallback(mySource.SequentialStampNullCounter), TEXT("source hook Temp Stamp Counter"));
    //_CHK(mySource.SetGrabberCallback(mySource.SequentialStampDisp), TEXT("source hook Temp Stamp Disp"));
    _CHK(mySource.Run(), TEXT("source Run again"));
    // Should be playing channel lchannel now
    LogText(__LINE__, pszFnName, TEXT("Sleep 10 seconds."));
    Sleep(10*1000);
    LogText(__LINE__, pszFnName, TEXT("Current source frame %d. Sink frame stamp %d."), mySource.Ex_iCount, CSinkFilterGraphEx::Ex_iCount);
    if(CSinkFilterGraphEx::Ex_iCount - mySource.Ex_iCount < 100)
    {
        LogWarning(__LINE__, pszFnName, TEXT("After 50 seconds pause the Gap between Sink and Source are too narrow."));
    }

    lchannel = 2;
    _CHK(mySink.TuneTo(lchannel,AMTUNER_SUBCHAN_DEFAULT,AMTUNER_SUBCHAN_DEFAULT), TEXT("IAMTvTuner tune to channel lchannel"));
    LogText(__LINE__, pszFnName, TEXT("Tune to channel %d."), lchannel);
    Sleep(20*1000);
    LogText(__LINE__, pszFnName, TEXT("Current source frame %d. Sink frame stamp %d."), mySource.Ex_iCount, CSinkFilterGraphEx::Ex_iCount);
    if(CSinkFilterGraphEx::Ex_iCount - mySource.Ex_iCount > 100)
    {
        LogError(__LINE__, pszFnName, TEXT("After Tune to Channel %d, the Gap between Sink and Source are too large (Live)."), lchannel);
        g_flag = false;
    }

    if(g_flag == false)
    {
        return TPR_FAIL;
    }
    else
    {
        return TPR_PASS;
    }
}

// requires the IAMStreamSelect interfaces
#if 0
TESTPROCAPI Helper_Test_IAMStreamSelect_Count(IAMStreamSelect *  pIAMStreamSelect)
{
    const TCHAR * pszFnName = TEXT("Helper_Test_IAMStreamSelect_Count");

    DWORD result = TPR_FAIL;
    HRESULT hr = E_POINTER;
    DWORD cStreams = 0;

    if(pIAMStreamSelect == NULL)
    {
        LogText(__LINE__, pszFnName, TEXT("pIAMStreamSelect is NULL."));
    }
    else
    {
        hr = pIAMStreamSelect->Count(&cStreams);

        if(SUCCEEDED(hr))
        {
            result = TPR_PASS;
            LogText(__LINE__, pszFnName, TEXT("pIAMStreamSelect->Count succeeded and returned %d."), cStreams);
        }
        else
        {
            LogError(__LINE__, pszFnName, TEXT("pIAMStreamSelect->Count failed %08x."), hr);
        }
    }

    return result;
}

TESTPROCAPI Helper_Test_IAMStreamSelect_Count_Negative(IAMStreamSelect *  pIAMStreamSelect)
{
    DWORD result = TPR_FAIL;
    const TCHAR *  pszFnName = TEXT("Helper_Test_IAMStreamSelect_Count_Negative");
    HRESULT hr = E_POINTER;

    if(pIAMStreamSelect == NULL)
    {
        LogText(__LINE__, pszFnName, TEXT("pIAMStreamSelect is NULL."));
    }
    else
    {
        // negative testing
        LogText(__LINE__, pszFnName, TEXT("Calling pIAMStreamSelect->Count(NULL)."));

        hr = pIAMStreamSelect->Count(NULL);

        if(SUCCEEDED(hr))
        {
            LogError(__LINE__, pszFnName, TEXT("pIAMStreamSelect->Count(NULL) succeeded unexpectedly."));
        }
        else
        {
            result = TPR_PASS;
            LogText(__LINE__, pszFnName, TEXT("pIAMStreamSelect->Count failed as expected %08x."), hr);
        }
    }

    return result;
}

TESTPROCAPI Helper_Test_IAMStreamSelect_Enable(IAMStreamSelect *  pIAMStreamSelect)
{
    DWORD result = TPR_PASS;
    const TCHAR *  pszFnName = TEXT("Helper_Test_IAMStreamSelect_Enable");
    HRESULT hr = E_POINTER;
    DWORD cStreams = 0;

    if(pIAMStreamSelect == NULL)
    {
        LogError(__LINE__, pszFnName, TEXT("pIAMStreamSelect is NULL."));
        result = TPR_FAIL;
    }
    else
    {
        hr = pIAMStreamSelect->Count(&cStreams);

        if(SUCCEEDED(hr))
        {
            LogText(__LINE__, pszFnName, TEXT("pIAMStreamSelect->Count succeeded and returned %d."), cStreams);

            for(int i=0; i<(int)cStreams; i++)
            {
                //disable stream
                LogText(__LINE__, pszFnName, TEXT("Disabling Stream %d."), i);

                hr = pIAMStreamSelect->Enable(i, 0);

                if(FAILED(hr))
                {
                    LogError(__LINE__, pszFnName, TEXT("Disabling Stream %d failed with error %08x."), i, hr);
                    result = TPR_FAIL;
                }
                else
                {
                    Sleep(2000);
                    //enable stream
                    LogText(__LINE__, pszFnName, TEXT("(re)enabling Stream %d."), i);
                    hr = pIAMStreamSelect->Enable(i, AMSTREAMSELECTENABLE_ENABLE);

                    if(FAILED(hr))
                    {
                        LogError(__LINE__, pszFnName, TEXT("(re)enabling Stream %d failed with error %x."), i, hr);
                        result = TPR_FAIL;
                    }
                    else
                    {
                        Sleep(2000);
                    }
                }
            }
        }
        else
        {
            LogError(__LINE__, pszFnName, TEXT("pIAMStreamSelect->Count failed %08x."), hr);
            result = TPR_FAIL;
        }
    }

    return result;
}

TESTPROCAPI Helper_Test_IAMStreamSelect_Enable_Negative(IAMStreamSelect *  pIAMStreamSelect)
{
    DWORD result = TPR_PASS;
    const TCHAR *  pszFnName = TEXT("Helper_Test_IAMStreamSelect_Enable");
    HRESULT hr = E_POINTER;
    DWORD cStreams = 0;

    if(pIAMStreamSelect == NULL)
    {
        LogError(__LINE__, pszFnName, TEXT("pIAMStreamSelect is NULL."));
        result = TPR_FAIL;
    }
    else
    {
        hr = pIAMStreamSelect->Count(&cStreams);

        if(SUCCEEDED(hr))
        {
            LogText(__LINE__, pszFnName, TEXT("pIAMStreamSelect->Count succeeded and returned %d."), cStreams);
        }
        else
        {
            LogText(__LINE__, pszFnName, TEXT("pIAMStreamSelect->Count failed %08x assuming a value of 9."), hr);
            cStreams = 9;
        }

        //negative testing
        cStreams++; //non-existing stream

        //disable non-existing stream
        LogText(__LINE__, pszFnName, TEXT("Disabling non-existing Stream %d."), cStreams);

        hr = pIAMStreamSelect->Enable(cStreams, 0);

        if(FAILED(hr))
        {
            LogText(__LINE__, pszFnName, TEXT("Disabling non-existing Stream failed as expected with error %08x."), hr);
        }
        else
        {
            result = TPR_FAIL;
            LogError(__LINE__, pszFnName, TEXT("Disabling non-existing Stream succeeded unexpectedly."));
        }

        //enable non-existing stream
        LogText(__LINE__, pszFnName, TEXT("(re)enabling non-existing Stream %d."), cStreams);

        hr = pIAMStreamSelect->Enable(cStreams, AMSTREAMSELECTENABLE_ENABLE);

        if(FAILED(hr))
        {
            LogText(__LINE__, pszFnName, TEXT("(re)enabling non-existing Stream failed as expected with error %08x."), hr);
        }
        else
        {
            result = TPR_FAIL;
            LogError(__LINE__, pszFnName, TEXT("(re)enabling non-existing Stream succeeded unexpectedly."));
        }
    }

    return result;
}

TESTPROCAPI Helper_Test_IAMStreamSelect_Info(IAMStreamSelect *  pIAMStreamSelect)
{
    DWORD result = TPR_PASS;
    const TCHAR *  pszFnName = TEXT("Helper_Test_IAMStreamSelect_Info");
    HRESULT hr = E_POINTER;
    DWORD cStreams = 0;

    AM_MEDIA_TYPE * pmt     = NULL;
    DWORD           dwFlags = NULL;
    LCID            lcid    = NULL;
    DWORD           dwGroup = NULL;
    WCHAR         * pszName = NULL;
    IUnknown      * pObject = NULL;
    IUnknown      * pUnk    = NULL;

    if(pIAMStreamSelect == NULL)
    {
        LogError(__LINE__, pszFnName, TEXT("pIAMStreamSelect is NULL."));
        result = TPR_FAIL;
    }
    else
    {
        hr = pIAMStreamSelect->Count(&cStreams);

        if(SUCCEEDED(hr))
        {
            LogText(__LINE__, pszFnName, TEXT("pIAMStreamSelect->Count succeeded and returned %d."), cStreams);

            for(int i=0; i<(int)cStreams; i++)
            {
                //disable stream
                LogText(__LINE__, pszFnName, TEXT("Getting Info on Stream %d."), i);

                hr = pIAMStreamSelect->Info(i,
                    &pmt,
                    &dwFlags,
                    &lcid,
                    &dwGroup,
                    &pszName,
                    &pObject,
                    &pUnk ); 

                if(FAILED(hr))
                {
                    LogError(__LINE__, pszFnName, TEXT("pIAMStreamSelect->Info %d failed with error %08x."), i, hr);
                    result = TPR_FAIL;
                }
                else
                {
                    LogText(__LINE__, pszFnName, TEXT("pIAMStreamSelect->Info %d succeeded."), i);

                    if(pmt == NULL)
                    {
                        result = TPR_FAIL;
                        LogError(__LINE__, pszFnName, TEXT("pIAMStreamSelect->Info %d mt == NULL."), i);
                    }
                    else
                    {
                        DeleteMediaType(pmt);
                    }

                    if(pszName == NULL)
                    {
                        result = TPR_FAIL;
                        LogError(__LINE__, pszFnName, TEXT("pIAMStreamSelect->Info %d pszName == NULL."), i);
                    }
                    else
                    {
                        CoTaskMemFree(pszName);
                    }

                    if(pObject == NULL)
                    {
                        result = TPR_FAIL;
                        LogError(__LINE__, pszFnName, TEXT("pIAMStreamSelect->Info %d pObject == NULL."), i);
                    }
                    else
                    {
                        pObject->Release();
                    }

                    if(pUnk == NULL)
                    {
                        result = TPR_FAIL;
                        LogError(__LINE__, pszFnName, TEXT("pIAMStreamSelect->Info %d pUnk == NULL."), i);
                    }
                    else
                    {
                        pUnk->Release();
                    }
                }
            }
        }
        else
        {
            LogError(__LINE__, pszFnName, TEXT("pIAMStreamSelect->Count failed %08x."), hr);
            result = TPR_FAIL;
        }
    }

    return result;
}

TESTPROCAPI Helper_Test_IAMStreamSelect_Info_Negative(IAMStreamSelect *  pIAMStreamSelect)
{
    DWORD result = TPR_PASS;
    const TCHAR *  pszFnName = TEXT("Helper_Test_IAMStreamSelect_Info");
    HRESULT hr = E_POINTER;
    DWORD cStreams = 0;

    if(pIAMStreamSelect == NULL)
    {
        LogError(__LINE__, pszFnName, TEXT("pIAMStreamSelect is NULL."));
        result = TPR_FAIL;
    }
    else
    {
        hr = pIAMStreamSelect->Count(&cStreams);

        if(SUCCEEDED(hr))
        {
            LogText(__LINE__, pszFnName, TEXT("pIAMStreamSelect->Count succeeded and returned %d."), cStreams);

            for(int i=0; i<=(int)cStreams; i++)
            {
                //disable stream
                LogText(__LINE__, pszFnName, TEXT("Getting Info on Stream %d."), i);

                hr = pIAMStreamSelect->Info(i,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL,
                    NULL ); 

                if(FAILED(hr))
                {
                    if(i == cStreams)
                    {
                        LogText(__LINE__, pszFnName, TEXT("pIAMStreamSelect->Info failed as expected for non-existant stream %d with error %08x."), i, hr);
                    }
                    else
                    {
                        LogError(__LINE__, pszFnName, TEXT("pIAMStreamSelect->Info %d failed with error %08x."), i, hr);
                        result = TPR_FAIL;
                    }
                }
                else
                {
                    LogText(__LINE__, pszFnName, TEXT("pIAMStreamSelect->Info %d succeeded."), i);
                }
            }
        }
        else
        {
            LogError(__LINE__, pszFnName, TEXT("pIAMStreamSelect->Count failed %08x."), hr);
            result = TPR_FAIL;
        }
    }

    return result;
}

TESTPROCAPI Helper_Test_IAMStreamSelect(CSourceFilterGraph * pSourceFilterGraph, TEST_SELECTION testSelection)
{
    const TCHAR *  pszFnName = TEXT("Helper_Test_IAMStreamSelect");
    DWORD result = TPR_FAIL;
    IAMStreamSelect *  pIAMStreamSelect = NULL;
    HRESULT hr = pSourceFilterGraph->FindInterface(IID_IAMStreamSelect, (void **)&pIAMStreamSelect);

    if(SUCCEEDED(hr))
    {
        LogText(__LINE__, pszFnName, TEXT("pSourceFilterGraph->FindInterface IID_IAMStreamSelect succeeded."));

        if(SUCCEEDED(hr))
        {
            switch(testSelection)
            {
            case TEST_SELECTION_IAMStreamSelect_COUNT:
                result = Helper_Test_IAMStreamSelect_Count(pIAMStreamSelect);
                break;
            case TEST_SELECTION_IAMStreamSelect_COUNT_NEGATIVE:
                result = Helper_Test_IAMStreamSelect_Count_Negative(pIAMStreamSelect);
                break;
            case TEST_SELECTION_IAMStreamSelect_ENABLE:
                result = Helper_Test_IAMStreamSelect_Enable(pIAMStreamSelect);
                break;
            case TEST_SELECTION_IAMStreamSelect_ENABLE_NEGATIVE:
                result = Helper_Test_IAMStreamSelect_Enable_Negative(pIAMStreamSelect);
                break;
            case TEST_SELECTION_IAMStreamSelect_INFO:
                result = Helper_Test_IAMStreamSelect_Info(pIAMStreamSelect);
                break;
            case TEST_SELECTION_IAMStreamSelect_INFO_NEGATIVE:
                result = Helper_Test_IAMStreamSelect_Info_Negative(pIAMStreamSelect);
                break;
            default:
                hr = E_UNEXPECTED;
            }
        }

        pIAMStreamSelect->Release();
    }
    else
    {
        LogError(__LINE__, pszFnName, TEXT("pSourceFilterGraph->FindInterface IID_IAMStreamSelect failed %08x."), hr);
    }

    return result;
}
#endif

TESTPROCAPI Helper_Test_SourceFilterGraph_StopOrPause_And_Run(CSourceFilterGraph * pSourceFilterGraph, TEST_SELECTION testSelection)
{
    LPCTSTR     pszFnName = TEXT("Helper_Test_SourceFilterGraph_StopOrPause_And_Run");
    HRESULT hr = E_UNEXPECTED;

    g_flag = TRUE;

    // let some video get buffered up, 4-9 seconds
    Sleep((rand() % 5000) + 4000);

    for(int i = 0; i < 10; i++)
    {
        switch(testSelection)
        {
            case TEST_SELECTION_SourceGraph_RewindStopAndRun:
                Sleep(8*1000);
                hr = pSourceFilterGraph->SetRate(-1);
            case TEST_SELECTION_SourceGraph_StopAndRun:
                LogText(__LINE__, pszFnName, TEXT("pSourceFilterGraph->Stop."));
                hr = pSourceFilterGraph->Stop();
                break;
            case TEST_SELECTION_SourceGraph_PauseAndRun:
                LogText(__LINE__, pszFnName, TEXT("pSourceFilterGraph->Pause."));
                hr = pSourceFilterGraph->Pause();
                break;
            default:
                break;
        }

        if(FAILED(hr))
        {
            LogError(__LINE__, pszFnName, TEXT("failed %08x."), hr);
            g_flag = FALSE;
        }
        else
        {
            LogText(__LINE__, pszFnName, TEXT("succeeded."));

            LogText(__LINE__, pszFnName, TEXT("Sleep 2000."));

            Sleep(2000);

            hr = pSourceFilterGraph->Run();

            if(FAILED(hr))
            {
                LogError(__LINE__, pszFnName, TEXT("pSourceFilterGraph->Run failed %08x."), hr);
                g_flag = FALSE;
            }
            else
            {
                LogText(__LINE__, pszFnName, TEXT("pSourceFilterGraph->Run succeeded."));
            }
        }
    }

    if(!g_flag)
        return TPR_FAIL;
    return TPR_PASS;
}

TESTPROCAPI Helper_Test_BeginMultipleRecordings(CSinkFilterGraph * pSinkFilterGraph, CSourceFilterGraph * pSourceFilterGraph, LONGLONG llBufferSizeInMilliseconds, TEST_SELECTION testSelection)
{
    LPCTSTR     pszFnName = TEXT("Helper_Test_BeginMultipleRecordings");
    HRESULT     hr        = E_UNEXPECTED;
//    LPOLESTR    currentRecordingFile;
//    LPOLESTR    currentPlayingFile;

    TEST_EVENT_DATA testEventData;

    g_flag = TRUE;

    hr = InitializeTestEventData(&testEventData, EC_COMPLETE);

    if(FAILED(hr))
    {
        LogError(__LINE__, pszFnName, TEXT("InitializeTestEventData failed %08x."), hr);
        g_flag = FALSE;
    }
    else
    {
        pSourceFilterGraph->RegisterOnEventCallback(CallbackOnMediaEvent, &testEventData);

        Sleep(60000);

        hr = pSinkFilterGraph->BeginPermanentRecording(0);

        if(FAILED(hr))
        {
            LogError(__LINE__, pszFnName, TEXT("pSinkFilterGraph->BeginPermanentRecording failed %08x."), hr);
            g_flag = FALSE;
        }
        else
        {
            switch(testSelection)
            {
            case TEST_SELECTION_BoundToLiveBeginMultiplePermanentRecording:
                //verify that source is still playing

                if(WaitForTestEvent(&testEventData, 60000))
                {
                    LogError(__LINE__, pszFnName, TEXT("Received EC_COMPLETE from Source Filter Graph unexpectedly."));
                    g_flag = FALSE;
                }
                else
                {
                    LogText(__LINE__, pszFnName, TEXT("Did NOT receive EC_COMPLETE from Source Filter Graph as expected."));
                }
                break;
            case TEST_SELECTION_BoundToRecordingBeginMultiplePermanentRecording:

                if(WaitForTestEvent(&testEventData, 60000))
                {
                    LogText(__LINE__, pszFnName, TEXT("Received EC_COMPLETE from Source Filter Graphs."));
                }
                else
                {
                    LogError(__LINE__, pszFnName, TEXT("Did NOT receive EC_COMPLETE from Source Filter Graphs."));
                    g_flag = FALSE;
                }
                break;
            default:
                break;
            }
        }
    }

    if(!g_flag)
        return TPR_FAIL;
    return TPR_PASS;
}

TESTPROCAPI Helper_Test_RateChangedEvent(CSourceFilterGraph * pSourceFilterGraph)
{
    LPCTSTR     pszFnName = TEXT("Helper_Test_RateChangedEvent");
    HRESULT     hr        = E_UNEXPECTED;
    TEST_EVENT_DATA testEventData;

    g_flag = TRUE;

    hr = InitializeTestEventData(&testEventData, STREAMBUFFER_EC_RATE_CHANGED);

    if(FAILED(hr))
    {
        LogText(__LINE__, pszFnName, TEXT("InitializeTestEventData failed %08x."), hr);
    }
    else
    {
        pSourceFilterGraph->RegisterOnEventCallback(CallbackOnMediaEvent, &testEventData);

        // need atleast 9 seconds of video buffered, so the rate stays at -1 and doesn't immediatly
        // pop back to 1 and trigger a beginning of pause buffer event.
        Sleep(9000);

        hr = pSourceFilterGraph->SetRate(-1.0);

        if(FAILED(hr))
        {
            LogError(__LINE__, pszFnName, TEXT("pSourceFilterGraph->SetRate failed %08x."), hr);
            g_flag = FALSE;
        }
        else
        {
            double dRate = 0;

            hr = pSourceFilterGraph->GetRate(&dRate);

            if(FAILED(hr) || dRate != -1.0)
            {
                LogError(__LINE__, pszFnName, TEXT("rate is incorrect %f != %f or pSourceFilterGraph->GetRate failed %08x."), dRate, -1.0, hr);
                g_flag = FALSE;
            }

            if(WaitForTestEvent(&testEventData, 10000))
            {
                LogText(__LINE__, pszFnName, TEXT("Received STREAMBUFFER_EC_RATE_CHANGED from Source Filter Graph as expected."));
            }
            else
            {
                LogError(__LINE__, pszFnName, TEXT("Did NOT receive STREAMBUFFER_EC_RATE_CHANGED from Source Filter Graph."));
                g_flag = FALSE;        
            }
            Sleep(1000);
        }
    }

    if(!g_flag)
        return TPR_FAIL;
    return TPR_PASS;
}

TESTPROCAPI Helper_Test_GetSetPositions_SeekBackward(CSourceFilterGraph * pSourceFilterGraph)
{
    LPWSTR pszFnName = TEXT("Helper_Test_GetSetPositions_SeekBackward");
    HRESULT hr = E_FAIL;
    __int64 /*LONGLONG*/ llCurrentPositionBeforeSet;
    __int64 /*LONGLONG*/ llStopPosition;
    __int64 /*LONGLONG*/ llCurrentPositionAfterSet = 1;

    LogText(__LINE__, pszFnName, TEXT("Sleeping for 20 seconds to get enough AV."));
    Sleep(30000);

    g_flag = TRUE;

    hr = pSourceFilterGraph->GetPositions(&llCurrentPositionBeforeSet, &llStopPosition);

    if(SUCCEEDED(hr))
    {
        LogText(__LINE__, pszFnName, TEXT("pSourceFilterGraph->GetPositions succeeded."));

        hr = pSourceFilterGraph->SetPositions(&llCurrentPositionAfterSet, AM_SEEKING_AbsolutePositioning, NULL, NULL);

        if(SUCCEEDED(hr))
        {
            LogText(__LINE__, pszFnName, TEXT("pSourceFilterGraph->SetPositions succeeded."));

            // lets delay five seconds. That should give any system ample time to carry out the seek and start playing from the new
            // location.  We have 30 seconds of video, so we should always end up seeking to long before our previous current position.
            Sleep(5000);

            hr = pSourceFilterGraph->GetPositions(&llCurrentPositionAfterSet, &llStopPosition);

            if(SUCCEEDED(hr))
            {
                LogText(__LINE__, pszFnName, TEXT("pSourceFilterGraph->GetPositions succeeded."));

                if(llCurrentPositionAfterSet < llCurrentPositionBeforeSet)
                {
                    LogText(__LINE__, pszFnName, TEXT("llCurrentPositionAfterSet [%I64u] < llCurrentPositionBeforeSet [%I64u]; seek backward worked."), llCurrentPositionAfterSet, llCurrentPositionBeforeSet);
                }
                else
                {
                    LogError(__LINE__, pszFnName, TEXT("llCurrentPositionAfterSet [%I64u] !< llCurrentPositionBeforeSet [%I64u]; seek backward failed."), llCurrentPositionAfterSet, llCurrentPositionBeforeSet);
                    g_flag = FALSE;
                }
            }
            else
            {
                LogError(__LINE__, pszFnName, TEXT("pSourceFilterGraph->GetPositions failed %08x."), hr);
                g_flag = FALSE;
            }
        }
        else
        {
            LogError(__LINE__, pszFnName, TEXT("pSourceFilterGraph->SetPositions failed %08x."), hr);
            g_flag = FALSE;
        }
    }
    else
    {
        LogError(__LINE__, pszFnName, TEXT("pSourceFilterGraph->GetPositions failed %08x."), hr);
        g_flag = FALSE;
    }

    if(!g_flag)
        return TPR_FAIL;
    return TPR_PASS;
}

TESTPROCAPI Helper_Test_GetSetPositions_SeekForward(CSourceFilterGraph * pSourceFilterGraph)
{
    LPWSTR pszFnName = TEXT("Helper_Test_GetSetPositions_SeekForward");
    HRESULT hr = E_FAIL;
    __int64 /*LONGLONG*/ llCurrentPositionBeforeSet;
    __int64 /*LONGLONG*/ llStopPosition;
    __int64 /*LONGLONG*/ llCurrentPositionAfterSet = 1;

    g_flag = TRUE;

    LogText(__LINE__, pszFnName, TEXT("Sleeping for 20 seconds to get enough AV."));

    Sleep(20000);

    hr = pSourceFilterGraph->GetPositions(&llCurrentPositionBeforeSet, &llStopPosition);

    if(SUCCEEDED(hr))
    {
        LogText(__LINE__, pszFnName, TEXT("pSourceFilterGraph->GetPositions succeeded."));

        hr = pSourceFilterGraph->SetPositions(&llCurrentPositionAfterSet, AM_SEEKING_AbsolutePositioning, NULL, NULL);

        if(SUCCEEDED(hr))
        {
            LogText(__LINE__, pszFnName, TEXT("pSourceFilterGraph->SetPositions succeeded."));

            // seeking may take up to 4 seconds.
            Sleep(4000);

            hr = pSourceFilterGraph->GetPositions(&llCurrentPositionAfterSet, &llStopPosition);

            if(SUCCEEDED(hr))
            {
                LogText(__LINE__, pszFnName, TEXT("pSourceFilterGraph->GetPositions succeeded."));

                if(llCurrentPositionAfterSet < llCurrentPositionBeforeSet)
                {
                    LogText(__LINE__, pszFnName, TEXT("llCurrentPositionAfterSet [%I64u] < llCurrentPositionBeforeSet [%I64u]; seek backward worked."), llCurrentPositionAfterSet, llCurrentPositionBeforeSet);
                }
                else
                {
                    LogError(__LINE__, pszFnName, TEXT("llCurrentPositionAfterSet [%I64u] !< llCurrentPositionBeforeSet [%I64u]; seek backward failed."), llCurrentPositionAfterSet, llCurrentPositionBeforeSet);
                    g_flag = FALSE;
                }

                // seek to the end
                hr = pSourceFilterGraph->SetPositions(&llStopPosition, AM_SEEKING_AbsolutePositioning, NULL, NULL);

                if(SUCCEEDED(hr))
                {
                    LogText(__LINE__, pszFnName, TEXT("pSourceFilterGraph->SetPositions succeeded."));

                    Sleep(1000);

                    hr = pSourceFilterGraph->GetPositions(&llCurrentPositionAfterSet, &llStopPosition);

                    if(SUCCEEDED(hr))
                    {
                        LogText(__LINE__, pszFnName, TEXT("pSourceFilterGraph->GetPositions succeeded."));

                        // if the new position isn't greater than the position we were at after the 20 second delay, we have a failure
                        if(llCurrentPositionAfterSet > llCurrentPositionBeforeSet)
                        {
                            LogText(__LINE__, pszFnName, TEXT("llCurrentPositionAfterSet [%I64u] < llCurrentPositionBeforeSet [%I64u]; seek forward worked."), llCurrentPositionAfterSet, llCurrentPositionBeforeSet);
                        }
                        else
                        {
                            LogError(__LINE__, pszFnName, TEXT("llCurrentPositionAfterSet [%I64u] !< llCurrentPositionBeforeSet [%I64u]; seek forward failed."), llCurrentPositionAfterSet, llCurrentPositionBeforeSet);
                            g_flag = FALSE;
                        }
                    }
                }

            }
            else
            {
                LogError(__LINE__, pszFnName, TEXT("pSourceFilterGraph->GetPositions failed %08x."), hr);
                g_flag = FALSE;
            }
        }
        else
        {
            LogError(__LINE__, pszFnName, TEXT("pSourceFilterGraph->SetPositions failed %08x."), hr);
            g_flag = FALSE;
        }
    }
    else
    {
        LogError(__LINE__, pszFnName, TEXT("pSourceFilterGraph->GetPositions failed %08x."), hr);
        g_flag = FALSE;
    }

    if(!g_flag)
        return TPR_FAIL;
    return TPR_PASS;}

TESTPROCAPI Helper_Test_GetSetPositions(CSourceFilterGraph * pSourceFilterGraph, TEST_SELECTION testSelection)
{
    switch(testSelection)
    {
        case TEST_SELECTION_Position_SeekBackward:
            return Helper_Test_GetSetPositions_SeekBackward(pSourceFilterGraph);
        case TEST_SELECTION_Position_SeekForward:
            return Helper_Test_GetSetPositions_SeekForward(pSourceFilterGraph);
        default:
            return TPR_ABORT;
    }
}

TESTPROCAPI Helper_Test_RateAndPositioning(CSourceFilterGraph * pSourceFilterGraph, GRAPH_TYPE GraphType)
{
    LPWSTR pszFnName = TEXT("Helper_Test_RateAndPositioning");
    HRESULT hr = S_OK;
    __int64 /*LONGLONG*/ llStopPosition = 0;
    __int64 /*LONGLONG*/ llCurrentPositionAtStart = 0;
    __int64 /*LONGLONG*/ llCurrentPositionAfterDelay = 0;
    __int64 /*LONGLONG*/ llNewPosition = 0;
    __int64 /*LONGLONG*/ llCurrentPositionAfterSet = 0;
    __int64 /*LONGLONG*/ llCurrentPositionAfterDelayAfterSet = 0;
    double dRate = 0;
    // if we're at our position within 6 seconds, it seems reasonable.
    __int64 /*LONGLONG*/ llAllowableDelta = 6 * 1000 * 10000;
    int nClipLength = 30000;


    g_flag = true;

    _CHK(hr = pSourceFilterGraph->GetPositions(&llCurrentPositionAtStart, &llStopPosition), TEXT("GetPositions before the delay."));

    LogText(__LINE__, pszFnName, TEXT("Sleeping for 20 seconds to get enough AV."));

    // we now have 30 seconds of video saved up between llCurrentPositionAtStart and llCurrentPositionAfterDelay, to play with.
    Sleep(nClipLength);

    _CHK(hr = pSourceFilterGraph->GetPositions(&llCurrentPositionAfterDelay, &llStopPosition), TEXT("GetPositions after the delay"));


    // Test 0 - verify while at live trying to go into the future fails as expected.
    hr = pSourceFilterGraph->SetRate(2);
    if(SUCCEEDED(hr))
    {
        LogError(__LINE__, pszFnName, TEXT("Succeeded setting rate to 2x while at the live portion of the video."));
        g_flag = false;
    }

    hr = pSourceFilterGraph->GetRate(&dRate);
    if(FAILED(hr) || dRate == 2)
    {
        LogError(__LINE__, pszFnName, TEXT("Rate is incorrect %f == %f or pSourceFilterGraph->GetRate failed 0x%08x."), dRate, 2.00, hr);
    }

    // test #1, go to the middle of the video captured and set our rate to 50%.  If we then play for cliplength additional seconds then
    // we should be roughly back to where we stopped after the initial cliplength delay.

    // a new position is half way between the where we are now and where we were when we started.  (10 seconds after llCurrentPositionAtStart)
    llNewPosition = llCurrentPositionAtStart + (llCurrentPositionAfterDelay - llCurrentPositionAtStart)/2;

    _CHK(hr = pSourceFilterGraph->SetRate(.5), TEXT("Setting the rate to 1/2 normal."));
    Sleep(400);
    // set to the middle and set our new rate
    _CHK(hr = pSourceFilterGraph->SetPositions(&llNewPosition, AM_SEEKING_AbsolutePositioning, &llStopPosition, AM_SEEKING_AbsolutePositioning), TEXT("SetPositions to the middle."));
    // it can take up to four seconds for the engine to carry out the request
    Sleep(4000);

    hr = pSourceFilterGraph->GetRate(&dRate);
    if(FAILED(hr) || dRate != .5)
    {
        LogError(__LINE__, pszFnName, TEXT("Rate is incorrect %f != %f or pSourceFilterGraph->GetRate failed %08x."), dRate, .5, hr);
    }

    // check that the new current position is where we expect to be.
    _CHK(hr = pSourceFilterGraph->GetPositions(&llCurrentPositionAfterSet, &llStopPosition), TEXT("GetPositions before the delay."));
    if(abs((int) (llCurrentPositionAfterSet - llNewPosition)) > (llAllowableDelta * 2))
    {
        LogError(__LINE__, pszFnName, TEXT("New position 0x%08x != set position 0x%08x."), (DWORD) llCurrentPositionAfterSet, (DWORD) llNewPosition);
        g_flag = false;
    }


    LogText(__LINE__, pszFnName, TEXT("Sleeping for %d seconds at half speed to get back to where we were."), nClipLength);

    Sleep(nClipLength);
    _CHK(hr = pSourceFilterGraph->GetPositions(&llCurrentPositionAfterDelayAfterSet, &llStopPosition), TEXT("GetPositions after the delay"));

    // if the current position after running at half speed isn't the same as the end position plus or minus some allowable variance, then fail.
    if(abs((int) (llCurrentPositionAfterDelay - llCurrentPositionAfterDelayAfterSet)) > llAllowableDelta)
    {
        LogError(__LINE__, pszFnName, TEXT("Position after the delay 0x%08x is too far away from the estimated position 0x%08x."), (DWORD) llCurrentPositionAfterDelay, (DWORD) llCurrentPositionAfterDelayAfterSet);
        g_flag = false;
    }

    // test #2, go to the beginning of the clip captured and set our rate to 2x.  If we then play for cliplength/2 additional seconds then
    // we should be back to where we stopped our capture

    // normalize the system for a second
    _CHK(hr = pSourceFilterGraph->SetRate(1), TEXT("Setting the rate to 1x normal to normalize the system."));
    Sleep(1000);

    // now set to the beginning of our clip
    _CHK(hr = pSourceFilterGraph->SetPositions(&llCurrentPositionAtStart, AM_SEEKING_AbsolutePositioning, &llStopPosition, AM_SEEKING_AbsolutePositioning), TEXT("SetPositions to the middle."));
    Sleep(400);
    _CHK(hr = pSourceFilterGraph->SetRate(2), TEXT("Setting the rate to 2x normal."));
    Sleep(200);

    hr = pSourceFilterGraph->GetRate(&dRate);
    if(FAILED(hr) || dRate != 2)
    {
        LogError(__LINE__, pszFnName, TEXT("Rate is incorrect %f != %f or pSourceFilterGraph->GetRate failed 0x%08x."), dRate, 2., hr);
        g_flag = false;
    }

    // check that the new current position is where we expect to be.
    _CHK(hr = pSourceFilterGraph->GetPositions(&llCurrentPositionAfterSet, &llStopPosition), TEXT("GetPositions before the 10 second delay."));
    if(abs((int) (llCurrentPositionAfterSet - llCurrentPositionAtStart)) > (llAllowableDelta * 2))
    {
        LogError(__LINE__, pszFnName, TEXT("New position 0x%08x != set position 0x%08x."), (DWORD) llCurrentPositionAfterSet, (DWORD) llCurrentPositionAtStart);
        g_flag = false;
    }

    LogText(__LINE__, pszFnName, TEXT("Sleeping for %d seconds at twice speed to get back to where we were."), nClipLength/2);

    Sleep(nClipLength/2);
    _CHK(hr = pSourceFilterGraph->GetPositions(&llCurrentPositionAfterDelayAfterSet, &llStopPosition), TEXT("GetPositions after the delay"));
    // we're at 2x, so our error for estimating the destination position is larger, so if we're within 2x the delta we're good
    // if the current position after running at twice speed isn't the same as the end position plus or minus some allowable variance, then fail.
    if(abs((int) (llCurrentPositionAfterDelay - llCurrentPositionAfterDelayAfterSet)) > (llAllowableDelta * 2))
    {
        LogError(__LINE__, pszFnName, TEXT("Position after the delay 0x%08x is too far away from the estimated position 0x%08x."), (DWORD) llCurrentPositionAfterDelay, (DWORD) llCurrentPositionAfterDelayAfterSet);
        g_flag = false;
    }


    // test #3, go to the end of the clip captured and set our rate to reverse half speed.  If we then play for clip length seconds then
    // we should be somewhere in the middle

    // set to the beginning of our clip
    _CHK(hr = pSourceFilterGraph->SetPositions(&llCurrentPositionAfterDelay, AM_SEEKING_AbsolutePositioning, &llStopPosition, AM_SEEKING_AbsolutePositioning), TEXT("SetPositions to the middle."));
    Sleep(300);
    _CHK(hr = pSourceFilterGraph->SetRate(-.5), TEXT("Setting the rate to -1/2 normal."));
    Sleep(100);

    hr = pSourceFilterGraph->GetRate(&dRate);
    if(FAILED(hr) || dRate != -.5)
    {
        LogError(__LINE__, pszFnName, TEXT("rate is incorrect %f != %f or pSourceFilterGraph->GetRate failed 0x%08x."), dRate, -.5, hr);
        g_flag = false;
    }

    // check that the new current position is where we expect to be.
    _CHK(hr = pSourceFilterGraph->GetPositions(&llCurrentPositionAfterSet, &llStopPosition), TEXT("GetPositions before the delay."));
    if(abs((int) (llCurrentPositionAfterSet - llCurrentPositionAfterDelay)) > llAllowableDelta)
    {
        LogError(__LINE__, pszFnName, TEXT("New position 0x%08x != set position 0x%08x."), (DWORD) llCurrentPositionAfterSet, (DWORD) llCurrentPositionAfterDelay);
        g_flag = false;
    }

    LogText(__LINE__, pszFnName, TEXT("Sleeping for %d seconds at reverse half speed."), nClipLength/2);

    // the real position we end up at depends on the decoder and whether or not it can process full frame reverse or just key frames.
    Sleep(nClipLength/2);

    // test #4, go to the end of the 20 seconds captured and set our rate to reverse twice speed.  If we then play for 10 additional seconds then
    // we should be back to where we started our 20 seconds

    // set to the beginning of our clip
    _CHK(hr = pSourceFilterGraph->SetPositions(&llCurrentPositionAfterDelay, AM_SEEKING_AbsolutePositioning, &llStopPosition, AM_SEEKING_AbsolutePositioning), TEXT("SetPositions to the middle."));
    Sleep(300);
    _CHK(hr = pSourceFilterGraph->SetRate(-2), TEXT("Setting the rate to -1/2 normal."));
    Sleep(100);

    hr = pSourceFilterGraph->GetRate(&dRate);
    if(FAILED(hr) || dRate != -2)
    {
        LogError(__LINE__, pszFnName, TEXT("rate is incorrect %f != %f or pSourceFilterGraph->GetRate failed 0x%08x."), dRate, -2., hr);
        g_flag = false;
    }

    // check that the new current position is where we expect to be.
    _CHK(hr = pSourceFilterGraph->GetPositions(&llCurrentPositionAfterSet, &llStopPosition), TEXT("GetPositions before the delay."));
    if(abs((int) (llCurrentPositionAfterSet - llCurrentPositionAfterDelay)) > (llAllowableDelta * 2))
    {
        LogError(__LINE__, pszFnName, TEXT("New position 0x%08x != set position 0x%08x."), (DWORD) llCurrentPositionAfterSet, (DWORD) llCurrentPositionAfterDelay);
        g_flag = false;
    }

    LogText(__LINE__, pszFnName, TEXT("Sleeping for %d seconds at reverse 2x."), nClipLength/2);

    // the real position we end up at depends on the decoder and whether or not it can process full frame reverse or just key frames.
    Sleep(nClipLength/2);


    if(!g_flag)
        return TPR_FAIL;
    return TPR_PASS;
}

TESTPROCAPI Helper_Test_RewindToBeginning(CSourceFilterGraph * pSourceFilterGraph, GRAPH_TYPE GraphType)
{
    LPWSTR pszFnName = TEXT("Helper_Test_RewindToBeginning");
    DWORD result = TPR_PASS;
    HRESULT hr = S_OK;
    TEST_EVENT_DATA testEventData;
    double dRate;

    g_flag = true;

    if(GraphType == BOUND_TO_RECORDING)
        hr = InitializeTestEventData(&testEventData, DVRENGINE_EVENT_RECORDING_END_OF_STREAM);
    else
        hr = InitializeTestEventData(&testEventData, DVRENGINE_EVENT_BEGINNING_OF_PAUSE_BUFFER);

    pSourceFilterGraph->RegisterOnEventCallback(CallbackOnMediaEvent, &testEventData);

    LogText(__LINE__, pszFnName, TEXT("Sleeping for 20 seconds to get enough AV."));

    // we now have 20 seconds of video saved up between llCurrentPositionAtStart and llCurrentPositionAfterDelay, to play with.
    Sleep(20000);

    // when we hit the beginning our rate is pushed back to 1x, so lets keep hitting it
    for(int i = 0; i < 20; i++)
    {
        LogText(__LINE__, pszFnName, TEXT("Setting the rate to -1x."));
        hr = pSourceFilterGraph->SetRate(-1);

        // if we succeed in setting the rate, then get the rate and make sure we get the event.
        if(SUCCEEDED(hr))
        {
            hr = pSourceFilterGraph->GetRate(&dRate);
            // if we failed to retrieve the rate, then it's a failure.
            // if we're at an interation after #1 and the rate isn't 1 or -1, then it's a problem.
            // subsequent iterations may pass or fail setting the rate, or the rate may change back to 1 before we can retrieve it.
            if(FAILED(hr))
            {
                result = TPR_FAIL;
                LogError(__LINE__, pszFnName, TEXT("pSourceFilterGraph->GetRate failed %08x."), hr);
            }
            // if we're at iteration 1, and the rate isn't -1, then it's a failure, it should always be -1 to start with
            else if(i == 0 && dRate != -1)
            {
                result = TPR_FAIL;
                LogError(__LINE__, pszFnName, TEXT("rate is incorrect %f != %f."), dRate, -1.);
            }
            else if(i > 0 && dRate != -1 && dRate != 1)
            {
                result = TPR_FAIL;
                LogError(__LINE__, pszFnName, TEXT("Expected rate is 1 or -1, however retrieved rate is %f."), dRate);
            }

            // verify that we recieved the end of pause buffer notification.
            // DVRENGINE_EVENT_BEGINNING_OF_PAUSE_BUFFER
            if(WaitForTestEvent(&testEventData, 21000))
            {
                if(GraphType == BOUND_TO_RECORDING)
                    LogText(__LINE__, pszFnName, TEXT("Received DVRENGINE_EVENT_RECORDING_END_OF_STREAM from Source Filter Graph as expected."));
                else
                    LogText(__LINE__, pszFnName, TEXT("Received DVRENGINE_EVENT_BEGINNING_OF_PAUSE_BUFFER from Source Filter Graph as expected."));

            }
            else
            {
                result = TPR_FAIL;
                if(GraphType == BOUND_TO_RECORDING)
                    LogError(__LINE__, pszFnName, TEXT("Did NOT receive DVRENGINE_EVENT_RECORDING_END_OF_STREAM from Source Filter Graph."));
                else
                    LogError(__LINE__, pszFnName, TEXT("Did NOT receive DVRENGINE_EVENT_BEGINNING_OF_PAUSE_BUFFER from Source Filter Graph."));
            }
        }
        // setting the rate should never fail on iteration 0
        else if(i == 0)
        {
            result = TPR_FAIL;
            LogError(__LINE__, pszFnName, TEXT("Failed to set the initial state of the rate to -1 after buffering 20 seconds of video."));
        }

        // delay an arbitrary amount of time to let the system play some
        Sleep(rand()%5000);
    }

    if(!g_flag || result != TPR_PASS)
        return TPR_FAIL;
    return TPR_PASS;
}

TESTPROCAPI Helper_Test_SlowAtBeginning(CSourceFilterGraph * pSourceFilterGraph)
{
    LPWSTR pszFnName = TEXT("Helper_Test_SlowAtBeginning");
    DWORD result = TPR_PASS;
    HRESULT hr = S_OK;
    TEST_EVENT_DATA testEventData;
    double dRate;

    g_flag = true;

    hr = InitializeTestEventData(&testEventData, DVRENGINE_EVENT_BEGINNING_OF_PAUSE_BUFFER);

    pSourceFilterGraph->RegisterOnEventCallback(CallbackOnMediaEvent, &testEventData);

    // delay for a little bit to fill in the buffer
    Sleep(1000);

    // when we hit the beginning our rate is popped back to 1x, so lets keep pushing it.
    for(int i = 0; i < 10; i++)
    {
        _CHK(hr = pSourceFilterGraph->SetRate(0.001), TEXT("Setting the rate to .001 normal"));

        hr = pSourceFilterGraph->GetRate(&dRate);
        if(FAILED(hr) || dRate != .001)
        {
            LogError(__LINE__, pszFnName, TEXT("rate is incorrect %f != %f or pSourceFilterGraph->GetRate failed %08x."), dRate, .001, hr);
        }

        LogText(__LINE__, pszFnName, TEXT("Waiting for pause buffer overflow."));

        // verify that we recieved the end of pause buffer notification.
        // DVRENGINE_EVENT_BEGINNING_OF_PAUSE_BUFFER
        if(WaitForTestEvent(&testEventData, 21000))
        {
            LogText(__LINE__, pszFnName, TEXT("Received DVRENGINE_EVENT_BEGINNING_OF_PAUSE_BUFFER from Source Filter Graph as expected."));
        }
        else
        {
            result = TPR_FAIL;
            LogError(__LINE__, pszFnName, TEXT("Did NOT receive DVRENGINE_EVENT_BEGINNING_OF_PAUSE_BUFFER from Source Filter Graph."));
        }
    }

    if(!g_flag || result != TPR_PASS)
        return TPR_FAIL;
    return TPR_PASS;
}

TESTPROCAPI Helper_Test_PauseAtBeginning(CSourceFilterGraph * pSourceFilterGraph)
{
    LPWSTR pszFnName = TEXT("Helper_Test_PauseAtBeginning");
    DWORD result = TPR_PASS;
    HRESULT hr = S_OK;

    g_flag = true;

    // delay for a little bit to fill in the buffer
    Sleep(1000);

    _CHK(hr = pSourceFilterGraph->Pause(), TEXT("Pausing the source graph"));

    LogText(__LINE__, pszFnName, TEXT("Waiting for pause buffer overflow."));

    Sleep(10000);

    LogText(__LINE__, pszFnName, TEXT("Restarting after pause."));

    _CHK(hr = pSourceFilterGraph->Run(), TEXT("Running the source graph"));

    Sleep(2000);

    if(!g_flag || result != TPR_PASS)
        return TPR_FAIL;
    return TPR_PASS;
}


TESTPROCAPI Helper_Test_FastForwardToEnd(CSourceFilterGraph * pSourceFilterGraph, GRAPH_TYPE GraphType)
{
    LPWSTR pszFnName = TEXT("Helper_Test_FastForwardToEnd");
    DWORD result = TPR_PASS;
    HRESULT hr = S_OK;
    TEST_EVENT_DATA testEventData;
    double dRate;

    g_flag = true;

    // initially delay 20 seconds, which allows the system to get running and clear out any
    // initial events
    Sleep(20000);

    // if we're bound to a recording, then we rewind until we hit an END_OF_STREAM event, not a BEGINNING_OF_PAUSE_BUFFER event.
    if(GraphType == BOUND_TO_RECORDING)
        hr = InitializeTestEventData(&testEventData, DVRENGINE_EVENT_RECORDING_END_OF_STREAM);
    else
        hr = InitializeTestEventData(&testEventData, DVRENGINE_EVENT_BEGINNING_OF_PAUSE_BUFFER);

    pSourceFilterGraph->RegisterOnEventCallback(CallbackOnMediaEvent, &testEventData);


    _CHK(hr = pSourceFilterGraph->SetRate(-5), TEXT("Setting the rate to -5 normal"));

    hr = pSourceFilterGraph->GetRate(&dRate);
    if(FAILED(hr) || dRate != -5)
    {
        LogError(__LINE__, pszFnName, TEXT("rate is incorrect %f != %f or pSourceFilterGraph->GetRate failed %08x."), dRate, -5., hr);
    }

    // verify that we recieved the event indicating we reached the beginning of the recording
    if(WaitForTestEvent(&testEventData, 21000))
    {
        LogText(__LINE__, pszFnName, TEXT("Received event indicating it reached the beginning from Source Filter Graph as expected."));
    }
    else
    {
        result = TPR_FAIL;
        LogError(__LINE__, pszFnName, TEXT("Did NOT receive the event indicating it reached the beginning from Source Filter Graph."));
    }

    LogText(__LINE__, pszFnName, TEXT("Restarting after pause."));

    // when we hit the beginning our rate is popped back to 1x, now lets fast forward to the end, and keep pushing it
    // on the end
    for(int i=0; i < 20; i++)
    {
        LogText(__LINE__, pszFnName, TEXT("Setting the rate to 3.3x."));
        hr = pSourceFilterGraph->SetRate(3.3);

        // if we succeed in setting the rate, then get the rate and make sure we get the event.
        if(SUCCEEDED(hr))
        {
            hr = pSourceFilterGraph->GetRate(&dRate);
            // if we failed to retrieve the rate, then it's a failure.
            // if we're at an interation after #0 and the rate isn't 1 or 3.3, then it's a problem.
            // subsequent iterations may pass or fail setting the rate, or the rate may change back to 1 before we can retrieve it.
            if(FAILED(hr))
            {
                result = TPR_FAIL;
                LogError(__LINE__, pszFnName, TEXT("pSourceFilterGraph->GetRate failed %08x."), hr);
            }
            // if we're at iteration 1, and the rate isn't -1, then it's a failure, it should always be -1 to start with
            else if(i == 0 && dRate != 3.3)
            {
                result = TPR_FAIL;
                LogError(__LINE__, pszFnName, TEXT("rate is incorrect %f != %f."), dRate, 3.3);
            }
            else if(i > 0 && dRate != 3.3 && dRate != 1)
            {
                result = TPR_FAIL;
                LogError(__LINE__, pszFnName, TEXT("Expected rate is 3.3 or 1, however retrieved rate is %f."), dRate);
            }

            hr = InitializeTestEventData(&testEventData, DVRENGINE_EVENT_END_OF_PAUSE_BUFFER);

            // verify that we recieved the end of pause buffer notification.
            // DVRENGINE_EVENT_END_OF_PAUSE_BUFFER
            if(WaitForTestEvent(&testEventData, 30000))
            {
                LogText(__LINE__, pszFnName, TEXT("Received DVRENGINE_EVENT_END_OF_PAUSE_BUFFER from Source Filter Graph as expected."));
            }
            else
            {
                result = TPR_FAIL;
                LogError(__LINE__, pszFnName, TEXT("Did NOT receive DVRENGINE_EVENT_END_OF_PAUSE_BUFFER from Source Filter Graph."));
            }
        }
        // setting the rate should never fail on iteration 0
        else if(i == 0)
        {
            result = TPR_FAIL;
            LogError(__LINE__, pszFnName, TEXT("Failed to set the initial state of the rate to 3.3 after returning to the beginning of the pause buffer."));
        }

        pSourceFilterGraph->Pause();
        // delay an arbitrary amount of time to let the incoming stream get ahead of us a little
        Sleep((rand()%5000) + 500);
        pSourceFilterGraph->Run();
    }

    if(!g_flag || result != TPR_PASS)
        return TPR_FAIL;
    return TPR_PASS;
}

TESTPROCAPI Helper_Test_StopSinkDuringSource(CSinkFilterGraph * pSinkFilterGraph, CSourceFilterGraph * pSourceFilterGraph)
{
    LPWSTR pszFnName = TEXT("Helper_Test_StopSinkDuringSource");
    DWORD result = TPR_PASS;
    HRESULT hr = S_OK;
    TEST_EVENT_DATA testEventData;
    LONGLONG llNewPosition = 0;

    g_flag = true;

    hr = InitializeTestEventData(&testEventData, DVRENGINE_EVENT_END_OF_PAUSE_BUFFER);

    pSourceFilterGraph->RegisterOnEventCallback(CallbackOnMediaEvent, &testEventData);

    Sleep(5000);

    // sometimes we pop back to the beginning of the clip, sometimes we don't. expected result is the same.
    if(rand() % 2)
    {
        _CHK(pSourceFilterGraph->SetPositions(&llNewPosition, AM_SEEKING_AbsolutePositioning, NULL, AM_SEEKING_NoPositioning), TEXT("Go to beginning of clip"));
    }

    // stop the sink graph where we are, and see what the source graph does.
    _CHK(pSinkFilterGraph->Stop(), TEXT("Stop the sink filter graph"));

    Sleep(10000);

    if(!g_flag || result != TPR_PASS)
        return TPR_FAIL;
    return TPR_PASS;
}

TESTPROCAPI Helper_SetupFilterGraphsAndRunTest(LONGLONG llBufferSizeInMilliseconds, GRAPH_TYPE GraphType, TEST_SELECTION testSelection)
{
    LPWSTR pszFnName = TEXT("Helper_SetupFilterGraphsAndRunTest");
    DWORD result = TPR_FAIL;
    HRESULT hr = E_FAIL;
    CSinkFilterGraphEx mySinkFilterGraph;
    CSourceFilterGraph mySourceFilterGraph;
    LPOLESTR pszRecordingNameOrToken;
    mySinkFilterGraph.LogToConsole(TRUE);
    mySourceFilterGraph.LogToConsole(TRUE);

    mySourceFilterGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);
    mySinkFilterGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);

    LogText(__LINE__, pszFnName, TEXT("mySinkFilterGraph.Initialize()."));

    hr = mySinkFilterGraph.Initialize();

    if(SUCCEEDED(hr))
    {
        LogText(__LINE__, pszFnName, TEXT("succeeded."));

        if(g_pszSinkFilterGraphFileName == NULL)
        {
            LogText(__LINE__, pszFnName, TEXT("mySinkFilterGraph.SetupFilters."));

            hr = mySinkFilterGraph.SetupFilters();
        }
        else
        {
            LogText(__LINE__, pszFnName, TEXT("mySinkFilterGraph.LoadGraphFile."));
            hr = mySinkFilterGraph.LoadGraphFile(g_pszSinkFilterGraphFileName);
        }

        if(SUCCEEDED(hr))
        {
            LogText(__LINE__, pszFnName, TEXT("succeeded."));

            hr = mySinkFilterGraph.SetRecordingPath();

            if(SUCCEEDED(hr))
            {
                LogText(__LINE__, pszFnName, TEXT("mySinkFilterGraph.SetRecordingPath() succeeded."));

                hr = mySinkFilterGraph.GetRecordingPath(&pszRecordingNameOrToken);

                if(SUCCEEDED(hr))
                {
                    LogText(__LINE__, pszFnName, TEXT("mySinkFilterGraph.GetRecordingPath() succeeded and returned %s."), pszRecordingNameOrToken);

                    if(GraphType == BOUND_TO_LIVE_TEMPORARY || GraphType == BOUND_TO_LIVE_TEMPORARY_FULL_PAUSEBUFFER)
                    {
                        hr = mySinkFilterGraph.BeginTemporaryRecording(llBufferSizeInMilliseconds);
                    }
                    else if(GraphType == BOUND_TO_LIVE_PERMANENT || GraphType == BOUND_TO_RECORDING)
                    {
                        hr = mySinkFilterGraph.BeginPermanentRecording(0);
                    }

                    if(SUCCEEDED(hr))
                    {
                        hr = mySinkFilterGraph.Run();

                        if(SUCCEEDED(hr))
                        {
                            LogText(__LINE__, pszFnName, TEXT("mySinkFilterGraph.Run() succeeded."));

                            if(GraphType == BOUND_TO_LIVE_PERMANENT || GraphType == BOUND_TO_LIVE_TEMPORARY || GraphType == BOUND_TO_LIVE_TEMPORARY_FULL_PAUSEBUFFER)
                            {
                                LogText(__LINE__, pszFnName, TEXT("mySinkFilterGraph.GetBoundToLiveToken."));
                                hr = mySinkFilterGraph.GetBoundToLiveToken(&pszRecordingNameOrToken);
                            }
                            else
                            {
                                LogText(__LINE__, pszFnName, TEXT("mySinkFilterGraph.GetCurFile."));
                                hr = mySinkFilterGraph.GetCurFile(&pszRecordingNameOrToken, NULL);
                            }

                            if(SUCCEEDED(hr))
                            {
                                LogText(__LINE__, pszFnName, TEXT("succeeded."));
                                hr = mySourceFilterGraph.Initialize();

                                if(SUCCEEDED(hr))
                                {
                                    LogText(__LINE__, pszFnName, TEXT("mySourceFilterGraph.Initialize() succeeded."));

                                    if(GraphType == BOUND_TO_RECORDING)
                                        Sleep(5 * 1000);
                                    // if we're running with a temporary recording and we want a full pause buffer, allow the temporary buffer to fill up
                                    else if(GraphType == BOUND_TO_LIVE_TEMPORARY_FULL_PAUSEBUFFER)
                                    {
                                        LogText(__LINE__, pszFnName, TEXT("Sleeping for %d seconds to allow the pause buffer to fill."), llBufferSizeInMilliseconds/1000);
                                        Sleep((DWORD) llBufferSizeInMilliseconds);
                                    }

                                    hr = mySourceFilterGraph.SetupFilters(pszRecordingNameOrToken);

                                    if(SUCCEEDED(hr))
                                    {
                                        LogText(__LINE__, pszFnName, TEXT("mySourceFilterGraph.SetupFilters() succeeded."));

                                        hr = mySourceFilterGraph.Run();

                                        if(SUCCEEDED(hr))
                                        {

                                            // if we're bound to live and let the temporary buffer fill, we may have fallen off of live a little,
                                            // so get back to live. This can happen if we're using a file source that may not source at exactly
                                            // the same rate as the renderer.
                                            if(GraphType == BOUND_TO_LIVE_TEMPORARY_FULL_PAUSEBUFFER)
                                            {
                                                __int64 /*LONGLONG*/ min = 0;
                                                __int64 /*LONGLONG*/ max = 0;
                                            
                                                _CHK(mySourceFilterGraph.GetAvailable(&min, &max), TEXT("Get available seek"));
                                                _CHK(mySourceFilterGraph.SetPositions(&max, AM_SEEKING_AbsolutePositioning, NULL, NULL), TEXT("SetPositions to the live."));
                                                Sleep(1000);
                                            }

                                            LogText(__LINE__, pszFnName, TEXT("mySourceFilterGraph.Run() succeeded."));

                                            switch(testSelection)
                                            {
// requires the iamstreamselect interfaces
#if 0
                                                case TEST_SELECTION_IAMStreamSelect_COUNT:
                                                case TEST_SELECTION_IAMStreamSelect_COUNT_NEGATIVE:
                                                case TEST_SELECTION_IAMStreamSelect_ENABLE:
                                                case TEST_SELECTION_IAMStreamSelect_ENABLE_NEGATIVE:
                                                case TEST_SELECTION_IAMStreamSelect_INFO:
                                                case TEST_SELECTION_IAMStreamSelect_INFO_NEGATIVE:
                                                    result = Helper_Test_IAMStreamSelect(&mySourceFilterGraph, testSelection);
                                                    break;
#endif
                                                case TEST_SELECTION_SourceGraph_RewindStopAndRun:
                                                case TEST_SELECTION_SourceGraph_StopAndRun:
                                                case TEST_SELECTION_SourceGraph_PauseAndRun:
                                                    result = Helper_Test_SourceFilterGraph_StopOrPause_And_Run(&mySourceFilterGraph, testSelection);
                                                    break;
                                                case TEST_SELECTION_BoundToLiveBeginMultiplePermanentRecording:
                                                case TEST_SELECTION_BoundToRecordingBeginMultiplePermanentRecording:
                                                    result = Helper_Test_BeginMultipleRecordings(&mySinkFilterGraph, &mySourceFilterGraph, llBufferSizeInMilliseconds, testSelection);
                                                    break;
                                                case TEST_SELECTION_Position_SeekBackward:
                                                    result = Helper_Test_GetSetPositions(&mySourceFilterGraph, testSelection);
                                                    break;
                                                case TEST_SELECTION_Position_SeekForward:
                                                    result = Helper_Test_GetSetPositions(&mySourceFilterGraph, testSelection);
                                                    break;
                                                case TEST_SELECTION_RateChangedEvent:
                                                    result = Helper_Test_RateChangedEvent(&mySourceFilterGraph);
                                                    break;
                                                case TEST_SELECTION_RateAndPositioning:
                                                    result = Helper_Test_RateAndPositioning(&mySourceFilterGraph, GraphType);
                                                    break;
                                                case TEST_SELECTION_RewindToBeginning:
                                                    result = Helper_Test_RewindToBeginning(&mySourceFilterGraph, GraphType);
                                                    break;
                                                case TEST_SELECTION_SlowAtBeginning:
                                                    result = Helper_Test_SlowAtBeginning(&mySourceFilterGraph);
                                                    break;
                                                case TEST_SELECTION_PauseAtBeginning:
                                                    result = Helper_Test_PauseAtBeginning(&mySourceFilterGraph);
                                                    break;
                                                case TEST_SELECTION_FastForwardToEnd:
                                                    result = Helper_Test_FastForwardToEnd(&mySourceFilterGraph, GraphType);
                                                    break;
                                                case TEST_SELECTION_StopSinkDuringSource:
                                                    result = Helper_Test_StopSinkDuringSource(&mySinkFilterGraph, &mySourceFilterGraph);
                                                    break;
                                                default:
                                                    break;
                                            }

                                            hr = mySourceFilterGraph.Stop();

                                            if(SUCCEEDED(hr))
                                            {
                                                LogText(__LINE__, pszFnName, TEXT("mySourceFilterGraph.Stop() succeeded."));
                                            }
                                            else
                                            {
                                                LogError(__LINE__, pszFnName, TEXT("mySourceFilterGraph.Stop() failed %08x."), hr);
                                            }
                                        }
                                        else
                                        {
                                            LogError(__LINE__, pszFnName, TEXT("mySourceFilterGraph.Run() failed %08x."), hr);
                                        }
                                    }
                                    else
                                    {
                                        LogError(__LINE__, pszFnName, TEXT("mySourceFilterGraph.SetupFilters() failed %08x."), hr);
                                    }

                                    mySourceFilterGraph.UnInitialize();
                                }
                                else
                                {
                                    LogError(__LINE__, pszFnName, TEXT("mySourceFilterGraph.Initialize() failed %08x."), hr);
                                }
                            }
                            else
                            {
                                LogError(__LINE__, pszFnName, TEXT("failed %08x."), hr);
                            }

                            hr = mySinkFilterGraph.Stop();

                            if(SUCCEEDED(hr))
                            {
                                LogText(__LINE__, pszFnName, TEXT("mySinkFilterGraph.Stop() succeeded."));
                            }
                            else
                            {
                                LogError(__LINE__, pszFnName, TEXT("mySinkFilterGraph.Stop() failed %08x."), hr);
                            }
                        }
                        else
                        {
                            LogError(__LINE__, pszFnName, TEXT("mySinkFilterGraph.Run() failed %08x."), hr);
                        }
                    }
                    else
                    {
                        LogError(__LINE__, pszFnName, TEXT("mySinkFilterGraph.Begin*Recording() failed %08x."), hr);
                    }
                }
                else
                {
                    LogError(__LINE__, pszFnName, TEXT("mySinkFilterGraph.GetRecordingPath() failed %08x."), hr);
                }
            }
            else
            {
                LogError(__LINE__, pszFnName, TEXT("mySinkFilterGraph.SetRecordingPath() failed %08x."), hr);
            }
        }
        else
        {
            LogError(__LINE__, pszFnName, TEXT("failed %08x."), hr);
        }

        mySinkFilterGraph.UnInitialize();
    }
    else
    {
        LogError(__LINE__, pszFnName, TEXT("failed %08x."), hr);
    }

    if(FAILED(hr))
    {
        result = TPR_FAIL;
    }

    return result;
}

// requires the IAMStreamSelect interfaces
#if 0
TESTPROCAPI Test_BoundToLive_IAMStreamSelect_Enable(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_TEMPORARY, TEST_SELECTION_IAMStreamSelect_ENABLE);
}

TESTPROCAPI Test_BoundToLivePerm_IAMStreamSelect_Enable(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_PERMANENT, TEST_SELECTION_IAMStreamSelect_ENABLE);
}

TESTPROCAPI Test_BoundToRecording_IAMStreamSelect_Enable(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_RECORDING, TEST_SELECTION_IAMStreamSelect_ENABLE);
}

TESTPROCAPI Test_BoundToLive_IAMStreamSelect_Enable_Negative(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_TEMPORARY, TEST_SELECTION_IAMStreamSelect_ENABLE_NEGATIVE);
}

TESTPROCAPI Test_BoundToLivePerm_IAMStreamSelect_Enable_Negative(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_PERMANENT, TEST_SELECTION_IAMStreamSelect_ENABLE_NEGATIVE);
}

TESTPROCAPI Test_BoundToRecording_IAMStreamSelect_Enable_Negative(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_RECORDING, TEST_SELECTION_IAMStreamSelect_ENABLE_NEGATIVE);
}

TESTPROCAPI Test_BoundToLive_IAMStreamSelect_Count(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_TEMPORARY, TEST_SELECTION_IAMStreamSelect_COUNT);
}

TESTPROCAPI Test_BoundToLivePerm_IAMStreamSelect_Count(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_PERMANENT, TEST_SELECTION_IAMStreamSelect_COUNT);
}

TESTPROCAPI Test_BoundToRecording_IAMStreamSelect_Count(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_RECORDING, TEST_SELECTION_IAMStreamSelect_COUNT);
}

TESTPROCAPI Test_BoundToLive_IAMStreamSelect_Count_Negative(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_TEMPORARY, TEST_SELECTION_IAMStreamSelect_COUNT_NEGATIVE);
}

TESTPROCAPI Test_BoundToLivePerm_IAMStreamSelect_Count_Negative(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_PERMANENT, TEST_SELECTION_IAMStreamSelect_COUNT_NEGATIVE);
}

TESTPROCAPI Test_BoundToRecording_IAMStreamSelect_Count_Negative(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_RECORDING, TEST_SELECTION_IAMStreamSelect_COUNT_NEGATIVE);
}

TESTPROCAPI Test_BoundToLive_IAMStreamSelect_Info(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_TEMPORARY, TEST_SELECTION_IAMStreamSelect_INFO);
}

TESTPROCAPI Test_BoundToLivePerm_IAMStreamSelect_Info(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_PERMANENT, TEST_SELECTION_IAMStreamSelect_INFO);
}

TESTPROCAPI Test_BoundToRecording_IAMStreamSelect_Info(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_RECORDING, TEST_SELECTION_IAMStreamSelect_INFO);
}

TESTPROCAPI Test_BoundToLive_IAMStreamSelect_Info_Negative(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_TEMPORARY, TEST_SELECTION_IAMStreamSelect_INFO_NEGATIVE);
}

TESTPROCAPI Test_BoundToLivePerm_IAMStreamSelect_Info_Negative(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_PERMANENT, TEST_SELECTION_IAMStreamSelect_INFO_NEGATIVE);
}

TESTPROCAPI Test_BoundToRecording_IAMStreamSelect_Info_Negative(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_RECORDING, TEST_SELECTION_IAMStreamSelect_INFO_NEGATIVE);
}
#endif

TESTPROCAPI Test_BoundToLive_SourceGraphStopAndRun(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_TEMPORARY, TEST_SELECTION_SourceGraph_StopAndRun);
}

TESTPROCAPI Test_BoundToLiveFullPB_SourceGraphStopAndRun(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_TEMPORARY_FULL_PAUSEBUFFER, TEST_SELECTION_SourceGraph_StopAndRun);
}

TESTPROCAPI Test_BoundToLivePerm_SourceGraphStopAndRun(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_PERMANENT, TEST_SELECTION_SourceGraph_StopAndRun);
}

TESTPROCAPI Test_BoundToRecording_SourceGraphStopAndRun(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_RECORDING, TEST_SELECTION_SourceGraph_StopAndRun);
}

TESTPROCAPI Test_BoundToLive_SourceGraphRewindStopAndRun(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_TEMPORARY, TEST_SELECTION_SourceGraph_RewindStopAndRun);
}

TESTPROCAPI Test_BoundToLiveFullPB_SourceGraphRewindStopAndRun(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_TEMPORARY_FULL_PAUSEBUFFER, TEST_SELECTION_SourceGraph_RewindStopAndRun);
}

TESTPROCAPI Test_BoundToLivePerm_SourceGraphRewindStopAndRun(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_PERMANENT, TEST_SELECTION_SourceGraph_RewindStopAndRun);
}

TESTPROCAPI Test_BoundToRecording_SourceGraphRewindStopAndRun(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_RECORDING, TEST_SELECTION_SourceGraph_RewindStopAndRun);
}


TESTPROCAPI Test_BoundToLive_SourceGraphPauseAndRun(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_TEMPORARY, TEST_SELECTION_SourceGraph_PauseAndRun);
}

TESTPROCAPI Test_BoundToLiveFullPB_SourceGraphPauseAndRun(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_TEMPORARY_FULL_PAUSEBUFFER, TEST_SELECTION_SourceGraph_PauseAndRun);
}

TESTPROCAPI Test_BoundToLivePerm_SourceGraphPauseAndRun(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_PERMANENT, TEST_SELECTION_SourceGraph_PauseAndRun);
}

TESTPROCAPI Test_BoundToRecording_SourceGraphPauseAndRun(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_RECORDING, TEST_SELECTION_SourceGraph_PauseAndRun);
}

TESTPROCAPI Test_BoundToLive_BeginMultiplePermanentRecording(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_PERMANENT, TEST_SELECTION_BoundToLiveBeginMultiplePermanentRecording);
}

TESTPROCAPI Test_BoundToRecording_BeginMultiplePermanentRecording(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_RECORDING, TEST_SELECTION_BoundToRecordingBeginMultiplePermanentRecording);
}

TESTPROCAPI Test_BoundToLive_SeekBackward(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_TEMPORARY, TEST_SELECTION_Position_SeekBackward);
}

TESTPROCAPI Test_BoundToLiveFullPB_SeekBackward(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_TEMPORARY_FULL_PAUSEBUFFER, TEST_SELECTION_Position_SeekBackward);
}

TESTPROCAPI Test_BoundToLivePerm_SeekBackward(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_PERMANENT, TEST_SELECTION_Position_SeekBackward);
}

TESTPROCAPI Test_BoundToRecording_SeekBackward(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_RECORDING, TEST_SELECTION_Position_SeekBackward);
}

TESTPROCAPI Test_BoundToLive_SeekForward(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_TEMPORARY, TEST_SELECTION_Position_SeekForward);
}

TESTPROCAPI Test_BoundToLiveFullPB_SeekForward(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_TEMPORARY_FULL_PAUSEBUFFER, TEST_SELECTION_Position_SeekForward);
}

TESTPROCAPI Test_BoundToLivePerm_SeekForward(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_PERMANENT, TEST_SELECTION_Position_SeekForward);
}

TESTPROCAPI Test_BoundToRecording_SeekForward(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_RECORDING, TEST_SELECTION_Position_SeekForward);
}

TESTPROCAPI Test_BoundToLive_RateChangedEvent(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_TEMPORARY, TEST_SELECTION_RateChangedEvent);
}

TESTPROCAPI Test_BoundToLiveFullPB_RateChangedEvent(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_TEMPORARY_FULL_PAUSEBUFFER, TEST_SELECTION_RateChangedEvent);
}

TESTPROCAPI Test_BoundToLivePerm_RateChangedEvent(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_PERMANENT, TEST_SELECTION_RateChangedEvent);
}

TESTPROCAPI Test_BoundToRecording_RateChangedEvent(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_RECORDING, TEST_SELECTION_RateChangedEvent);
}

TESTPROCAPI Test_BoundToLive_RateAndPositioning(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // The bound to live temporary recording buffer needs to be atleast 4 minutes for this test. Why? because we're running for more than two minutes,
    // but using the video that was captured in the first 20 seconds of the engine running. Therefore when we're at the end of the test two minutes in, we 
    // need the temporary buffer from two minutes ago.
    return Helper_SetupFilterGraphsAndRunTest(240000, BOUND_TO_LIVE_TEMPORARY, TEST_SELECTION_RateAndPositioning);
}

TESTPROCAPI Test_BoundToLiveFullPB_RateAndPositioning(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // The bound to live temporary recording buffer needs to be atleast 4 minutes for this test. Why? because we're running for more than two minutes,
    // but using the video that was captured in the first 20 seconds of the engine running. Therefore when we're at the end of the test two minutes in, we 
    // need the temporary buffer from two minutes ago.
    return Helper_SetupFilterGraphsAndRunTest(240000, BOUND_TO_LIVE_TEMPORARY_FULL_PAUSEBUFFER, TEST_SELECTION_RateAndPositioning);
}

TESTPROCAPI Test_BoundToLivePerm_RateAndPositioning(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_LIVE_PERMANENT, TEST_SELECTION_RateAndPositioning);
}

TESTPROCAPI Test_BoundToRecording_RateAndPositioning(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(g_llDefaultRecordingBufferSizeInMilliseconds, BOUND_TO_RECORDING, TEST_SELECTION_RateAndPositioning);
}

TESTPROCAPI Test_BoundToLive_RewindToBeginning(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(60000, BOUND_TO_LIVE_TEMPORARY, TEST_SELECTION_RewindToBeginning);
}

TESTPROCAPI Test_BoundToLiveFullPB_RewindToBeginning(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(60000, BOUND_TO_LIVE_TEMPORARY_FULL_PAUSEBUFFER, TEST_SELECTION_RewindToBeginning);
}

TESTPROCAPI Test_BoundToLivePerm_RewindToBeginning(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(60000, BOUND_TO_LIVE_PERMANENT, TEST_SELECTION_RewindToBeginning);
}

TESTPROCAPI Test_BoundToRecording_RewindToBeginning(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(60000, BOUND_TO_RECORDING, TEST_SELECTION_RewindToBeginning);
}

TESTPROCAPI Test_BoundToLive_PauseAtBeginning(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(10000, BOUND_TO_LIVE_TEMPORARY, TEST_SELECTION_PauseAtBeginning);
}

TESTPROCAPI Test_BoundToLiveFullPB_PauseAtBeginning(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(10000, BOUND_TO_LIVE_TEMPORARY_FULL_PAUSEBUFFER, TEST_SELECTION_PauseAtBeginning);
}

TESTPROCAPI Test_BoundToLivePerm_PauseAtBeginning(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(10000, BOUND_TO_LIVE_PERMANENT, TEST_SELECTION_PauseAtBeginning);
}

TESTPROCAPI Test_BoundToRecording_PauseAtBeginning(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(10000, BOUND_TO_RECORDING, TEST_SELECTION_PauseAtBeginning);
}

TESTPROCAPI Test_BoundToLive_SlowAtBeginning(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(10000, BOUND_TO_LIVE_TEMPORARY, TEST_SELECTION_SlowAtBeginning);
}

TESTPROCAPI Test_BoundToLiveFullPB_SlowAtBeginning(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(10000, BOUND_TO_LIVE_TEMPORARY_FULL_PAUSEBUFFER, TEST_SELECTION_SlowAtBeginning);
}

TESTPROCAPI Test_BoundToLive_FastForwardToEnd(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(30000, BOUND_TO_LIVE_TEMPORARY, TEST_SELECTION_FastForwardToEnd);
}

TESTPROCAPI Test_BoundToLiveFullPB_FastForwardToEnd(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(30000, BOUND_TO_LIVE_TEMPORARY_FULL_PAUSEBUFFER, TEST_SELECTION_FastForwardToEnd);
}

TESTPROCAPI Test_BoundToLivePerm_FastForwardToEnd(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(10000, BOUND_TO_LIVE_PERMANENT, TEST_SELECTION_FastForwardToEnd);
}

TESTPROCAPI Test_BoundToRecording_FastForwardToEnd(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(10000, BOUND_TO_RECORDING, TEST_SELECTION_FastForwardToEnd);
}

TESTPROCAPI Test_BoundToLive_StopSinkDuringSource(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(10000, BOUND_TO_LIVE_TEMPORARY, TEST_SELECTION_StopSinkDuringSource);
}

TESTPROCAPI Test_BoundToLiveFullPB_StopSinkDuringSource(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(10000, BOUND_TO_LIVE_TEMPORARY_FULL_PAUSEBUFFER, TEST_SELECTION_StopSinkDuringSource);
}

TESTPROCAPI Test_BoundToLivePerm_StopSinkDuringSource(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(10000, BOUND_TO_LIVE_PERMANENT, TEST_SELECTION_StopSinkDuringSource);
}

TESTPROCAPI Test_BoundToRecording_StopSinkDuringSource(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return Helper_SetupFilterGraphsAndRunTest(10000, BOUND_TO_RECORDING, TEST_SELECTION_StopSinkDuringSource);
}

TESTPROCAPI Helper_GeneratePermRecording(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    const TCHAR * pszFnName = TEXT("Helper_GeneratePermRecording");

    LONGLONG TempBufferSize = 0;    // set 0 second buffer size to convert so always start a new recording file;
    int iTime = 10 ;    // Let it run for 20 seconds so any temp recording will appear.
    int iLoop = 5;        // Check file size 5 times

    CSinkFilterGraphEx mySinkFilterGraph;
    LPOLESTR pszFileName = NULL;
    LPOLESTR pszFileName_01 = NULL;
    WIN32_FIND_DATAW wData;
    HANDLE hFile = NULL;
    TCHAR tszDefaultRecordingPath[MAX_PATH];

    HRESULT hr = E_FAIL;
    g_flag = true;

    mySinkFilterGraph.LogToConsole(TRUE);

    mySinkFilterGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);

    LogText(__LINE__, pszFnName, TEXT("The iTime is %d."), iTime);
    pszFileName_01 = new WCHAR[120];
 
    if(pszFileName_01)
    {
        hr = mySinkFilterGraph.Initialize();

        if(SUCCEEDED(hr))
        {
            // Adding Filters
            _CHK(mySinkFilterGraph.SetupFilters(), TEXT("Setup Sink Graph"));

            mySinkFilterGraph.GetDefaultRecordingPath(tszDefaultRecordingPath);

            // Call BegineTemp Rec
            CreateDirectoryW(tszDefaultRecordingPath, NULL);
            LogText(TEXT("Set recording Path as %s."), tszDefaultRecordingPath);
            _CHK(mySinkFilterGraph.SetRecordingPath(tszDefaultRecordingPath), TEXT("SetRecording Path"));
            // Begine Perm Rec
            LogText(__LINE__, pszFnName, TEXT("Call BeginPermRec now."));
            // initialize the temp buffer size
            _CHK(mySinkFilterGraph.BeginTemporaryRecording(60000), TEXT("BeginTemporaryRecording"));
            _CHK(mySinkFilterGraph.BeginPermanentRecording(TempBufferSize), TEXT("BeginPermRecording"));
        
            _CHK(mySinkFilterGraph.Run(), TEXT("Run Graph"));
            // Wait 10 seconds for file creation
            LogText(__LINE__, pszFnName, TEXT("Wait %d seconds."), iTime);
            Sleep(iTime * 1000);    // Sleep for one Temp File filled. 
            // Get Cur File
            _CHK(mySinkFilterGraph.GetCurFile(&pszFileName, NULL), TEXT("GetCurFile"));
        
            LogText(__LINE__, pszFnName, TEXT("The gotten file name is %s."), pszFileName);
            _tcscpy(pszFileName_01, pszFileName);
            _tcscat(pszFileName_01, TEXT("*.dvr-dat"));    // The 1st Dat file;
            LogText(TEXT("The Perm file name is %s."), pszFileName_01);

            hFile = FindFirstFileW(pszFileName_01, &wData);

            if(hFile == INVALID_HANDLE_VALUE)
            {
                LogError(__LINE__, pszFnName, TEXT("Did not find the filename gotten by pFileSink->GetCurFile."));
                g_flag = false;
            }
            else
            {
                LogText(__LINE__, pszFnName, TEXT("Find the filename gotten by pFileSink->GetCurFile; succeed."));
            }

            FindClose(hFile);

            // start a temporary recording now to free the permanent recording just completed
            _CHK(mySinkFilterGraph.BeginTemporaryRecording(60000), TEXT("BeginTemporaryRecording"));

            // now that the sink graph is stopped, make sure we can delete the recording we just finished
            _CHK(mySinkFilterGraph.DeleteRecording(pszFileName), TEXT("DeleteRecording"));


            // the perm file was just put into the temporary buffer instead of immediatly deleted, so now we wait
            // until the temporary buffer no longer contains that recording. Once that happens the file should actually be deleted.
            Sleep(120000);

            hFile = FindFirstFileW(pszFileName_01, &wData);

            if(hFile == INVALID_HANDLE_VALUE)
            {
                LogText(__LINE__, pszFnName, TEXT("Did not find the deleted permanent recording, as expected."));
            }
            else
            {
                LogError(__LINE__, pszFnName, TEXT("Found a permanent recording that was just deleted; error."));
                g_flag = false;
            }

            FindClose(hFile);

            _CHK(mySinkFilterGraph.Stop(), TEXT("Sink graph stop"));

            CoTaskMemFree(pszFileName);
        }

        mySinkFilterGraph.UnInitialize();

        delete []pszFileName_01;
    }
    else
    {
        LogError(__LINE__, pszFnName, TEXT("Failed to allocate a string to store the file name."));
        g_flag = false;
    }

    if(FAILED(hr) || (g_flag == false))
    {
        return TPR_FAIL;
    }
    else
    {
        return TPR_PASS;
    }
}

TESTPROCAPI Helper_GeneratePermRecording2(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    const TCHAR * pszFnName = TEXT("Helper_GeneratePermRecording");

    LONGLONG TempBufferSize = 0;    // set 0 second buffer size to convert so always start a new recording file;
    int iTime = 10 ;    // Let it run for 20 seconds so any temp recording will appear.
    int iLoop = 5;        // Check file size 5 times

    CSinkFilterGraphEx mySinkFilterGraph;
    LPOLESTR pszFileName_01 = NULL;
    LPOLESTR pszFileName = NULL;
    WIN32_FIND_DATAW wData;
    HANDLE hFile = NULL;
    TCHAR tszDefaultRecordingPath[MAX_PATH];

    HRESULT hr = E_FAIL;
    g_flag = true;

    mySinkFilterGraph.LogToConsole(TRUE);

    mySinkFilterGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);

    LogText(__LINE__, pszFnName, TEXT("The iTime is %d."), iTime);
    pszFileName_01 = new WCHAR[120];
 
    if(pszFileName_01)
    {
        hr = mySinkFilterGraph.Initialize();

        if(SUCCEEDED(hr))
        {
            // Adding Filters
            _CHK(mySinkFilterGraph.SetupFilters(), TEXT("Setup Sink Graph"));

            mySinkFilterGraph.GetDefaultRecordingPath(tszDefaultRecordingPath);

            // Call BegineTemp Rec
            CreateDirectoryW(tszDefaultRecordingPath, NULL);
            LogText(TEXT("Set recording Path as %s."), tszDefaultRecordingPath);
            _CHK(mySinkFilterGraph.SetRecordingPath(tszDefaultRecordingPath), TEXT("SetRecording Path"));
            // Begine Perm Rec
            LogText(__LINE__, pszFnName, TEXT("Call BeginPermRec now."));
            _CHK(mySinkFilterGraph.BeginTemporaryRecording(60000), TEXT("BeginTemporaryRecording"));
            _CHK(mySinkFilterGraph.BeginPermanentRecording(TempBufferSize), TEXT("BeginPermRecording"));
        
            _CHK(mySinkFilterGraph.Run(), TEXT("Run Graph"));
            // Wait 10 seconds for file creation
            LogText(__LINE__, pszFnName, TEXT("Wait %d seconds."), iTime);
            Sleep(iTime * 1000);    // Sleep for one Temp File filled. 
            // Get Cur File
            _CHK(mySinkFilterGraph.GetCurFile(&pszFileName, NULL), TEXT("GetCurFile"));
        
            LogText(__LINE__, pszFnName, TEXT("The gotten file name is %s."), pszFileName);
            _tcscpy(pszFileName_01, pszFileName);
            _tcscat(pszFileName_01, TEXT("*.dvr-dat"));    // The 1st Dat file;
            LogText(TEXT("The Perm file name is %s."), pszFileName_01);

            hFile = FindFirstFileW(pszFileName_01, &wData);

            if(hFile == INVALID_HANDLE_VALUE)
            {
                LogError(__LINE__, pszFnName, TEXT("Did not find the filename gotten by pFileSink->GetCurFile."));
                g_flag = false;
            }
            else
            {
                LogText(__LINE__, pszFnName, TEXT("Find the filename gotten by pFileSink->GetCurFile; succeed."));
            }

            FindClose(hFile);

            // start a temporary recording now to free the permanent recording just completed
            _CHK(mySinkFilterGraph.BeginTemporaryRecording(60000), TEXT("BeginTemporaryRecording"));

            // the perm file was just put into the temporary buffer instead of immediatly deleted, so now we wait
            // until the temporary buffer no longer contains that recording. Once that happens the file should actually be deleted.
            Sleep(120000);

            // now that the sink graph is stopped, make sure we can delete the recording we just finished
            _CHK(mySinkFilterGraph.DeleteRecording(pszFileName), TEXT("DeleteRecording"));

            hFile = FindFirstFileW(pszFileName_01, &wData);

            if(hFile == INVALID_HANDLE_VALUE)
            {
                LogText(__LINE__, pszFnName, TEXT("Did not find the deleted permanent recording, as expected."));
            }
            else
            {
                LogError(__LINE__, pszFnName, TEXT("Found a permanent recording that was just deleted; error."));
                g_flag = false;
            }

            FindClose(hFile);


            CoTaskMemFree(pszFileName);

            _CHK(mySinkFilterGraph.Stop(), TEXT("Sink graph stop"));
        }

        mySinkFilterGraph.UnInitialize();

        delete []pszFileName_01;
    }
    else
    {
        LogError(__LINE__, pszFnName, TEXT("Failed to allocate a string to store the file name."));
        g_flag = false;
    }

    if(FAILED(hr) || (g_flag == false))
    {
        return TPR_FAIL;
    }
    else
    {
        return TPR_PASS;
    }
}


TESTPROCAPI PreviousPlaybackDuringRecord(BOOL bTemporary)
{
    const TCHAR * pszFnName = TEXT("PreviousPlaybackDuringRecord");
    CSinkFilterGraph mySinkGraph;
    CSourceFilterGraph mySourceGraph;

    g_flag = true;//set default as true;

    // TEST 1, start a record, then at a later point start a playback.
    mySinkGraph.LogToConsole(TRUE);
    mySourceGraph.LogToConsole(TRUE);

    mySourceGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);
    mySinkGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);

    _CHK(mySinkGraph.Initialize(), TEXT("Initialize Sink Filter graph"));
    _CHK(mySinkGraph.SetupFilters(), TEXT("Setting up sink graph"));
    _CHK(mySinkGraph.SetRecordingPath(), TEXT("SetRecording Path"));

    if(bTemporary)
    {
        _CHK(mySinkGraph.BeginTemporaryRecording(10000), TEXT("Sink BeginTemporaryRecording"));
    }
    else
    {
        _CHK(mySinkGraph.BeginPermanentRecording(0), TEXT("Sink BeginPermanentRecording"));
    }
    _CHK(mySinkGraph.Run(), TEXT("Run Sink"));

    _CHK(mySourceGraph.Initialize(), TEXT("Initialize Source FilterGraph"));
    if(SUCCEEDED(mySourceGraph.SetupFilters()))
    {
        _CHK(mySourceGraph.Run(), TEXT("Run Source"));
        Sleep(5000);
        _CHK(mySourceGraph.Stop(), TEXT("Stop source Graph"));
        _CHK(mySinkGraph.Stop(), TEXT("Stop sink Graph"));
        mySourceGraph.UnInitialize();
        mySinkGraph.UnInitialize();

        // test 2, start both a playback and record simultaniously
        mySinkGraph.LogToConsole(TRUE);
        mySourceGraph.LogToConsole(TRUE);

        _CHK(mySinkGraph.Initialize(), TEXT("Initialize Sink Filter graph"));
        _CHK(mySourceGraph.Initialize(), TEXT("Initialize Source FilterGraph"));
        _CHK(mySinkGraph.SetupFilters(), TEXT("Setting up sink graph"));
        _CHK(mySourceGraph.SetupFilters(), TEXT("Setup Perm Rec Playback graph"));
        _CHK(mySinkGraph.SetRecordingPath(), TEXT("SetRecording Path"));

        if(bTemporary)
        {
            _CHK(mySinkGraph.BeginTemporaryRecording(10000), TEXT("Sink BeginTemporaryRecording"));
        }
        else
        {
            _CHK(mySinkGraph.BeginPermanentRecording(0), TEXT("Sink BeginPermanentRecording"));
        }
        _CHK(mySinkGraph.Run(), TEXT("Run Sink"));
        _CHK(mySourceGraph.Run(), TEXT("Run Source"));
        Sleep(5000);
        _CHK(mySourceGraph.Stop(), TEXT("Stop source Graph"));
    }
    else
    {
        LogError(__LINE__, pszFnName, TEXT("Failed to setup the source graph; error. This test uses a pre recorded file, is that file missing?"));
        g_flag = false;        
    }


    _CHK(mySinkGraph.Stop(), TEXT("Stop sink Graph"));
    mySourceGraph.UnInitialize();
    mySinkGraph.UnInitialize();

    if(g_flag == false)
        return TPR_FAIL;
    return TPR_PASS;
}


TESTPROCAPI PreviousPlaybackDuringTemporaryRecord(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return(PreviousPlaybackDuringRecord(TRUE));
}

TESTPROCAPI PreviousPlaybackDuringPermanentRecord(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    return(PreviousPlaybackDuringRecord(FALSE));
}


TESTPROCAPI TemporaryBuffersizeTest(BOOL bTemporary)
{
    const TCHAR * pszFnName = TEXT("PreviousPlaybackDuringRecord");
    CSinkFilterGraph mySinkGraph;
    CSourceFilterGraph mySourceGraph;

    g_flag = true;//set default as true;

    // TEST 1, start a record, then at a later point start a playback.
    mySinkGraph.LogToConsole(TRUE);
    mySourceGraph.LogToConsole(TRUE);

    mySourceGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);
    mySinkGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);

    _CHK(mySinkGraph.Initialize(), TEXT("Initialize Sink Filter graph"));
    _CHK(mySinkGraph.SetupFilters(), TEXT("Setting up sink graph"));
    _CHK(mySinkGraph.SetRecordingPath(), TEXT("SetRecording Path"));

    if(bTemporary)
    {
        _CHK(mySinkGraph.BeginTemporaryRecording(10000), TEXT("Sink BeginTemporaryRecording"));
    }
    else
    {
        _CHK(mySinkGraph.BeginPermanentRecording(0), TEXT("Sink BeginPermanentRecording"));
    }
    _CHK(mySinkGraph.Run(), TEXT("Run Sink"));
    _CHK(mySourceGraph.Initialize(), TEXT("Initialize Source FilterGraph"));
    _CHK(mySourceGraph.SetupFilters(), TEXT("Setup Perm Rec Playback graph"));
    _CHK(mySourceGraph.Run(), TEXT("Run Source"));
    Sleep(5000);
    _CHK(mySourceGraph.Stop(), TEXT("Stop source Graph"));
    _CHK(mySinkGraph.Stop(), TEXT("Stop sink Graph"));
    mySourceGraph.UnInitialize();
    mySinkGraph.UnInitialize();

    // test 2, start both a playback and record simultaniously
    mySinkGraph.LogToConsole(TRUE);
    mySourceGraph.LogToConsole(TRUE);

    _CHK(mySinkGraph.Initialize(), TEXT("Initialize Sink Filter graph"));
    _CHK(mySourceGraph.Initialize(), TEXT("Initialize Source FilterGraph"));
    _CHK(mySinkGraph.SetupFilters(), TEXT("Setting up sink graph"));
    _CHK(mySourceGraph.SetupFilters(), TEXT("Setup Perm Rec Playback graph"));
    _CHK(mySinkGraph.SetRecordingPath(), TEXT("SetRecording Path"));

    if(bTemporary)
    {
        _CHK(mySinkGraph.BeginTemporaryRecording(10000), TEXT("Sink BeginTemporaryRecording"));
    }
    else
    {
        _CHK(mySinkGraph.BeginPermanentRecording(0), TEXT("Sink BeginPermanentRecording"));
    }
    _CHK(mySinkGraph.Run(), TEXT("Run Sink"));
    _CHK(mySourceGraph.Run(), TEXT("Run Source"));
    Sleep(5000);
    _CHK(mySourceGraph.Stop(), TEXT("Stop source Graph"));
    _CHK(mySinkGraph.Stop(), TEXT("Stop sink Graph"));
    mySourceGraph.UnInitialize();
    mySinkGraph.UnInitialize();

    if(g_flag == false)
        return TPR_FAIL;
    return TPR_PASS;
}

