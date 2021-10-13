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

TCHAR *g_pszSinkFilterGraphFileName   = NULL;
TCHAR *g_pszSourceFilterGraphFileName = NULL;
bool g_flag = false;    // used for callback to tell system if test passed
int g_iCount = 0;    // used to count how many sample pass, detect stream stall.

HRESULT g_hr;
#define _CHK(x, y) if(FAILED(g_hr = x)) {LogError(__LINE__, pszFnName, TEXT("Failed at %s : 0x%x."), y, g_hr); g_flag = false; } else LogText(__LINE__, pszFnName, TEXT("Succeed at %s."), y);

#if 0
// this is a callback for the ASFDemuxCanRender test case
HRESULT Helper_ASFDemuxCanRender(IMediaSample *pSample, REFERENCE_TIME *StartTime, REFERENCE_TIME *StopTime, BOOL TypeChanged)
{
    const TCHAR * pszFnName = TEXT("Helper_ASFDemuxCanRender");
    BYTE * pFileBuffer = NULL;
    BYTE * pSampleBuffer = NULL;
    int iSampleSize = 0;
    int iFileSize = 0;
    int i = 0;

    g_iCount++;
    if(g_iCount % 30 == 0)
    {
        LogText(__LINE__, pszFnName, TEXT("Frame at %d."), g_iCount);
    }
    return S_OK;
}

TESTPROCAPI ASFDemuxCanRender(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    /*
    This test will launch Em10 + asf Demux + Decoder + GrabberSample + render
    The code will load Sample.wmv and check if the 140th frame is rendered.
    */

    NO_MESSAGES;

    const TCHAR * pszFnName = TEXT("ASFDemuxCanRender");
    CFilterGraph myGraph;
    IGrabberSample *pGrabberSample = NULL;
    IFileSourceFilter *pFileSource = NULL;
    IBaseFilter * pBaseFilterEM10 = NULL;
    IBaseFilter * pBaseFilterGrabber = NULL;
    IBaseFilter * pBaseFilterDemux = NULL;

    g_flag = true;
    g_iCount = 0;

    LogText(__LINE__, pszFnName, TEXT("Initializing myGraph Class."));
    myGraph.Initialize();
    // Insert all filters
    _CHK(myGraph.AddFilterByCLSID(CLSID_Babylon_EM10, TEXT("CLSID_Babylon_EM10 filter"), &pBaseFilterEM10), TEXT("Inserting pBaseFilterEM10 Filter"));
    // cause crash 
    // _CHK(myGraph.FindInterface(IID_IFileSourceFilter, (void **)&pFileSource),TEXT("Find IFileSource interface to load file into EM10"));
    _CHK(pBaseFilterEM10->QueryInterface(IID_IFileSourceFilter, (void**)&pFileSource), TEXT("Queryiny IFileSource Interface from pBaseFilterEM10"));
    _CHK(pFileSource->Load(MPGFile, NULL), TEXT("Load MPGFile byron05 in current dir"));
    _CHK(myGraph.AddFilterByCLSID(CLSID_myGrabberSample, TEXT("Grabber filter"), &pBaseFilterGrabber), TEXT("Inserting Grabber Filter"));
    _CHK(myGraph.AddFilterByCLSID(CLSID_Demux, TEXT("Demux filter"), &pBaseFilterDemux), TEXT("Inserting Demux Filter"));
    // Connect all filters
    _CHK(myGraph.ConnectFilters(pBaseFilterEM10, pBaseFilterGrabber), TEXT("Connected pBaseFilterEM10 to Grabber"));
    _CHK(myGraph.ConnectFilters(pBaseFilterGrabber, pBaseFilterDemux), TEXT("Connected Grabber to Demux"));
    // Render Demux
    IPin * pAudio = myGraph.GetPinByName(pBaseFilterDemux, TEXT("Audio"));
    IPin * pVideo = myGraph.GetPinByName(pBaseFilterDemux, TEXT("Video"));
    if(pAudio == NULL || pVideo == NULL)
    {
        LogError(__LINE__, pszFnName, TEXT("Failed to render Audio or video Pin."));
        g_flag = false;
    }
    else
    {
        _CHK(myGraph.RenderPin(pAudio), TEXT("Render Audio Pin"));
        _CHK(myGraph.RenderPin(pVideo), TEXT("Render Video Pin"));
    }
    // Hookup Grabber to monitor.
    _CHK(myGraph.FindInterface(__uuidof(IGrabberSample), (void **)&pGrabberSample),TEXT("Find pGrabberSample interface"));
    _CHK(pGrabberSample->SetCallback(Helper_ASFDemuxCanRender, NULL),TEXT("Hookup Callback Handler"));
    //All set now, play.
    myGraph.Run();
    Sleep(30*1000);
    myGraph.Stop();

    pGrabberSample->SetCallback(NULL, NULL);
    pGrabberSample->Release();
    pAudio->Release();
    pVideo->Release();
    pBaseFilterDemux->Release();
    pBaseFilterGrabber->Release();
    pFileSource->Release();
    pBaseFilterEM10->Release();
    myGraph.UnInitialize();
    LogDetail(__LINE__, pszFnName, TEXT("Total sample played are %d."), g_iCount);
    if(g_iCount < 40)
        g_flag = false;
    if(g_flag == false)
        return TPR_FAIL;
    return TPR_PASS;
}
 #endif
 
HRESULT RecordingCallBack(IMediaSample *pSample, REFERENCE_TIME *StartTime, REFERENCE_TIME *StopTime, BOOL TypeChanged, AM_MEDIA_TYPE *mtype, LPVOID lpUser)
{
    g_iCount++;
    if(g_iCount % 10 == 0)
        LogText(__LINE__, TEXT("RecordingCallBack"), TEXT("\t%d"), g_iCount);
    return S_OK;
}

TESTPROCAPI EngineCanWorkPerm(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    /*    
    This test will send 300 samples to DVR engine (app 30 MB). Use a GrabberSample to detect if stream is
    going and read back correctly. Stream are stamped with a unique number from 1 to UINT32.MAX. Just make sure
    we read back from 1 to whatever we write to disk.
    */

    NO_MESSAGES;

    const TCHAR * pszFnName = TEXT("EngineCanWorkPerm");
    HRESULT hr;
    CSinkFilterGraphEx mySinkGraph;
    CSourceFilterGraph mySourceGraph;
    CComPtr<IGrabberSample> pGrabberSample = NULL;    // For Sample Grabber

    UINT32 ui32StartCount = 0;
    UINT32 ui32EndCount = 0;
    // BUGBUG: set the number of samples via command line or function table header
    UINT iNumOfSample = 300;    // Default send 300 Samples
    UINT32 ui32Time = iNumOfSample / 15;
    UINT iAllowableVariance = 200;
    LPOLESTR wszFileName = NULL;

    mySinkGraph.LogToConsole(true);
    mySourceGraph.LogToConsole(true);

    mySinkGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);
    mySourceGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);

    g_flag = true;        //set default as true;
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
    LogText(__LINE__, pszFnName, TEXT("Sleep %d sec."), ui32Time);
    Sleep(ui32Time * 1000);

    // Get Current Perm File Name
    _CHK(mySinkGraph.GetCurFile(&wszFileName ,NULL), TEXT("Get Perm Rec File Name"));
    LogText(__LINE__, pszFnName, TEXT("Recording to file : %s."), wszFileName);
    // Now set up playback filter; DVR source + Grabber + Demux;
    _CHK(mySourceGraph.Initialize(), TEXT("Init Source FilterGraph"));

    hr = mySourceGraph.SetupFilters(wszFileName, TRUE);
    _CHK(hr, TEXT("Setup Source Graph"));

    // hookup callback
    _CHK(mySourceGraph.FindInterface(__uuidof(IGrabberSample), (void **)&pGrabberSample), TEXT("Querying IGrabberSample Interface"));
    _CHK(pGrabberSample->SetCallback(CSourceFilterGraphEx::SequentialPermStampChecker, NULL), TEXT("SetCallback of GrabberSample CSinkFilterGraphEx::SequentialPermStampChecker"));

    _CHK(mySourceGraph.Run() ,TEXT("Run Source Graph"));
    LogText(__LINE__, pszFnName, TEXT("Sleep %d seconds."), (ui32Time + 4));
    Sleep((ui32Time +4) * 1000);
    _CHK(pGrabberSample->SetCallback(NULL, NULL), TEXT("SetCallback of GrabberSample as NULL"));

    // make sure we're 
    if(CSinkFilterGraphEx::Ex_iCount < (iNumOfSample - iAllowableVariance) || CSinkFilterGraphEx::Ex_iCount > (iNumOfSample + iAllowableVariance))
    {
        LogError(__LINE__, pszFnName, TEXT("The number of samples are %d. less than %d or greater than %d."), CSinkFilterGraphEx::Ex_iCount, iNumOfSample - iAllowableVariance, iNumOfSample + iAllowableVariance);
        g_flag = false;
    }
    // Stop graph
    _CHK(mySourceGraph.Stop(), TEXT("Stop source Graph"));
    _CHK(mySinkGraph.Stop(), TEXT("Stop sink Graph"));    

    mySourceGraph.UnInitialize();
    mySinkGraph.UnInitialize();

    if(g_flag == false)
        return TPR_FAIL;
    return TPR_PASS;
}

