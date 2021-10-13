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
#include "csvlog.h"
#include "cpumon.h"

/****************************************************************************************************
        file logger
****************************************************************************************************/

cCSVFile::cCSVFile() : m_hFile(NULL)
{
}

cCSVFile::~cCSVFile()
{
    CloseFile();
}

BOOL
cCSVFile::OpenFile(TCHAR *tszFileName)
{
    m_hFile = _tfopen(tszFileName, TEXT("a"));

    if(!m_hFile)
        return FALSE;

    return TRUE;
}

BOOL
cCSVFile::CloseFile()
{
    if(m_hFile)
    {
        fclose(m_hFile);
        m_hFile = NULL;
    }

    return TRUE;
}

BOOL
cCSVFile::LogLine(LPCTSTR szFormat, ...)
{
    // we need to be able to log very large strings, however
    // if we're logging KATO, then we're going to log more than kato can handle in
    // a single line.
    TCHAR   szBuffer[4096] = {NULL};

    va_list pArgs;

    va_start(pArgs, szFormat);
    // _vsntprintf deprecated
    if(FAILED(StringCbVPrintf(szBuffer, countof(szBuffer) - 1, szFormat, pArgs)))
       FAIL(TEXT("StringCbVPrintf failed"));
    va_end(pArgs);

    if(!m_hFile)
    {
        // Kato adds on some extra spaces for the verbosity level, so subtract 20 from the
        // max string length.
        TCHAR szTemp[MAX_PATH - 20] = {NULL};
        TCHAR *szBufferPortion = szBuffer;

        // OutputDebugString has a maximum string length of MAX_PATH,
        // anything beyond that length isn't outputted at all.
        do
        {
            _tcsncpy(szTemp, szBufferPortion, countof(szTemp) - 1);
            Log(TEXT("%s"), szTemp);
            if(_tcslen(szBufferPortion) - 1 > countof(szTemp))
                szBufferPortion += (countof(szTemp) - 1);
            else break;
        }
        while( 1 );
    }
    // write szBuffer to the file
    else
        _ftprintf(m_hFile, TEXT("%s\n"), szBuffer);

    return TRUE;
}


/****************************************************************************************************
        camera perf data logger
****************************************************************************************************/

DWORD myThreadProc(LPVOID lpParameter)
{
    cCameraCSV *CSVOutput = (cCameraCSV *) lpParameter;
    int nActive = 1;

    if(!CSVOutput)
        return 0;

    // kato logging must only be used when holding the logging mutex, 
    // otherwise there's the risk of the thread being suspended while
    // holding the KATO lock.

    do
    {
        // only do this once every STANDARD_REFRESH_RATE time since any more often
        // is a waste (cpumon can't gather the data any faster.)
        WaitForSingleObject(CSVOutput->m_hLoggingEvent, STANDARD_REFRESH_RATE);
        WaitForSingleObject(CSVOutput->m_hLoggingMutex, INFINITE);

        if(CSVOutput->m_bLoggingThreadActive)
            CSVOutput->OutputTimestampData();
        else nActive = 0;

        ReleaseMutex(CSVOutput->m_hLoggingMutex);
    }
    while(nActive);

    return nActive;
}

cCameraCSV::cCameraCSV() : 
    cCSVFile(), m_hLoggingMutex(NULL), m_hLoggerThread(NULL), m_hLoggingEvent(NULL),
    m_bLoggingThreadActive(FALSE), m_pCaptureGraph(NULL), m_pTestVector(NULL), m_nTestID(0), m_nRunNumber(0)
{
}

cCameraCSV::~cCameraCSV()
{
    Cleanup();
}

