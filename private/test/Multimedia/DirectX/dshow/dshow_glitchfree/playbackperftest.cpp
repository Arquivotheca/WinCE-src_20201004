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
#include <winsock2.h>
#include <iphlpapi.h>
#include "utility.h"
#include "PerfTest.h"
#include "PlaylistParser.h"

static const DWORD MAX_VMR_INPUTS = 16;

CPlaylistParser g_PlaylistParser;

//this is the max time we will wait, for the clip to end,
//if we can't get duration of clip (currently 10 min)
//update this if it is too short, we do not want to wait infinately
#define MAX_WAIT 600000

CPlaybackTest::CPlaybackTest()
{
    LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);
    
    Init();
    
    LOGZONES(TEXT("%S: EXIT%d@%S"), __FUNCTION__, __LINE__, __FILE__);
}

CPlaybackTest::~CPlaybackTest()
{
}

void CPlaybackTest::Init()
{
    LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);
    
    EnableBWSwitch(false);
    pCPUMon = NULL;

    LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
}

TESTPROCAPI CPlaybackTest::Execute(PerfTestConfig* pConfig)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    HANDLE hLogFile = 0;

    int nMediaClips = 0;     // Number of media clips in the playlist
    bool bAllClips = 0;     // Should we play all clips
    int nClipIDs = 0; // Number of media clips to play through
    int nProtocols = 0; // Number of protocols to test for
    bool bAllProtocols = false; // Should we use all protocols
    
    int nSkippedClips = 0;
    int nFailedTests = 0;
    int nSkippedProtocols = 0;

    LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);
    
    // Set pointer to the global singleton parser
    CPlaylistParser *pPlaylist = &g_PlaylistParser;

    // Log file for the overall stats
    if (pConfig->UsePerflog())
    {
        TCHAR szStatsLogFile[256] = {0};
        StringCchPrintf(szStatsLogFile, 256, TEXT("\\release\\dshow_pb_stats.log"));
        hLogFile = CreateFile(szStatsLogFile, GENERIC_WRITE, 0, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (!hLogFile)
        {
            LOG(TEXT("%S: ERROR %d@%S - Failed to create stats log: %s (GLE: 0x%0x)"), __FUNCTION__, __LINE__, __FILE__, szStatsLogFile, GetLastError());
            retval = TPR_FAIL;
            goto EXECUTE_EXIT;            
        }
        else {
            SetFilePointer(hLogFile, 0, 0, FILE_END);
        }
    }

    // setup COM
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        LOG(TEXT("%S: ERROR %d@%S - failed to to initialize COM. CoInitializeEx returned (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
        goto EXECUTE_EXIT;        
    }

    // The media clips to run this on
    nMediaClips = pPlaylist->GetNumMediaClips();
    if (!pConfig->GetNumClipIDs() || !_tcsncmp(pConfig->GetClipID(0), ALL, Countof(ALL)))
    {
        // All clips
        nClipIDs = nMediaClips;
        bAllClips = true;
    }
    else
    {
        nClipIDs = pConfig->GetNumClipIDs();
    }

    // What protocols do we need to use?
    if (!pConfig->GetNumProtocols() || !_tcsncmp(pConfig->GetProtocol(0), ALL, Countof(ALL)))
    {
        nProtocols = nSupportedProtocols;
        bAllProtocols = true;
    }
    else {
        nProtocols = pConfig->GetNumProtocols();
    }

    // If it's alpha blending test, make sure there are at least two clips
    if ( pConfig->IsAlphaBlend() )
    {
        if ( nClipIDs > MAX_VMR_INPUTS )
        {
            LOG( TEXT("%S: WARNING %d@%S - Invalid number of streams (%d) for alpha blending test"), __FUNCTION__, __LINE__, __FILE__, nClipIDs );
            retval = TPR_SKIP;
            goto EXECUTE_EXIT;        
        }
    }

    // Run over multiple repetitions
    for(int rep = 0; rep < pConfig->GetRepeat(); rep++)
    {
        LOG(_T("\n\n%S: Repetition: %d"), __FUNCTION__, rep);

        if ( pConfig->IsAlphaBlend() )
        {
            retval = AlphaBlendingTest( hLogFile, pConfig, pPlaylist, rep );
            // skip the rest of the loop
            continue;
        }

        // Run over the clip indices (not clip ids - clip ids are unique values but not necessarily in sequence or even numbers)
        for(int clipi = 0; clipi < nClipIDs; clipi++)
        {
            // Get the clip id
            TCHAR szClipID[MAX_PATH] = TEXT("");
            TCHAR szProto[MAX_PATH] = TEXT("");
            TCHAR *szUrl[1] = {0};
            
            // Get the clip id for this index
            if (bAllClips) 
                pPlaylist->GetMediaClipID(clipi, szClipID, MAX_PATH);
            else
                pConfig->GetClipID(clipi, szClipID);

            // Get the media content class for this clip index
            CMediaContent* pMediaContent = pPlaylist->GetMediaContent(szClipID);

            if (!pMediaContent)
            {
                LOG(TEXT("%S: WARNING %d@%S - Clip id not found in xml file: %s . Skipping this file"), __FUNCTION__, __LINE__, __FILE__, szClipID);            
                nSkippedClips++;
                continue;
            }
            
            // If we are using DRM
            if (pConfig->UseDRM())
                pMediaContent->m_bDRM = true;
            
            TCHAR szClipName[MAX_PATH];
            MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, (pMediaContent->m_bDRM) ? pMediaContent->m_DRMClipName.c_str() : pMediaContent->m_ClipName.c_str(), -1, szClipName, Countof(szClipName));

            //run each clip for every protocols required
            szUrl[0] = new TCHAR[MAX_PATH];
            
            for (int protoi = 0; protoi < nProtocols; protoi++)
            {
                if (bAllProtocols) {
                    errno_t err = _tcsncpy_s(szProto, sizeof(szProto), SupportedProtocols[protoi], _TRUNCATE);
                    if(err == 0 || err == 80)
                    {
                        szProto[Countof(szProto) - 1] = 0;
                    }
                    else
                    {
                        LOG(TEXT("%S: Failed copying the STRING"), __FUNCTION__);
                        retval = TPR_FAIL;
                    }
                }
                else {
                    pConfig->GetProtocol(protoi, szProto);
                }
            
                ZeroMemory( szUrl[0], MAX_PATH );
                // Get the url from the media content given protocol
                LocationToPlay(szUrl[0], szProto, pMediaContent);

                // If the url is empty, continue
                if (!_tcsncmp(szUrl[0], TEXT(""), 1))
                {
                    LOG(TEXT("%S: WARNING %d@%S - Could not get url for proto %s URL for clip: %s . Skipping this file"), __FUNCTION__, __LINE__, __FILE__, szProto, szClipID);            
                    nSkippedProtocols++;
                    continue;
                }

                //if we are asked for CPU/MEM status, set it up
                if (pConfig->GetNumStatusToReport() > 0)
                {
                    if (!pCPUMon)
                    {
                        TCHAR szCpuLogName[256];
                        //create a new CPU log for each clip, each iteration, each protocol
                        _stprintf_s(szCpuLogName, sizeof(szCpuLogName), _T("\\release\\dshow_pb_cpu_%s_%s_%d.txt"), szClipName, szProto, rep);
                        LOG(TEXT("%S: Starting CPUMon with %s"), __FUNCTION__, szCpuLogName);            
                        
                        pCPUMon = new CPerformanceMonitor(pConfig->GetSampleInterval(), szCpuLogName);
                    }
                    
                    if (!pCPUMon)
                    {
                        LOG(TEXT("%S: ERROR %d@%S - Failed to start CPU monitor"), __FUNCTION__, __LINE__, __FILE__);            
                        if (pCPUMon) {
                            delete pCPUMon;
                            pCPUMon = NULL;
                        }
                    } else {
                        if(pConfig->StatusToReport(STATUS_MEM)) {
                            LOG(TEXT("%S: Starting Memory Monitor"), __FUNCTION__);            
                            pCPUMon->SetMemoryMonitor();
                        }

                    }
                }

                LOG(TEXT("%S: running test with url %s"), __FUNCTION__, szUrl[0]);
                retval = PlaybackPerfTest( pConfig, pMediaContent, szUrl );

                // Assign result as 100 for pass, 50 for skip, 0 for fail/abort
                int result = 0;
                switch (retval)
                {
                case TPR_SKIP:
                    LOG(TEXT("%S: test skipped for url %s"), __FUNCTION__, szUrl[0]);
                    result = 50;
                    break;

                case TPR_PASS:
                    LOG(TEXT("%S: test passed for url %s"), __FUNCTION__, szUrl[0]);
                    result = 100;
                    break;
                
                default:
                    LOG(TEXT("%S: test failed for url %s"), __FUNCTION__, szUrl[0]);
                    nFailedTests++;
                    result = 0;
                    break;
                }

                //if perf is to be logged, write up this clip's data to file
                if (pConfig->UsePerflog())
                {
                
                    // Log the overall stats: clipid, protocol, utilization, dropped, drawn, avgUtil
                    TCHAR logstr[256] = {0};
                    DWORD nBytesWritten = 0;
                    StringCchPrintf(logstr, 256, TEXT("%s,%s,%d,%d,%d,%d\r\n"), szClipID, szProto, 
                        result,
                        (pbStats.droppedFramesDecoder + pbStats.droppedFramesRenderer + pbStats.droppedFramesDelivery),
                        pbStats.framesDrawn, 
                        pbStats.avgCpuUtilization);
                    if ( !WriteFile(hLogFile, logstr, _tcslen(logstr)*sizeof(TCHAR), &nBytesWritten, NULL) )
                    {
                        DWORD dwLastErr = GetLastError();
                        LOG( TEXT("Writing to the log file failed!!! Error code: %0x8"), dwLastErr );
                    }
                    LOGZONES(TEXT("%S: Logging data to log file \n %s"), __FUNCTION__, logstr);
                    LOGZONES(TEXT("Frames dropped in decoder: %d\n"), pbStats.droppedFramesDecoder);
                    LOGZONES(TEXT("Frames dropped in renderer: %d\n"), pbStats.droppedFramesRenderer);    
                    LOGZONES(TEXT("Frames dropped during delivery: %d\n"), pbStats.droppedFramesDelivery);                        
                    LOGZONES(TEXT("Total frames drawn: %d\n"), pbStats.framesDrawn);            
                    LOGZONES(TEXT("Average CPU utilization: %d\n"), pbStats.avgCpuUtilization);                                            
                }
                // Print out CPU warning if it exists
                if (pConfig->StatusToReport(STATUS_CPU) && pCPUMon)
                {
                    //print out warning if CPU was ever over 90% utalized
                    if(pCPUMon->WasPegged()) {
                        LOG(TEXT("%S: WARNING %d@%S - CPU utalization went over 90% for this clip."), __FUNCTION__, __LINE__, __FILE__);
                    }
                }

                //start with a new status monitor every time
                if ((pConfig->GetNumStatusToReport() > 0) && pCPUMon)
                {
                    delete pCPUMon;
                    pCPUMon = NULL;
                }
            }
            delete [] szUrl[0];
        }
    }

    retval = (nFailedTests) ? TPR_FAIL : ((nSkippedClips) ? TPR_SKIP : TPR_PASS);

     // Unitialize COM
    CoUninitialize();

EXECUTE_EXIT:

    if (pConfig->UsePerflog())
    {
        if (hLogFile)
            CloseHandle(hLogFile);
    }
    LOG(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    return retval;
}

TESTPROCAPI 
CPlaybackTest::AlphaBlendingTest( HANDLE hLogFile, PerfTestConfig* pConfig, CPlaylistParser *pPlaylist, int rep )
{
    DWORD retval = TPR_PASS;
    int nFailedTests = 0;
    TCHAR *szUrl[1] = {0};
    DWORD nClipIDs = pConfig->GetNumClipIDs();
    DWORD nProtocols = pConfig->GetNumProtocols();

    szUrl[0] = new TCHAR[MAX_PATH];
    if ( !szUrl[0] )
        goto error;
        
    CMediaContent *VMRStreams[MAX_VMR_INPUTS] = {0};    
    // create a unique string for logging
    TCHAR IDs[MAX_PATH] = {0};
    for ( DWORD i = 0; i < nClipIDs; i++ )
    {
        TCHAR szClipID[MAX_PATH] = TEXT("");
        if ( 0 != IDs[0] )
            StringCchCat( IDs, MAX_PATH, TEXT(":") );
        pConfig->GetClipID( i, szClipID );
        StringCchCat( IDs, MAX_PATH, szClipID );
    }

    for ( DWORD protoi = 0; protoi < nProtocols; protoi++ )
    {
        TCHAR szProto[MAX_PATH] = TEXT("");
        pConfig->GetProtocol( protoi, szProto );

        TCHAR szClipID[MAX_PATH] = TEXT("");
        ZeroMemory( szUrl[0], MAX_PATH );
        for ( DWORD i = 0; i < nClipIDs; i++ )
        {
            pConfig->GetClipID( i, szClipID );
            VMRStreams[i] = pPlaylist->GetMediaContent(szClipID);
            if ( !VMRStreams[i] )
            {
                LOG( TEXT("%S: WARNING %d@%S - Clip id %d not found in xml file: %s . Skipping the test"), 
                            __FUNCTION__, __LINE__, __FILE__, i, szClipID );
                retval = TPR_SKIP;
                goto error;
            }

            // Get the url from the media content given protocol
            LocationToPlay( szUrl[0], szProto, VMRStreams[i] );
        }

        TCHAR szClipName[MAX_PATH] = {0};
        MultiByteToWideChar( CP_ACP, 
                             MB_PRECOMPOSED, 
                             (VMRStreams[0]->m_bDRM) ? VMRStreams[0]->m_DRMClipName.c_str() : VMRStreams[0]->m_ClipName.c_str(), 
                             -1, 
                             szClipName, 
                             Countof(szClipName) );

        //if we are asked for CPU/MEM status, set it up
        if (pConfig->GetNumStatusToReport() > 0)
        {
            if (!pCPUMon)
            {
                TCHAR szCpuLogName[256];
                //create a new CPU log for each iteration, each protocol
                _stprintf_s(szCpuLogName, sizeof(szCpuLogName), _T("\\release\\dshow_pb_cpu_%s_%s_%d.txt"), szClipName, szProto, rep);
                LOG(TEXT("%S: Starting CPUMon with %s"), __FUNCTION__, szCpuLogName);            
                
                pCPUMon = new CPerformanceMonitor(pConfig->GetSampleInterval(), szCpuLogName);
            }
            
            if (!pCPUMon)
            {
                LOG(TEXT("%S: ERROR %d@%S - Failed to start CPU monitor"), __FUNCTION__, __LINE__, __FILE__);            
                if (pCPUMon) {
                    delete pCPUMon;
                    pCPUMon = NULL;
                }
            } else {
                if(pConfig->StatusToReport(STATUS_MEM)) {
                    LOG(TEXT("%S: Starting Memory Monitor"), __FUNCTION__);            
                    pCPUMon->SetMemoryMonitor();
                }

            }
        }

        // Config VMR and start the tests
        // BUGBUG: PlaybackPerfTest uses predefine data in playlist.xml to verify the playback status.
        // BUGBUG: In mixing case, since we have multiple streams, not sure which stream's data should be used by
        // BUGBUG: the filter graph manager.
        LOG(TEXT("%S: running test with url %s"), __FUNCTION__, szUrl[0]);
        retval = PlaybackPerfTest( pConfig, VMRStreams[0], szUrl );

        // collect data
        // Assign result as 100 for pass, 50 for skip, 0 for fail/abort
        int result = 0;
        switch (retval)
        {
        case TPR_SKIP:
            for ( DWORD i = 0; i < nClipIDs; i++ )
                LOG(TEXT("%S: test skipped for url %d: %s"), __FUNCTION__, i, szUrl[0]);

            result = 50;
            break;

        case TPR_PASS:
            for ( DWORD i = 0; i < nClipIDs; i++ )
                LOG(TEXT("%S: test passed for url %d: %s"), __FUNCTION__, i, szUrl[0]);
            result = 100;
            break;
        
        default:
            for ( DWORD i = 0; i < nClipIDs; i++ )
                LOG(TEXT("%S: test failed for url %d: %s"), __FUNCTION__, i, szUrl[0]);
            nFailedTests++;
            result = 0;
            break;
        }

        //if perf is to be logged, write up this clip's data to file
        if (pConfig->UsePerflog())
        {
        
            // Log the overall stats: clipid, protocol, utilization, dropped, drawn, avgUtil
            TCHAR logstr[256] = {0};
            DWORD nBytesWritten = 0;

            StringCchPrintf(logstr, 256, TEXT("%s,%s,%d,%d,%d,%d\r\n"), IDs, szProto, 
                result,
                (pbStats.droppedFramesDecoder + pbStats.droppedFramesRenderer + pbStats.droppedFramesDelivery),
                pbStats.framesDrawn, 
                pbStats.avgCpuUtilization);
            if ( !WriteFile(hLogFile, logstr, _tcslen(logstr)*sizeof(TCHAR), &nBytesWritten, NULL) )
            {
                DWORD dwLastErr = GetLastError();
                LOG( TEXT("Writing to the log file failed!!! Error code: %0x8"), dwLastErr );
            }

            LOGZONES(TEXT("%S: Logging data to log file \n %s"), __FUNCTION__, logstr);
            LOGZONES(TEXT("Frames dropped in decoder: %d\n"), pbStats.droppedFramesDecoder);
            LOGZONES(TEXT("Frames dropped in renderer: %d\n"), pbStats.droppedFramesRenderer);    
            LOGZONES(TEXT("Frames dropped during delivery: %d\n"), pbStats.droppedFramesDelivery);                        
            LOGZONES(TEXT("Total frames drawn: %d\n"), pbStats.framesDrawn);            
            LOGZONES(TEXT("Average CPU utilization: %d\n"), pbStats.avgCpuUtilization);                                            
        }
        // Print out CPU warning if it exists
        if (pConfig->StatusToReport(STATUS_CPU) && pCPUMon)
        {
            //print out warning if CPU was ever over 90% utalized
            if(pCPUMon->WasPegged()) {
                LOG(TEXT("%S: WARNING %d@%S - CPU utalization went over 90% for this clip."), __FUNCTION__, __LINE__, __FILE__);
            }
        }

        //start with a new status monitor every time
        if ((pConfig->GetNumStatusToReport() > 0) && pCPUMon)
        {
            delete pCPUMon;
            pCPUMon = NULL;
        }
    }
    if ( nFailedTests ) 
        retval = TPR_FAIL;

error:
    if ( szUrl[0] )
        delete [] szUrl[0];
    return retval;
}


bool IsError(long eventCode)
{
    bool error = true;

    LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);
    
    switch(eventCode)
    {
    case EC_ERRORABORT:
        LOG(TEXT("%S: EC_ERRORABORT received"), __FUNCTION__);                        
        break;
    case EC_STREAM_ERROR_STOPPED:
        LOG(TEXT("%S: EC_STREAM_ERROR_STOPPED received"), __FUNCTION__);                        
        break;
    case EC_USERABORT:
        LOG(TEXT("%S: EC_USERABORT received"), __FUNCTION__);
        break;
    default:
        error = false;
    }            

    LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    return error;
}