TESTPROCAPI EngineCanWorkTemp(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    /*    
    This test will send 300 samples to DVR engine (app 30 MB). Use a GrabberSample to detect if stream is
    going and read back correctly. Stream are stamped with a unique number from 1 to UINT32.MAX. Just make sure
    we read back from 1 to whatever we write to disk.
    */

    NO_MESSAGES;

    const TCHAR * pszFnName = TEXT("EngineCanWorkTemp");
    CSinkFilterGraphEx * p_mySinkGraph = new CSinkFilterGraphEx();
    CSourceFilterGraph * p_mySourceGraph = new CSourceFilterGraph();;
    CComPtr<IGrabberSample> pGrabberSample = NULL;    // For Sample Grabber

    UINT32 ui32StartCount = 0;
    UINT32 ui32EndCount = 0;
    // BUGBUG: set the number of samples via command line or function table header
    UINT iNumOfSample = 300;    // Default send 300 Samples
    UINT32 ui32Time = iNumOfSample / 15;        // Should finish in iNumOfSample/20 seconds when we run 30 sample/sec with EM10
    UINT32 iAllowableVariance = 200;
    LPOLESTR wszFileName = NULL;
    LPOLESTR wszLiveToken = NULL;

    g_flag = true;        //set default as true;

    if(p_mySinkGraph && p_mySourceGraph)
    {
        p_mySinkGraph->LogToConsole(true);
        p_mySourceGraph->LogToConsole(true);

        p_mySinkGraph->SetCommandLine(g_pShellInfo->szDllCmdLine);
        p_mySourceGraph->SetCommandLine(g_pShellInfo->szDllCmdLine);

        //Set up the Sink Graph with Em10 + Grabber + DVR sink
        p_mySinkGraph->Initialize();

        _CHK(p_mySinkGraph->SetupFilters(TRUE), TEXT("Setting up sink graph"));

        _CHK(p_mySinkGraph->SetGrabberCallback(CSinkFilterGraphEx::SequentialStamper), TEXT("Setting up sink graph callback stamper"));

        LPOLESTR pRecordingPath = NULL;
        _CHK(p_mySinkGraph->SetRecordingPath(), TEXT("SetRecording Path"));
        _CHK(p_mySinkGraph->GetRecordingPath(&pRecordingPath), TEXT("SetRecording Path"));
        LogText(TEXT("Set recording Path as %s."), pRecordingPath);
        CoTaskMemFree(pRecordingPath);
        pRecordingPath = NULL;

        _CHK(p_mySinkGraph->BeginTemporaryRecording(30*1000), TEXT("SBC BeginTemporaryRecording"));
        p_mySinkGraph->Run();

        _CHK(p_mySinkGraph->GetCurFile(&wszFileName, NULL), TEXT("Sink GetCurFile"));
        LogText(__LINE__, pszFnName, TEXT("Recording to file : %s."), wszFileName);
        _CHK(p_mySinkGraph->GetBoundToLiveToken(&wszLiveToken), TEXT("Sink GetBoundToLiveToken"));
        LogText(__LINE__, pszFnName, TEXT("LiveToken is : %s."), wszLiveToken);
        // Now set up playback filter; DVR source + Grabber + Demux;
        _CHK(p_mySourceGraph->Initialize(), TEXT("Init Source FilterGraph"));


        _CHK(p_mySourceGraph->SetupFilters(wszLiveToken, TRUE), TEXT("Setup Source Graph"));
        LogText(__LINE__, pszFnName, TEXT("Sleep %d second."), ui32Time);
        Sleep(ui32Time * 1000);
        // hookup callback
        _CHK(p_mySourceGraph->FindInterface(__uuidof(IGrabberSample), (void **)&pGrabberSample), TEXT("Querying IGrabberSample Interface"));
        _CHK(pGrabberSample->SetCallback(CSourceFilterGraphEx::SequentialTempStampChecker, NULL), TEXT("SetCallback of GrabberSample Checker CSinkFilterGraphEx::SequentialStampChecker"));

        _CHK(p_mySourceGraph->Run() ,TEXT("Run Source Graph"));
        LogText(__LINE__, pszFnName, TEXT("Sleep %d seconds."), ui32Time + 2);
        Sleep((ui32Time + 2) * 1000);
        _CHK(p_mySinkGraph->SetGrabberCallback(NULL), TEXT("Setting up sink graph callback as NULL"));
        _CHK(pGrabberSample->SetCallback(NULL, NULL), TEXT("SetCallback of GrabberSample as NULL"));

        // make sure we're 
        if(CSinkFilterGraphEx::Ex_iCount < (iNumOfSample - iAllowableVariance) || CSinkFilterGraphEx::Ex_iCount > (iNumOfSample + iAllowableVariance))
        {
            LogError(__LINE__, pszFnName, TEXT("The number of samples are %d. less than %d or greater than %d."), CSinkFilterGraphEx::Ex_iCount, iNumOfSample - iAllowableVariance, iNumOfSample + iAllowableVariance);
            g_flag = false;
        }

        // Stop graph
        _CHK(p_mySinkGraph->Stop(), TEXT("Stop sink Graph"));    
        _CHK(p_mySourceGraph->Stop(), TEXT("Stop source Graph"));

        LogText(__LINE__, pszFnName, TEXT("Sink Graph decomposed."));

        p_mySourceGraph->UnInitialize();
        p_mySinkGraph->UnInitialize();
    }
    else
    {
        LogError(__LINE__, pszFnName, TEXT("Failed to allocate the source or sink graph."));
        g_flag = false;
    }

    if(p_mySourceGraph)
        delete p_mySourceGraph;
    if(p_mySinkGraph)
        delete p_mySinkGraph;

    if(g_flag == false)
        return TPR_FAIL;
    return TPR_PASS;
}

