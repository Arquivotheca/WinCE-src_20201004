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

#include "multiperflogger.h"

// output formats used by loggers
LPCTSTR PERFLOG_TESTNAME_FORMAT     = _T("### PERF TEST [ %s ] ###\r\n");
LPCTSTR PERFLOG_PROMPT              = _T("## PERF ## ");
LPCTSTR PERFLOG_DEBUG_OUTPUT_FORMAT = _T("## PERF ## [%s] took [%I64d] ticks\r\n");
LPCTSTR PERFLOG_CSV_NEXTLINE_FORMAT = _T("\r\n%s,%I64d");
LPCTSTR PERFLOG_CSV_HEAD_FORMAT     = _T("%s,%I64d");
LPCTSTR PERFLOG_CSV_DATA_FORMAT     = _T(",%I64d");
LPCTSTR PERFLOG_CALI_NAME           = _T("Calibrating");

// PerfLogStub methods

PerfLogStub::PerfLogStub() 
{ 
    InitializeCriticalSection(&m_cs); 
    memset(m_data, 0, sizeof(m_data)); 
    m_flag = 0; 
}

BOOL PerfLogStub::InitTickCounter()
{
    LARGE_INTEGER tick;
    // get the frequency
    if (!QueryPerformanceFrequency(&m_freq)
        || !QueryPerformanceCounter(&tick))
    {
        // using GetTickCount() instead
        m_flag |= PERFLOG_FLAG_USE_GTC;

        // GTC's frequency is 1000 (1ms)
        m_freq.QuadPart = 1000;
    }
    // output the frequecy information
    PerfLogText(_T("%sFrequency is [%d] ticks per second\r\n"), PERFLOG_PROMPT, m_freq.QuadPart);
    return TRUE;
}

void PerfLogStub::GetCurrentTick(LARGE_INTEGER *pTick)
{
    if (m_flag & PERFLOG_FLAG_USE_GTC) {
        pTick->QuadPart = GetTickCount();
    } else {
        QueryPerformanceCounter(pTick);
    }
}

// check the ID number to prevent invalid memory access
BOOL PerfLogStub::ValidID(DWORD id) 
{ 
    return (id < m_maxId); 
}

// compare the calibration samples
int Compare2LargeInteger(const void *a, const void *b)
{
    LARGE_INTEGER result, *pa, *pb;
    int retVal = 0;

    pa = (LARGE_INTEGER *)a;
    pb = (LARGE_INTEGER *)b;
    result.QuadPart = pa->QuadPart - pb->QuadPart;

    if (result.QuadPart < 0) {
        retVal = -1;
    } else if (result.QuadPart > 0) {
        retVal = 1;
    }
    return retVal;
}

// Get the overhead of calling PerfBegin() and PerfEnd()
BOOL PerfLogStub::Calibrate()
{
    const DWORD caliId = PERFLOG_CALI_ID;
    DWORD counts = PERFLOG_CALI_COUNTS;
    BOOL retVal = FALSE;

    m_maxId = PERFLOG_CALI_ID+1;   
    LARGE_INTEGER *pTime = new LARGE_INTEGER[counts];
    if (pTime == NULL) {
        // the call to PerfBegin() and PerfEnd() should fail after set it to 0
        m_maxId = 0;
        return FALSE;
    }
    
    EnterCriticalSection(&m_cs);
    m_data[caliId].counts = 0;
    HRESULT hr = StringCbCopy(m_data[caliId].name, sizeof(m_data[caliId].name), PERFLOG_CALI_NAME);
    LeaveCriticalSection(&m_cs);
    
    if (SUCCEEDED(hr)) {
        m_flag |= PERFLOG_FLAG_READY;
        for (DWORD i = 0; i < counts; i++) {
            PerfBegin(caliId);
            PerfEnd(caliId);
            pTime[i].QuadPart = m_data[caliId].durations.QuadPart;
        }
        // select a middle one instead of using the average number
        // because some sample should be extra large, so the average
        // count will be impacted by that single sample.
        qsort(pTime, counts, sizeof(pTime[0]), Compare2LargeInteger);
        m_caliTick.QuadPart = pTime[counts/2].QuadPart;
        
        retVal = TRUE;
    }
    delete [] pTime;
    m_maxId--;
    return retVal;
}


