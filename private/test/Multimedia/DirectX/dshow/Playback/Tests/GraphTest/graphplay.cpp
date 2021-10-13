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
////////////////////////////////////////////////////////////////////////////////
//
//  Graph Playback Test functions
// 
//
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <tux.h>
//#include <fstream>
#include "logging.h"
#include "TuxGraphTest.h"
#include "GraphTestDesc.h"
#include "TestGraph.h"
#include "GraphTest.h"
#include "utility.h"
#include "string.h"
#include "CSystemMonitor.hpp"

#define EC_COMPLETE_TOLERANCE 2000
void PrintSystemMonitoringData(DWORD MemoryLoadRecord[], DWORD PhyMemoryUseRecord[], DWORD VirMemoryUseRecord[], int CPUUseRecord[], UINT RecordCount, WCHAR *StrFileNamePrefix);
DWORD AnalyzeSystemMonitoringData(DWORD MemoryLoadRecord[], DWORD PhyMemoryUseRecord[], DWORD VirMemoryUseRecord[], int CPUUseRecord[], UINT RecordCount);
DWORD GetMemoryLoadData(MEMORYSTATUS MemoryBefore, MEMORYSTATUS MemoryAfter );

#define SYSTEM_MONITOR_DATA_MAX           1100

//TCHAR    TestCase[50];
extern int      ItterationCount;
extern DWORD    TimeElapsed;
extern DWORD    LastTime;

LPSYSTEMTIME    SystemTime;

TESTPROCAPI ManualPlaybackTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    PlaybackTestDesc *pTestDesc = (PlaybackTestDesc*)lpFTE->dwUserData;

    // From the test desc, determine the media file to be used
    PlaybackMedia* pMedia = pTestDesc->GetMedia(0);
    if (!pMedia)
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to retrieve media object"), __FUNCTION__, __LINE__, __FILE__);
        return TPR_FAIL;
    }

    // Instantiate the test graph
    TestGraph testGraph;
    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );

    testGraph.SetMedia(pMedia);
    
     //First try to RenderFile()and then try build graph for Transport Stream if the first try fails
    hr = testGraph.BuildGraph();
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to build graph for media %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
        retval = TPR_FAIL;
    }
    else {
        LOG(TEXT("%S: successfully built graph for %s"), __FUNCTION__, pMedia->GetUrl());
    }

    // Get duration and set the playback start and end positions.
    LONGLONG start = pTestDesc->start;
    LONGLONG stop = pTestDesc->stop;
    LONGLONG duration = 0;
    if (retval == TPR_PASS)
    {
        hr = testGraph.GetDuration(&duration);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to get the duration (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
        }
    }

    if (retval == TPR_PASS)
    {
        //start and stop were in %, convert to actual values
        start = (pTestDesc->start*duration)/100;
        stop = (pTestDesc->stop*duration)/100;
        if (stop == 0)
             stop = duration;

        hr = testGraph.SetAbsolutePositions(start, stop);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to set positions to %I64x and %I64x (0x%x)"), __FUNCTION__, __LINE__, __FILE__, start, stop, hr);
            retval = TPR_FAIL;
        }
    }
    
    // Calculate expected duration of playback
    duration = (stop - start);

    // Run the graph
    if (retval == TPR_PASS)
    {
        // Change the state to running - the call doesn't return until the state change completes
        hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to change state to Running for media %s (0x%x)"), 
                        __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
            retval = TPR_FAIL;
        }
    }
    
    // Wait for completion of playback with a duration of twice the expected time
    if (retval == TPR_PASS)
    {
        LOG(TEXT("%S: changed state to Running."), __FUNCTION__);        
        DWORD timeout = 2*TICKS_TO_MSEC(duration);
        LOG(TEXT("%S: waiting for completion with a timeout of %d msec."), __FUNCTION__, timeout);
        hr = testGraph.WaitForCompletion(timeout);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - WaitForCompletion failed (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
        }
    }

    if (retval == TPR_PASS)
    {
        // Ask the tester if playback was ok
        int query = MessageBox(NULL, TEXT("Did the playback complete succesfully with no or few glitches?"), TEXT("Manual Playback Test"), MB_YESNO | MB_SETFOREGROUND);
        if (query == IDYES)
            LOG(TEXT("%S: playback completed successfully."), __FUNCTION__);
        else if (query == IDNO)
        {
            LOG(TEXT("%S: playback was not ok."), __FUNCTION__);
            retval = TPR_FAIL;
        }
        else {
            LOG(TEXT("%S: unknown response or MessageBox error."), __FUNCTION__);
            retval = TPR_ABORT;
        }
    }

    if (retval == TPR_PASS)
    {
        LOG(TEXT("%S: successfully verified playback."), __FUNCTION__);
    }
    else {
        LOG(TEXT("%S: ERROR %d@%S - failed the playback test."), __FUNCTION__, __LINE__, __FILE__);
        retval = TPR_FAIL;
    }

    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    return retval;
}