TESTPROCAPI GetCurFileTempRecStartedTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{

    NO_MESSAGES;

    const TCHAR * pszFnName = TEXT("GetCurFileTempRecStartedTest");
    LONGLONG TempBufferSize = 10 * 1000;    // set 10 second buffer size;
    int iTime = 10 ;    // Let it run for 10 seconds
    // Make sure iTime is much less than TempBuffer otherwsie the 1st file may be overwritten

    CSinkFilterGraphEx mySinkFilterGraph;
    LPOLESTR pszFileName = NULL;
    LPOLESTR pszFileName_01 = NULL;
    WIN32_FIND_DATAW wData;
    HANDLE hFile = NULL;
    HRESULT hr = E_FAIL;

    // Default pass
    g_flag = true;

    mySinkFilterGraph.LogToConsole(true);
    mySinkFilterGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);

    LogText(__LINE__, pszFnName, TEXT("The iTime is %d."), iTime);
    pszFileName_01 = new WCHAR[120];

    if(pszFileName_01)
    {
        hr = mySinkFilterGraph.Initialize();

        if(SUCCEEDED(hr))
        {
            // Setting up Graph

            _CHK(mySinkFilterGraph.SetupFilters(TRUE), TEXT("Setting up Sink Graph"));

            // Call BegineTemp Rec
            LPOLESTR pRecordingPath = NULL;
            _CHK(mySinkFilterGraph.SetRecordingPath(), TEXT("SetRecording Path"));
            _CHK(mySinkFilterGraph.GetRecordingPath(&pRecordingPath), TEXT("SetRecording Path"));
            LogText(TEXT("Set recording Path as %s."), pRecordingPath);
            CoTaskMemFree(pRecordingPath);
            pRecordingPath = NULL;

            _CHK(mySinkFilterGraph.BeginTemporaryRecording(TempBufferSize), TEXT("BeginTemporaryRecording"));
            _CHK(mySinkFilterGraph.Run(), TEXT("Run Graph"));
            // Wait 10 seconds for file creation
            LogText(__LINE__, pszFnName, TEXT("Wait %d seconds."), iTime);
            Sleep(iTime * 1000);    // Sleep for one Temp File filled. 

            // Get Cur File
            _CHK(mySinkFilterGraph.GetCurFile(&pszFileName, NULL), TEXT("Get Cur File"));

            LogText(__LINE__, pszFnName, TEXT("The gotten file name is %s."), pszFileName);
            _tcscpy(pszFileName_01, pszFileName);
            _tcscat(pszFileName_01, TEXT("*.dvr-dat"));    // The 1st Dat file;
            LogText(TEXT("The Temp file name is %s."), pszFileName_01);
            hFile = FindFirstFileW(pszFileName_01, &wData);
            if(hFile == INVALID_HANDLE_VALUE)
            {
                LogError(__LINE__, pszFnName, TEXT("Did not find the filename gotten by GetCurFile."));
                g_flag = false;
            }
            else
            {
                LogText(__LINE__, pszFnName, TEXT("Find the filename gotten by GetCurFile; succeed."));
            }
            FindClose(hFile);

            CoTaskMemFree(pszFileName);
            //
            _CHK(mySinkFilterGraph.Stop(), TEXT("Stoppping the Graph"));
            mySinkFilterGraph.UnInitialize();
        }

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

TESTPROCAPI GetCurFilePermRecStartedTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    const TCHAR * pszFnName = TEXT("GetCurFilePermRecStartedTest");
    LONGLONG TempBufferSize = 10 * 1000;    // set 20 second buffer size to convert;
    int iTime = 20 ;    // Let it run for 10 seconds
    // Make sure iTime is much less than TempBuffer otherwsie the 1st file may be overwritten

    AM_MEDIA_TYPE mt;
    CSinkFilterGraphEx mySinkFilterGraph;
    LPOLESTR pszFileName = NULL;
    LPWSTR pszFileName_01 = NULL;
    WIN32_FIND_DATAW wData;
    HRESULT hr = E_FAIL;

    g_flag = true;

    mySinkFilterGraph.LogToConsole(true);
    mySinkFilterGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);

    LogText(__LINE__, pszFnName, TEXT("The iTime is %d."), iTime);
    pszFileName_01 = new WCHAR[120];

    if(pszFileName_01)
    {
        hr = mySinkFilterGraph.Initialize();

        if(SUCCEEDED(hr))
        {
            // Setting up Graph

            _CHK(mySinkFilterGraph.SetupFilters(TRUE), TEXT("Setting up Sink Graph"));

            // Call BegineTemp Rec
            LPOLESTR pRecordingPath = NULL;
            _CHK(mySinkFilterGraph.SetRecordingPath(), TEXT("SetRecording Path"));
            _CHK(mySinkFilterGraph.GetRecordingPath(&pRecordingPath), TEXT("SetRecording Path"));
            LogText(TEXT("Set recording Path as %s."), pRecordingPath);
            CoTaskMemFree(pRecordingPath);
            pRecordingPath = NULL;

            LogText(__LINE__, pszFnName, TEXT("Call BeingPermRec now."));
            //_CHK(mySinkFilterGraph.BeginPermanentRecording(TempBufferSize), TEXT("BeginPermRecording."));
            _CHK(mySinkFilterGraph.BeginPermanentRecording(0), TEXT("BeginPermRecording"));
            _CHK(mySinkFilterGraph.Run(), TEXT("Run Graph"));
            // Wait 40 seconds for file creation
            LogText(__LINE__, pszFnName, TEXT("Wait %d seconds."), iTime);
            Sleep(iTime * 1000);    // Sleep for one Temp File filled. 
            // Begine Perm Rec
            ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
            // Sleep another 5 seconds then check
            Sleep(5*1000);
            // Get Cur File
            _CHK(mySinkFilterGraph.GetCurFile(&pszFileName, &mt), TEXT("Get Cur File"));

            LogText(__LINE__, pszFnName, TEXT("The gotten file name is %s."), pszFileName);
            _tcscpy(pszFileName_01, pszFileName);
            _tcscat(pszFileName_01, TEXT("*.dvr-dat"));    // The 1st Dat file;
            LogText(TEXT("The Temp file name is %s."), pszFileName_01);

            if(FindFirstFileW(pszFileName_01, &wData) == INVALID_HANDLE_VALUE)
            {
                LogError(__LINE__, pszFnName, TEXT("Did not find the filename gotten by GetCurFile."));
                g_flag = false;
            }
            else
            {
                LogText(__LINE__, pszFnName, TEXT("Find the filename gotten by GetCurFile; succeed."));
            }

            CoTaskMemFree(pszFileName);

            hr = mySinkFilterGraph.Stop();
            mySinkFilterGraph.UnInitialize();
        }
        else
        {
            LogError(__LINE__, pszFnName, TEXT("Failed to allocate a string to store the file name."));
            g_flag = false;
        }

        delete [] pszFileName_01;
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

TESTPROCAPI BeginTempRecTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;


    // This test should 1st start the temporary recording, then set buffer as TempBufferSize.
    // After that, wait TempBufferSize + 10 seconds, which make sure buffer is filled.
    // When later file getting larger, this should be longer as well.
    // Now is all read sample are correctly stamped with DVR source filter

    const TCHAR * pszFnName = TEXT("BeginTempRecTest");
    LONGLONG TempBufferSize = 20 * 1000;    // set 10 second buffer size;
    int iTime = 10*1000 ;    // Check if new file generated and old file gone interval
    // When later if the temp file size are large, this number has to change.
    int iTmp = 5;    // recheck several time;
    int iFileCount = 0;
    int iFileCountOld = 0;
    int iFrameCountOld = 0;
    // plus or minus 10 files seems reasonable
    int iError = 10;
    LPWSTR wszFileName = NULL;
    LPWSTR wszFileNameAll = NULL;
    HANDLE hFirstFile = NULL;
    WIN32_FIND_DATA wFindData;

    IFileSinkFilter * pFileSink = NULL;
    IStreamBufferCapture * pSBC = NULL;
    IGrabberSample *pGrabberSample = NULL;

    CSinkFilterGraphEx myGraph;
    LPOLESTR pszFileName = NULL;

    g_flag = true;

    myGraph.LogToConsole(true);
    myGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);

    LogText(__LINE__, pszFnName, TEXT("The iTime is %d."), iTime);
    _CHK(myGraph.Initialize(), TEXT("Initialize Graph"));

    // Setting up Graph

    _CHK(myGraph.SetupFilters(TRUE), TEXT("Setting up Sink Graph"));

    _CHK(myGraph.FindInterface(__uuidof(IStreamBufferCapture), (void **)&pSBC), TEXT("Querying IStreamBUfferCapture Interface"));
    _CHK(myGraph.FindInterface(__uuidof(IGrabberSample), (void **)&pGrabberSample), TEXT("Querying IGrabberSample Interface"));
    LogText(__LINE__, pszFnName, TEXT("Temp buffer size is %d."), TempBufferSize);

    LPOLESTR pRecordingPath = NULL;
    _CHK(myGraph.SetRecordingPath(), TEXT("SetRecording Path"));
    _CHK(myGraph.GetRecordingPath(&pRecordingPath), TEXT("SetRecording Path"));
    LogText(TEXT("Set recording Path as %s."), pRecordingPath);
    CoTaskMemFree(pRecordingPath);
    pRecordingPath = NULL;

    _CHK(pSBC->BeginTemporaryRecording(TempBufferSize), TEXT("BeginTemporaryRecording"));
    _CHK(pGrabberSample->SetCallback(RecordingCallBack, NULL), TEXT("SetCallback of GrabberSample"));
    g_iCount = 0;
    _CHK(myGraph.Run(), TEXT("Run Graph"));

    LogText(__LINE__, pszFnName, TEXT("Going to sleep %d Milli seconds to fill Temp Buffer."), TempBufferSize);
    Sleep((int)TempBufferSize + 5 * 1000);    // Sleep for buffer filled.

    myGraph.FindInterface(IID_IFileSinkFilter, (void**)&pFileSink);
    pFileSink->GetCurFile(&wszFileName ,NULL);
    LogText(__LINE__, pszFnName, TEXT("Recording to file : %s."), wszFileName);
    wszFileNameAll = new TCHAR[40];
    _tcscpy(wszFileNameAll, wszFileName);
    _tcscat(wszFileNameAll, TEXT("*.dvr-dat"));    // The 1st Dat file;
    LogText(__LINE__, pszFnName, TEXT("Will search all %s files."), wszFileNameAll);
    hFirstFile = FindFirstFileW(wszFileNameAll, &wFindData);
    if(hFirstFile != INVALID_HANDLE_VALUE)
    {
        iFileCountOld++;
        while(FindNextFile(hFirstFile, &wFindData))
        {
            iFileCountOld++;
        }
        LogText(__LINE__, pszFnName, TEXT("The # of recorded file in dir start is %d."), iFileCountOld);
        FindClose(hFirstFile);
    }

    Sleep(5*1000);
    while(iTmp-- > 0)
    {    
        iFileCount = 0;
        FindClose(hFirstFile);
        hFirstFile = FindFirstFileW(wszFileNameAll, &wFindData);
        if(hFirstFile != INVALID_HANDLE_VALUE)
        {
            iFileCount++;
            while( FindNextFile(hFirstFile, &wFindData) )
            {
                iFileCount++;
            }
            LogText(__LINE__, pszFnName, TEXT("The # of recorded file in dir is %d."), iFileCount);
            if(abs(iFileCount - iFileCountOld ) >= iError)
            {
                LogError(__LINE__, pszFnName, TEXT("The file number are changing too much; error."));
                g_flag = false;
            }
            else
            {
                LogText(__LINE__, pszFnName, TEXT("The file number are keeping consistant."));
                LogText(__LINE__, pszFnName, TEXT("Sleeping 10 seconds for next test."));
                Sleep(10*1000);
            }
        }
        else
        {
            LogError(__LINE__, pszFnName, TEXT("Did not find recorded file; error."));
            g_flag = false;
            // break;
        }
        if(iFrameCountOld == g_iCount)
        {
            LogError(__LINE__, pszFnName, TEXT("Stream stopped flow; error."));
            g_flag = false;
        }
        iFrameCountOld = g_iCount;
    }


    _CHK(myGraph.Stop(), TEXT("Stop Graph"));
    pSBC->Release();
    pGrabberSample->Release();
    pFileSink->Release();
    myGraph.UnInitialize();
    CoTaskMemFree(wszFileName);
    delete []wszFileNameAll;
    if(g_flag == false)
        return TPR_FAIL;
    return TPR_PASS;
}

TESTPROCAPI BeginPermRecTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    const TCHAR * pszFnName = TEXT("BeginPermRecTest");
    LONGLONG TempBufferSize = 0;    // set 0 second buffer size to convert so always start a new recording file;
    LONGLONG lBuffer = 10*1000 ;    // Let it run for 20 seconds so any temp recording will appear.
    int iLoop = 5;        // Check file size 5 times

    CSinkFilterGraphEx mySinkFilterGraph;
    LPOLESTR pszFileName = NULL;
    LPOLESTR pszFileName_01 = NULL;
    WIN32_FIND_DATAW wData;
    HANDLE hFile = NULL;

    g_flag = true;

    mySinkFilterGraph.LogToConsole(true);
    mySinkFilterGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);

    pszFileName_01 = new WCHAR[120];

    HRESULT hr = mySinkFilterGraph.Initialize();

    if(SUCCEEDED(hr))
    {
        // Setting up Filters

        _CHK(mySinkFilterGraph.SetupFilters(TRUE), TEXT("Setting up Sink Graph"));

        // Call BegineTemp Rec
        LPOLESTR pRecordingPath = NULL;
        _CHK(mySinkFilterGraph.SetRecordingPath(), TEXT("SetRecording Path"));
        _CHK(mySinkFilterGraph.GetRecordingPath(&pRecordingPath), TEXT("SetRecording Path"));
        LogText(TEXT("Set recording Path as %s."), pRecordingPath);
        CoTaskMemFree(pRecordingPath);
        pRecordingPath = NULL;

        _CHK(mySinkFilterGraph.BeginTemporaryRecording(lBuffer), TEXT("Start Temp Rec"));
        _CHK(mySinkFilterGraph.Run(), TEXT("Run Graph"));
        // Wait lBuffer seconds for file creation
        LogText(__LINE__, pszFnName, TEXT("Wait %d Milliseconds."), lBuffer);
        Sleep((DWORD)lBuffer);    // Sleep for one Temp File filled. 
        // Begine Perm Rec
        LogText(__LINE__, pszFnName, TEXT("Call BeginPermRec now."));
        _CHK(mySinkFilterGraph.BeginPermanentRecording(TempBufferSize), TEXT("Begin Perm Recording"));

        // Get Cur Perm File
        _CHK(mySinkFilterGraph.GetCurFile(&pszFileName, NULL), TEXT("Get Cur File"));
        LogText(__LINE__, pszFnName, TEXT("The gotten Perm file name is %s."), pszFileName);
        _tcscpy(pszFileName_01, pszFileName);
        _tcscat(pszFileName_01, TEXT("*.dvr-dat"));    // The 1st Dat file;
        LogText(TEXT("The search match Perm file name is %s."), pszFileName_01);

        // Sleep another 3 seconds then check
        Sleep(3*1000);
        // Now start Temp recording for lBuffer long bugger again.
        _CHK(mySinkFilterGraph.BeginTemporaryRecording(lBuffer), TEXT("Start Temp Rec Again"));
        Sleep((DWORD)(2*lBuffer));    // Make sure Permenant rec out of buffer window

        hFile = FindFirstFileW(pszFileName_01, &wData);

        if(hFile == INVALID_HANDLE_VALUE)
        {
            LogError(__LINE__, pszFnName, TEXT("Did not find the filename gotten by GetCurFile."));
            g_flag = false;
        }
        else
        {
            LogText(__LINE__, pszFnName, TEXT("Find the filename gotten by GetCurFile."));
        }

        FindClose(hFile);

        CoTaskMemFree(pszFileName);
        hr = mySinkFilterGraph.Stop();
    }

    if(pszFileName_01)
        delete[] pszFileName_01;

    if(FAILED(hr) || (g_flag == false))
    {
        return TPR_FAIL;
    }
    else
    {
        return TPR_PASS;
    }
}