BOOL
cCameraCSV::Init(TCHAR *tszFileName, PCAPTUREFRAMEWORK pCaptureFramework, PTESTVECTOR pTestVector)
{
    // if the file open fails, then fail the call,
    // if no file name was given, then the logs go to tux.
    if(tszFileName && !OpenFile(tszFileName))
        return FALSE;

    if(!pCaptureFramework || !pTestVector)
    {
        FAIL(TEXT("No capture framework or test vector given"));
        return FALSE;
    }

    // grab our copy of the capture framework
    m_pCaptureGraph = pCaptureFramework;
    m_pTestVector= pTestVector;

    // if we fail creating the logging mutex or the logging event, then the logging thread
    // cannot be created because we'll have no way to control it.

    // create file logging mutex
    m_hLoggingMutex = CreateMutex(NULL, TRUE, NULL);
    if(m_hLoggingMutex)
    {
        // create timing thread and event
        m_hLoggingEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

        if(m_hLoggingEvent)
        {
            m_hLoggerThread = CreateThread(NULL, 0, myThreadProc, this, CREATE_SUSPENDED, NULL);
            if(!m_hLoggerThread)
                return FALSE;
            m_bLoggingThreadActive = TRUE;

            // start monitoring thread, it won't actually output anything because we hold the mutex.
            ResumeThread(m_hLoggerThread);

            // let it get running and waiting on the event
            Sleep(0);

            // pause monitoring thread, this may fail if the thread is doing kernel work, 
            // so keep trying until it's paused.
            while(0xFFFFFFFF == SuspendThread(m_hLoggerThread)) Sleep(0);
        }
        else return FALSE;
    }
    else return FALSE;
    
    // setup CPUMON
    Log(TEXT("Calibrating CPUMON"));
    Sleep(2000);
    if(FAILED(CalibrateCPUMon()))
    {
        FAIL(TEXT("Failed to calibrate CPUMon."));
        return FALSE;
    }

    if(FAILED(StartCPUMonitor()))
    {
        FAIL(TEXT("Failed to start CPUMon."));
        return FALSE;
    }

    ReleaseMutex(m_hLoggingMutex);

    // wait for one CPUMon refresh rate after setting everything up.
    Sleep(STANDARD_REFRESH_RATE);

    OutputHeaders();

    return TRUE;
}

BOOL
cCameraCSV::Cleanup()
{
    DWORD dwExitCode;

    // only shut down the logger thread if it was created.
    // if the logger thread was created, then we're guaranteed
    // the loggin mutex and event was created
    if(m_hLoggerThread)
    {
        // shutdown the thread and related things
        WaitForSingleObject(m_hLoggingMutex, INFINITE);
        m_bLoggingThreadActive = FALSE;
        ReleaseMutex(m_hLoggingMutex);

        ResumeThread(m_hLoggerThread);
        PulseEvent(m_hLoggingEvent);

        do
        {
            if(!GetExitCodeThread(m_hLoggerThread, &dwExitCode))
                break;
        }while(dwExitCode == STILL_ACTIVE);

        // now the logger thread is shut down, close it out
        CloseHandle(m_hLoggerThread);
        m_hLoggerThread = NULL;
    }

    if(m_hLoggingMutex)
    {
        CloseHandle(m_hLoggingMutex);
        m_hLoggingMutex = NULL;
    }

    if(m_hLoggingEvent)
    {
        CloseHandle(m_hLoggingEvent);
        m_hLoggingEvent = NULL;
    }

    // close the file after everything else is done.
    CloseFile();

    // shutdown cpumon
    StopCPUMonitor();

    m_pCaptureGraph = NULL;
    m_pTestVector = NULL;

    return TRUE;
}