BOOL PerfLogStub::PerfRegister(DWORD id, LPCTSTR format,...)
{
    // initialize the counter 
    if (! (m_flag & PERFLOG_FLAG_READY)) {
        if (!InitTickCounter() || !Calibrate()) {
            return FALSE;
        }
    }
    // sanity check the inboud parameters
    if (format == NULL || !ValidID(id) || (m_flag & PERFLOG_FLAG_INIT_ERROR)) {
        return FALSE;
    }

    va_list va;
    va_start(va, format);
    EnterCriticalSection(&m_cs);
    
    m_data[id].counts = 0;
    HRESULT hr = StringCbVPrintfEx(m_data[id].name, sizeof(m_data[id].name), 
        NULL, NULL, STRSAFE_NULL_ON_FAILURE, format, va);
    
    LeaveCriticalSection(&m_cs);
    va_end(va);
    return (SUCCEEDED(hr));
}


BOOL PerfLogStub::PerfBegin(DWORD id)
{
    if (ValidID(id)) {
        //Sleep(0) forces a rescheduing
        Sleep(0);
        // make the statTick cached
        m_data[id].startTick.QuadPart = 0;
        GetCurrentTick(&m_data[id].startTick);
        return TRUE;
    }
    return FALSE;
}


BOOL PerfLogStub::PerfEnd(DWORD id)
{
    if (ValidID(id)) {
        LARGE_INTEGER currentTick;
        
        GetCurrentTick(&currentTick);
        m_data[id].durations.QuadPart = currentTick.QuadPart - m_data[id].startTick.QuadPart;
        if (id != PERFLOG_CALI_ID) {
            // minus the overhead
            m_data[id].durations.QuadPart -= m_caliTick.QuadPart;
        }
        EnterCriticalSection(&m_cs);

        // log the perf data
        BOOL retVal = Log(id);
        ++m_data[id].counts;
        LeaveCriticalSection(&m_cs);
        return retVal;
    }
    return FALSE;
}


// log omments and other necessary information
BOOL PerfLogStub::PerfLogText(LPCTSTR format,...)
{
    BOOL retVal = FALSE;

    if (format == NULL) {
        return FALSE;
    }
    va_list va;
    va_start(va, format);
    EnterCriticalSection(&m_cs);
    HRESULT hr = StringCbVPrintfEx(m_tcharBuf, sizeof(m_tcharBuf), NULL, NULL, STRSAFE_NULL_ON_FAILURE,
        format, va);
    if (SUCCEEDED(hr)) {
        retVal = Output();
    }
    LeaveCriticalSection(&m_cs);
    va_end(va);
    return retVal;
}

// utput the test name
BOOL PerfLogStub::PerfSetTestName(LPCTSTR testName)
{
    HRESULT hr;
    BOOL retVal = FALSE;

    if (testName == NULL) {
        return FALSE;
    }
    EnterCriticalSection(&m_cs);
    hr = StringCbPrintfEx(m_tcharBuf, sizeof(m_tcharBuf), NULL, NULL, STRSAFE_NULL_ON_FAILURE,
        PERFLOG_TESTNAME_FORMAT, testName);
    if (SUCCEEDED(hr)) {
        retVal = Output();
    }
    LeaveCriticalSection(&m_cs);
    return retVal;
}


// methods for DebugOutPut

BOOL DebugOutput::Log(DWORD id)
{
    BOOL retVal = FALSE;
    
    HRESULT hr = StringCbPrintfEx(m_tcharBuf, sizeof(m_tcharBuf) , NULL, NULL, STRSAFE_NULL_ON_FAILURE, 
        PERFLOG_DEBUG_OUTPUT_FORMAT, m_data[id].name, m_data[id].durations.QuadPart);
    if (SUCCEEDED(hr) && Output()) {
        retVal = TRUE;
    }
    return retVal;
}

BOOL DebugOutput::Output()
{
    m_dump(m_tcharBuf);
    return TRUE;
}


// methods for CsvFile