TESTPROCAPI SwitchRecModeTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // This test should 1st start the temporary recording, then set buffer as TempBufferSize.
    // After that, wait TempBufferSize + 10 seconds, which make sure buffer is filled.
    // When later file getting larger, this should be longer as well.
    // Now check if the 1st file are changing and total disk size keep same.
    // If so means temp recording is written within a limited buffer.

    const TCHAR * pszFnName = TEXT("SwitchRecModeTest");
    LONGLONG TempBufferSize = 10 * 1000;    // set 10 second buffer size;
    // When later if the temp file size are large, this number has to change.
    int iTmp = 3;    // Looper iTmp time;
    int iCountOld = 0;
    int iInterval = 10*1000;    //interval as 10 seconds
    LPOLESTR pFileName = new WCHAR[120];

    IFileSinkFilter * pFileSink = NULL;
    IStreamBufferCapture * pSBC = NULL;
    IGrabberSample *pGrabberSample = NULL;
    CSinkFilterGraphEx mySinkGraph;

    g_flag = true;

    mySinkGraph.LogToConsole(true);
    mySinkGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);

    if(pFileName)
    {
        _CHK(mySinkGraph.Initialize(), TEXT("Initialize Graph"));
        // Setting up Filters

        _CHK(mySinkGraph.SetupFilters(TRUE), TEXT("Setting up Sink Graph"));

        // Search Interfaces
        _CHK(mySinkGraph.FindInterface(IID_IFileSinkFilter, (void **)&pFileSink), TEXT("Querying IFileSinkFilter Interface"));
        _CHK(mySinkGraph.FindInterface(__uuidof(IStreamBufferCapture), (void **)&pSBC), TEXT("Querying IStreamBUfferCapture Interface"));
        _CHK(mySinkGraph.FindInterface(__uuidof(IGrabberSample), (void **)&pGrabberSample), TEXT("Querying IGrabberSample Interface"));
        _CHK(pGrabberSample->SetCallback(RecordingCallBack, NULL), TEXT("SetCallback of GrabberSample"));
        //Setting recording path

        LPOLESTR pRecordingPath = NULL;
        _CHK(mySinkGraph.SetRecordingPath(), TEXT("SetRecording Path"));
        _CHK(mySinkGraph.GetRecordingPath(&pRecordingPath), TEXT("SetRecording Path"));
        LogText(TEXT("Set recording Path as %s."), pRecordingPath);
        CoTaskMemFree(pRecordingPath);
        pRecordingPath = NULL;

        _CHK(pSBC->BeginTemporaryRecording(30*1000), TEXT("SBC BeginTemporaryRecording"));

        g_iCount = 0;
        _CHK(mySinkGraph.Run(), TEXT("Run Graph"));

        Sleep(iInterval);    //let graph full functional
        srand(12345);
        STRMBUF_CAPTURE_MODE Mode;
        LONGLONG BufLenMilli = 0;

        while(iTmp-- > 0)
        {
            LPOLESTR pTMP = NULL;
            TempBufferSize = rand();
            _CHK(pSBC->BeginTemporaryRecording(TempBufferSize), TEXT("BeginTemporaryRecording"));
            _CHK(pSBC->GetCaptureMode(&Mode, &BufLenMilli), TEXT("GetCaptureMode"));
            LogText(__LINE__, pszFnName, TEXT("Temp Buffer is set as : %d."), TempBufferSize);
            LogText(__LINE__, pszFnName, TEXT("Get Mode:%d,\tLength:%d || Should be %d, \t > %d."), Mode, (int)BufLenMilli, STRMBUF_TEMPORARY_RECORDING, (int)TempBufferSize);
            if(Mode != STRMBUF_TEMPORARY_RECORDING
                || BufLenMilli < TempBufferSize)
            {
                LogError(__LINE__, pszFnName, TEXT("Mode or BufferSize wrong."));
                g_flag = false;
                break;
            }
            Sleep(iInterval);
            _CHK(pFileSink->GetCurFile(&pTMP, NULL), TEXT("Get Temp FileName"));
            if(_tcscmp(pTMP, pFileName) == 0)
            {
                LogError(__LINE__, pszFnName, TEXT("New Temp recording Mode same with old Perm filename as %s, failed."), pTMP);
                g_flag = false;
                break;
            }
            else
            {
                LogText(TEXT("New recording Mode started with new filename as %s."), pTMP);
                _tcscpy(pFileName, pTMP);
                CoTaskMemFree(pTMP);
            }
            // Now set permanent rec.
            LONGLONG llTemp;
            _CHK(pSBC->BeginPermanentRecording(0, &llTemp), TEXT("BeginPermanentRecording"));
            _CHK(pSBC->GetCaptureMode(&Mode, &BufLenMilli), TEXT("GetCaptureMode"));
            LogText(__LINE__, pszFnName, TEXT("Get Mode:%d,\tLength:%d."),Mode, (int)BufLenMilli);
            if(Mode != STRMBUF_PERMANENT_RECORDING)
            {
                LogError(__LINE__, pszFnName, TEXT("Mode wrong, should be Permenant Rec now."));
                g_flag = false;
                break;
            }

            Sleep(iInterval);    //totally sleep iTmp seconds
            if(g_iCount <= iCountOld)
            {
                LogError(__LINE__, pszFnName, TEXT("Stream stalled."));
                g_flag = false;
                break;
            }
            else
            {
                iCountOld = g_iCount;
            }
            _CHK(pFileSink->GetCurFile(&pTMP, NULL), TEXT("Get Perm FileName"));
            if(_tcscmp(pTMP, pFileName) == 0)
            {
                LogText(__LINE__, pszFnName, TEXT("New Perm recording reusing old temp file name %s."), pTMP);
            }
            else
            {
                LogText(TEXT("New recording Mode started with new filename as %s."), pTMP);
                _tcscpy(pFileName, pTMP);
            }

            CoTaskMemFree(pTMP);
        }

        _CHK(mySinkGraph.Stop(), TEXT("Stop Graph"));
        delete []pFileName;
        pSBC->Release();
        pGrabberSample->Release();
        pFileSink->Release();
        mySinkGraph.UnInitialize();
    }
    else
    {
        LogError(__LINE__, pszFnName, TEXT("Failed to allocate a string to store the file name."));
        g_flag = false;
    }

    if(g_flag == false)
        return TPR_FAIL;
    return TPR_PASS;
}

TESTPROCAPI TempRec2TempRecTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;


    // This test should 1st start the temporary recording, then set buffer as TempBufferSize.
    // After that, wait TempBufferSize + 10 seconds, which make sure buffer is filled.
    // When later file getting larger, this should be longer as well.
    // Now is all read sample are correctly stamped with DVR source filter

    const TCHAR * pszFnName = TEXT("TempRec2TempRecTest");
    LONGLONG TempBufferSize = 20 * 1000;    // set 10 second buffer size;
    int iTime = 10*1000 ;    // Check if new file generated and old file gone interval
    // When later if the temp file size are large, this number has to change.
    int iTmp = 3;    // recheck several time;
    int iTemporaryIterations = 4;
    int iFileCount = 0;
    int iFileCountOld = 0;
    int iFrameCountOld = 0;
    int iError = 6;
    LPWSTR wszFileName = NULL;
    LPWSTR wszFileNameAll = NULL;
    HANDLE hFirstFile = NULL;
    WIN32_FIND_DATA wFindData;

    IFileSinkFilter * pFileSink = NULL;
    IStreamBufferCapture * pSBC = NULL;
    IGrabberSample *pGrabberSample = NULL;

    CSinkFilterGraphEx myGraph;
    LPOLESTR pszFileName = NULL;

    g_flag = true;

    myGraph.LogToConsole(true);
    myGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);

    LogText(__LINE__, pszFnName, TEXT("The iTime is %d."), iTime);
    _CHK(myGraph.Initialize(), TEXT("Initialize Graph"));

    // Setting up Graph

    _CHK(myGraph.SetupFilters(TRUE), TEXT("Setting up Sink Graph"));

    _CHK(myGraph.FindInterface(__uuidof(IStreamBufferCapture), (void **)&pSBC), TEXT("Querying IStreamBUfferCapture Interface"));
    _CHK(myGraph.FindInterface(__uuidof(IGrabberSample), (void **)&pGrabberSample), TEXT("Querying IGrabberSample Interface"));
    LogText(__LINE__, pszFnName, TEXT("Temp buffer size is %d."), TempBufferSize);

    LPOLESTR pRecordingPath = NULL;
    _CHK(myGraph.SetRecordingPath(), TEXT("SetRecording Path"));
    _CHK(myGraph.GetRecordingPath(&pRecordingPath), TEXT("SetRecording Path"));
    LogText(TEXT("Set recording Path as %s."), pRecordingPath);
    CoTaskMemFree(pRecordingPath);
    pRecordingPath = NULL;

    _CHK(pGrabberSample->SetCallback(RecordingCallBack, NULL), TEXT("SetCallback of GrabberSample"));

    myGraph.FindInterface(IID_IFileSinkFilter, (void**)&pFileSink);

    _CHK(myGraph.Run(), TEXT("Run Graph"));

    // do mulitple temporary recordings back to back
    for(int i=0; i < iTemporaryIterations; i++)
    {
        LogText(__LINE__, pszFnName, TEXT("Beginning temporary recording %d."), i+1);

        _CHK(pSBC->BeginTemporaryRecording(TempBufferSize), TEXT("BeginTemporaryRecording"));
        g_iCount = 0;
        iFileCountOld = 0;
        iFileCount = 0;
        iFrameCountOld = 0;
        iTmp = 3;    // recheck several time;

        LogText(__LINE__, pszFnName, TEXT("Going to sleep %d Milli seconds to fill Temp Buffer."), TempBufferSize);
        Sleep((int)TempBufferSize + 5 * 1000);    // Sleep for buffer filled.
        //

        pFileSink->GetCurFile(&wszFileName ,NULL);
        LogText(__LINE__, pszFnName, TEXT("Recording to file : %s."), wszFileName);
        wszFileNameAll = new TCHAR[40];
        _tcscpy(wszFileNameAll, wszFileName);
        _tcscat(wszFileNameAll, TEXT("*.dvr-dat"));    // The 1st Dat file;
        LogText(__LINE__, pszFnName, TEXT("Will search all %s files."), wszFileNameAll);
        hFirstFile = FindFirstFileW(wszFileNameAll, &wFindData);
        if(hFirstFile != INVALID_HANDLE_VALUE)
        {
            iFileCountOld++;
            while(FindNextFile(hFirstFile, &wFindData))
            {
                iFileCountOld++;
            }
            LogText(__LINE__, pszFnName, TEXT("The # of recorded file in dir start is %d."), iFileCountOld);
            FindClose(hFirstFile);
        }

        Sleep(5*1000);
        while(iTmp-- > 0)
        {    
            iFileCount = 0;
            FindClose(hFirstFile);
            hFirstFile = FindFirstFileW(wszFileNameAll, &wFindData);
            if(hFirstFile != INVALID_HANDLE_VALUE)
            {
                iFileCount++;
                while( FindNextFile(hFirstFile, &wFindData) )
                {
                    iFileCount++;
                }
                LogText(__LINE__, pszFnName, TEXT("The # of recorded file in dir is %d."), iFileCount);
                if(abs(iFileCount - iFileCountOld ) >= iError)
                {
                    LogError(__LINE__, pszFnName, TEXT("The file number are changing too much; error."));
                    g_flag = false;
                }
                else
                {
                    LogText(__LINE__, pszFnName, TEXT("The file number are keeping consistant."));
                    LogText(__LINE__, pszFnName, TEXT("Sleeping 10 seconds for next test."));
                    Sleep(10*1000);
                }
            }
            else
            {
                LogError(__LINE__, pszFnName, TEXT("Did not find recorded file; error."));
                g_flag = false;
            }
            if(iFrameCountOld == g_iCount)
            {
                LogError(__LINE__, pszFnName, TEXT("Stream stopped flow; error."));
                g_flag = false;
            }
            iFrameCountOld = g_iCount;
        }

        if(wszFileName)
            CoTaskMemFree(wszFileName);

        if(wszFileNameAll)
            delete []wszFileNameAll;
    }

    _CHK(myGraph.Stop(), TEXT("Stop Graph"));
    pSBC->Release();
    pGrabberSample->Release();
    pFileSink->Release();
    myGraph.UnInitialize();
    if(g_flag == false)
        return TPR_FAIL;
    return TPR_PASS;
}