BOOL
cCameraCSV::OutputHeaders()
{
    TCHAR tszVideoEncoderParams[1024] = {NULL};
    TCHAR tszAudioEncoderParams[1024] = {NULL};
    WCHAR *wzName, *wzRecipient;
    double dValue;
    DWORD dwIndex;

    // grab the video and audio encoder names from the vector
    for(dwIndex = 0; m_pTestVector->FindEntryIndex(dwIndex, L"*", L"VideoEncoder", &dwIndex, NULL) > 0; dwIndex++)
    {
        if(SUCCEEDED(m_pTestVector->GetEntry(dwIndex, &wzName, &wzRecipient, &dValue)))
        {
            _tcsncat(tszVideoEncoderParams, TEXT(","), 1024 - _tcslen(tszVideoEncoderParams));
            _tcsncat(tszVideoEncoderParams, wzName, 1024 - _tcslen(tszVideoEncoderParams));
        }
    }

    // grab the video and audio encoder names from the vector
    for(dwIndex = 0; m_pTestVector->FindEntryIndex(dwIndex, L"*", L"AudioEncoder", &dwIndex, NULL) > 0; dwIndex++)
    {
        if(SUCCEEDED(m_pTestVector->GetEntry(dwIndex, &wzName, &wzRecipient, &dValue)))
        {
            _tcsncat(tszAudioEncoderParams, TEXT(","), 1024 - _tcslen(tszAudioEncoderParams));
            _tcsncat(tszAudioEncoderParams, wzName, 1024 - _tcslen(tszAudioEncoderParams));
        }
    }

    LogLine(TEXT("SETTINGHDR,ID,Perf counter frequency, pre-format,pre-width,pre-height,pre-frame rate,cap-format,cap-width,cap-height,cap-frame rate,still format,still-width,still-height,audio format, audio sample rate%s%s,Still dest,capture dest,length,angle,Renderer,pin count"), tszVideoEncoderParams, tszAudioEncoderParams);

    LogLine(TEXT("RSLTHDR0,ID,run number,time stamp,cpu load,available program memory"));
    LogLine(TEXT("RSLTHDR1,ID,run number,time stamp,cpu load,available program memory"));
    LogLine(TEXT("RSLTHDR2,ID,run number,time stamp,cpu load,available program memory,encoded file size,vid frames processed, vid frames dropped, aud frames processed, aud frames dropped"));
    LogLine(TEXT("RSLTHDR3,ID,run number, time stamp,cpu load,available program memory"));

    return TRUE;
}