TESTPROCAPI CPlaybackTest::PlaybackPerfTest(PerfTestConfig* pConfig, CMediaContent* pMediaContent, TCHAR* url[])
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    int bandwidthChange = 0;

    // IP Status
    MIB_IPSTATS ipStats;

    LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    //print out network statistics
    if (NO_ERROR == GetIpStatisticsEx(&ipStats, AF_INET))
    {
        LOG(TEXT("%S: Start IP Stats %d dgRecvd  %d HdrErr  %d AddrErr %d Discarded %d ReasmReqd %d ReasmOk %d ReasmFail \r\n"), __FUNCTION__,
            ipStats.dwInReceives, ipStats.dwInHdrErrors, ipStats.dwInAddrErrors, ipStats.dwInDiscards, ipStats.dwReasmReqds, ipStats.dwReasmOks, ipStats.dwReasmFails);
    }
    

    // Get information about our current memory
    MEMORYSTATUS mst;    
    memset(&mst, 0, sizeof(mst));
    mst.dwLength = sizeof(mst);
    GlobalMemoryStatus(&mst);
    LOG(TEXT("\t%S: Start Memory (KB):     %5d total  %5d used  %5d free  %%%d loaded\r\n"), __FUNCTION__,
        mst.dwTotalPhys/1024, (mst.dwTotalPhys - mst.dwAvailPhys) / 1024, mst.dwAvailPhys / 1024, mst.dwMemoryLoad);

    //Compact the heap before playing
    LOGZONES(TEXT("%S: Calling CompactAllHeaps."), __FUNCTION__);

    CompactAllHeaps();

    LOGZONES(TEXT("%S: CompactAllHeaps returned."), __FUNCTION__);

    // Zero out the current stats
    memset(&pbStats, 0, sizeof(PlaybackStats));

    // Create the filter graph with the given url
    FilterGraph fg;
    hr = CreateFilterGraph(url, &fg, pConfig);

    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - CreateFilterGraph failed (0x%0x)"), __FUNCTION__, __LINE__, __FILE__, hr);            
        ReleaseFilterGraph(&fg);
        retval = TPR_FAIL;
        goto PLAYBACK_PERF_EXIT;
    }

    // The interfaces that we are interested in
    IGraphBuilder* pGraph = fg.pGraph ;
    IMediaControl* pMediaControl = fg.pMediaControl;
    IMediaEvent* pMediaEvent = fg.pMediaEvent;
    IMediaSeeking* pMediaSeeking = fg.pMediaSeeking;

    // Needed when waiting for events
    long eventCode;

    // Unpause the cpu monitor and reset its state if it exists
    if (pCPUMon)
    {
        pCPUMon->UnPause();
        pCPUMon->ResetData();
        pCPUMon->SetStatsInterfaces(fg.pDroppedFrames, fg.pQualProp, fg.pNetworkStatus);
    }

    // Get the overall duration
    LONGLONG duration = 0;
    DWORD durationms = 0; 
    hr = pMediaSeeking->GetDuration(&duration);
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - could not get duration (0x%0x)"), __FUNCTION__, __LINE__, __FILE__, hr);            
    }
    else {
        // Convert from 100 ns units to us
        durationms = (DWORD)duration/10000;
    }

    LOGZONES(TEXT("%S: GetDuration returned: 0x%I64x"), __FUNCTION__, duration);

    //get time right now so can calculate total time we ran for
    pbStats.actualDuration = GetTickCount();
    hr = pMediaControl->Run();
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - IMediaControl::Run() failed for URL:  (0x%0x)"), __FUNCTION__, __LINE__, __FILE__, url[0], hr);                
        retval = TPR_FAIL;
    }


    if (retval != TPR_FAIL)
    {
        //wait for clip to finish, monitor any errors in the meantime
        HANDLE hEvent;
        long param1, param2;
        hr = pMediaEvent->GetEventHandle((OAEVENT*)&hEvent);
        while (true)
        {
            if (SUCCEEDED(hr))
            {
                MSG  msg;
                while ( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) )
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
                //if we can get duration of clip, timeout is duration * 2
                hr = pMediaEvent->GetEvent(&eventCode, &param1, &param2, (durationms) ? (durationms * 2) : MAX_WAIT);
                LOGZONES(TEXT("%S: Returned from GetEvent( ). hr (0x%0x), event code (0x%0x), lParam1 (0x%08x), lParam2 (0x%08x)"), __FUNCTION__, hr, eventCode, param1, param2);

                //if there is a problem flag that
                if (FAILED(hr) || IsError(eventCode))                
                {
                    // Store the system playback statistics, before trying anything else
                    pbStats.actualDuration = GetTickCount() - pbStats.actualDuration;
                    
                    LOG(TEXT("%S: ERROR %d@%S - MediaEvent::Wait returned eventCode: %x (0x%0x)"), __FUNCTION__, __LINE__, __FILE__, eventCode, hr);                                
                    retval = TPR_FAIL;
                    pMediaEvent->FreeEventParams(eventCode, param1, param2);
                    break;
                }
                else {
                    if ((eventCode == EC_BANDWIDTHCHANGE))
                    {
                        LOGZONES(TEXT("%S: bandwidth change: %x, %x"), __FUNCTION__, param1, param2);                    
                        bandwidthChange++;
                    }
                    else if (eventCode == EC_COMPLETE)
                    {
                        // Store the system playback statistics, before trying anything else
                        pbStats.actualDuration = GetTickCount() - pbStats.actualDuration;

                        LOG(TEXT("%S: received event EC_COMPLETE"), __FUNCTION__);        
                        
                        // Free event data
                        pMediaEvent->FreeEventParams(eventCode, param1, param2);
                        break;
                    }
                }
                pMediaEvent->FreeEventParams(eventCode, param1, param2);
            }
        }
        
    }

    //if we are getting CPU stats, now is a good time to collect the information
    if (pCPUMon)
    {
        pCPUMon->Pause();
        if(pConfig->StatusToReport(STATUS_CPU))
            pbStats.avgCpuUtilization = (int)pCPUMon->GetAvgUtilization();
    }

    // Get the audio/video playback statistics
    hr = GetAudioVideoPlaybackStats(pMediaContent, &fg, &pbStats);
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to collect audio video stats: (0x%0x)"), __FUNCTION__, __LINE__, __FILE__, hr);                                        
        retval = TPR_FAIL;
    }

    if (retval != TPR_FAIL)
    {
        DWORD sysQualTest = TPR_PASS;
        DWORD vidQualTest = TPR_PASS;
        DWORD audQualTest = TPR_PASS;

        //are we passing all the playback criteria

        sysQualTest = TestSystemPlaybackQuality(pMediaContent, &fg, &pbStats);
        
        //only test for video accuracy if we actually have video
        if(fg.pDroppedFrames || fg.pQualProp)
            vidQualTest = TestVideoPlaybackQuality(pMediaContent, &fg, &pbStats);

        //only test for audio if we have audio
        if(fg.pAudioRendererStats) 
            audQualTest = TestAudioPlaybackQuality(pMediaContent, &fg, &pbStats);

        retval = ((sysQualTest == TPR_FAIL) || (vidQualTest == TPR_FAIL) || (audQualTest == TPR_FAIL)) ? TPR_FAIL : TPR_PASS;

        if (sysQualTest == TPR_FAIL)
        {
            LOG(TEXT("%S: ERROR %d@%S - system playback quality failed"), __FUNCTION__, __LINE__, __FILE__);                                        
        }
        else if (vidQualTest == TPR_FAIL)
        {
            LOG(TEXT("%S: ERROR %d@%S - video quality failed."), __FUNCTION__, __LINE__, __FILE__);                                                
        }
        else if (audQualTest == TPR_FAIL)
        {
            LOG(TEXT("%S: ERROR %d@%S - audio quality failed"), __FUNCTION__, __LINE__, __FILE__);                                        
        }
    }

    if (bandwidthChange > 0)
    {
        LOG(TEXT("%S: bandwidth controller kicked in %d times during playback."), __FUNCTION__, bandwidthChange);
    }

    //Pause the CPU Monitor and give the thread time to exit gracefully
    if (pCPUMon)
    {
        pCPUMon->Pause();
        Sleep(1000);
    }

    //clean up graph
    ReleaseFilterGraph(&fg);

    //print out global memory information
    GlobalMemoryStatus(&mst);
    LOG(TEXT("\t%S: End Memory (KB):     %5d total  %5d used  %5d free  %%%d loaded\r\n"), __FUNCTION__,
        mst.dwTotalPhys/1024, (mst.dwTotalPhys - mst.dwAvailPhys) / 1024, mst.dwAvailPhys / 1024, mst.dwMemoryLoad);

    if (NO_ERROR == GetIpStatisticsEx(&ipStats, AF_INET))
    {
        LOG(TEXT("%S: End IP Stats %d dgRecvd  %d HdrErr  %d AddrErr %d Discarded %d ReasmReqd %d ReasmOk %d ReasmFail \r\n"), __FUNCTION__,
            ipStats.dwInReceives, ipStats.dwInHdrErrors, ipStats.dwInAddrErrors, ipStats.dwInDiscards, ipStats.dwReasmReqds, ipStats.dwReasmOks, ipStats.dwReasmFails);
    }