// Create log files and truncate it if necessary
CsvFile::CsvFile(LPCTSTR filename, BOOL bTruncated)
{
    if (filename != NULL) {
        m_hFile = CreateFile(filename, GENERIC_WRITE, 0, NULL, 
            bTruncated ? CREATE_ALWAYS : OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
        if (m_hFile != INVALID_HANDLE_VALUE) {
            // set write pointer to the end for append mode 
            if (!bTruncated) {
                SetFilePointer(m_hFile, 0, NULL, FILE_END);
            } 
            m_bNewLine = TRUE;
            return ;
        }
    }
    // failed to create the file, set the error flag
    m_hFile = NULL;
    m_flag |= PERFLOG_FLAG_INIT_ERROR;
}


CsvFile::~CsvFile()
{
    const CHAR lineEnds[] = "\r\n\r\n";
    if (m_hFile != NULL) {
        DWORD bytes = 0;
        // In case in append mode, separate different test logs
        WriteFile(m_hFile, lineEnds, 
            sizeof(lineEnds) - 1, // remove the last ' '
            &bytes, NULL);
        CloseHandle(m_hFile);
    }
}

BOOL CsvFile::Log(DWORD id)
{
    BOOL retVal = FALSE;
    HRESULT hr;
    
    // if it's the first log for a registred item, should output it's name
    if (m_data[id].counts == 0) {
        hr = StringCbPrintfEx(m_tcharBuf, sizeof(m_tcharBuf) , NULL, NULL, STRSAFE_NULL_ON_FAILURE, 
            m_bNewLine ? PERFLOG_CSV_HEAD_FORMAT : PERFLOG_CSV_NEXTLINE_FORMAT,
            m_data[id].name, m_data[id].durations.QuadPart);
        m_bNewLine = FALSE;
    } else {
        hr = StringCbPrintfEx(m_tcharBuf, sizeof(m_tcharBuf) , NULL, NULL, STRSAFE_NULL_ON_FAILURE,
            PERFLOG_CSV_DATA_FORMAT, m_data[id].durations.QuadPart);
    }
    if(SUCCEEDED(hr) && Output()) {
        retVal = TRUE;
    }
    return retVal;
}

BOOL CsvFile::Output()
{
    BOOL retVal = FALSE;

    // Should use ASCII format in the log file to imporve the portability
    if (WideCharToMultiByte(CP_ACP, 0, m_tcharBuf, -1, m_charBuf, sizeof(m_charBuf), NULL, NULL) > 0) {
        DWORD bytesWritten = 0;
        size_t bytesToWrite = 0;
        HRESULT hr = StringCbLengthA(m_charBuf, sizeof(m_charBuf), &bytesToWrite);
        if (SUCCEEDED(hr)) {
            retVal = WriteFile(m_hFile, m_charBuf, (DWORD)bytesToWrite, &bytesWritten, NULL);
            if (bytesWritten != bytesToWrite) {
                retVal = FALSE;
            }
            // Flush file to ensure data has physically been written 
            FlushFileBuffers(m_hFile);
        }
    }
    return retVal;
}



// methods for CEPerfLog, simply call the according PerfLog method

BOOL CEPerfLog::PerfSetTestName(LPCTSTR testName)
{
    if (testName != NULL) {
        return Perf_SetTestName(testName);
    }
    return FALSE;
}


BOOL CEPerfLog::PerfRegister(DWORD id, LPCTSTR format,...)
{
    if (format == NULL) {
        return FALSE;
    }
    va_list va;
    va_start(va, format);
    HRESULT hr = StringCbVPrintfEx(m_data[id].name, sizeof(m_data[id].name), 
        NULL, NULL, STRSAFE_NULL_ON_FAILURE, format, va);
    if (SUCCEEDED(hr)) {
        return Perf_RegisterMark(id, _T("%s"), m_data[id].name);
    }
    va_end(va);
    return FALSE;
}

BOOL CEPerfLog::PerfBegin(DWORD id)
{
    return Perf_MarkBegin(id);
}

BOOL CEPerfLog::PerfEnd(DWORD id)
{
    return Perf_MarkEnd(id);
}