TESTPROCAPI PlaybackTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    PlaybackTestDesc *pTestDesc = (PlaybackTestDesc*)lpFTE->dwUserData;

    // From the test desc, determine the media file to be used
    PlaybackMedia* pMedia = pTestDesc->GetMedia(0);
    if (!pMedia)
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to retrieve media object"), __FUNCTION__, __LINE__, __FILE__);
        return TPR_FAIL;
    }

    // Instantiate the test graph
    TestGraph testGraph;

    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );

    testGraph.SetMedia(pMedia);
    
    // Check the verification required
    bool bGraphVerification = false;
    PlaybackDurationTolerance* pDurationTolerance = NULL;
    VerificationList verificationList = pTestDesc->GetVerificationList();
    VerificationList::iterator iterator = verificationList.begin();
    while (iterator != verificationList.end())
    {
        VerificationType type = (VerificationType)iterator->first;
        if (type == VerifyPlaybackDuration) {
            pDurationTolerance = (PlaybackDurationTolerance*)iterator->second;
            // We need the startup latency to measure playback duration accurately - so enable measurement
            // Keep the tolerance as INFINITE since we just want to measure it
            LONGLONG startupLatencyTolerance = INFINITE_TIME;
            hr = testGraph.EnableVerification(StartupLatency, (void*)&startupLatencyTolerance);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to enable startup latency measurement)"), __FUNCTION__, __LINE__, __FILE__);
                retval = TPR_FAIL;
                break;
            }
        }
        else if (testGraph.IsCapable(type)) {
            hr = testGraph.EnableVerification(type, iterator->second);
            if (FAILED_F(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to enable verification %S)"), __FUNCTION__, __LINE__, __FILE__, GetVerificationString(type));
                break;
            }
            bGraphVerification = true;
        }
        else {
            LOG(TEXT("%S: WARNING %d@%S - unrecognized verification requested %S)"), __FUNCTION__, __LINE__, __FILE__, GetVerificationString(type));
        }
        iterator++;
    }

    // Build the graph
     //First try to RenderFile()and then try build graph for Transport Stream if the first try fails
    hr = testGraph.BuildGraph();
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to build graph for media %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
        retval = TPR_FAIL;
    }
    else {
        LOG(TEXT("%S: successfully built graph for %s"), __FUNCTION__, pMedia->GetUrl());
    }

    // Set the playback start and end positions.
    LONGLONG start = pTestDesc->start;
    LONGLONG stop = pTestDesc->stop;
    LONGLONG duration = 0;
    LONGLONG actualDuration = 0;

    // if running as an xbvt test, then we don't want to use the IMediaSeeking::GetDuration interface
    // instead, assume duration is 5 minutes (with a 10 minute timeout)
    if (retval == TPR_PASS && pTestDesc->XBvt == false)
    {        
        hr = testGraph.GetDuration(&duration);      
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to get the duration (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
        }
    }
    
    if (retval == TPR_PASS)
    {        
        if ( pTestDesc->XBvt == false )
        {
            //start and stop were in %, convert to actual values
            start = (pTestDesc->start*duration)/100;
            stop = (pTestDesc->stop*duration)/100;
            if (stop == 0)
                 stop = duration;

            hr = testGraph.SetAbsolutePositions(start, stop);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to set positions to %I64x and %I64x (0x%x)"), __FUNCTION__, __LINE__, __FILE__, start, stop, hr);
                retval = TPR_FAIL;
            }
        } else {
            start = 0;
            stop = 1500000000;  // in ticks; expect this converts to 2.5 minutes
        }

        // Calculate expected duration of playback
        duration = (stop - start);
    }

    // Run the graph and wait for completion
    if (retval == TPR_PASS)
    {
        // Change the state to running - the call doesn't return until the state change completes
        LOG(TEXT("%S: changing state to Running and waiting for EC_COMPLETE."), __FUNCTION__);    

        // Instantiate timer for measurement
        Timer timer;

        DWORD timeout = 2*TICKS_TO_MSEC(duration);

        // Start the playback timer
        timer.Start();

        // Signal the start of verification
        hr = testGraph.StartVerification();
        if (FAILED_F(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to start the verification (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
        }

        // Change state
        if (retval == TPR_PASS)
        {
            hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to change state to Running (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
                retval = TPR_FAIL;
            }
        }
        
        // Wait for the playback to complete
        if (retval == TPR_PASS)
        {
            hr = testGraph.WaitForCompletion(timeout);
            
            // Measure the actual duration
            actualDuration = timer.Stop();

            // Did the wait succeed?
            if (FAILED(hr))
            {
                // If the duration is very short, the WaitForCompletion will return E_ABORT since it is 
                // possible that before it gets called the playback already completed.
                // BUGBUG: from the test, it seems 86.375ms made it through. Hard coded it to 100 ms
                if ( E_ABORT == hr && duration < DURATION_THRESHOLD_IN_MS * 10000 ) 
                {
                    LOG( TEXT("%S: WARN %d@%S - WaitForCompletion aborted since the duraton is very short (%d ms)."), 
                                __FUNCTION__, __LINE__, __FILE__, duration / 10000 );
                }
                else
                {
                    LOG(TEXT("%S: ERROR %d@%S - WaitForCompletion failed (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
                    retval = TPR_FAIL;
                }
            }
        }
    }

    // Check the results of verification if there was any specified
    if (bGraphVerification && (retval == TPR_PASS))
    {
        hr = testGraph.GetVerificationResults();
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - verification failed"), __FUNCTION__, __LINE__, __FILE__);
            retval = TPR_FAIL;
        }
        else {
            LOG(TEXT("%S: verification succeeded"), __FUNCTION__);
        }
    }

    if (retval == TPR_PASS)
    {
        LOG(TEXT("%S: playback completed successfully."), __FUNCTION__);

        // Verification of playback time
        if (pDurationTolerance)
        {
            // Measure time from RenderFile to getting EC_COMPLETE notification
            LOG(TEXT("%S: total playback time (Run to EC_COMPLETE): %d msec"), __FUNCTION__, TICKS_TO_MSEC(actualDuration));

            // Measure the startup latency 
            LONGLONG startupLatency = 0;
            hr = testGraph.GetVerificationResult(StartupLatency, &startupLatency, sizeof(LONGLONG));
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to calculate startup latency"), __FUNCTION__, __LINE__, __FILE__);
            }
            else {
                LOG(TEXT("%S: startup latency %d msec"), __FUNCTION__, TICKS_TO_MSEC(startupLatency));
            }

            // Account for startup latency
            actualDuration -= startupLatency;

            // Check the percentage tolerance
            if (!ToleranceCheck(duration, actualDuration, pDurationTolerance->pctDurationTolerance) &&
                !ToleranceCheckAbs(duration, actualDuration, MSEC_TO_TICKS(pDurationTolerance->absDurationTolerance)))
            {
                LOG(TEXT("%S: ERROR %d@%S - expected duration: %d does not match actual duration %d msec. Tolerance: %d%%,%d msec: failed"), __FUNCTION__, __LINE__, __FILE__, TICKS_TO_MSEC(duration), TICKS_TO_MSEC(actualDuration), pDurationTolerance->pctDurationTolerance, pDurationTolerance->absDurationTolerance);
                retval = TPR_FAIL;
            }
            else {
                LOG(TEXT("%S: expected duration: %d matches actual duration: %d msec. Tolerance: %d%%,%d msec: suceeded"), __FUNCTION__, TICKS_TO_MSEC(duration), TICKS_TO_MSEC(actualDuration), pDurationTolerance->pctDurationTolerance, pDurationTolerance->absDurationTolerance);
            }
        }
    }

    if (retval == TPR_PASS)
    {
        LOG(TEXT("%S: successfully verified playback."), __FUNCTION__);
    }
    else {
        LOG(TEXT("%S: ERROR %d@%S - failed the playback test."), __FUNCTION__, __LINE__, __FILE__);
        retval = TPR_FAIL;
    }

  // Reset the testGraph
     testGraph.Reset();
     
     // Delete downloaded media files after each test case
    pMedia->Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    return retval;
}
DWORD CompareMemory(MEMORYSTATUS MemoryBefore, MEMORYSTATUS MemoryAfter )
{
    DWORD MemoryLoadBefore = ( MemoryBefore.dwMemoryLoad );
    DWORD TotalPhysBefore  = ( MemoryBefore.dwTotalPhys )/1024;
    DWORD AvailPhysBefore  = ( MemoryBefore.dwAvailPhys )/1024;
    DWORD TotalPgBefore    = ( MemoryBefore.dwTotalPageFile )/1024;
    DWORD AvailPgBefore    = ( MemoryBefore.dwAvailPageFile )/1024;
    DWORD TotalVirBefore   = ( MemoryBefore.dwTotalVirtual )/1024;
    DWORD AvailVirBefore   = ( MemoryBefore.dwAvailVirtual )/1024;

    DWORD MemoryLoadAfter = ( MemoryAfter.dwMemoryLoad );
    DWORD TotalPhysAfter  = ( MemoryAfter.dwTotalPhys )/1024;
    DWORD AvailPhysAfter  = ( MemoryAfter.dwAvailPhys )/1024;
    DWORD TotalPgAfter    = ( MemoryAfter.dwTotalPageFile )/1024;
    DWORD AvailPgAfter    = ( MemoryAfter.dwAvailPageFile )/1024;
    DWORD TotalVirAfter   = ( MemoryAfter.dwTotalVirtual )/1024;
    DWORD AvailVirAfter   = ( MemoryAfter.dwAvailVirtual )/1024;

    DWORD MemoryLoadDiff = ( MemoryLoadAfter - MemoryLoadBefore );
    DWORD TotalPhysDiff  = ( TotalPhysAfter  - TotalPhysBefore )/1024;
    DWORD AvailPhysDiff  = ( AvailPhysBefore - AvailPhysAfter )/1024;
    DWORD TotalPgDiff    = ( TotalPgAfter    - TotalPgBefore )/1024;
    DWORD AvailPgDiff    = ( AvailPgAfter    - AvailPgBefore )/1024;
    DWORD TotalVirDiff   = ( TotalVirAfter   - TotalVirBefore )/1024;
    DWORD AvailVirDiff   = ( AvailVirAfter   - AvailVirBefore )/1024;

    LOG(TEXT("Memory Status (in KBytes)"));
    LOG(TEXT("MemoryLoad - Available Physical Memory"));
    LOG(TEXT("Before: (%d)      - (%d)"),MemoryLoadBefore, AvailPhysBefore);
    LOG(TEXT("After:  (%d)      - (%d)"),MemoryLoadAfter,  AvailPhysAfter);
    LOG(TEXT("Diffe:  (%d)      - (%d)"),MemoryLoadDiff,   AvailPhysDiff);

    return MemoryLoadDiff;
}

DWORD PlayFile(const WCHAR *pwszPath)
{
    DWORD retVal = TPR_PASS;
    HRESULT hr = S_OK;
    IGraphBuilder   *pGraph = NULL;
    IMediaControl   *pMediaControl = NULL;

    // Create the filter graph manager and render the file.
    hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC, IID_IGraphBuilder, (void **)&pGraph);
    if (FAILED(hr))
    {
        LOG( TEXT("%S: ERROR %d@%S - CoCreateInstance 0x%08x."), 
            __FUNCTION__, __LINE__, __FILE__, hr );
        retVal = TPR_FAIL;
        goto Exit;
    }

    hr =  pGraph->QueryInterface(IID_IMediaControl, (void **)&pMediaControl);
    if(FAILED(hr))
    {
        LOG( TEXT("%S: ERROR %d@%S - IGraphBuilder Failed to create IID_IMediaControl 0x%08x."), 
            __FUNCTION__, __LINE__, __FILE__, hr );
        retVal = TPR_FAIL;
        goto Exit;
    }

    LOG( TEXT(" Playing [%s]"),pwszPath );
    hr = pGraph->RenderFile(pwszPath, NULL);
    if(FAILED(hr))
    {
        LOG( TEXT("%S: ERROR %d@%S - IGraphBuilder Failed to render file [%s] 0x%08x."), 
            __FUNCTION__, __LINE__, __FILE__, pwszPath, hr );
        retVal = TPR_FAIL;
        goto Exit;
    }

    // Run the graph.
    hr = pMediaControl->Run();
    if(FAILED(hr))
    {
        LOG( TEXT("%S: ERROR %d@%S - IMediaControl Failed to run file [%s] 0x%08x."), 
            __FUNCTION__, __LINE__, __FILE__, pwszPath, hr );
        retVal = TPR_FAIL;
        goto Exit;
    }

    // Play for a while
    Sleep(8000);
    hr = pMediaControl->Pause();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IMediaControl failed to pause. (0x%x)"),
            __FUNCTION__, __LINE__, __FILE__, hr );
        retVal = TPR_FAIL;
        goto Exit;
    }
    Sleep(100);
    hr = pMediaControl->Stop();
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - IMediaControl failed to stop. (0x%x)"),
            __FUNCTION__, __LINE__, __FILE__, hr );
        retVal = TPR_FAIL;
        goto Exit;
    }

    LOG( TEXT("Finish [%s]"),pwszPath );

Exit:
    if (pMediaControl)
    {
        pMediaControl->Release();
    }

    if (pGraph)
    {
        pGraph->Release();
    }
    return retVal;
}