TESTPROCAPI SetGetRecordingPathExist(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // This test will build a directory "DHGFYGTSDJHG" under current directory;
    // Set recording path under it and then see if the recorded file are truly under it.
    // This will start Temp for 10 seconds (maybe large later) and Perm for 10 seoconds,
    // and make sure found file under the Dir "DHGFYGTSDJHG" but not under current dir.
    const TCHAR * pszFnName = TEXT("SetGetRecordingPathExist");

    LONGLONG TempBufferSize = 20 * 1000;    // set 10 second buffer size; Also used to test if Perm get rid of this limit.
    int iCountOld = 0;
    LPOLESTR pPath = TEXT("SetGetRecordingPathExist\\");
    LPOLESTR pPathGet = NULL;

    IFileSinkFilter * pFileSink = NULL;
    IStreamBufferCapture * pSBC = NULL;
    IGrabberSample *pGrabberSample = NULL;
    CSinkFilterGraphEx mySinkGraph;
    LPOLESTR pszFileName = NULL;
    LPOLESTR pszFileNameAll = new WCHAR[120];
    LPOLESTR pszCurDir = new WCHAR[120];
    WIN32_FIND_DATA wFindData;

    g_flag = true;

    mySinkGraph.LogToConsole(true);
    mySinkGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);

    if(pszFileNameAll && pszCurDir)
    {
        // Set the default recording path, and then retrieve it back
        _CHK(mySinkGraph.GetDefaultRecordingPath(pszCurDir), TEXT("SetRecording Path"));

        // now tweak our retrieved path for this tests purposes.
        LogText(TEXT("Current dir is %s."), pszCurDir);
        _tcscat(pszCurDir, pPath);
        LogText(TEXT("going to set Dir as %s."), pszCurDir);
        _CHK(mySinkGraph.Initialize(), TEXT("Initialize Graph"));
        //Inserting Filters: EM10, SampleGrabber and DVR sink
        // Setting up Filters

        _CHK(mySinkGraph.SetupFilters(TRUE), TEXT("Setting up Sink Graph"));

        // Got all interfaces
        _CHK(mySinkGraph.FindInterface(IID_IFileSinkFilter, (void **)&pFileSink), TEXT("Querying IFileSinkFilter Interface"));
        _CHK(mySinkGraph.FindInterface(__uuidof(IStreamBufferCapture), (void **)&pSBC), TEXT("Querying IStreamBUfferCapture Interface"));
        _CHK(mySinkGraph.FindInterface(__uuidof(IGrabberSample), (void **)&pGrabberSample), TEXT("Querying IGrabberSample Interface"));
        LogText(TEXT("Set recording Path as %s."), pszCurDir);
        if(!CreateDirectoryW(pszCurDir, NULL))
        {
            LogWarning(__LINE__, pszFnName, TEXT("Faile to create recording path, Dir may already there."));
        }
        _CHK(pSBC->SetRecordingPath(pszCurDir), TEXT("SetRecordingPath"));
        _CHK(pSBC->BeginTemporaryRecording(TempBufferSize), TEXT("BeginTemporaryRecording"));
        _CHK(pGrabberSample->SetCallback(RecordingCallBack, NULL), TEXT("SetCallback of GrabberSample"));
        g_iCount = 0;

        _CHK(mySinkGraph.Run(), TEXT("Run Graph."));

        Sleep((int)TempBufferSize);    // Sleep buffer filled.
        iCountOld = g_iCount;

        LogText(__LINE__, pszFnName, TEXT("Going to start Permanent rec with previous buffer saved."));
        LONGLONG llTemp;
        _CHK(pSBC->BeginPermanentRecording(TempBufferSize, &llTemp), TEXT("BeginPermanentRecording"));
        Sleep(10*1000);    // Sleep 10 seconds.
        LogText(__LINE__, pszFnName, TEXT("iCOuntOld = %d and g_iCount = %d."), iCountOld, g_iCount);
        if(iCountOld == 0 || g_iCount < iCountOld)
        {
            LogError(__LINE__, pszFnName, TEXT("Stream are not flowing correctly as iCount and g_iCount indicated."));
            g_flag = false;
            goto end;
        }
        _CHK(pSBC->GetRecordingPath(&pPathGet), TEXT("GetRecordingPath"));
        LogText(TEXT("Get recording path as %s."), pPathGet);
        _CHK(pFileSink->GetCurFile(&pszFileName, NULL), TEXT("pFileSink->GetCurFile"));
        LogText(TEXT("Retriveing current FileName as %s."), pszFileName);
        ZeroMemory(pszFileNameAll, sizeof(pszFileNameAll));
        _tcscat(pszFileNameAll, pszFileName);
        _tcscat(pszFileNameAll, TEXT("*.*"));
        LogText(TEXT("Will search files as %s."), pszFileNameAll);
        if(_tcsstr(pszFileNameAll, pPath) == NULL)        // does not contain pPath; wrong
        {
            LogError(__LINE__, pszFnName, TEXT("Get recording Path does not get correct path."));
            g_flag = false;
        }
        if(FindFirstFileW(pszFileNameAll, &wFindData) == INVALID_HANDLE_VALUE)
        {
            LogError(__LINE__, pszFnName, TEXT("Did not find the specified file."));
            g_flag = false;
        }
        else
        {
            LogText(__LINE__, pszFnName, TEXT("Find the specified recorded files; succeed."));
        }


end:

        _CHK(mySinkGraph.Stop(), TEXT("Stop Graph"));
        CoTaskMemFree(pPathGet);
        CoTaskMemFree(pszFileName);
        pSBC->Release();
        pGrabberSample->Release();
        pFileSink->Release();
        mySinkGraph.UnInitialize();
    }

    Helper_CleanupRecordPath(pszCurDir);

    if(pszFileNameAll)
        delete []pszFileNameAll;

    if(pszCurDir)
        delete []pszCurDir;

    if(g_flag == false)
        return TPR_FAIL;
    return TPR_PASS;
}

TESTPROCAPI DeleteRecordedItemTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    /*    
    We will.
    1. Start a Temp recording for 10 seconds, with buffer as 10 seconds
    2. Start a Permanent recording with 0 for 3 seconds, Get the file name.
    3. Start temp recording again with 10 seconds buffer.
    4. wait 20 seconds, then see 
    a. if the permenent recording still there.
    b. if Helper_deleterecording get rid of it.
    */
    const TCHAR * pszFnName = TEXT("DeleteRecordedItemTest");

    LONGLONG TempBufferSize = 0;    // set 0 second buffer size to convert so always start a new recording file;
    LONGLONG lBuffer = 10*1000 ;    // Let it run for 20 seconds so any temp recording will appear.
    int iLoop = 5;        // Check file size 5 times

    CSinkFilterGraphEx mySinkFilterGraph;
    //
    LPOLESTR pszFileName = NULL;
    LPOLESTR pszFileName_01 = NULL;
    WIN32_FIND_DATAW wData;
    HANDLE hFile = NULL;
    //
    CComPtr<IDVREngineHelpers> pDVRHelper;
    HRESULT hr = E_FAIL;

    // Default pass
    g_flag = true;

    pszFileName_01 = new WCHAR[120];

    if(pszFileName_01)
    {
        mySinkFilterGraph.LogToConsole(true);
        mySinkFilterGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);

        hr = mySinkFilterGraph.Initialize();

        if(SUCCEEDED(hr))
        {
            // Setting up Filters

            _CHK(mySinkFilterGraph.SetupFilters(TRUE), TEXT("Setting up Sink Graph"));

            // Call BegineTemp Rec
            LPOLESTR pRecordingPath = NULL;
            _CHK(mySinkFilterGraph.SetRecordingPath(), TEXT("SetRecording Path"));
            _CHK(mySinkFilterGraph.GetRecordingPath(&pRecordingPath), TEXT("SetRecording Path"));
            LogText(TEXT("Set recording Path as %s."), pRecordingPath);
            CoTaskMemFree(pRecordingPath);
            pRecordingPath = NULL;

            _CHK(mySinkFilterGraph.BeginTemporaryRecording(lBuffer), TEXT("Start Temp Rec"));
            _CHK(mySinkFilterGraph.Run(), TEXT("Run Graph"));
            // Wait 40 seconds for file creation
            LogText(__LINE__, pszFnName, TEXT("Wait %d Milliseconds."), lBuffer);
            Sleep((DWORD)lBuffer);    // Sleep for one Temp File filled. 
            // Begine Perm Rec
            LogText(__LINE__, pszFnName, TEXT("Call Begin Perm Rec now."));
            //_CHK(mySinkFilterGraph.BeginPermanentRecording(TempBufferSize), TEXT("Begin Perm Recording."));
            _CHK(mySinkFilterGraph.BeginPermanentRecording(0), TEXT("Begin Perm Recording"));

            // Get Cur File
            _CHK(mySinkFilterGraph.GetCurFile(&pszFileName, NULL), TEXT("Get Cur File"));
            LogText(__LINE__, pszFnName, TEXT("The gotten file name is %s"), pszFileName);
            // Sleep another 3 seconds then Check file is there
            Sleep(3*1000);
            // Make the search filename
            _tcscpy(pszFileName_01, pszFileName);
            _tcscat(pszFileName_01, TEXT("*.dvr-dat"));    // The 1st Dat file;
            LogText(TEXT("The search file name is %s."), pszFileName_01);
            // Find the recording file
            hFile = FindFirstFileW(pszFileName_01, &wData);
            if(hFile == 0)
            {
                LogError(__LINE__, pszFnName, TEXT(" Did Find the filename gotten by GetCurFile."));
                g_flag = false;
            }
            else
            {
                LogText(__LINE__, pszFnName, TEXT("Find the filename gotten by GetCurFile; succeed."));
            }
            FindClose(hFile);
            // Now start Temp recording for lBuffer long bugger again.
            _CHK(mySinkFilterGraph.BeginTemporaryRecording(lBuffer), TEXT("Start Temp Rec Again"));
            // wait until out of window
            Sleep((DWORD)(2*lBuffer));    // Make sure Permenant rec out of buffer window
            // Deleted the permanent recording
            _CHK(mySinkFilterGraph.FindInterface(__uuidof(IDVREngineHelpers), (void **)&pDVRHelper), TEXT("Querying Helper interface"));
            _CHK(pDVRHelper->DeleteRecording(pszFileName), TEXT("Delete the out window recorded item."));

            hFile = FindFirstFileW(pszFileName_01, &wData);

            if(hFile != INVALID_HANDLE_VALUE)
            {
                LogError(__LINE__, pszFnName, TEXT("Found the deleted filename gotten by GetCurFile."));
                g_flag = false;
            }
            else
            {
                LogText(__LINE__, pszFnName, TEXT("Did not Find the filename gotten by GetCurFile; succeed."));
            }

            FindClose(hFile);
            CoTaskMemFree(pszFileName);
            hr = mySinkFilterGraph.Stop();
        }

        delete [] pszFileName_01;
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