BOOL
cCameraCSV::StartTest(int nTestID, int nTestLength, DWORD dwStream)
{
    WaitForSingleObject(m_hLoggingMutex, INFINITE);

    Log(TEXT("Starting the test."));

    // set the test ID
    m_nTestCaseNumber = nTestID * 10000;
    m_nTestID++;
    m_dwStream = dwStream;

    AM_MEDIA_TYPE *pamtPreview = NULL, *pamtCapture = NULL, *pamtStill = NULL, *pamtAudio = NULL;
    DWORD dwOrientation;
    DWORD dwMode;
    UINT uiPinCount;
    TCHAR tszStillFileDest[MAX_PATH], tszCaptureFileDest[MAX_PATH];
    int nStringLength;
    int i;
    DWORD dwIndex;
    WCHAR *wzName, *wzRecipient;
    double dValue;
    TCHAR tszVideoEncoderString[1024] = {NULL};
    TCHAR tszTempString[1024];
    VARIANT var;
    TCHAR tszAudioEncoderString[1024] = {NULL};
    TCHAR tszPreviewInfo[1024] = {NULL};
    TCHAR tszCaptureInfo[1024] = {NULL};
    TCHAR tszStillInfo[1024] = {NULL};
    TCHAR tszAudioInfo[1024] = {NULL};

    TCHAR tszOrientation[10] ={NULL};
    TCHAR tszRenderer[10] = {NULL};

    VIDEOINFOHEADER *pvih;
    WAVEFORMATEX *pwfex;
    LARGE_INTEGER lpPerfFreq;

    // get the preview stream info
    if(GetStreamInfo(STREAM_PREVIEW, &pamtPreview))
    {
        pvih = (VIDEOINFOHEADER *) pamtPreview->pbFormat;
        StringFromGUID2(pamtPreview->subtype, tszTempString, countof(tszTempString));
        _stprintf(tszPreviewInfo, TEXT("%s,%d,%d,%d"), tszTempString, pvih->bmiHeader.biWidth, pvih->bmiHeader.biHeight, (int) 10000000/pvih->AvgTimePerFrame);
    }

    // get the capture stream info
    if(GetStreamInfo(STREAM_CAPTURE, &pamtCapture))
    {
        pvih = (VIDEOINFOHEADER *) pamtCapture->pbFormat;
        StringFromGUID2(pamtCapture->subtype, tszTempString, countof(tszTempString));
        _stprintf(tszCaptureInfo, TEXT("%s,%d,%d,%d"), tszTempString, pvih->bmiHeader.biWidth, pvih->bmiHeader.biHeight, (int) 10000000/pvih->AvgTimePerFrame);
    }

    // get the still stream info
    if(GetStreamInfo(STREAM_STILL, &pamtStill))
    {
        pvih = (VIDEOINFOHEADER *) pamtStill->pbFormat;
        StringFromGUID2(pamtStill->subtype, tszTempString, countof(tszTempString));
        _stprintf(tszStillInfo, TEXT("%s,%d,%d"), tszTempString, pvih->bmiHeader.biWidth, pvih->bmiHeader.biHeight);
    }

    // get the audio stream info
    if(GetStreamInfo(STREAM_AUDIO, &pamtAudio))
    {
        pwfex = (WAVEFORMATEX *) pamtAudio->pbFormat;
        StringFromGUID2(pamtAudio->subtype, tszTempString, countof(tszTempString));
        _stprintf(tszAudioInfo, TEXT("%s,%d"), tszTempString, pwfex->nSamplesPerSec);
    }

    nStringLength = MAX_PATH;

    // get the file destinations.
    m_pCaptureGraph->GetStillCaptureFileName(tszStillFileDest, &nStringLength);

    // wipe off everything until the first \, from the right side in (remove the file name)
    for(i = _tcslen(tszStillFileDest); i >= 0 && tszStillFileDest[i] != '\\'; tszStillFileDest[i--] = '\0');
    // if the only thing there was the file name, then put in a single backslash (device root);
    if(i == 0)
        tszStillFileDest[0] = '\\';

    nStringLength = MAX_PATH;

    // repeat the above, but for the video capture file name.
    m_pCaptureGraph->GetVideoCaptureFileName(tszCaptureFileDest, &nStringLength);
    for(i = _tcslen(tszCaptureFileDest); i > 0 && tszCaptureFileDest[i] != '\\'; tszCaptureFileDest[i--] = '\0');
    if(i == 0)
        tszCaptureFileDest[0] = '\\';

    // grab the video and audio encoder names from the vector.
    // get the encoder info
    // video encoder properties

    VariantInit(&var);
    for(dwIndex = 0; m_pTestVector->FindEntryIndex(dwIndex, L"*", L"VideoEncoder", &dwIndex, NULL) > 0; dwIndex++)
    {
        if(SUCCEEDED(m_pTestVector->GetEntry(dwIndex, &wzName, &wzRecipient, &dValue)))
        {
            VariantClear(&var);

            m_pCaptureGraph->GetVideoEncoderInfo(wzName, &var);
            _stprintf(tszTempString, TEXT("%d"), var.lVal);

            _tcsncat(tszVideoEncoderString, TEXT(","), 1024 - _tcslen(tszVideoEncoderString));

            // append the new string to the encoder params.
            _tcsncat(tszVideoEncoderString, tszTempString, 1024 - _tcslen(tszVideoEncoderString));
        }
    }

    for(dwIndex = 0; m_pTestVector->FindEntryIndex(dwIndex, L"*", L"AudioEncoder", &dwIndex, NULL) > 0; dwIndex++)
    {
        if(SUCCEEDED(m_pTestVector->GetEntry(dwIndex, &wzName, &wzRecipient, &dValue)))
        {
            VariantClear(&var);

            m_pCaptureGraph->GetAudioEncoderInfo(wzName, &var);
            _stprintf(tszTempString, TEXT("%d"), var.lVal);

            _tcsncat(tszVideoEncoderString, TEXT(","), 1024 - _tcslen(tszVideoEncoderString));

            // append the new string to the encoder params.
            _tcsncat(tszVideoEncoderString, tszTempString, 1024 - _tcslen(tszVideoEncoderString));
        }
    }

    // get the orientation
    m_pCaptureGraph->GetScreenOrientation(&dwOrientation);
    switch(dwOrientation)
    {
        case DMDO_0:
            _tcsncpy(tszOrientation, TEXT("0"), countof(tszOrientation));
            break;
        case DMDO_90:
            _tcsncpy(tszOrientation, TEXT("90"), countof(tszOrientation));
            break;
        case DMDO_180:
            _tcsncpy(tszOrientation, TEXT("180"), countof(tszOrientation));
            break;
        case DMDO_270:
            _tcsncpy(tszOrientation, TEXT("270"), countof(tszOrientation));
            break;
    }


    if(SUCCEEDED(m_pCaptureGraph->GetVideoRenderMode(&dwMode)))
    {
        switch(dwMode)
        {
            case AM_VIDEO_RENDERER_MODE_GDI:
                _tcsncpy(tszRenderer, TEXT("GDI"), countof(tszRenderer));
                break;
            case AM_VIDEO_RENDERER_MODE_DDRAW:
                _tcsncpy(tszRenderer, TEXT("DDRAW"), countof(tszRenderer));
                break;
        }
    }
    else
        _tcsncpy(tszRenderer, TEXT("Unknown"), countof(tszRenderer));

    // get the pin count
    m_pCaptureGraph->GetPinCount(&uiPinCount);


    if(!QueryPerformanceFrequency(&lpPerfFreq))
    {
        lpPerfFreq.HighPart = 0;
        lpPerfFreq.LowPart = 1000;
    }

    // add some spacing for human readability.
    LogLine(TEXT(""));

    // output the string.
    LogLine(TEXT("SETTINGDATA,%d,%x%08x,%s,%s,%s,%s%s%s,%s,%s,%d,%s,%s,%d"), m_nTestCaseNumber + m_nTestID, lpPerfFreq.HighPart,lpPerfFreq.LowPart, tszPreviewInfo, tszCaptureInfo, tszStillInfo, tszAudioInfo, tszVideoEncoderString, tszAudioEncoderString, tszCaptureFileDest, tszStillFileDest, nTestLength, tszOrientation, tszRenderer, uiPinCount);

    ReleaseMutex(m_hLoggingMutex);

    return TRUE;
}