PLAYBACK_PERF_EXIT:
    LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
    
    return retval;
}

HRESULT CPlaybackTest::GetAudioVideoPlaybackStats(CMediaContent* pMediaContent, FilterGraph* pFG, PlaybackStats* pStats)
{
    HRESULT hr = S_OK;

    LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    // Video Decoder stats
    if (pFG->pDroppedFrames) {
        hr = pFG->pDroppedFrames->GetNumDropped(&pStats->droppedFramesDecoder);
        if(FAILED(hr)) {
            LOG(TEXT("%S: ERROR %d@%S - Calling GetNumDropped on IAMDroppedFrames object failed: (0x%0x)"), __FUNCTION__, __LINE__, __FILE__, hr);                                                    
        }
        else {
            LOG(_T("%S: Decoder dropped: %d"), __FUNCTION__, pStats->droppedFramesDecoder);            
        }
    }

    // Video Renderer stats
    if(SUCCEEDED(hr) && pFG->pQualProp) {
        // Get the number of frames dropped in the renderer
        if (SUCCEEDED(hr))
        {
            hr = pFG->pQualProp->get_FramesDroppedInRenderer(&pStats->droppedFramesRenderer);
            if(FAILED(hr)) {
                LOG(_T("%S: ERROR %d@%S - Could not find the number of frames dropped in renderer: (%08lx)"), __FUNCTION__, __LINE__, __FILE__, hr);
            } 
            else {
                LOG(_T("%S: Renderer dropped: %d"), __FUNCTION__, pStats->droppedFramesRenderer);
            }
        }

        if (SUCCEEDED(hr))
        {
            hr = pFG->pQualProp->get_FramesDrawn(&pStats->framesDrawn);
            if (pbStats.framesDrawn)
                pbStats.framesDrawn += 1;
            if(FAILED(hr)) {
                LOG(_T("%S: ERROR %d@%S - Could not find number of frames drawn: (%08lx)"), __FUNCTION__, __LINE__, __FILE__, hr);
            } 
            else {
                LOG(_T("%S: Frames drawn: %d, actual: %d"), __FUNCTION__, pStats->framesDrawn, pMediaContent->m_QualityControl.m_lFramesDrawn);
            }
        }

        if (SUCCEEDED(hr))
        {
            hr = pFG->pQualProp->get_AvgSyncOffset(&pStats->avgSyncOffset);
            if(FAILED(hr)) {
                LOG(_T("%S: ERROR %d@%S - Could not find average sync offset: (%08lx)"), __FUNCTION__, __LINE__, __FILE__, hr);
            }
            else{
                LOG(_T("%S: Average SyncOffset(ms): %d"), __FUNCTION__, pStats->avgSyncOffset);
            }
        }

        if (SUCCEEDED(hr))
        {
            hr = pFG->pQualProp->get_DevSyncOffset(&pStats->devSyncOffset);
            if(FAILED(hr)) {
                LOG(_T("%S: ERROR %d@%S - Could not find deviation sync offset: (%08lx)"), __FUNCTION__, __LINE__, __FILE__, hr);
            }
            else{
                LOG(_T("%S: Standard deviation SyncOffset: %d"), __FUNCTION__, pStats->devSyncOffset);
            }            
        }

        if (SUCCEEDED(hr))
        {
            hr = pFG->pQualProp->get_AvgFrameRate(&pStats->avgFrameRate);
            if(FAILED(hr)) {
                LOG(_T("%S: ERROR %d@%S - Could not find average frame rate: (%08lx)"), __FUNCTION__, __LINE__, __FILE__, hr);
            }
            else{
                LOG(_T("%S: Average frame rate(frames per 100 seconds): %d"), __FUNCTION__, pStats->avgFrameRate);
            }            
        }

        if (SUCCEEDED(hr))
        {
            hr = pFG->pQualProp->get_Jitter(&pStats->jitter);
            if(FAILED(hr)) {
                LOG(_T("%S: ERROR %d@%S - Could not find the jitter: (%08lx)"), __FUNCTION__, __LINE__, __FILE__, hr);
            }
            else{
                LOG(_T("%S: Jitter (ms): %d"), __FUNCTION__, pStats->jitter);
            }                
        }
    }

    // Audio Render Stats

    //currently we only check for discontinuities but you can query for more data here if you need
    //Check out documentation for IAMAudioRendererStats interface for information on the other kinds of information available
    
    if (pFG->pAudioRendererStats)
    {
        DWORD dwParam2;
        hr = pFG->pAudioRendererStats->GetStatParam(AM_AUDREND_STAT_PARAM_DISCONTINUITIES, &pStats->audioDiscontinuities, &dwParam2);
        if(FAILED(hr)) {
            LOG(_T("%S: ERROR %d@%S - Could not get audio discontinuities: (%08lx)"), __FUNCTION__, __LINE__, __FILE__, hr);
        } 
        else{
            LOG(_T("%S: Total discontinuities in the audio stream: %d"), __FUNCTION__, pStats->audioDiscontinuities);
        }                
    }

    // Network stats: whatever else was dropped must have been dropped during delivery
    if (pMediaContent->m_QualityControl.m_lFramesDrawn)
        pStats->droppedFramesDelivery = pMediaContent->m_QualityControl.m_lFramesDrawn - 
                (pStats->framesDrawn + pStats->droppedFramesDecoder + pStats->droppedFramesRenderer);

    //get network stats from the source filter
    if (pFG->pNetworkStatus)
    {
        pFG->pNetworkStatus->get_LostPackets(&pStats->lostPackets);
        LOG(_T("%S: Total lost network packets: %d"), __FUNCTION__, pStats->lostPackets);        
        pFG->pNetworkStatus->get_RecoveredPackets(&pStats->recoveredPackets);
        LOG(_T("%S: Total recovered network packets: %d"), __FUNCTION__, pStats->recoveredPackets);        
        pFG->pNetworkStatus->get_ReceivedPackets(&pStats->receivedPackets);
        LOG(_T("%S: Total recieved network packets: %d"), __FUNCTION__, pStats->receivedPackets);        
    }

    LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
    
    return hr;
}