TESTPROCAPI DeleteInBufferRecordTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    /*    This test make sure in Buffer permanent recording will go back
    to buffer upon deletion

    We will.
    1. Start a Temp recording for 10 seconds, with buffer as 50 seconds
    2. Start a Permanent recording with 0 for 10 seconds, Get the file name.
    3. Start temp recording again with 60 seconds buffer. wait 10 seconds.
    now we have
    [ 10(s) temp -- 10 s Perm -- 10 S Temp  ]
    thay are all in Buffer
    4. Now see
    a. if the permenent recording still there before deletion.
    b. if recording there after Helper_deleterecording deletion.
    c. Sleep 30 s and the recording should be gone (converted to Temp now);
    This is a cool test :)
    */
    const TCHAR * pszFnName = TEXT("DeleteInBufferRecordTest");

    LONGLONG TempBufferSize = 60 * 1000;    // set 60 second buffer size to convert so always start a new recording file;
    int iSleep = 10*1000;            // Sleep 10 seconds for recording to progress.

    CSinkFilterGraphEx mySinkFilterGraph;
    //
    LPOLESTR pszFileName = NULL;
    LPOLESTR pszFileName_01 = new WCHAR[120];
    WIN32_FIND_DATAW wData;
    HANDLE hFile = NULL;
    //
    CComPtr<IDVREngineHelpers> pDVRHelper;
    HRESULT hr = E_FAIL;

    // Default pass
    g_flag = true;

    if(pszFileName_01)
    {
        mySinkFilterGraph.LogToConsole(true);
        mySinkFilterGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);

        hr = mySinkFilterGraph.Initialize();

        if(SUCCEEDED(hr))
        {
            // Setting up Filters
            _CHK(mySinkFilterGraph.SetupFilters(TRUE), TEXT("Setting up Sink Graph."));

            // Call BeginTemp Rec
            LPOLESTR pRecordingPath = NULL;
            _CHK(mySinkFilterGraph.SetRecordingPath(), TEXT("SetRecording Path"));
            _CHK(mySinkFilterGraph.GetRecordingPath(&pRecordingPath), TEXT("SetRecording Path"));
            LogText(TEXT("Set recording Path as %s."), pRecordingPath);
            CoTaskMemFree(pRecordingPath);
            pRecordingPath = NULL;

            _CHK(mySinkFilterGraph.BeginTemporaryRecording(TempBufferSize), TEXT("Start Temp Rec"));
            _CHK(mySinkFilterGraph.Run(), TEXT("Run Graph"));
            // Wait 10 seconds for file creation
            LogText(__LINE__, pszFnName, TEXT("Wait %d Milliseconds."), iSleep);
            Sleep(iSleep);    // Sleep for some Temp File.
            // Begine Perm Rec for 10 seconds
            LogText(__LINE__, pszFnName, TEXT("Call Begin Perm Rec now for 10 seconds."));
            _CHK(mySinkFilterGraph.BeginPermanentRecording(0), TEXT("Begin Perm Recording"));
            // Get Cur File
            _CHK(mySinkFilterGraph.GetCurFile(&pszFileName, NULL), TEXT("Get Cur File"));
            LogText(__LINE__, pszFnName, TEXT("The gotten file name is %s."), pszFileName);
            // Sleep another 10 seconds
            Sleep(iSleep);
            // Make the search filename
            _tcscpy(pszFileName_01, pszFileName);
            _tcscat(pszFileName_01, TEXT("*.dvr-dat"));    // The 1st Dat file;
            LogText(TEXT("The search file name is %s."), pszFileName_01);
            // Find the recording file
            hFile = FindFirstFileW(pszFileName_01, &wData);
            if(hFile == INVALID_HANDLE_VALUE)
            {
                LogError(__LINE__, pszFnName, TEXT(" Did not Find the Perm rec filename gotten by GetCurFile."));
                g_flag = false;
            }
            else
            {
                LogText(__LINE__, pszFnName, TEXT("Find the filename gotten by GetCurFile."));
            }
            FindClose(hFile);
            // Now start Temp recording for TempBufferSize again.
            _CHK(mySinkFilterGraph.BeginTemporaryRecording(TempBufferSize), TEXT("Start Temp Rec Again"));
            // wait 10 seconds
            Sleep(iSleep);    // now Perm is in middle of buffer now
            // Deleted the permanent recording
            _CHK(mySinkFilterGraph.FindInterface(__uuidof(IDVREngineHelpers), (void **)&pDVRHelper), TEXT("Querying Helper interface"));
            _CHK(pDVRHelper->DeleteRecording(pszFileName), TEXT("Delete the IN BUFFER recorded item"));
            // Sleep 1 seconds then check again
            LogText(__LINE__, pszFnName, TEXT("Sleep 1 seconds and check if file is gone. File should stay since it was converted."));
            Sleep(1*1000);
            hFile = FindFirstFileW(pszFileName_01, &wData);
            if(hFile == INVALID_HANDLE_VALUE)
            {
                LogError(__LINE__, pszFnName, TEXT("Did not find the deleted filename gotten by pFileSink->GetCurFile."));
                g_flag = false;
            }
            else
            {
                LogText(__LINE__, pszFnName, TEXT("Find the filename gotten by pFileSink->GetCurFile; succeed."));
            }

            FindClose(hFile);
            // Sleep 40 seconds then check again, file should gone since Temp now
            LogText(__LINE__, pszFnName, TEXT("Sleep TempBufferSize seconds and check if file is gone. File should be gone it was since converted."));
            Sleep((DWORD)(TempBufferSize * 2)); /// multiplying by 2 to give teh dvr engine source some time to delete.
            hFile = FindFirstFileW(pszFileName_01, &wData);
            if(hFile != INVALID_HANDLE_VALUE)
            {
                LogError(__LINE__, pszFnName, TEXT("Find the deleted filename gotten by pFileSink->GetCurFile."));
                g_flag = false;
            }
            else
            {
                LogText(__LINE__, pszFnName, TEXT("Did not find the filename gotten by pFileSink->GetCurFile; succeed."));
            }
            FindClose(hFile);

            CoTaskMemFree(pszFileName);
            hr = mySinkFilterGraph.Stop();
        }

        delete [] pszFileName_01;
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

