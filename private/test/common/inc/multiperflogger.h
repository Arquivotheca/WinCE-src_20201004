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

#include <windows.h>
#include <tchar.h>
#include <strsafe.h>
#include <kato.h>
#include "PerfLoggerApi.h"

/*
 * Different methods used for performance log
 *
 * Using debug output
 *
 * ## PERF ## [Item1] took [tick count1] ticks
 * ## PERF ## [Item2] took [tick count2] ticks
 *
 *
 * Using CSV file
 *
 * Item1,tick count(sample1),tick count(sample2),...
 * Item2,tick count(sample1)
 * 
 *
 * Using Perflog
 *
 * REGISTERED MARKER [Item1] AS [1]
 * REGISTERED MARKER [Item2] AS [2]
 * ...
 * REGISTERED MARKER [Item2] AS [N]
 * EVT [1] DUR [tick count1]
 * EVT [2] DUR [tick count2]
 * ...
 * EVT [N] DUR [tick countN]
 */


/*
    // Usage:

    // create the logger instances
    PerfLogStub *pLogDbg = new DebugOutput();
    PerfLogStub *pLogCsv = new CsvFile(_T("\\release\\sample.csv"), 0);
    PerfLogStub *pLogCEPerf = new CEPerfLog();
    
    pLogDbg->PerfSetTestName(_T("MultiperfLogger test"));
    pLogCsv->PerfSetTestName(_T("MultiperfLogger test"));
    pLogCEPerf->PerfSetTestName(_T("MultiperfLogger test"));
    
    pLogDbg->PerfRegister(1, _T("DebugOutput Test"));
    pLogCsv->PerfRegister(2, _T("CSV File Test"));
    pLogCEPerf->PerfRegister(1, _T("CE PerfLog Test"));
    
    pLogDbg->PerfBegin(1);
    Sleep(10);
    pLogDbg->PerfEnd(1);

    pLogCsv->PerfBegin(2);
    Sleep(30);
    pLogCsv->PerfEnd(2);
    
    pLogCEPerf->PerfBegin(1);
    Sleep(20);
    pLogCEPerf->PerfEnd(1);

    // don't foget to delete them :)
    delete pLogDbg;
    delete pLogCsv;
    delete pLogCEPerf;

*/


// stores the information for a perf item
struct PerfData
{
    TCHAR         name[MAX_MARKERNAME_LEN]; // item name
    LARGE_INTEGER startTick;                // start tick
    LARGE_INTEGER durations;                // durations
    DWORD         counts;                   // counts calling PerfEnd() for this item
};


// PerfLogStub is a interface class.
// Use common interfaces for different logging ways.
class PerfLogStub
{
public:
    virtual ~PerfLogStub() { DeleteCriticalSection(&m_cs); }

    // common methods used by clients
    virtual BOOL PerfSetTestName(LPCTSTR testName);
    virtual BOOL PerfRegister(DWORD id, LPCTSTR format,...);
    virtual BOOL PerfBegin(DWORD id);
    virtual BOOL PerfEnd(DWORD id);
    virtual BOOL PerfLogText(LPCTSTR format,...);

protected:
    // constant defines
    enum { 
        STRING_FORMAT_MIX  = 1,
        STRING_FORMAT_DATA = 2,
        STRING_FORMAT_TEXT = 3,
        MAX_TESTNAME_LEN = MAX_APPNAME_LEN,
        MAX_ID_NUM = MAX_MARKER_ID,
        PERFLOG_CALI_ID = MAX_ID_NUM,
        MAX_OUTPUT_BUFFER_SIZE = 1024, 
        PERFLOG_FLAG_READY = (1 << 0),
        PERFLOG_FLAG_INIT_ERROR = (1 << 1),
        PERFLOG_FLAG_USE_GTC = (1 << 2),
        PERFLOG_CALI_COUNTS = 10
    };

    PerfLogStub();

    // sub-classes can overload these methods 
    virtual BOOL Output() { return TRUE; }
    virtual BOOL Log(DWORD id) { return TRUE; }

    PerfData m_data[MAX_ID_NUM+1]; // the extra one used by calibrating
    TCHAR    m_tcharBuf[MAX_OUTPUT_BUFFER_SIZE];
    DWORD    m_flag;

private:
    LARGE_INTEGER m_freq;     // frequency 
    LARGE_INTEGER m_caliTick; // calibration ticks
    DWORD m_maxId;            // maximum allowed Perflog ID + 1
    CRITICAL_SECTION m_cs;

    // these methods used by itself
    BOOL PerfBeginInternal(DWORD id);
    BOOL InitTickCounter(void);
    BOOL ValidID(DWORD id);
    BOOL Calibrate(void);
    void GetCurrentTick(LARGE_INTEGER *pTick);
    
    // declarations only, prevent copying
    PerfLogStub(const PerfLogStub&);
    PerfLogStub& operator=(const PerfLogStub&);
};


typedef void (* PerfDbgout)(LPCTSTR);

// DebugOutput can be used for logging to debug output
class DebugOutput: public PerfLogStub
{
public:
    DebugOutput(PerfDbgout _perfdbg): m_dump(_perfdbg) { }
protected:
    BOOL Log(DWORD id);
    virtual BOOL Output();
private:
    DebugOutput();
    PerfDbgout m_dump;
};


// CsvFile can be used for logging to CSV files
class CsvFile: public PerfLogStub
{
public:
    // should specify the filename
    CsvFile(PCTSTR filename, BOOL bTruncated);
    virtual ~CsvFile();

protected:
    virtual BOOL Output();
    BOOL Log(DWORD id);

private:
    CHAR   m_charBuf[MAX_OUTPUT_BUFFER_SIZE];
    HANDLE m_hFile;
    BOOL   m_bNewLine;
};


// CEPerfLog is a simple wrapper class for PerfLog APIs.
// It adapts the PerfLog APIs to PerfLogStub interface
class CEPerfLog: public PerfLogStub
{
public:
    virtual BOOL PerfSetTestName(LPCTSTR testName);
    virtual BOOL PerfRegister(DWORD id, LPCTSTR format,...);
    virtual BOOL PerfBegin(DWORD id);
    virtual BOOL PerfEnd(DWORD id);
};