TESTPROCAPI CPlaybackTest::TestSystemPlaybackQuality(CMediaContent* pMediaContent, 
                                                    FilterGraph* pFG,
                                                    PlaybackStats *pStats)
{
    DWORD retval = TPR_PASS;

    LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    // Check that the overall duration was ok
    LONGLONG duration;
    HRESULT hr = pFG->pMediaSeeking->GetDuration(&duration);
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - could not get duration: (%08lx)"), __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
    }

    if(SUCCEEDED(hr)) 
    {
        //if we have provided a duration margin of error then verify this
        if(pMediaContent->m_QualityControl.m_dDuration)
        {
            if((abs(pMediaContent->m_QualityControl.m_dDuration - pStats->actualDuration) > pMediaContent->m_QualityControl.m_dDurationMarginOfError) && 
                (pMediaContent->m_QualityControl.m_dDurationMarginOfError >= 0))
            {
                LOG(TEXT("%S: ERROR %d@%S - Difference between actual duration(%d ms) and expected duration(%d ms) not within margin of error specified (%d)"), __FUNCTION__, __LINE__, __FILE__, 
                    pStats->actualDuration, pMediaContent->m_QualityControl.m_dDuration, pMediaContent->m_QualityControl.m_dDurationMarginOfError);
                retval = TPR_FAIL;
            }
        }
    }
    
    // Can verify other things like A/V sync here if needed

    LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    return retval;
}