BOOL
cCameraCSV::StopTest()
{
    MEMORYSTATUS g_MemStatus;
    DWORD dwAvailablePhys;
    float fCPUUtilization;
    LARGE_INTEGER lpPerfCount;

    WaitForSingleObject(m_hLoggingMutex, INFINITE);

    Log(TEXT("Stopping the test."));

    fCPUUtilization = GetCurrentCPUUtilization();

    // compact the heaps now that the test completed, so we get an accurage memory usage measurement
    CompactAllHeaps();

    GlobalMemoryStatus(&g_MemStatus);
    dwAvailablePhys = g_MemStatus.dwAvailPhys;
    QueryPerformanceCounter(&lpPerfCount);

    LogLine(TEXT("RSLT3,%d,%d,%x%08x,%3.3f,%d"), m_nTestCaseNumber + m_nTestID,m_nRunNumber,lpPerfCount.HighPart, lpPerfCount.LowPart, fCPUUtilization, dwAvailablePhys);

    // the test is stopped, reset everything to a new state.
    m_nRunNumber = 0;

    ReleaseMutex(m_hLoggingMutex);

    return TRUE;
}

BOOL
cCameraCSV::StartRun()
{
    MEMORYSTATUS g_MemStatus;
    DWORD dwAvailablePhys;
    float fCPUUtilization;
    LARGE_INTEGER lpPerfCount;

    WaitForSingleObject(m_hLoggingMutex, INFINITE);

    Log(TEXT("Starting run data collection."));

    // we're starting a new run, increment our run number
    m_nRunNumber++;

    fCPUUtilization = GetCurrentCPUUtilization();
    GlobalMemoryStatus(&g_MemStatus);
    dwAvailablePhys = g_MemStatus.dwAvailPhys;
    QueryPerformanceCounter(&lpPerfCount);

    LogLine(TEXT("RSLT0,%d,%d,%x%08x,%3.3f,%d"), m_nTestCaseNumber + m_nTestID, m_nRunNumber,lpPerfCount.HighPart, lpPerfCount.LowPart, fCPUUtilization, dwAvailablePhys);

    // start monitoring thread
    ResumeThread(m_hLoggerThread);

    ReleaseMutex(m_hLoggingMutex);
    return TRUE;
}

