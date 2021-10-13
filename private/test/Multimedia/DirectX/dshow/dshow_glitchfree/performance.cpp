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
// --------------------------------------------------------------------
//                                                                     
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A      
// PARTICULAR PURPOSE.                                                 
//                                                                     
// --------------------------------------------------------------------
#include "Performance.h"
#include <windows.h>
#include "utility.h"

using namespace std;

CPerformanceMonitor::CPerformanceMonitor(DWORD period, TCHAR* log)
{
    LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    m_bEndThread = false;
    
    // Set period
    m_dwSamplingPeriod = period;

    // Reset data
    m_TotalCpuUtil = 0.0;
    m_nRecordings = 0;
    m_bWasPegged = false;
    
    // Current state: paused
    m_sCurrentState = CPerformanceMonitor::Paused;

    // Events for controlling the thread
    m_evStartThread = CreateEvent(NULL, false, false, _T("PerfMonitor:StartThread"));
    if (!(m_evStartThread))
    {
        LOG(TEXT("%S: ERROR %d@%S - Could not create start thread event (GLE: 0x%0x)"), __FUNCTION__, __LINE__, __FILE__, GetLastError());
        throw;
    }
    m_evDestroyThread = CreateEvent(NULL, false, false, _T("PerfMonitor:DestroyThread"));
    if (!(m_evDestroyThread))
    {
        LOG(TEXT("%S: ERROR %d@%S - Could not create destroy thread event (GLE: 0x%0x)"), __FUNCTION__, __LINE__, __FILE__, GetLastError());
        throw;
    }

    // Create the monitor thread
    m_hPerformanceThread = CreateThread(NULL, 0, StaticThreadProc, this, 0, &m_dwThreadID);
    if (!(m_hPerformanceThread))
    {
        LOG(TEXT("%S: ERROR %d@%S - Could not create performance thread (GLE: 0x%0x)"), __FUNCTION__, __LINE__, __FILE__, GetLastError());
        throw;
    }

    // Set this to the highest priority so we never get interrupted when taking the tick and idle counts
    SetThreadPriority(m_hPerformanceThread, THREAD_PRIORITY_HIGHEST);

    if (log)
    {
        /*StringCbPrintf(m_szLogFile, (_countof(m_szLogFile) * sizeof(TCHAR)), _T("%s"), log);*/
        StringCbPrintf(m_szLogFile, sizeof(m_szLogFile), _T("%s"), log);
        // Share Mode: 0, Sec Desc: NULL, 
        m_hLogFile = CreateFile(m_szLogFile, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if(m_hLogFile == INVALID_HANDLE_VALUE) {
            LOG(TEXT("%S: ERROR %d@%S - CreateFile for %s failed with (GLE: 0x%0x)"), __FUNCTION__, __LINE__, __FILE__, m_szLogFile, GetLastError());
        }
    }
    else m_hLogFile = INVALID_HANDLE_VALUE;
    
    m_pDroppedFrames = NULL;
    m_pRendererQualProp = NULL;

    dwOverflow = 0;

    //by default we do not calculate memory status
    bMemoryStatus = FALSE;

    LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
    
}

CPerformanceMonitor::~CPerformanceMonitor()
{
    LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    // Signal that the monitor thread should exit and wait for it to do so
    m_bEndThread = true;

    // Unpause the monitor thread
    UnPause();

    WaitForSingleObject(m_evDestroyThread, INFINITE);

    // Now we are paused
    m_sCurrentState = CPerformanceMonitor::Paused;

    // Close the event handles
    if(m_evStartThread)
        CloseHandle(m_evStartThread);
    if(m_evDestroyThread)
        CloseHandle(m_evDestroyThread);

    //Close thread handle
    if(m_hPerformanceThread)
        CloseHandle(m_hPerformanceThread);

    // Close the log file if there was one
    if (m_hLogFile != INVALID_HANDLE_VALUE)
    {
        FlushLog();
        CloseHandle(m_hLogFile);
    }

    LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
    
}

void CPerformanceMonitor::SetMemoryMonitor()
{
    LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    bMemoryStatus = TRUE;
    
    LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
}


//set the interface pointers for getting dropped frame information

void CPerformanceMonitor::SetStatsInterfaces(IAMDroppedFrames* pDroppedFrames, IQualProp* pQualProp, IAMNetworkStatus *pNetworkStatus)
{
    LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    m_pDroppedFrames = pDroppedFrames;
    m_pRendererQualProp = pQualProp;
    m_pNetworkStatus = pNetworkStatus;

    LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
}

void CPerformanceMonitor::ResetData()
{
    LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    // Reset recordings
    m_TotalCpuUtil = 0.0;
    m_nRecordings = 0;
    dwOverflow = 0;

    LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
}

HRESULT CPerformanceMonitor::Pause()
{
    LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    // Set as paused - the thread wil eventually pass
    m_sCurrentState = CPerformanceMonitor::Paused;

    LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
    
    return S_OK;
}

HRESULT CPerformanceMonitor::UnPause()
{
    LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    // Make sure the pause flag is not set
    m_sCurrentState = CPerformanceMonitor::Running;

    // Signal the thread to start
    SetEvent(m_evStartThread);

    LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
    
    return S_OK;
}

CPerformanceMonitor::State CPerformanceMonitor::GetState()
{
    LOGZONES(TEXT("%S: ENTER/EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    return m_sCurrentState;
}

//what is the avg utalization across all samples

float CPerformanceMonitor::GetAvgUtilization()
{
    float fAvgCPU = 0;

    LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    //need to account for the number of times we reset m_nRecordings in Monitor( )
    if (m_nRecordings || dwOverflow)
        fAvgCPU = (float)(m_TotalCpuUtil/((dwOverflow*MAX_RECORDINGS) + m_nRecordings));

    LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
    
    return fAvgCPU;

}

//did CPU utalization ever go above 90%

bool CPerformanceMonitor::WasPegged()
{
    LOGZONES(TEXT("%S: ENTER/EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
    
    return m_bWasPegged;
}

//write the infromation in our buffer into file

void CPerformanceMonitor::FlushLog()
{
    TCHAR logstr[256];
    HRESULT hr = S_OK;
    DWORD nBytesWritten = 0;    

    LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    //Print out file header, but only print first time we are in here
    if(!dwOverflow) {
        //we always print out this information
        hr = StringCbPrintf(logstr, sizeof(logstr), _T("Time(ms),CPU Utalization"));

        //no need to print video information if we had none
        if(m_pRendererQualProp || m_pDroppedFrames) 
            hr = StringCbPrintf(logstr, sizeof(logstr), _T("%s,Frames drawn, Decoder dropped,Renderer dropped"), logstr);

        //no need to print network info if we had none
        if(m_pNetworkStatus) 
            hr = StringCbPrintf(logstr, sizeof(logstr), _T("%s,Pkt Lost,Pkt Recovered,Pkt Recieved"), logstr);

        //if we are asked for memory information append that too
        if(bMemoryStatus) 
            hr = StringCbPrintf(logstr, sizeof(logstr), _T("%s,Memory Load(%)"), logstr);

        //finally append the new line
        hr = StringCbPrintf(logstr, sizeof(logstr), _T("%s\r\n"), logstr);
        
        WriteFile(m_hLogFile, logstr, sizeof(logstr), &nBytesWritten, NULL);
    }
    
    for(int i = 0; i < m_nRecordings; i++)
    {
        //we always print out this information
        hr = StringCbPrintf(logstr, sizeof(logstr), _T("%d,%d"), 
            recordingBuffer[i].time, 
            recordingBuffer[i].utilization);

        //no need to print video information if we had none
        if(m_pRendererQualProp || m_pDroppedFrames) {
            hr = StringCbPrintf(logstr, sizeof(logstr), _T("%s,%d,%d,%d"), 
                logstr,
                recordingBuffer[i].drawn, 
                recordingBuffer[i].decdrops,  
                recordingBuffer[i].rendrops);
        }

        //no need to print network info if we had none
        if(m_pNetworkStatus) {
            hr = StringCbPrintf(logstr, sizeof(logstr), _T("%s,%d,%d,%d"), 
                logstr,
                recordingBuffer[i].lostpkts,
                recordingBuffer[i].recoveredpkts,
                recordingBuffer[i].recvdpkts);
        }

        //if we are asked for memory information append that too
        if(bMemoryStatus) {
            hr = StringCbPrintf(logstr, sizeof(logstr), _T("%s,%d"), 
                logstr,
                recordingBuffer[i].MemLoad);
        }

        //finally append the new line
        hr = StringCbPrintf(logstr, sizeof(logstr), _T("%s\r\n"), 
                logstr);
        
        nBytesWritten = 0;
        WriteFile(m_hLogFile, logstr, sizeof(logstr), &nBytesWritten, NULL);
    }
    
    LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
    
}

//threadProc

DWORD CPerformanceMonitor::Monitor()
{
    int utilization = 0;    
    DWORD dwInitialIdle, dwIdleEnd, dwIdleStart;
    DWORD dwInitialTick = 0, dwStartTick, dwStopTick;
    DWORD dwIdleCount, dwTickCount, dwTotalTicks;
    MEMORYSTATUS mst;   
    DWORD dwTotalPages, dwPagesFree, dwPagesUsed, dwMemLoad = 0;
    
    LOGZONES(TEXT("%S: ENTER %d@%S"), __FUNCTION__, __LINE__, __FILE__);

    // Initialize variables to get information about our memory state
    memset(&mst, 0, sizeof(mst));
    mst.dwLength = sizeof(mst);

    //continue monitoring until we are asked to stop
    while(1)
    {
        // If we have been signaled to pause - wait on the start event until it is signaled
        if( m_sCurrentState == CPerformanceMonitor::Paused) {
            
            LOGZONES(TEXT("%S: Waiting on start thread"), __FUNCTION__, __LINE__, __FILE__);
            
            WaitForSingleObject(m_evStartThread, INFINITE);

            LOGZONES(TEXT("%S: Done waiting on start thread"), __FUNCTION__, __LINE__, __FILE__);
            
            // Get the starting tick and idle counts
            dwInitialTick = GetTickCount();
            dwInitialIdle = GetIdleTime();
        }

        //if we are about to exceed the size of our data buffer, flush the information into the log and start over
        //remeber to increment the dwOverflow field, otherwise Avg Utalization calculations will be off
        if (m_nRecordings == (MAX_RECORDINGS - 1))
        {
            FlushLog();
            m_nRecordings = 0;
            dwOverflow++;
            LOGZONES(TEXT("%S: In-memory status buffer full, flushing data to log for the %dth time."), __FUNCTION__, __LINE__, __FILE__, dwOverflow);                      
        }

        if (m_bEndThread)
        {
            SetEvent(m_evDestroyThread);
            LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);         
            return 0;
        }

        dwStartTick = GetTickCount();
        dwIdleStart = GetIdleTime();

        //sleep for the time specified, this determines our sampling rate, this value is passed into the constructor
        Sleep(m_dwSamplingPeriod);

        dwStopTick = GetTickCount();
        dwIdleEnd = GetIdleTime();

        //how long was the cpu idle while we slept
        dwIdleCount = (dwIdleEnd < dwIdleStart) ? (0xffffffff - (dwIdleStart - dwIdleEnd)) : (dwIdleEnd - dwIdleStart);
        //how long did we sleep
        dwTickCount = (dwStopTick < dwStartTick) ? (0xffffffff - (dwStartTick - dwStopTick)) : (dwStopTick - dwStartTick);
        //how long has it been since we started this thread
        dwTotalTicks = (dwStopTick < dwInitialTick) ? (0xffffffff - (dwInitialTick - dwStopTick)) : (dwStopTick - dwInitialTick);

        //utalization is percentage of time the CPU was not idle
        utilization = 100 - (100*dwIdleCount/dwTickCount);

        //we consider pegged if CPU utalization went over 90%, this can be changed if you want to check a higher or lower bound
        if (utilization >= 90)
            m_bWasPegged = true;

        //we have also been asked for memory status
        if(bMemoryStatus) {
            GlobalMemoryStatus(&mst);
            dwTotalPages = mst.dwTotalPhys/1024;
            dwPagesFree = mst.dwAvailPhys / 1024;
            dwPagesUsed = (mst.dwTotalPhys - mst.dwAvailPhys) / 1024;
            dwMemLoad= mst.dwMemoryLoad;
        }

        if (m_hLogFile)
        {           
            long decdrops = -1;
            int drawn = -1;
            int rendrops = -1;
            long lostPackets = -1;
            long recoveredPackets = -1;
            long receivedPackets = -1;          
            if (!m_bEndThread)
            {
                //if you want to see this information printed out in the debug log, set the ENABLE_LOG variable
                //WARNING: performance can be effected by printing of this information
                LOGZONES( _T("%S: Time:CPU:%d:%d:"), __FUNCTION__, dwTotalTicks, utilization);
                if (m_pRendererQualProp)
                {
                    m_pRendererQualProp->get_FramesDrawn(&drawn);
                    m_pRendererQualProp->get_FramesDroppedInRenderer(&rendrops);
                    LOGZONES( _T("Drawn,Ren: %d,%d:"), drawn, rendrops);                    
                }
                if(m_pDroppedFrames) 
                {
                    m_pDroppedFrames->GetNumDropped(&decdrops);
                    LOGZONES( _T("Dec: %d:"), drawn, decdrops);                                     
                }
                if (m_pNetworkStatus)
                {   m_pNetworkStatus->get_LostPackets(&lostPackets);
                    m_pNetworkStatus->get_RecoveredPackets(&recoveredPackets);
                    m_pNetworkStatus->get_ReceivedPackets(&receivedPackets);
                    LOGZONES( _T("Net: %d,%d,%d"), lostPackets, recoveredPackets, receivedPackets);
                }
                LOGZONES( _T("\n"));
                
                //print out memory information if asked
                if(bMemoryStatus) {
                    LOGZONES(TEXT("%S: Memory (KB):     %5d total  %5d used  %5d free  %%%d loaded\r\n"), __FUNCTION__,
                    dwTotalPages, dwPagesUsed, dwPagesFree, dwMemLoad);
                }
            }
            //save off the information in the buffer, we can write off the buffer to file later
            recordingBuffer[m_nRecordings].time = dwTotalTicks;
            recordingBuffer[m_nRecordings].utilization = utilization;
            recordingBuffer[m_nRecordings].drawn = drawn;
            recordingBuffer[m_nRecordings].decdrops = decdrops;
            recordingBuffer[m_nRecordings].rendrops = rendrops;
            recordingBuffer[m_nRecordings].lostpkts = lostPackets;
            recordingBuffer[m_nRecordings].recoveredpkts = recoveredPackets;
            recordingBuffer[m_nRecordings].recvdpkts = receivedPackets;
            
            if(bMemoryStatus) {
                recordingBuffer[m_nRecordings].MemLoad = dwMemLoad;
            }

        }

        // Cumulative utalization, used to calculate avg utalization across all recordings
        m_TotalCpuUtil += utilization;
        m_nRecordings++;
    }

    LOGZONES(TEXT("%S: EXIT %d@%S"), __FUNCTION__, __LINE__, __FILE__);
    
    return NULL;
}

//ThreadProc that will actually call the threadProc function inside the CPerformanceMonitor

DWORD WINAPI CPerformanceMonitor::StaticThreadProc(PVOID pArg)
{
    LOGZONES(TEXT("%S: ENTERING THREAD %d@%S"), __FUNCTION__, __LINE__, __FILE__);
    
    CPerformanceMonitor *pCPM = (CPerformanceMonitor*) pArg;

    DWORD retval = pCPM->Monitor();

    LOGZONES(TEXT("%S: EXITING THREAD %d@%S"), __FUNCTION__, __LINE__, __FILE__);
    
    return retval;
}