TESTPROCAPI CPlaybackTest::TestVideoPlaybackQuality(CMediaContent* pMediaContent, 
                                            FilterGraph* pFG,
                                            PlaybackStats *pStats)
{
    DWORD retval = TPR_PASS;

    LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    // Decoder quality: frames dropped in the decoder
    if((abs(pMediaContent->m_QualityControl.m_lFramesDroppedInDecoder - pStats->droppedFramesDecoder) > pMediaContent->m_QualityControl.m_lFramesDroppedInDecoderMarginOfError)
        && (pMediaContent->m_QualityControl.m_lFramesDroppedInDecoderMarginOfError >= 0))
    {
        LOG(TEXT("%S: ERROR %d@%S - dropped frames in decoder not within expected margin of error (%d): %d"), __FUNCTION__, __LINE__, __FILE__, 
            pMediaContent->m_QualityControl.m_lFramesDroppedInDecoderMarginOfError, pStats->droppedFramesDecoder);
        retval = TPR_FAIL;
    }

    // Renderer quality: frames dropped in the renderer
    if((abs(pMediaContent->m_QualityControl.m_lFramesDroppedInRenderer - pStats->droppedFramesRenderer) > pMediaContent->m_QualityControl.m_lFramesDroppedInRendererMarginOfError) && 
       (pMediaContent->m_QualityControl.m_lFramesDroppedInRendererMarginOfError >= 0))
    {
        LOG(TEXT("%S: ERROR %d@%S - dropped frames in renderer not within expected margin of error (%d): %d"), __FUNCTION__, __LINE__, __FILE__, 
            pMediaContent->m_QualityControl.m_lFramesDroppedInRendererMarginOfError, pStats->droppedFramesRenderer);
        retval = TPR_FAIL;
    }

    // Make sure that the expected number of frames adds up
    if((abs(pMediaContent->m_QualityControl.m_lFramesDrawn - pStats->framesDrawn) > pMediaContent->m_QualityControl.m_lFramesDrawnMarginOfError) && 
       (pMediaContent->m_QualityControl.m_lFramesDrawnMarginOfError >= 0))
    {
        LOG(TEXT("%S: ERROR %d@%S - frames drawn does not match margin of error: %d, %d"), __FUNCTION__, __LINE__, __FILE__, pStats->framesDrawn, pMediaContent->m_QualityControl.m_lFramesDrawn);
        retval = TPR_FAIL;
    }

    // Overall frame rate quality
    float fps = (float)(pStats->avgFrameRate/100.0);
    float actualFps = (float)(pMediaContent->m_QualityControl.m_iAvgFrameRate/100.0);
    float ratio = fps/actualFps;

    // check to see if the frame rate did not fall within the set margin of error
    int percentMarginOfError = pMediaContent->m_QualityControl.m_iAvgFrameRateMarginOfError;
    if (((100 - ratio) > (float)percentMarginOfError/100.0) && (percentMarginOfError >= 0))
    {
        LOG(TEXT("%S: ERROR %d@%S - framerate does not match margin of error: %f, %f"), __FUNCTION__, __LINE__, __FILE__, fps, actualFps);
        retval = TPR_FAIL;
    }

    // Sync offset
    if((abs(pMediaContent->m_QualityControl.m_iAvgSyncOffset - pStats->avgSyncOffset) > pMediaContent->m_QualityControl.m_iAvgSyncOffsetMarginOfError) && 
       (pMediaContent->m_QualityControl.m_iAvgSyncOffsetMarginOfError >= 0))
    {
        LOG(TEXT("%S: ERROR %d@%S - sync offset does not match margin of error: %d, %d"), __FUNCTION__, __LINE__, __FILE__, pStats->avgSyncOffset, pMediaContent->m_QualityControl.m_iAvgSyncOffset);
        retval = TPR_SKIP;
    }

    // Dev sync offset
    if((abs(pMediaContent->m_QualityControl.m_iDevSyncOffset - pStats->devSyncOffset) > pMediaContent->m_QualityControl.m_iDevSyncOffsetMarginOfError) && 
        (pMediaContent->m_QualityControl.m_iDevSyncOffsetMarginOfError >= 0))
    {
        LOG(TEXT("%S: ERROR %d@%S - Dev sync offset does not match margin of error: %d, %d"), __FUNCTION__, __LINE__, __FILE__, pStats->devSyncOffset, pMediaContent->m_QualityControl.m_iDevSyncOffset);
        retval = TPR_FAIL;
    }

    // Jitter
    if((abs(pMediaContent->m_QualityControl.m_iJitter - pStats->jitter) > pMediaContent->m_QualityControl.m_iJitterMarginOfError) && 
        (pMediaContent->m_QualityControl.m_iJitterMarginOfError >= 0))
    {
        LOG(TEXT("%S: ERROR %d@%S - jitter does not match margin of error: %d, %d"), __FUNCTION__, __LINE__, __FILE__, pStats->jitter, pMediaContent->m_QualityControl.m_iJitter);
        retval = TPR_FAIL;
    }

    LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    return retval;
}

TESTPROCAPI CPlaybackTest::TestAudioPlaybackQuality(CMediaContent* pMediaContent, 
                                            FilterGraph* pFG,
                                            PlaybackStats* pStats)
{
    DWORD retval = TPR_PASS;

    LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    // We permit one discontinuity at the end of a stream
    if (pStats->audioDiscontinuities > 1)
        return TPR_FAIL;

    LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
    
    return retval;
}