// This test plays back the media specified for say 1000 times and verifies if there is any memory leak
// This test fails if either memory leak is found out or if any dshow api throws error
// Config File: Specify the media file to be played.
TESTPROCAPI PlaybackLeakTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retVal = TPR_PASS;
    HRESULT hr = S_OK;
    MEMORYSTATUS MemoryBefore;
    MEMORYSTATUS MemoryAfter;

    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if ( FAILED(hr) )
    {
        LOG( TEXT("%S: ERROR %d@%S - CoInitializeEx w/ COINIT_MULTITHREADED failed. 0x%08x."), 
            __FUNCTION__, __LINE__, __FILE__, hr );
        retVal = TPR_FAIL;
        goto cleanup;
    }

    // Get the test config object from the test parameter
    PlaybackTestDesc *pTestDesc = (PlaybackTestDesc*)lpFTE->dwUserData;

    // From the test desc, determine the media file to be used
    PlaybackMedia* pMedia = pTestDesc->GetMedia(0);
    if (!pMedia)
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to retrieve media object"), __FUNCTION__, __LINE__, __FILE__);
        retVal = TPR_FAIL;
        goto cleanup;
    }

    _TCHAR tszPath[MAX_PATH];
    _tcscpy_s(tszPath, countof(tszPath), pTestDesc->szDownloadLocation);
    _tcscat_s(tszPath, countof(tszPath), pMedia->m_szFileName);
    // get present memory status before wav file is played for 1000 times
    GlobalMemoryStatus( &MemoryBefore ); //GlobalMemoryStatus does not return any value

#define FILE_PLAY_ITERATIONS 1000

    for (int n=0; n<FILE_PLAY_ITERATIONS; n++)
    {
        retVal = PlayFile(tszPath);
        if ( retVal == TPR_FAIL )
        {
            LOG( TEXT("%S: ERROR %d@%S - PlayFile failed. 0x%08x."), 
                __FUNCTION__, __LINE__, __FILE__, hr );
            goto cleanup;
        }
    }
    // get present memory status after wav file is played for 1000 times
    GlobalMemoryStatus( &MemoryAfter ); //GlobalMemoryStatus does not return any value
    DWORD MemoryLoadDiff = CompareMemory( MemoryBefore, MemoryAfter );

#define THRESHOLD_MEMORY_LOAD_DIFF 2

    if(MemoryLoadDiff > THRESHOLD_MEMORY_LOAD_DIFF) 
    {
        LOG( TEXT(" Memory Load has been increased. Possible Memory leak !!"));
        retVal = TPR_FAIL;
    }

cleanup:
    CoUninitialize();
    return retVal;
}

TESTPROCAPI MediaCubePlaybackTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    //ifstream inFile;
    int numofFile = 0;
    BOOL fProcessAlive;
    WCHAR szCmdLine[MAX_PATH];
    STARTUPINFOW startupInfo = { 0 };
    PROCESS_INFORMATION procInfo = { 0 };
    

    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    //Read the number of media files in file "NumofFile.txt"
/*  inFile.open("NumofFile.txt");
    if (inFile.fail()) 
    {
        LOG(TEXT("%S: ERROR %@%S-unable to open file NumofFile.txt for reading"),__FUNCTION__, __LINE__, __FILE__);
      return TPR_FAIL;
    }
    inFile >> numofFile;
    if(numofFile <= 0)
    {
        LOG(TEXT("%S: ERROR %d@%S - No media File founded"),__FUNCTION__, __LINE__, __FILE__);
        return TPR_FAIL;
    }
*/

    // Get the test config object from the test parameter
    PlaybackTestDesc *pTestDesc = (PlaybackTestDesc*)lpFTE->dwUserData;

    // From the test desc, determine the media file to be used
    PlaybackMedia* pMedia = pTestDesc->GetMedia(0);
    if (!pMedia)
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to retrieve media object"), __FUNCTION__, __LINE__, __FILE__);
        return TPR_FAIL;
    }

    // Instantiate the test graph
    TestGraph testGraph;

    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );

    testGraph.SetMedia(pMedia);
    
    // Check the verification required
    bool bGraphVerification = false;
    PlaybackDurationTolerance* pDurationTolerance = NULL;
    VerificationList verificationList = pTestDesc->GetVerificationList();
    VerificationList::iterator iterator = verificationList.begin();
    while (iterator != verificationList.end())
    {
        VerificationType type = (VerificationType)iterator->first;
        if (type == VerifyPlaybackDuration) {
            pDurationTolerance = (PlaybackDurationTolerance*)iterator->second;
            // We need the startup latency to measure playback duration accurately - so enable measurement
            // Keep the tolerance as INFINITE since we just want to measure it
            LONGLONG startupLatencyTolerance = INFINITE_TIME;
            hr = testGraph.EnableVerification(StartupLatency, (void*)&startupLatencyTolerance);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to enable startup latency measurement)"), __FUNCTION__, __LINE__, __FILE__);
                retval = TPR_FAIL;
                break;
            }
        }
        else if (testGraph.IsCapable(type)) {
            hr = testGraph.EnableVerification(type, iterator->second);
            if (FAILED_F(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to enable verification %S)"), __FUNCTION__, __LINE__, __FILE__, GetVerificationString(type));
                break;
            }
            bGraphVerification = true;
        }
        else {
            LOG(TEXT("%S: WARNING %d@%S - unrecognized verification requested %S)"), __FUNCTION__, __LINE__, __FILE__, GetVerificationString(type));
        }
        iterator++;
    }

    // Build the graph
 //First try to RenderFile()and then try build graph for Transport Stream if the first try fails
    hr = testGraph.BuildGraph();
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to build graph for media %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
        retval = TPR_FAIL;
    }
    else {
        LOG(TEXT("%S: successfully built graph for %s"), __FUNCTION__, pMedia->GetUrl());
    }
    // Set the playback start and end positions.
    LONGLONG start = pTestDesc->start;
    LONGLONG stop = pTestDesc->stop;
    LONGLONG duration = 0;
    LONGLONG actualDuration = 0;

    // if running as an xbvt test, then we don't want to use the IMediaSeeking::GetDuration interface
    // instead, assume duration is 5 minutes (with a 10 minute timeout)
    if (retval == TPR_PASS && pTestDesc->XBvt == false)
    {        
        hr = testGraph.GetDuration(&duration);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to get the duration (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
        }
    }
    
    if (retval == TPR_PASS)
    {        
        if ( pTestDesc->XBvt == false )
        {
            //start and stop were in %, convert to actual values
            start = (pTestDesc->start*duration)/100;
            stop = (pTestDesc->stop*duration)/100;
            if (stop == 0)
                 stop = duration;

            hr = testGraph.SetAbsolutePositions(start, stop);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to set positions to %I64x and %I64x (0x%x)"), __FUNCTION__, __LINE__, __FILE__, start, stop, hr);
                retval = TPR_FAIL;
            }
        } else {
            start = 0;
            stop = 1500000000;  // in ticks; expect this converts to 2.5 minutes
        }

        // Calculate expected duration of playback
        duration = (stop - start);
    }

    // Run the graph and wait for completion
    if (retval == TPR_PASS)
    {
        // Change the state to running - the call doesn't return until the state change completes
        LOG(TEXT("%S: changing state to Running and waiting for EC_COMPLETE."), __FUNCTION__);    

        // Instantiate timer for measurement
        Timer timer;

        DWORD timeout = 2*TICKS_TO_MSEC(duration);

        // Start the playback timer
        timer.Start();

        // Signal the start of verification
        hr = testGraph.StartVerification();
        if (FAILED_F(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to start the verification (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
        }

        // Change state
        if (retval == TPR_PASS)
        {
            hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to change state to Running (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
                retval = TPR_FAIL;
            }
        }
        
        // Wait for the playback to complete
        if (retval == TPR_PASS)
        {
            // Wait for completion of playback with a timeout of twice the expected time
            hr = testGraph.WaitForCompletion(timeout);
            
            // Measure the actual duration
            actualDuration = timer.Stop();

            // Did the wait succeed?
            if (FAILED(hr))
            {
                // If the duration is very short, the WaitForCompletion will return E_ABORT since it is 
                // possible that before it gets called the playback already completed.
                // BUGBUG: from the test, it seems 86.375ms made it through. Hard coded it to 100 ms
                if ( E_ABORT == hr && duration < DURATION_THRESHOLD_IN_MS * 10000 ) 
                {
                    LOG( TEXT("%S: WARN %d@%S - WaitForCompletion aborted since the duraton is very short (%d ms)."), 
                                __FUNCTION__, __LINE__, __FILE__, duration / 10000 );
                }
                else
                {
                    LOG(TEXT("%S: ERROR %d@%S - WaitForCompletion failed (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
                    retval = TPR_FAIL;
                }
            }
        }
    }

    // Check the results of verification if there was any specified
    if (bGraphVerification && (retval == TPR_PASS))
    {
        hr = testGraph.GetVerificationResults();
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - verification failed"), __FUNCTION__, __LINE__, __FILE__);
            retval = TPR_FAIL;
        }
        else {
            LOG(TEXT("%S: verification succeeded"), __FUNCTION__);
        }
    }

    if (retval == TPR_PASS)
    {
        LOG(TEXT("%S: playback completed successfully."), __FUNCTION__);

        // Verification of playback time
        if (pDurationTolerance)
        {
            // Measure time from RenderFile to getting EC_COMPLETE notification
            LOG(TEXT("%S: total playback time (Run to EC_COMPLETE): %d msec"), __FUNCTION__, TICKS_TO_MSEC(actualDuration));

            // Measure the startup latency 
            LONGLONG startupLatency = 0;
            hr = testGraph.GetVerificationResult(StartupLatency, &startupLatency, sizeof(LONGLONG));
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to calculate startup latency"), __FUNCTION__, __LINE__, __FILE__);
            }
            else {
                LOG(TEXT("%S: startup latency %d msec"), __FUNCTION__, TICKS_TO_MSEC(startupLatency));
            }

            // Account for startup latency
            actualDuration -= startupLatency;

            // Check the percentage tolerance
            if (!ToleranceCheck(duration, actualDuration, pDurationTolerance->pctDurationTolerance) &&
                !ToleranceCheckAbs(duration, actualDuration, MSEC_TO_TICKS(pDurationTolerance->absDurationTolerance)))
            {
                LOG(TEXT("%S: ERROR %d@%S - expected duration: %d does not match actual duration %d msec. Tolerance: %d%%,%d msec: failed"), __FUNCTION__, __LINE__, __FILE__, TICKS_TO_MSEC(duration), TICKS_TO_MSEC(actualDuration), pDurationTolerance->pctDurationTolerance, pDurationTolerance->absDurationTolerance);
                retval = TPR_FAIL;
            }
            else {
                LOG(TEXT("%S: expected duration: %d matches actual duration: %d msec. Tolerance: %d%%,%d msec: suceeded"), __FUNCTION__, TICKS_TO_MSEC(duration), TICKS_TO_MSEC(actualDuration), pDurationTolerance->pctDurationTolerance, pDurationTolerance->absDurationTolerance);
            }
        }
    }

    if (retval == TPR_PASS)
    {
        LOG(TEXT("%S: successfully verified playback."), __FUNCTION__);
    }
    else {
        LOG(TEXT("%S: ERROR %d@%S - failed the playback test."), __FUNCTION__, __LINE__, __FILE__);
        retval = TPR_FAIL;
    }
      
    if (retval == TPR_PASS)
    {
        hr = StringCchPrintfW(szCmdLine,countof(szCmdLine),L"%s %s","Playback_clear ",pMedia->m_szFileID);
        

    }
    else
    {
        hr = StringCchPrintfW(szCmdLine,countof(szCmdLine),L"%s %s","Playback_investigation ",pMedia->m_szFileID);
    }
    fProcessAlive = ::CreateProcessW(L"\\Release\\MediaCubeUpdate.exe",szCmdLine,NULL,NULL,FALSE,0,NULL,NULL,&startupInfo,&procInfo);
    if (!fProcessAlive)
    {
        LOG(TEXT("%S: ERROR %d@%S - failed in Create Process of MediaCubeUpdate.exe."), __FUNCTION__, __LINE__, __FILE__);
        hr = (HRESULT)(GetLastError());
        retval = TPR_FAIL;

    }
    DWORD dw = WaitForSingleObject(procInfo.hProcess,1000*120);
    if(dw != WAIT_OBJECT_0)
    {
        LOG(TEXT("%S: ERROR %d@%S - failed in Exiting Process of MediaCubeUpdate.exe."), __FUNCTION__, __LINE__, __FILE__);
        if(procInfo.hProcess)
            ::TerminateProcess(procInfo.hProcess,1);
        retval = TPR_FAIL;
        hr = S_FALSE;
    }

  // Reset the testGraph
     testGraph.Reset();
     
     // Delete downloaded media files after each test case
    pMedia->Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);
    if(procInfo.hProcess)
    {
        ::CloseHandle(procInfo.hProcess);
    }
    if(procInfo.hThread)
    {
        ::CloseHandle(procInfo.hThread);
    }
    return retval;
}