TESTPROCAPI CleanupOrphanedRecordingsTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // This test will build 2 Graph. Sink1 and Sink2
    // Use Sink1 to create some Temp Rec and Perm Rec
    // Destroy Sink1 then we have Perm Rec and Orphaned Rec
    // The use Sink2 to Clean up and make sure:
    // (a). Temp are gone    (b). Perm stays


    const TCHAR * pszFnName = TEXT("CleanupOrphanedRecordingsTest");

    LONGLONG TempBufferSize = 30*1000;    // set 20 fpr temp
    LONGLONG PermSize = 10*1000;    // Perm run 10 seconds

    CSinkFilterGraphEx * pMySink1 = new CSinkFilterGraphEx();
    CSinkFilterGraphEx * pMySink2 = new CSinkFilterGraphEx();
    //
    LPOLESTR pszTempRecName = NULL;
    LPOLESTR pszPermRecName = NULL;
    LPWSTR pszTempRecNameSH = new WCHAR[256];
    LPWSTR pszTempRecNameSH_idx = new WCHAR[256];
    LPWSTR pszPermRecNameSH = new WCHAR[256];
    WIN32_FIND_DATAW wData;
    HANDLE hFile = NULL;
    //
    CComPtr<IDVREngineHelpers> pDVRHelper;
    HRESULT hr = E_FAIL;

    // Default pass
    g_flag = true;

    if(pMySink1 && pMySink2 && pszTempRecNameSH && pszPermRecNameSH)
    {
        pMySink1->LogToConsole(true);
        pMySink2->LogToConsole(true);
        pMySink1->SetCommandLine(g_pShellInfo->szDllCmdLine);
        pMySink2->SetCommandLine(g_pShellInfo->szDllCmdLine);

        hr = pMySink1->Initialize();

        if(SUCCEEDED(hr))
        {
            // Setting up Filters
            _CHK(pMySink1->SetupFilters(TRUE), TEXT("Setting up Sink1 Graph."));

            // Call BegineTemp Rec
            LPOLESTR pRecordingPath = NULL;
            _CHK(pMySink1->SetRecordingPath(), TEXT("SetRecording Path"));
            _CHK(pMySink1->GetRecordingPath(&pRecordingPath), TEXT("SetRecording Path"));
            LogText(TEXT("Set recording Path as %s."), pRecordingPath);

            // Start Perm first
            LogText(__LINE__, pszFnName, TEXT("Call Begin Perm Rec now."));
            // Begine Perm Rec
            _CHK(pMySink1->BeginPermanentRecording(0), TEXT("Begin Perm Recording"));
            _CHK(pMySink1->Run(), TEXT("Run Graph"));
            // Get Perm Cur File
            _CHK(pMySink1->GetCurFile(&pszPermRecName, NULL), TEXT("Get Perm Cur File"));
            LogText(__LINE__, pszFnName, TEXT("The gotten Perm file name is %s."), pszPermRecName);
            // Sleep a while
            Sleep((DWORD)PermSize);
            _CHK(pMySink1->BeginTemporaryRecording(TempBufferSize), TEXT("Start Temp Rec"));
            // Get Temp File
            _CHK(pMySink1->GetCurFile(&pszTempRecName, NULL), TEXT("Get temp Recording FileName"));
            LogText(__LINE__, pszFnName, TEXT("Cur Temp Rec Filename is %s."), pszTempRecName);
            // Wait TempBufferSize seconds for file creation
            LogText(__LINE__, pszFnName, TEXT("Wait %d Milliseconds."), TempBufferSize);
            Sleep((DWORD)TempBufferSize);    // Sleep for one Temp File filled. 

            // create the file names for the temporary idx and dat files
            _tcscpy(pszTempRecNameSH, pszTempRecName);
            _tcscat(pszTempRecNameSH, TEXT("*.dvr-dat"));    // The 1st Dat file;
            _tcscpy(pszTempRecNameSH_idx, pszTempRecName);
            _tcscat(pszTempRecNameSH_idx, TEXT("*.dvr-idx"));    // The 1st idx file;

            // The DVR engine now cleans up after itself, so we have to force it to orphan the temporary recording.
            // we can do this by opening the file for reading, resulting in open file handles and the DeleteFile in the
            // DVREngine writer destructor failing to delete the file.

            HANDLE hTempDatFile = NULL;
            HANDLE hTempIDXFile = NULL;
            int index = 0;

            hFile = FindFirstFileW(pszTempRecNameSH, &wData);
            if(hFile == INVALID_HANDLE_VALUE)
            {
                LogError(__LINE__, pszFnName, TEXT(" Did not Find the Temp filename gotten by pFileSink->GetCurFile."));
                g_flag = false;
            }

            for(index = _tcslen(pszTempRecNameSH); index > 0 && pszTempRecNameSH[index] != '\\'; index--);
            pszTempRecNameSH[index + 1] = NULL;
            _tcsncat(pszTempRecNameSH, wData.cFileName, _tcslen(wData.cFileName));
    
            hTempDatFile = CreateFile(pszTempRecNameSH, GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if(hTempDatFile == INVALID_HANDLE_VALUE)
            {
                LogError(__LINE__, pszFnName, TEXT("Failed to open the temp dat file for reading."));
                g_flag = false;
            }
            FindClose(hFile);

            hFile = FindFirstFileW(pszTempRecNameSH_idx, &wData);
            if(hFile == INVALID_HANDLE_VALUE)
            {
                LogError(__LINE__, pszFnName, TEXT(" Did not Find the Temp filename gotten by pFileSink->GetCurFile."));
                g_flag = false;
            }

            for(index = _tcslen(pszTempRecNameSH_idx); index > 0 && pszTempRecNameSH_idx[index] != '\\'; index--);
            pszTempRecNameSH_idx[index + 1] = NULL;
            _tcsncat(pszTempRecNameSH_idx, wData.cFileName, _tcslen(wData.cFileName));

            hTempIDXFile = CreateFile(pszTempRecNameSH_idx, GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if(hTempIDXFile == INVALID_HANDLE_VALUE)
            {
                LogError(__LINE__, pszFnName, TEXT("Failed to open the temp idx file for reading."));
                g_flag = false;
            }
            FindClose(hFile);

            // Destroy the 1st Graph
            _CHK(pMySink1->Stop(), TEXT("Stop Sink1"));
            pMySink1->UnInitialize();

            delete pMySink1;
            pMySink1 = NULL;

            // now that the sink graph is destroyed, we can release our file handles.
            CloseHandle(hTempDatFile);
            CloseHandle(hTempIDXFile);

            // now let's verify everything worked as expected.

            // Make the search filename
            _tcscpy(pszTempRecNameSH, pszTempRecName);
            _tcscat(pszTempRecNameSH, TEXT("*.dvr-dat"));    // The 1st Dat file;
            LogText(__LINE__, pszFnName, TEXT("The search Temp file name is %s."), pszTempRecNameSH);
        
            _tcscpy(pszPermRecNameSH, pszPermRecName);
            _tcscat(pszPermRecNameSH, TEXT("*.dvr-dat"));    // The 1st Dat file;
            LogText(__LINE__, pszFnName, TEXT("The search Perm file name is %s."), pszPermRecNameSH);
            // Find the recording file
            hFile = FindFirstFileW(pszTempRecNameSH, &wData);
            if(hFile == INVALID_HANDLE_VALUE)
            {
                LogError(__LINE__, pszFnName, TEXT(" Did not Find the Temp filename gotten by pFileSink->GetCurFile."));
                g_flag = false;
            }
            else
            {
                LogText(__LINE__, pszFnName, TEXT("Find the Temp filename gotten by pFileSink->GetCurFile; succeed."));
            }
            FindClose(hFile);
            hFile = FindFirstFileW(pszPermRecNameSH, &wData);
            if(hFile == INVALID_HANDLE_VALUE)
            {
                LogError(__LINE__, pszFnName, TEXT(" Did not Find the Perm filename gotten by pFileSink->GetCurFile."));
                g_flag = false;
            }
            else
            {
                LogText(__LINE__, pszFnName, TEXT("Find the Perm filename gotten by pFileSink->GetCurFile; succeed."));
            }
            FindClose(hFile);
            // Now use ClearOphan to see if Temp is gone and Perm stays
            _CHK(pMySink2->Initialize(), TEXT("Sink2 Init"));

            _CHK(pMySink2->SetupFilters(TRUE), TEXT("Sink2 Setup Graph"));

            _CHK(pMySink2->CleanupOrphanedRecordings(pRecordingPath), TEXT("Sink2 Clean Orphaned Files"));
            // Search Temp and Perm Rec again
            // Find the recording file
            hFile = FindFirstFileW(pszTempRecNameSH, &wData);
            if(hFile != INVALID_HANDLE_VALUE)
            {
                LogError(__LINE__, pszFnName, TEXT(" Find the Temp filename After CleanOrphan."));
                g_flag = false;
            }
            else
            {
                LogText(__LINE__, pszFnName, TEXT("Did not find the Temp filename after cleanOrphan; succeed."));
            }
            FindClose(hFile);
            hFile = FindFirstFileW(pszPermRecNameSH, &wData);
            if(hFile == INVALID_HANDLE_VALUE)
            {
                LogError(__LINE__, pszFnName, TEXT(" Did not Find the Perm filename After CleanOrphan."));
                g_flag = false;
            }
            else
            {
                LogText(__LINE__, pszFnName, TEXT("Find the Perm filename after cleanOrphan."));
            }
            FindClose(hFile);

            CoTaskMemFree(pRecordingPath);
            pRecordingPath = NULL;
            delete pMySink2;
            pMySink2 = NULL;
        }
    }
    else
    {
        LogError(__LINE__, pszFnName, TEXT(" Failed to allocate my sink1, mysink2, or one of the record names."));
        g_flag = false;
    }


    if(pMySink1)
        delete pMySink1;
    if(pMySink2)
        delete pMySink2;
    if(pszTempRecNameSH)
        delete [] pszTempRecNameSH;
    if(pszTempRecNameSH_idx)
        delete [] pszTempRecNameSH_idx;
    if(pszPermRecNameSH)
        delete [] pszPermRecNameSH;

    if(FAILED(hr) || (g_flag == false))
    {
        return TPR_FAIL;
    }
    else
    {
        return TPR_PASS;
    }
}

#ifdef BVT_USETEMPMETHOD    //    Remove Temp Code
TESTPROCAPI Test_Em10Tune(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Put Test1.wmv as Byron10.wmv and test2.wmv as byron11.wmv.
    const TCHAR * pszFnName = TEXT("Test_Em10Tune");

    DWORD result = TPR_FAIL;
    HRESULT hr = E_FAIL;
    IBaseFilter * pFilter = NULL;
    CComPtr<IAMTVTuner> pTvTuner = NULL;
    //CComPtr<IAMCrossbar> pCrossbar = NULL;
    long lFoundSignal = 0;
    long lchannel = 0;

    CSourceFilterGraphEx mySource;

    mySource.LogToConsole(true);
    mySource.SetCommandLine(g_pShellInfo->szDllCmdLine);

    mySource.Initialize();
    _CHK(mySource.SetupDemuxDirectPlayBackFromFile(MPGFile), TEXT("Setup WMV09 File Playback"));
    _CHK(mySource.FindInterface(IID_IAMTVTuner, (void **)&pTvTuner), TEXT("Finding TVTuner interface"));
    // CrossBar
    /*
    _CHK(mySource.FindInterface(IID_IAMCrossbar, (void **)&pCrossbar), TEXT("Finding pCrossbar interface."));
    long lOutputPinCount, lInputPinCount;
    _CHK(pCrossbar->get_PinCounts( &lOutputPinCount, &lInputPinCount ), TEXT("Get crossbar Pin Counts"));
    LogText(__LINE__, pszFnName, TEXT("Crossbar output pin count: %d, input pin count: %d."), lOutputPinCount, lInputPinCount);
    */
    mySource.Run();

    Sleep(15*1000);
    lFoundSignal = 0;
    lchannel = 1;
    _CHK(pTvTuner->AutoTune(lchannel, &lFoundSignal), TEXT("Tune to 1"));
    LogText(__LINE__, pszFnName, TEXT("Tune to Channel 1, signal is %d."), lFoundSignal);

    Sleep(15*1000);
    lFoundSignal = 0;
    lchannel = 2;
    _CHK(pTvTuner->AutoTune(lchannel, &lFoundSignal), TEXT("Tune to 2"));
    LogText(__LINE__, pszFnName, TEXT("Tune to Channel 2, signal is %d."), lFoundSignal);

    Sleep(15*1000);
    lFoundSignal = 0;
    lchannel = 1;
    _CHK(pTvTuner->AutoTune(lchannel, &lFoundSignal),TEXT("Tune to 1"));
    LogText(__LINE__, pszFnName, TEXT("Tune to Channel 1, signal is %d."), lFoundSignal);

    Sleep(10*1000);
    return result;
}

TESTPROCAPI Test_Em10Tune01(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Put Test1.wmv as Byron10.wmv and test2.wmv as byron11.wmv.
    const TCHAR * pszFnName = TEXT("Test_Em10Tune01");

    DWORD result = TPR_FAIL;
    HRESULT hr = E_FAIL;
    long lFoundSignal = 0;
    long lchannel = 0;

    CFilterGraph mySource;
    CComPtr<IBaseFilter> pEm10 = NULL;
    CComPtr<IAMTVTuner> pTvTuner = NULL;
    CComPtr<IBaseFilter> pDemux = NULL;

    mySource.LogToConsole(true);    // no popup window;
    mySource.SetCommandLine(g_pShellInfo->szDllCmdLine);

    mySource.Initialize();
    _CHK(mySource.AddFilterByCLSID(CLSID_Babylon_EM10, TEXT("Em10 Src"), &pEm10), TEXT("Adding EM10"));
    _CHK(mySource.FindInterface(IID_IAMTVTuner, (void **)&pTvTuner), TEXT("Find IAMTvTuner Interface"));
    lchannel = 2;
    _CHK(pTvTuner->put_Channel(lchannel,AMTUNER_SUBCHAN_DEFAULT,AMTUNER_SUBCHAN_DEFAULT), TEXT("IAMTvTuner tune to channel lchannel"));
    LogText(__LINE__, pszFnName, TEXT("Tune to channel %d, signal strength is %d."), lchannel, lFoundSignal);
    // Now building graph
    _CHK(mySource.AddFilterByCLSID(CLSID_Demux, TEXT("ASF Demux"), &pDemux), TEXT("Adding Demux"));
    _CHK(mySource.ConnectFilters(pEm10, pDemux), TEXT("Connecting Em10 to Demux"));
    _CHK(mySource.RenderFilter(pDemux), TEXT("render Demux"));
    Sleep(3*1000);

    lchannel = 1;
    _CHK(pTvTuner->AutoTune(lchannel, &lFoundSignal), TEXT("IAMTvTuner autotune to channel lchannel"));
    _CHK(pTvTuner->StoreAutoTune(), TEXT("IAMTvTuner store autotune"));

    _CHK(pTvTuner->put_Channel(lchannel,AMTUNER_SUBCHAN_DEFAULT,AMTUNER_SUBCHAN_DEFAULT), TEXT("IAMTvTuner tune to channel lchannel"));
    LogText(__LINE__, pszFnName, TEXT("Tune to channel %d."), lchannel);
    _CHK(mySource.Run(), TEXT("Run Graph"));

    int i = 1;
    while(true)
    {
        lchannel = 1;
        _CHK(pTvTuner->put_Channel(lchannel,AMTUNER_SUBCHAN_DEFAULT,AMTUNER_SUBCHAN_DEFAULT), TEXT("IAMTvTuner tune to channel lchannel"));
        LogText(__LINE__, pszFnName, TEXT("Tune to channel %d."), lchannel);
        Sleep(10*1000);
        lchannel = 2;
        _CHK(pTvTuner->put_Channel(lchannel,AMTUNER_SUBCHAN_DEFAULT,AMTUNER_SUBCHAN_DEFAULT), TEXT("IAMTvTuner tune to channel lchannel"));
        LogText(__LINE__, pszFnName, TEXT("Tune to channel %d."), lchannel);
        Sleep(10*1000);
    }

    return result;
}


TESTPROCAPI Test_PauseAndPlay(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    // Just tune, start a sink, start a source, play, pause and play again
    const TCHAR * pszFnName = TEXT("Test_PauseAndPlay");

    DWORD result = TPR_FAIL;
    HRESULT hr = E_FAIL;
    long lFoundSignal = 0;
    long lchannel = 0;
    UINT32 uiSourceStamp = 0;
    UINT32 uiSinkStamp = 0;

    CSinkFilterGraph mySink;
    CSourceFilterGraphEx mySource;

    LPOLESTR lpLiveToken = NULL;

    g_flag = true;

    mySink.LogToConsole(true);    // no popup window;
    mySource.LogToConsole(true);

    mySink.SetCommandLine(g_pShellInfo->szDllCmdLine);
    mySource.SetCommandLine(g_pShellInfo->szDllCmdLine);

    mySink.Initialize();

    _CHK(mySink.SetupFilters(TRUE), TEXT("mySink.SetupFilters"));

    lchannel = 2;
    _CHK(mySink.TuneTo(lchannel,AMTUNER_SUBCHAN_DEFAULT,AMTUNER_SUBCHAN_DEFAULT), TEXT("IAMTvTuner putchannel to channel lchannel"));
    LogText(__LINE__, pszFnName, TEXT("Tune to channel %d."), lchannel);

    _CHK(mySink.SetRecordingPath(), TEXT("Setup recording path"));
    _CHK(mySink.BeginTemporaryRecording(300*1000), TEXT("SBC Beging Temp Rec"));

    Sleep(3*1000);

    lchannel = 1;
    //    _CHK(pTvTuner->AutoTune(lchannel, &lFoundSignal), TEXT("IAMTvTuner autotune to channel lchannel."));
    //    _CHK(pTvTuner->StoreAutoTune(), TEXT("IAMTvTuner store autotune."));
    // Start Sink
    _CHK(mySink.Run(), TEXT("Start Sink"));
    _CHK(mySink.GetBoundToLiveToken(&lpLiveToken), TEXT("get live token now"));
    // Setup Source
    LogText(__LINE__, pszFnName, TEXT("Sleep 30 sec."));
    Sleep(10*1000);    // Let system rest a while
    _CHK(mySource.Initialize(), TEXT("Initialize Source"));
    _CHK(mySource.SetupFilters(lpLiveToken, TRUE), TEXT("source setup filters"));
    _CHK(mySource.Run(), TEXT("source Run"));
    Sleep(30*1000);
    _CHK(mySource.Pause(), TEXT("source Pause 30 sec"));
    Sleep(30*1000);
    _CHK(mySource.Run(), TEXT("source Run again"));
    Sleep(30*1000);


    if(g_flag == false)
    {
        return TPR_FAIL;
    }
    else
    {
        return TPR_PASS;
    }
}

TESTPROCAPI Test_FreePlayPerm(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    /*    
    This test will send 1000 asf sample to DVR engine (app 65 MB). Use a GrabberSample to detect if stream is
    going and read back correctly. Stream are stamped with a unique number from 1 to UINT32.MAX. Just make sure
    we read back from 1 to whatever we write to disk.
    */

    const TCHAR * pszFnName = TEXT("Test_FreePlayPerm");

    CSinkFilterGraphEx mySinkGraph;
    CSourceFilterGraphEx mySourceGraph;

    UINT32 ui32StartCount = 0;
    UINT32 ui32EndCount = 0;
    int iNumOfSample = 300;    // Default send 1000 Samples
    UINT32 ui32Time = iNumOfSample / 20;
    LPOLESTR wszFileName = NULL;


    g_flag = true;        //set default as true;
    mySourceGraph.LogToConsole(true);
    mySinkGraph.LogToConsole(true);

    mySinkGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);
    mySourceGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);

    //Set up the Sink Graph with EM10 + Grabber + DVR sink
    mySinkGraph.Initialize();

    _CHK(mySinkGraph.SetupFilters(TRUE), TEXT("Setting up sink graph"));

    LPOLESTR pRecordingPath = NULL;
    _CHK(mySinkGraph.SetRecordingPath(), TEXT("SetRecording Path"));
    _CHK(mySinkGraph.GetRecordingPath(&pRecordingPath), TEXT("SetRecording Path"));
    LogText(TEXT("Set recording Path as %s."), pRecordingPath);
    CoTaskMemFree(pRecordingPath);
    pRecordingPath = NULL;

    _CHK(mySinkGraph.BeginPermanentRecording(0), TEXT("Sink BeginPermanentRecording"));
    mySinkGraph.Run();
    LogText(__LINE__, pszFnName, TEXT("Sleep 5 sec."));
    Sleep(5 * 1000);

    // Get Current Perm File Name
    _CHK(mySinkGraph.GetCurFile(&wszFileName ,NULL), TEXT("Get Perm Rec File Name"));
    LogText(__LINE__, pszFnName, TEXT("Recording to file : %s."), wszFileName);
    // Now set up playback filter; DVR source + Grabber + Demux;
    _CHK(mySourceGraph.Initialize(), TEXT("Init Source FilterGraph"));
    _CHK(mySourceGraph.SetupFilters(wszFileName), TEXT("Setup Source Graph"));
    LogText(__LINE__, pszFnName, TEXT("Sleep 3 second."));
    Sleep(3 * 1000);

    _CHK(mySourceGraph.Run() ,TEXT("Run Source Graph"));
    LogText(__LINE__, pszFnName, TEXT("Sleep %d seconds."), ui32Time + 5);
    Sleep((ui32Time + 5) * 1000);
    // Stop graph
    _CHK(mySourceGraph.Stop(), TEXT("Stop source Graph"));
    _CHK(mySinkGraph.Stop(), TEXT("Stop sink Graph"));    

    if(g_flag == false)
        return TPR_FAIL;
    return TPR_PASS;
}
TESTPROCAPI CleanTempPlayback(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    NO_MESSAGES;

    /*    
    This test will send 1000 asf sample to DVR engine (app 65 MB). Use a GrabberSample to detect if stream is
    going and read back correctly. Stream are stamped with a unique number from 1 to UINT32.MAX. Just make sure
    we read back from 1 to whatever we write to disk.
    */
    const TCHAR * pszFnName = TEXT("CleanTempPlayback");

    CSinkFilterGraph mySinkGraph;
    CSourceFilterGraph mySourceGraph;

    LPOLESTR wszLiveToken = NULL;

    mySinkGraph.LogToConsolre(true);
    mySinkGraph.SetCommandLine(g_pShellInfo->szDllCmdLine);

    g_flag = true;        //set default as true;
    //Set up the Sink Graph with Em10 + Grabber + DVR sink
    mySinkGraph.Initialize();

    _CHK(mySinkGraph.SetupFilters(), TEXT("Setting up sink graph"));

    _CHK(mySinkGraph.SetRecordingPath(), TEXT("SetRecording Path"));
    _CHK(mySinkGraph.BeginTemporaryRecording(3000*1000), TEXT("SBC BeginTemporaryRecording"));
    mySinkGraph.Run();
    LogText(__LINE__, pszFnName, TEXT("Sleep 5 seconds."));
    Sleep(5 * 1000);

    _CHK(mySinkGraph.GetBoundToLiveToken(&wszLiveToken), TEXT("Sink GetBoundToLiveToken"));
    LogText(__LINE__, pszFnName, TEXT("LiveToken is : %s"), wszLiveToken);
    // Now set up playback filter; DVR source + Grabber + Demux;
    _CHK(mySourceGraph.Initialize(), TEXT("Init Source FilterGraph"));
    _CHK(mySourceGraph.SetupFilters(wszLiveToken), TEXT("Source setup FilterGraph"));
    Sleep(5*1000);
    _CHK(mySourceGraph.Run() ,TEXT("Run Source Graph"));
    Sleep(15 * 1000);
    return TPR_PASS;
}
#endif