BOOL
cCameraCSV::StopRun()
{
    MEMORYSTATUS g_MemStatus;
    DWORD dwAvailablePhys;
    float fCPUUtilization;
    LARGE_INTEGER lpPerfCount;
    int nFileSize;
    LONG lVideoFramesProcessed, lAudioFramesProcessed;
    LONG lVideoFramesDropped, lAudioFramesDropped;

    WaitForSingleObject(m_hLoggingMutex, INFINITE);

    Log(TEXT("Stopping run data collection."));

    // pause monitoring thread, this may fail if the thread is doing kernel work, 
    // so keep trying until it's paused.
    while(0xFFFFFFFF == SuspendThread(m_hLoggerThread)) Sleep(0);

    // gather info on file size for the current file.
    nFileSize = GetFileSize();

    if(FAILED(m_pCaptureGraph->GetFramesProcessed(&lVideoFramesProcessed, &lAudioFramesProcessed)))
    {
        FAIL(TEXT("Unable to retrieve the number of frames processed"));
        lVideoFramesProcessed = lAudioFramesProcessed = 0;
    }

    if(FAILED(m_pCaptureGraph->GetFramesDropped(&lVideoFramesDropped, &lAudioFramesDropped)))
    {
        FAIL(TEXT("Unable to retrieve the number of frames processed"));
        lVideoFramesDropped = lAudioFramesDropped = 0;
    }

    fCPUUtilization = GetCurrentCPUUtilization();

    GlobalMemoryStatus(&g_MemStatus);
    dwAvailablePhys = g_MemStatus.dwAvailPhys;
    QueryPerformanceCounter(&lpPerfCount);

    LogLine(TEXT("RSLT2,%d,%d,%x%08x,%3.3f,%d,%d,%d, %d, %d, %d"), m_nTestCaseNumber + m_nTestID, m_nRunNumber,lpPerfCount.HighPart, lpPerfCount.LowPart, fCPUUtilization, dwAvailablePhys, nFileSize, lVideoFramesProcessed, lVideoFramesDropped, lAudioFramesProcessed, lAudioFramesDropped);

    ReleaseMutex(m_hLoggingMutex);

    return TRUE;
}

BOOL
cCameraCSV::OutputTimestampData()
{
    MEMORYSTATUS g_MemStatus;
    DWORD dwAvailablePhys;
    float fCPUUtilization;
    LARGE_INTEGER lpPerfCount;

    WaitForSingleObject(m_hLoggingMutex, INFINITE);

    fCPUUtilization = GetCurrentCPUUtilization();

    GlobalMemoryStatus(&g_MemStatus);
    dwAvailablePhys = g_MemStatus.dwAvailPhys;
    QueryPerformanceCounter(&lpPerfCount);

    LogLine(TEXT("RSLT1,%d,%d,%x%08x,%3.3f,%d"), m_nTestCaseNumber + m_nTestID, m_nRunNumber,lpPerfCount.HighPart, lpPerfCount.LowPart, fCPUUtilization, dwAvailablePhys);

    ReleaseMutex(m_hLoggingMutex);

    return TRUE;
}

int
cCameraCSV::GetFileSize()
{
    TCHAR FileName[MAX_PATH];
    int nStringLength = MAX_PATH;
    HANDLE hFile;
    WIN32_FIND_DATA fd;

    switch(m_dwStream)
    {
        case STREAM_STILL:
            if(FAILED(m_pCaptureGraph->GetStillCaptureFileName(FileName, &nStringLength)))
                return 0;
            break;
        case STREAM_CAPTURE:
            if(FAILED(m_pCaptureGraph->GetVideoCaptureFileName(FileName, &nStringLength)))
                return 0;
            break;
        case STREAM_PREVIEW:
            return 0;
        default:
            return 0;
    }

    hFile = FindFirstFile(FileName, &fd);
    if(INVALID_HANDLE_VALUE != hFile)
    {
        FindClose(hFile);
        return fd.nFileSizeLow;
    }

    return 0;
}

BOOL cCameraCSV::GetStreamInfo(DWORD dwStream, AM_MEDIA_TYPE **pamt)
{
    if(!pamt)
        return FALSE;

    // if we can't retrieve the format, and it's not a preview stream, then fail.
    if(FAILED(m_pCaptureGraph->GetFormat(dwStream, pamt)))
    {
        if(dwStream != STREAM_PREVIEW)
            return FALSE;
        // if it's the preview stream, then it's a two pin driver, so return the capture stream format.
        else if(FAILED(m_pCaptureGraph->GetFormat(STREAM_CAPTURE, pamt)))
            return FALSE;
    }

    // we succeded retrieving the format, so return success.
    return TRUE;
}