////////////////////////////////////////////////////////////////////////////////
// CreatePath
// Creates the necessary directories in szMediaPath
//
// Parameters:
// szMediaPath            Download Location
//
// Return value:
// True if successful in CreateDirectory, False if it fails.
////////////////////////////////////////////////////////////////////////////////
BOOL CreatePath(TCHAR * szMediaPath)
{   
    if (!CreateDirectory(szMediaPath, NULL) && 
        (GetLastError() == ERROR_PATH_NOT_FOUND || GetLastError() == ERROR_FILE_NOT_FOUND))
    {
        TCHAR * pBack;
        pBack = _tcsrchr(szMediaPath, TEXT('\\'));
        if (pBack)
        {
            *pBack = 0;
            CreatePath(szMediaPath);
            *pBack = TEXT('\\');
        }
        else
            return false;
    }
    if (CreateDirectory(szMediaPath, NULL) 
        || (GetLastError() == ERROR_FILE_EXISTS
            || GetLastError() == ERROR_ALREADY_EXISTS)
        )
    {
        return true;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////
// DeletePath
// Deletes the directories in szMediaPath
//
// Parameters:
// szMediaPath            Download Location
//
// Return value:
// True if successful in RemoveDirectory, False if it fails. 
////////////////////////////////////////////////////////////////////////////////
BOOL DeletePath(TCHAR * szMediaPath)
{          
    TCHAR *szNodeList[10];
    
    //Tokenize the Directory path and read into an array
    TCHAR *token = _tcstok(szMediaPath, TEXT("\\"));
    int nIndex = 0;
    szNodeList[nIndex] = token;
    nIndex++;  
    while( token != NULL )
    { 
        // Get next token:
        token = _tcstok( NULL, TEXT("\\") ); 
        if(token != NULL)
        {
            szNodeList[nIndex] = token;
            nIndex++;
        }
    }   

   //Now delete the folders 
   while(nIndex>0)
   {
       TCHAR RootPath[MAX_PATH] = TEXT("");       
       _tcsncat(RootPath, TEXT("\\"), countof(TEXT("\\")));
       for (int nIndex2=0; nIndex2<nIndex; nIndex2++)
       {      
            _tcsncat(RootPath,szNodeList[nIndex2], _tcslen(szNodeList[nIndex2]));
            if(nIndex2 != (nIndex-1))
            _tcsncat(RootPath, TEXT("\\"), countof(TEXT("\\")));
       }
       if (!RemoveDirectory(RootPath) && (GetLastError() == ERROR_PATH_NOT_FOUND))
       {
           return false;
       }
       nIndex--;
   }
   return true;
}

TESTPROCAPI PlaybackLongPathTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;  

    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    PlaybackTestDesc *pTestDesc = (PlaybackTestDesc*)lpFTE->dwUserData;

    //Create the necessary path to download the media files    
    TCHAR *szMediaPath = pTestDesc->szDownloadLocation;
    if(szMediaPath[0] != '\0')
    {
        if(!CreatePath(szMediaPath)){
            LOG(TEXT("Create folder for long path test failed."));
            return TPR_FAIL;  
        }
    } 

    // From the test desc, determine the media file to be used
    PlaybackMedia* pMedia = pTestDesc->GetMedia(0);
    if (!pMedia)
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to retrieve media object"), __FUNCTION__, __LINE__, __FILE__);
        return TPR_FAIL;
    }

    // Instantiate the test graph
    TestGraph testGraph;
    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );
    
    
    testGraph.SetMedia(pMedia);

    // Build the graph
     //First try to RenderFile()and then try build graph for Transport Stream if the first try fails
    hr = testGraph.BuildGraph();
    if (FAILED(hr))
    {
    LOG(TEXT("%S: ERROR %d@%S - failed to build graph for media %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, 
        pMedia->GetUrl(), hr);
        retval = TPR_FAIL;
    }
    else {
        LOG(TEXT("%S: successfully built graph for %s"), __FUNCTION__, pMedia->GetUrl());
    }
    // Set the playback start and end positions.
    LONGLONG start = pTestDesc->start;
    LONGLONG stop = pTestDesc->stop;
    LONGLONG duration = 0;

    if (retval == TPR_PASS)
    {       
        hr = testGraph.GetDuration(&duration);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to get the duration (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
        }
    }
    
    if (retval == TPR_PASS)
    {       
        //start and stop were in %, convert to actual values
        start = (pTestDesc->start*duration)/100;
        stop = (pTestDesc->stop*duration)/100;
        if (stop == 0)
             stop = duration;

        hr = testGraph.SetAbsolutePositions(start, stop);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to set positions to %I64x and %I64x (0x%x)"), __FUNCTION__, __LINE__, __FILE__, start, stop, hr);
            retval = TPR_FAIL;
        }

        // Calculate expected duration of playback
        duration = (stop - start);
    }

    // Run the graph and wait for completion
    if (retval == TPR_PASS)
    {
        // Change the state to running - the call doesn't return until the state change completes
        LOG(TEXT("%S: changing state to Running and waiting for EC_COMPLETE."), __FUNCTION__);  

        DWORD timeout = 2*TICKS_TO_MSEC(duration);

        // Change state
        if (retval == TPR_PASS)
        {
            hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to change state to Running (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
                retval = TPR_FAIL;
            }
        }
        
        // Wait for the playback to complete
        if (retval == TPR_PASS)
        {
            // Wait for completion of playback with a timeout of twice the expected time
            hr = testGraph.WaitForCompletion(timeout);
            
            // Did the wait succeed?
            if (FAILED(hr))
            {
                // If the duration is very short, the WaitForCompletion will return E_ABORT since it is 
                // possible that before it gets called the playback already completed.
                // BUGBUG: from the test, it seems 86.375ms made it through. Hard coded it to 100 ms
                if ( E_ABORT == hr && duration < DURATION_THRESHOLD_IN_MS * 10000 ) 
                {
                    LOG( TEXT("%S: WARN %d@%S - WaitForCompletion aborted since the duraton is very short (%d ms)."), 
                                __FUNCTION__, __LINE__, __FILE__, duration / 10000 );
                }
                else
                {
                    LOG(TEXT("%S: ERROR %d@%S - WaitForCompletion failed (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
                    retval = TPR_FAIL;
                }
            }
        }
    }

    
    if (retval == TPR_PASS)
    {
        LOG(TEXT("%S: successfully long path playback."), __FUNCTION__);
    }
    else {
        LOG(TEXT("%S: ERROR %d@%S - failed the long path playback test."), __FUNCTION__, __LINE__, __FILE__);
        retval = TPR_FAIL;
    }

    // Reset the testGraph
    testGraph.Reset();
    
    // Delete downloaded media files after each test case
    pMedia->Reset();

    //Delete directories
    if(szMediaPath[0] != '\0')
    {
        if(!DeletePath(szMediaPath)){
            LOG(TEXT("Delete folder for long path test failed."));
            retval = TPR_FAIL;
        }
    } 
    return retval;
}

TESTPROCAPI PlaybackDurationTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    PlaybackTestDesc *pTestDesc = (PlaybackTestDesc*)lpFTE->dwUserData;

    // From the test desc, determine the media file to be used
    PlaybackMedia* pMedia = pTestDesc->GetMedia(0);
    if (!pMedia)
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to retrieve media object"), __FUNCTION__, __LINE__, __FILE__);
        return TPR_FAIL;
    }

    // Instantiate the test graph
    TestGraph testGraph;
    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );

    testGraph.SetMedia(pMedia);
    
    // This test doesn not handle generic verifications - so don't check for them
    // But this test needs the playback duration verifier
    PlaybackDurationTolerance* pDurationTolerance = NULL;
    VerificationList verificationList = pTestDesc->GetVerificationList();
    VerificationList::iterator iterator = verificationList.begin();
    VerificationType type = (VerificationType)iterator->first;
    if (type == VerifyPlaybackDuration)
    {
        // Get the duration tolerance
        pDurationTolerance = (PlaybackDurationTolerance*)iterator->second;
        hr = testGraph.EnableVerification(VerifyPlaybackDuration, NULL);
        if (FAILED(hr))
            retval = TPR_FAIL;
    }
    else
    {
        retval = TPR_FAIL;
    }

    // Build the graph
     //First try to RenderFile()and then try build graph for Transport Stream if the first try fails
    hr = testGraph.BuildGraph();
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to build graph for media %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
        retval = TPR_FAIL;
    }
    else {
        LOG(TEXT("%S: successfully built graph for %s"), __FUNCTION__, pMedia->GetUrl());
    }
    // Set the playback start and end positions.
    LONGLONG start = pTestDesc->start;
    LONGLONG stop = pTestDesc->stop;
    // Expected duration of media
    LONGLONG duration = 0;
    // Acutal time that it took
    LONGLONG actualDuration = 0;
    // Latency of getting EC_COMPLETE
    LONGLONG ecCompletionLatency = 0;
    
    // Get the duration
    if (retval == TPR_PASS)
    {
        hr = testGraph.GetDuration(&duration);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to get duration (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
        }
    }

    // Set the positions requested
    if (retval == TPR_PASS)
    {
        start = pTestDesc->start*duration/100;
        stop = pTestDesc->stop*duration/100;
        if (stop == 0)
             stop = duration;

        hr = testGraph.SetAbsolutePositions(start, stop);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to set positions to %I64x and %I64x (0x%x)"), __FUNCTION__, __LINE__, __FILE__, start, stop, hr);
            retval = TPR_FAIL;
        }

        // Calculate expected duration of playback
        duration = (stop - start);
    }

    // Instantiate timer for measurement
    Timer timer;

    // Run the graph and wait for completion
    if (retval == TPR_PASS)
    {
        // Change the state to running - the call doesn't return until the state change completes
        LOG(TEXT("%S: changing state to Running and waiting for EC_COMPLETE."), __FUNCTION__);    

        DWORD timeout = 2*TICKS_TO_MSEC(duration);

        // Signal the start of verification
        hr = testGraph.StartVerification();
        if (FAILED_F(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to start the verification (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
        }

        // Start the playback timer
        timer.Start();

        // Change state
        if (retval == TPR_PASS)
        {
            hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to change state to Running (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
                retval = TPR_FAIL;
            }
        }
        
        // Wait for the clip to playback
        if (retval == TPR_PASS)
        {
            hr = testGraph.WaitForCompletion(timeout);
            
            // Measure the playback duration and the current time in ticks
            ecCompletionLatency = MSEC_TO_TICKS(GetTickCount());
            actualDuration = timer.Stop();

            // Did the wait succeed?
            if (FAILED(hr))
            {
                // If the duration is very short, the WaitForCompletion will return E_ABORT since it is 
                // possible that before it gets called the playback already completed.
                // BUGBUG: from the test, it seems 86.375ms made it through. Hard coded it to 100 ms
                if ( E_ABORT == hr && duration < DURATION_THRESHOLD_IN_MS * 10000 ) 
                {
                    LOG( TEXT("%S: WARN %d@%S - WaitForCompletion aborted since the duraton is very short (%d ms)."), 
                                __FUNCTION__, __LINE__, __FILE__, duration / 10000 );
                }
                else
                {
                    LOG(TEXT("%S: ERROR %d@%S - WaitForCompletion failed (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
                    retval = TPR_FAIL;
                }
            }
        }
    }

    // Check the results of playback duration verification
    if (retval == TPR_PASS)
    {
        PlaybackDurationData data;
        hr = testGraph.GetVerificationResult(VerifyPlaybackDuration, &data, sizeof(PlaybackDurationData));
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - playback duration verification failed"), __FUNCTION__, __LINE__, __FILE__);
            retval = TPR_FAIL;
        }
        else {
            LOG(TEXT("%S: first sample time - %d msec, EOS time - %d msec, playback duration - %d msec"), __FUNCTION__, TICKS_TO_MSEC(data.firstSampleTime), TICKS_TO_MSEC(data.eosTime), TICKS_TO_MSEC(actualDuration));

            // Take out the first sample time
            actualDuration -= data.firstSampleTime;

            // Check with the pct and tolerance absolute tolerances
            if (!ToleranceCheck(duration, actualDuration, pDurationTolerance->pctDurationTolerance) &&
                !ToleranceCheckAbs(duration, actualDuration, MSEC_TO_TICKS(pDurationTolerance->absDurationTolerance)))
            {
                LOG(TEXT("%S: ERROR %d@%S - expected duration: %d does not match actual duration %d msec. Tolerance: %d%%,%d msec: failed"), __FUNCTION__, __LINE__, __FILE__, TICKS_TO_MSEC(duration), TICKS_TO_MSEC(actualDuration), TICKS_TO_MSEC(pDurationTolerance->pctDurationTolerance), TICKS_TO_MSEC(pDurationTolerance->absDurationTolerance));
                retval = TPR_FAIL;
            }
            else {
                LOG(TEXT("%S: expected duration: %d matches actual duration: %d msec. Tolerance: %d%%,%d msec: suceeded"), __FUNCTION__, TICKS_TO_MSEC(duration), TICKS_TO_MSEC(actualDuration), TICKS_TO_MSEC(pDurationTolerance->pctDurationTolerance), TICKS_TO_MSEC(pDurationTolerance->absDurationTolerance));
            }

            // Calculate the time taken to send the EC_COMPLETE after it first flowed through the graph
            ecCompletionLatency = (ecCompletionLatency > data.eosTime) ? (ecCompletionLatency - data.eosTime) : (0xffffffff - data.eosTime + ecCompletionLatency);

            // If the time taken to send the EC_COMPLETE is greater than the tolerance
            if (ecCompletionLatency > MSEC_TO_TICKS(EC_COMPLETE_TOLERANCE))
            {
                LOG(TEXT("%S: ERROR %d@%S - EC_COMPLETE arrived %d msec after the EOS flowed through graph. Failing."), __FUNCTION__, __LINE__, __FILE__, TICKS_TO_MSEC(ecCompletionLatency));
                retval = TPR_FAIL;
            }
            else {
                LOG(TEXT("%S: EC Completion latency - %d msec"), __FUNCTION__, TICKS_TO_MSEC(ecCompletionLatency));
            }
        }
    }

    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    return retval;
}
//MultiplePlaybackTest test building multiple graphs and state changes for the graphs, it will wait for each
//graph to complete its playingback( end of stream) and then exit, if you have XML flag "ResMonitor", it will also
//Monitor the resource including Memory(physical, virtual) and CPU usuage during testing,if CPU is over 80%, the thread
//would exit to avoid system hanging. 
//BUGBUG: For now we are not set memory limit since have not got memory reqirement from PM yet.
TESTPROCAPI MultiplePlaybackTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;
    HANDLE* eventArray = NULL;
    TestGraph * testGraphArray = NULL;

    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    //CPU monitoring and memory stats variables  

    MEMORYSTATUS MemoryBeforeStreaming;
    MEMORYSTATUS MemoryAfterStreaming;    
    DWORD    MemoryLoadRecord    [SYSTEM_MONITOR_DATA_MAX];
    DWORD    PhyMemoryUseRecord  [SYSTEM_MONITOR_DATA_MAX];
    DWORD    VirMemoryUseRecord  [SYSTEM_MONITOR_DATA_MAX];
    int      CPUUseRecord        [SYSTEM_MONITOR_DATA_MAX];

    // Get the test config object from the test parameter
    PlaybackTestDesc *pTestDesc = (PlaybackTestDesc*)lpFTE->dwUserData;

    // From the test desc, determine the media file to be used
    int nMedia = pTestDesc->GetNumMedia();   
    
    GlobalMemoryStatus( &MemoryBeforeStreaming );           
    SystemMonitor& SM = SystemMonitor::GetInstance();

    // Instantiate the multiple test graphs
    testGraphArray = new TestGraph[nMedia];
    if (!testGraphArray)
    {
        LOG(TEXT("%S: ERROR %d@%s - failed to allocate multiple TestGraph objects."), __FUNCTION__, __LINE__, __FILE__);
        return TPR_FAIL;
    }
  
    if (pTestDesc->resMonitor)
    {       
         SM.StartSystemMonitoring(10);  //every 10 sec.
    }

    if ( pTestDesc->RemoteGB() )
    {
        for ( int i = 0; i < nMedia; i++ )
            testGraphArray[i].UseRemoteGB( TRUE );
    }

    // Build the graphs
    for(int i = 0; i < nMedia; i++)
    {
        PlaybackMedia* pMedia = pTestDesc->GetMedia(i);
        if (!pMedia)
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to retrieve media object"), __FUNCTION__, __LINE__, __FILE__);
            retval = TPR_FAIL;
            break;
        }

        testGraphArray[i].SetMedia(pMedia);
        //First try to RenderFile()and then try build graph for Transport Stream if the first try fails
        hr = testGraphArray[i].BuildGraph();
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to build graph for media %d - %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, i, pMedia->GetUrl(), hr);
            retval = TPR_FAIL;
        }
    }

    if (retval == TPR_PASS)
    {
        LOG(TEXT("%S: successfully built graph for the %d media files."), __FUNCTION__, nMedia);

        // PlaybackMedia* pMedia = pTestDesc->GetMedia(i);
        for(int i = 0; i < nMedia; i++)
        {
            PlaybackMedia* pMedia = pTestDesc->GetMedia(i);
            // Change the state to running - the call doesn't return until the state change completes and the graph starts rendering
            hr = testGraphArray[i].SetState(State_Running,TestGraph::SYNCHRONOUS);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to change state to Running for file %s with code(0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
                retval = TPR_FAIL;
            }
            else
                LOG(TEXT("%S: changed state to Running for Media %s for the %d-th graph"), __FUNCTION__,pMedia->GetUrl(),i + 1);
        }
    }


    if (retval == TPR_PASS)
    {
        LOG(TEXT("%S: changed state to Running"), __FUNCTION__);

        eventArray = new HANDLE[nMedia];
        if (eventArray)
        {
            //PlaybackMedia* pMedia = pTestDesc->GetMedia(i);
            for(int i = 0; i < nMedia; i++)
            {
                PlaybackMedia* pMedia = pTestDesc->GetMedia(i);
                hr = testGraphArray[i].GetEventHandle(&eventArray[i]);
                if (FAILED(hr))
                {
                    LOG(TEXT("%S: ERROR %d@%S - failed to get event handle for media %d - %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, i, pMedia->GetUrl(), hr);
                    retval = TPR_FAIL;
                }
            }
        }
        else {
            LOG(TEXT("%S: ERROR %d@%s - failed to allocate event handle array."), __FUNCTION__, __LINE__, __FILE__);
            retval = TPR_FAIL;
        }
    }

    if (retval == TPR_PASS)
    {
        LOG(TEXT("%S: got event handles"), __FUNCTION__);

        // Wait for completion on all events
        //  hr = WaitForMultipleObjects(nMedia, eventArray, TRUE, INFINITE); 
        for ( int i = 0; i < nMedia; i++)
        {
            //hr = WaitForSingleObject(eventArray[i],INFINITE);
            LONG eventCode=0;
            hr = testGraphArray[i].WaitForCompletion(INFINITE,&eventCode);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - wait failed on event handles (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
                LOG(TEXT("%S: ERROR %d@%S - failed to receive EC_COMPLETE (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
                LOG(TEXT("The extended Error Code from GetLastError() is 0x%x"),(HRESULT)(GetLastError()));        
                retval = TPR_FAIL;
                break;
            }
        }
    }

    if (retval == TPR_FAIL)    {
        LOG(TEXT("%S: ERROR %d@%S - failed the multiple playback test"), __FUNCTION__, __LINE__, __FILE__);
    }
    else {
        LOG(TEXT("%S: successfully verified multiple playback."), __FUNCTION__);
    }

    for (int i = 0; i < nMedia; i++)
    {
        if(eventArray && eventArray[i])
        {
            CloseHandle(eventArray[i]);
        }

    }
    if (eventArray)
        delete eventArray;
    // Reset the testGraph
    if (testGraphArray)
    {
        for(int i = 0; i < nMedia; i++)
        {
            testGraphArray[i].Reset();
        }
        delete [] testGraphArray;
    }

    if (pTestDesc->resMonitor)
    {
        SM.StopSystemMonitoring();
        UINT SysmonRecordCount = SM.GetSysmonInfoRecordCount();
        if(SysmonRecordCount > SYSTEM_MONITOR_DATA_MAX)
        {
            SysmonRecordCount = SYSTEM_MONITOR_DATA_MAX;
        }

        for (UINT i = 0; i < SysmonRecordCount; i++)
        {
            const SysmonInfo& si = SM[i];
            MemoryLoadRecord[i] = si.MemoryLoad;
            PhyMemoryUseRecord[i] = si.PhysMemoryUse;
            VirMemoryUseRecord[i] = si.VirtMemoryUse;
            CPUUseRecord[i] = si.CpuUse;
        }

        PrintSystemMonitoringData(MemoryLoadRecord, PhyMemoryUseRecord, VirMemoryUseRecord, 
                                  CPUUseRecord, SysmonRecordCount, NULL);
        retval = AnalyzeSystemMonitoringData(MemoryLoadRecord, PhyMemoryUseRecord, VirMemoryUseRecord, 
                                  CPUUseRecord, SysmonRecordCount);
                 
        CoUninitialize();

        GlobalMemoryStatus( &MemoryAfterStreaming );
        LOG(L"%s: Global Memory Status Before and After the test", __FUNCTION__);
        GetMemoryLoadData( MemoryBeforeStreaming, MemoryAfterStreaming );
     
    }   
    
    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    return retval;
}

HRESULT RandomStateChangeTest(TestGraph& testGraph, int nStateChanges, FILTER_STATE initialState, int seed, DWORD dwTimeBetwenStates)
{
    HRESULT hr = S_OK;

    FILTER_STATE currState = initialState;
    FILTER_STATE nextState = currState;

    // Set the initial state
    testGraph.SetState(currState, TestGraph::SYNCHRONOUS);

    // Now randomly switch states
    LOG(TEXT("%S: switching randomly between states"), __FUNCTION__);

    for(int i = 0; i < nStateChanges; i++)
    {
        // Get the next state
        nextState = testGraph.GetRandomState();

        // Change to the new state
        hr = testGraph.SetState(nextState, TestGraph::SYNCHRONOUS);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to change state from %s to %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, GetStateName(currState), GetStateName(nextState), hr);
            break;
        }
        
        currState = nextState;

        Sleep(dwTimeBetwenStates);
    }

    return hr;
}


TESTPROCAPI StateChangeTest(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    StateChangeTestDesc *pTestDesc = (StateChangeTestDesc*)lpFTE->dwUserData;

    // From the test desc, determine the media file to be used
    PlaybackMedia* pMedia = pTestDesc->GetMedia(0);
    if (!pMedia)
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to retrieve media object"), __FUNCTION__, __LINE__, __FILE__);
        return TPR_FAIL;
    }


    // Instantiate the test graph
    TestGraph testGraph;
    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );

    testGraph.SetMedia(pMedia);

    // Build the graph
     //First try to RenderFile()and then try build graph for Transport Stream if the first try fails
    hr = testGraph.BuildGraph();
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to build graph (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
    }
    else {
        LOG(TEXT("%S: successfully built graph for %s"), __FUNCTION__, pMedia->GetUrl());
    }
    if (retval == TPR_PASS)
    {
        DWORD* pLatency = NULL;
        
        // If the latency has to be verified
        bool bVerifyLatency = pTestDesc->GetVerification(VerifyStateChangeLatency, (void**)&pLatency);

        // Now run through the state changes
        Timer timer;
        LONGLONG latency = 0;
        FILTER_STATE currState = pTestDesc->m_StateChange.state;
        FILTER_STATE nextState;
        LONGLONG llAvgLatency = 0;
        for(DWORD i = 0; (retval == TPR_PASS) && (i < pTestDesc->m_StateChange.nStateChanges); i++)
        {
            LOG(TEXT("%S: setting graph state to %s"), __FUNCTION__, GetStateName(currState));

            // Set the state
            timer.Start();
            hr = testGraph.SetState(currState, TestGraph::SYNCHRONOUS);
            latency = timer.Stop();
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to set state to %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, GetStateName(currState), hr);
                retval = TPR_FAIL;
            }

            // Check latency if needed
            if ((retval == TPR_PASS) && bVerifyLatency)
            {
                if (TICKS_TO_MSEC(latency) > *pLatency)
                {
                    LOG(TEXT("%S: ERROR %d@%S - state change latency (%d) ms exceeded tolerance (%d) msec (0x%x)"), __FUNCTION__, __LINE__, __FILE__, TICKS_TO_MSEC(latency), *pLatency);
                    retval = TPR_FAIL;
                }
                else {
                    LOG(TEXT("%S: latency %u"), __FUNCTION__, TICKS_TO_MSEC(latency));
                }
                llAvgLatency = llAvgLatency + latency;
            }

            // Get the next state
            if (retval == TPR_PASS)
            {
                // Get the next state to set
                hr = GetNextState(pTestDesc->m_StateChange.sequence, currState, &nextState);
                if (FAILED(hr))
                {
                    LOG(TEXT("%S: ERROR %d@%S - failed to get next state (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
                    retval = TPR_FAIL;
                }
                currState = nextState;
            }

            if (retval == TPR_FAIL)
                break;

            Sleep(pTestDesc->m_StateChange.dwTimeBetweenStates);
        }

        //calculate avg latency
        if(i!=0) 
        {
            llAvgLatency = llAvgLatency / i;
            LOG(TEXT("%S: Average latency for this run: %u"), __FUNCTION__, TICKS_TO_MSEC(llAvgLatency));
        }
        
        // Verification of state change
        if (retval == TPR_PASS)
        {
            LOG(TEXT("%S: successfully verified state changes."), __FUNCTION__);
        }
        else
        {
            LOG(TEXT("%S: ERROR %d@%S - failed the state change test (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
        }
    }

    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    return retval;
}


TESTPROCAPI Run_Pause_Run_Test(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD retval = TPR_PASS;
    HRESULT hr = S_OK;

    if (HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED)
        return SPR_HANDLED;
    else if (TPM_EXECUTE != uMsg)
        return TPR_NOT_HANDLED;

    // Get the test config object from the test parameter
    PlaybackTestDesc *pTestDesc = (PlaybackTestDesc*)lpFTE->dwUserData;

    // From the test desc, determine the media file to be used
    PlaybackMedia* pMedia = pTestDesc->GetMedia(0);
    if (!pMedia)
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to retrieve media object"), __FUNCTION__, __LINE__, __FILE__);
        return TPR_FAIL;
    }

    // Instantiate the test graph
    TestGraph testGraph;
    if ( pTestDesc->RemoteGB() )
        testGraph.UseRemoteGB( TRUE );

    testGraph.SetMedia(pMedia);
    
    // Enable the verification required
    bool bGraphVerification = false;
    LONGLONG posTolerance = 0;
    VerificationList verificationList = pTestDesc->GetVerificationList();
    VerificationList::iterator iterator = verificationList.begin();
    while (iterator != verificationList.end())
    {
        VerificationType type = (VerificationType)iterator->first;
        if (testGraph.IsCapable(type)) {
            hr = testGraph.EnableVerification(type, iterator->second);
            if (FAILED_F(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to enable verification %S)"), __FUNCTION__, __LINE__, __FILE__, GetVerificationString(type));
                break;
            }
            bGraphVerification = true;
        }
        else {
            LOG(TEXT("%S: WARNING %d@%S - unrecognized verification requested %S)"), __FUNCTION__, __LINE__, __FILE__, GetVerificationString(type));
        }
        iterator++;
    }

    // Build the graph
     //First try to RenderFile()and then try build graph for Transport Stream if the first try fails
    hr = testGraph.BuildGraph();
    if (FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to build graph for media %s (0x%x)"), __FUNCTION__, __LINE__, __FILE__, pMedia->GetUrl(), hr);
        retval = TPR_FAIL;
        goto cleanup;
    }
    else {
        LOG(TEXT("%S: successfully built graph for %s"), __FUNCTION__, pMedia->GetUrl());
    }

    // Get the duration
    LONGLONG duration = 0;

    // if running as an xbvt test, then we don't want to use the IMediaSeeking::GetDuration interface
    // instead, assume duration is 5 minutes (with a 10 minute timeout)
    if (retval == TPR_PASS && pTestDesc->XBvt == false)
    {
        if (retval == TPR_PASS)
        {
            hr = testGraph.GetDuration(&duration);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to get the duration (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
                retval = TPR_FAIL;
            }
        }

    } else {
        duration = 1500000000;  // in ticks; expect this converts to 2.5 minutes
    }

    // The test consists of the following steps
    // 1. Set the graph in running state
    // 2. Pause the graph
    // 3. Start verification
    // 4. Run the graph
    // 3. Get the verification results

    // Set the graph in running state
    if (retval == TPR_PASS)
    {
        hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to change state to Running (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
        }
        else {
            LOG(TEXT("%S: changed state to running."), __FUNCTION__);
        }
    }

    // Sleep some time
    if (retval == TPR_PASS)
    {
        // BUGBUG: add the delay specification to the config file
        Sleep(WAIT_BEFORE_NEXT_OPERATION);
    }

    // Set the graph in pause state
    if (retval == TPR_PASS)
    {
        hr = testGraph.SetState(State_Paused, TestGraph::SYNCHRONOUS);
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to change state to Paused (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
            retval = TPR_FAIL;
        }
        else {
            LOG(TEXT("%S: changed state to Pause."), __FUNCTION__);
        }
    }

    // Sleep some more time
    if (retval == TPR_PASS)
    {
        // BUGBUG: add the delay specification to the config file
        Sleep(WAIT_BEFORE_NEXT_OPERATION);
    }

    // Signal the start of verification
    if ((retval == TPR_PASS) && bGraphVerification)
    {
        hr = testGraph.StartVerification();
        if (FAILED_F(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - failed to start the verification (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        }
    }

    // Sleep some more time
    if (retval == TPR_PASS)
    {
        // BUGBUG: add the delay specification to the config file
        Sleep(WAIT_BEFORE_NEXT_OPERATION);
    }


    if (retval == TPR_PASS)
    {
        // Set the graph in running state
        if (retval == TPR_PASS)
        {
            hr = testGraph.SetState(State_Running, TestGraph::SYNCHRONOUS);
            if (FAILED(hr))
            {
                LOG(TEXT("%S: ERROR %d@%S - failed to change state to Running (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
                retval = TPR_FAIL;
            }
            else {
                LOG(TEXT("%S: changed state to running."), __FUNCTION__);
            }
        }
    }

    // Sleep some more time
    if (retval == TPR_PASS)
    {
        // BUGBUG: add the delay specification to the config file
        Sleep(WAIT_FOR_VERIFICATION);
    }

    // Check the results of verification
    if (bGraphVerification && (retval == TPR_PASS))
    {
        hr = testGraph.GetVerificationResults();
        if (FAILED(hr))
        {
            LOG(TEXT("%S: ERROR %d@%S - verification failed"), __FUNCTION__, __LINE__, __FILE__);
            retval = TPR_FAIL;
        }
        else {
            LOG(TEXT("%S: verification succeeded"), __FUNCTION__);
        }
    }

    // Set the state back to stop
    hr = testGraph.SetState(State_Stopped, TestGraph::SYNCHRONOUS);
    if ((retval == TPR_PASS) && FAILED(hr))
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to change state to stopped (0x%x)"), __FUNCTION__, __LINE__, __FILE__, hr);
        retval = TPR_FAIL;
    }

cleanup:
    // Reset the testGraph
    testGraph.Reset();

    // Readjust return value if HRESULT matches/differs from expected in some instances
    retval = pTestDesc->AdjustTestResult(hr, retval);

    // Reset the test
    pTestDesc->Reset();

    return retval;
}


void PrintSystemMonitoringData(DWORD MemoryLoadRecord[], DWORD PhyMemoryUseRecord[], DWORD VirMemoryUseRecord[], int CPUUseRecord[], UINT RecordCount, WCHAR *pwzFileNamePrefix)
{
    LOG ( L"\n- MemLoad    PhyMemUse    VirMemUse    CPUUse -\n");
    for(UINT i = 0; i < RecordCount; i++)
    {
        LOG(L"%8d %13d %13d %8d", MemoryLoadRecord[i], PhyMemoryUseRecord[i], VirMemoryUseRecord[i], CPUUseRecord[i]);
    }
    LOG ( L"\n"); 
    return;
}

DWORD AnalyzeSystemMonitoringData(DWORD MemoryLoadRecord[], DWORD PhyMemoryUseRecord[], DWORD VirMemoryUseRecord[], int CPUUseRecord[], UINT RecordCount)
{
    UINT MaxLoadIndex, MaxPhyMemIndex, MaxVirMemIndex, MaxCPUIndex;
    UINT MinLoadIndex, MinPhyMemIndex, MinVirMemIndex, MinCPUIndex;

    UINT index = 2;
    MinLoadIndex = MinPhyMemIndex = MinVirMemIndex = MinCPUIndex = index;
    MaxLoadIndex = MaxPhyMemIndex = MaxVirMemIndex = MaxCPUIndex = index;

    DWORD retval = TPR_PASS;
    //we don't use the first and last two data points, 
    //because they corresponds they were not captured during the test
    for(index = 3; index < RecordCount-3; index++)
    {
        if(MemoryLoadRecord[MaxLoadIndex] <= MemoryLoadRecord[index])//using <= allows us to find the later data points among many same value max
        {
            MaxLoadIndex = index;
        }

        if(MemoryLoadRecord[MinLoadIndex] >= MemoryLoadRecord[index])
        {
            MinLoadIndex = index;
        }
    }
    for(index = 3; index < RecordCount-3; index++)
    {
        if(PhyMemoryUseRecord[MaxPhyMemIndex] <= PhyMemoryUseRecord[index])
        {
            MaxPhyMemIndex = index;
        }
        if(PhyMemoryUseRecord[MinPhyMemIndex] >= PhyMemoryUseRecord[index])
        {
            MinPhyMemIndex = index;
        }
    }
    for(index = 3; index < RecordCount-3; index++)
    {
        if(VirMemoryUseRecord[MaxVirMemIndex] <= VirMemoryUseRecord[index])
        {
            MaxVirMemIndex = index;
        }
        if(VirMemoryUseRecord[MinVirMemIndex] >= VirMemoryUseRecord[index])
        {
            MinVirMemIndex = index;
        }
    }
    for(index = 3; index < RecordCount-3; index++)
    {
        if(CPUUseRecord[MaxCPUIndex] <= CPUUseRecord[index])
        {
            MaxCPUIndex = index;
        }
        if(CPUUseRecord[MinCPUIndex] >= CPUUseRecord[index])
        {
            MinCPUIndex = index;
        }
    }

    LOG ( TEXT("MinLoad [%d] = %d"), MinLoadIndex, MemoryLoadRecord[MinLoadIndex] );
    LOG ( TEXT("MaxLoad [%d] = %d"), MaxLoadIndex, MemoryLoadRecord[MaxLoadIndex] );
    LOG ( L"MinPhyMem   [%d] = %d", MinPhyMemIndex,  PhyMemoryUseRecord[MinPhyMemIndex]);
    LOG ( L"MaxPhyMem   [%d] = %d", MaxPhyMemIndex,  PhyMemoryUseRecord[MaxPhyMemIndex]);
    LOG ( L"MinVirMem   [%d] = %d", MinVirMemIndex,  VirMemoryUseRecord[MinVirMemIndex]);
    LOG ( L"MaxVirMem   [%d] = %d", MaxVirMemIndex,  VirMemoryUseRecord[MaxVirMemIndex]);
    LOG ( TEXT("MinCPU  [%d] = %d"), MinCPUIndex,  CPUUseRecord[MinCPUIndex]);
    LOG ( TEXT("MaxCPU  [%d] = %d"), MaxCPUIndex,  CPUUseRecord[MaxCPUIndex]);
    LOG ( TEXT("Total Data Points = %d"), RecordCount-1);
    LOG ( TEXT("If thre were multiple min and max values then the last one is shown here"));
    LOG ( TEXT("where value withing [] is the index of the data point"));
 
            //If CPU use is ever more than 80, test should fail
            if(MaxCPUIndex > 1 && CPUUseRecord[MaxCPUIndex] > 80)
            {
                LOG ( TEXT("Failing multiple graph test based on CPU uses data, Max CPU Uses is = %d"), CPUUseRecord[MaxCPUIndex]);
                retval = TPR_FAIL;//failing test
            }


            DWORD AvgPhyMemUseHead = (PhyMemoryUseRecord[2] + 
                PhyMemoryUseRecord[3] + 
                PhyMemoryUseRecord[4])/3;

            DWORD AvgPhyMemUseTail = (PhyMemoryUseRecord[RecordCount-3] + 
                PhyMemoryUseRecord[RecordCount-4] + 
                PhyMemoryUseRecord[RecordCount-5])/3;

            DWORD AvgPhyMemUseAroundMax = 0;
            if((MaxPhyMemIndex > 2) && (MaxPhyMemIndex < (RecordCount - 3)))
            {
                AvgPhyMemUseAroundMax = (PhyMemoryUseRecord[MaxPhyMemIndex-1] + 
                    PhyMemoryUseRecord[MaxPhyMemIndex] + 
                    PhyMemoryUseRecord[MaxPhyMemIndex+1])/3;
            }

            if( AvgPhyMemUseTail > AvgPhyMemUseHead)
            {
                if(AvgPhyMemUseAroundMax > 0)
                {
                    if(AvgPhyMemUseTail > AvgPhyMemUseAroundMax)
                    {
                        LOG ( TEXT("Failing multiple graph test based on Memory uses data!"));
                        LOG ( TEXT("avg Memory use just after test started = %d"), AvgPhyMemUseHead);
                        LOG ( TEXT("avg Memory use around where the max mem use found, is = %d"), AvgPhyMemUseAroundMax);
                        LOG ( TEXT("avg Memory use just before test ended is = %d"), AvgPhyMemUseTail);
                        LOG ( TEXT("avg mem use during the end of the test is expected to be lower than where the max mem use was found."));
                        retval = TPR_FAIL;//failing test
                    }
                }
                else
                {
                    LOG ( TEXT("Failing multiple graph test based on Memory uses data!"));
                    LOG ( TEXT("avg Memory use just after test started = %d"), AvgPhyMemUseHead);
                    LOG ( TEXT("avg Memory use just before test ended is = %d"), AvgPhyMemUseTail);
                    LOG ( TEXT("virtually there should have been no difference in memory use for these two cases."));
                    retval = TPR_FAIL;//failing test

                }
            }
    return retval;
}

DWORD GetMemoryLoadData(MEMORYSTATUS MemoryBefore, MEMORYSTATUS MemoryAfter )
{
    DWORD MemoryLoadBefore = ( MemoryBefore.dwMemoryLoad ); 
    DWORD TotalPhysBefore  = ( MemoryBefore.dwTotalPhys )/1024; 
    DWORD AvailPhysBefore  = ( MemoryBefore.dwAvailPhys )/1024;
    DWORD TotalPgBefore    = ( MemoryBefore.dwTotalPageFile )/1024; 
    DWORD AvailPgBefore    = ( MemoryBefore.dwAvailPageFile )/1024;
    DWORD TotalVirBefore   = ( MemoryBefore.dwTotalVirtual )/1024;
    DWORD AvailVirBefore   = ( MemoryBefore.dwAvailVirtual )/1024;

    DWORD MemoryLoadAfter = ( MemoryAfter.dwMemoryLoad ); 
    DWORD TotalPhysAfter  = ( MemoryAfter.dwTotalPhys )/1024; 
    DWORD AvailPhysAfter  = ( MemoryAfter.dwAvailPhys )/1024;
    DWORD TotalPgAfter    = ( MemoryAfter.dwTotalPageFile )/1024; 
    DWORD AvailPgAfter    = ( MemoryAfter.dwAvailPageFile )/1024;
    DWORD TotalVirAfter   = ( MemoryAfter.dwTotalVirtual )/1024;
    DWORD AvailVirAfter   = ( MemoryAfter.dwAvailVirtual )/1024;

    DWORD MemoryLoadDiff = ( MemoryLoadAfter - MemoryLoadBefore ); 
    DWORD AvailPhysDiff  = ( MemoryBefore.dwAvailPhys - MemoryAfter.dwAvailPhys);

    LOG ( L"Memory Status (in Bytes)");
    LOG ( L"       MemoryLoad - TotalPhys Available");
    LOG ( L"Before: (%d)      - (%d)",MemoryLoadBefore, MemoryBefore.dwAvailPhys);
    LOG ( L"After:  (%d)      - (%d)",MemoryLoadAfter,  MemoryAfter.dwAvailPhys);
    LOG ( L"Diffe:  (%d)      - (%d)",MemoryLoadDiff,   AvailPhysDiff);
    
    return MemoryLoadDiff;
}
