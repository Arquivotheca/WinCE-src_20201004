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

#include <string.h>

#include "radiotest.h"
#include "framework.h"
#include "radiotest_metric.h"
#include "radiotest_utils.h"

#define LOCAL_STRING_BUFFER_SIZE 0x200
#define LOG_EXCEPTION          0
#define LOG_FAIL               2
#define LOG_ABORT              4
#define LOG_SKIP               6
#define LOG_NOT_IMPLEMENTED    8
#define LOG_PASS              10
#define LOG_DETAIL            12
#define LOG_COMMENT           14

extern CKato *g_pKato;

// Global logging pointer
CTestLogger* g_pLog;

#define CACHE_WIDTH 1024    

// Test stats
typedef struct
{
    DWORD dwFailCount;
    DWORD dwPassCount;
}tsTestStats;

static tsTestStats g_tsStats;

VOID TAssert( BOOL fCondition, LPCSTR const pszExpression,
             LPCSTR const pszFileName, const INT iLineNumber )
             ///////////////////////////////////////////////////////////////////////////////////
             //
             ///////////////////////////////////////////////////////////////////////////////////
{
    if ( !fCondition ) {
        WCHAR tszBuf[ MAX_LOGBUFFSIZE ];
        _sntprintf_s( tszBuf, MAX_LOGBUFFSIZE, _TRUNCATE,
            _T( "T ASSERT - exp:%hs, file:%hs, line:%d\r\n" ),
            pszExpression, pszFileName, iLineNumber );

        OutputDebugString( tszBuf );
        DebugBreak();
    }
}

BOOL WINAPI LoadGlobalKato(void)
/////////////////////////////////////////////////////////////////////////////////////
//
//This method initializes the Kato Logging Object
//
/////////////////////////////////////////////////////////////////////////////////////
{
    if(g_pKato)
        return TRUE;
    else
        g_pKato = (CKato*)KatoGetDefaultObject();
    if(g_pKato == NULL)
    {
        //printf("FATAL ERROR: Could not get a Kato Logging Object");
        return FALSE;
    }
    return TRUE;

}
BOOL WINAPI CheckKatoInit(void)
/////////////////////////////////////////////////////////////////////////////////////
//
//  This method checks whether the Kato Logging Object is initialized
//
/////////////////////////////////////////////////////////////////////////////////////
{
    // Check the Global Kato Object
    if(!g_pKato)
    {
        if(!LoadGlobalKato())
            return FALSE;
    }
    return TRUE;

}
/*
void WINAPI Log(LPTSTR szFormat, ...)
/////////////////////////////////////////////////////////////////////////////////////
//
//  This method is used for Logging to the Debug Console
//
/////////////////////////////////////////////////////////////////////////////////////
{
// Check if the Kato Logging Object is initialised.
if(!CheckKatoInit())
return;
va_list pArgs;
va_start(pArgs,szFormat);
g_pKato->LogV(LOG_DETAIL,szFormat,pArgs);
va_end(pArgs);
}*/

void WINAPI LogConditionalVerbose(BOOL fVerbose, LPTSTR szFormat, ...)
/////////////////////////////////////////////////////////////////////////////////////
//
//  This method is used for logging to the debug console but conditionally whether verbose or
//  simple logging
//
/////////////////////////////////////////////////////////////////////////////////////
{
    if(!CheckKatoInit())
        return;
    va_list pArgs;
    va_start(pArgs, szFormat);
    va_end(pArgs);

    if (fVerbose)
        g_pKato->LogV(LOG_DETAIL, szFormat, pArgs );
    else
        g_pKato->LogV(LOG_COMMENT, szFormat, pArgs );

}

void WINAPI LogWarn(LPTSTR szFormat, ...)
/////////////////////////////////////////////////////////////////////////////////////
//
//  This method is used for logging Warnings
//
/////////////////////////////////////////////////////////////////////////////////////
{
    TCHAR szBuffer[LOCAL_STRING_BUFFER_SIZE];

    if(!CheckKatoInit())
        return;
    va_list pArgs;
    va_start(pArgs, szFormat);
    _vsntprintf_s(szBuffer,LOCAL_STRING_BUFFER_SIZE,_TRUNCATE,szFormat,pArgs);
    va_end(pArgs);
    szBuffer[LOCAL_STRING_BUFFER_SIZE-1] = 0;
    // NEED TO DEFINE A CONSTANT FOR THE LOG_WARNING INSTEAD USING LOG_COMMENT
    g_pKato->Log(LOG_COMMENT,TEXT("WARN: %s"),szBuffer);

}

void WINAPI LogPass(LPTSTR szFormat, ...)
/////////////////////////////////////////////////////////////////////////////////////
//
//  This method is used for Logging a Pass result
//
/////////////////////////////////////////////////////////////////////////////////////
{
    TCHAR szBuffer[LOCAL_STRING_BUFFER_SIZE];

    if(!CheckKatoInit())
        return;
    va_list pArgs;
    va_start(pArgs, szFormat);
    _vsntprintf_s(szBuffer,LOCAL_STRING_BUFFER_SIZE,_TRUNCATE,szFormat,pArgs);
    va_end(pArgs);
    szBuffer[LOCAL_STRING_BUFFER_SIZE-1] = 0;
    g_pKato->Log(LOG_PASS,TEXT("PASS: %s"),szBuffer);

}

void WINAPI LogFail(LPTSTR szFormat, ...)
/////////////////////////////////////////////////////////////////////////////////////
//
//  This method is used for logging a Fail result
//
/////////////////////////////////////////////////////////////////////////////////////
{
    TCHAR szBuffer[LOCAL_STRING_BUFFER_SIZE];

    if(!CheckKatoInit())
        return;
    va_list pArgs;
    va_start(pArgs, szFormat);
    _vsntprintf_s(szBuffer,LOCAL_STRING_BUFFER_SIZE,_TRUNCATE,szFormat,pArgs);
    va_end(pArgs);
    szBuffer[LOCAL_STRING_BUFFER_SIZE-1] = 0;
    g_pKato->Log(LOG_FAIL,TEXT("FAIL: %s"),szBuffer);
}

void WINAPI LogComment(LPTSTR szFormat, ...)
/////////////////////////////////////////////////////////////////////////////////////
//
//  This method is used for logging a comment
//
/////////////////////////////////////////////////////////////////////////////////////
{
    TCHAR szBuffer[LOCAL_STRING_BUFFER_SIZE];

    if(!CheckKatoInit())
        return;
    va_list pArgs;
    va_start(pArgs, szFormat);
    _vsntprintf_s(szBuffer,LOCAL_STRING_BUFFER_SIZE,_TRUNCATE,szFormat,pArgs);
    va_end(pArgs);
    szBuffer[LOCAL_STRING_BUFFER_SIZE-1] = 0;
    g_pKato->Log(LOG_COMMENT,TEXT("INFO: %s"),szBuffer);

}

void WINAPI Log(LPCTSTR tszFormat, ...)
{
    TCHAR logBuf[2048];
    TCHAR *pszEnd = NULL;
    size_t cbFree = sizeof(logBuf)/sizeof(logBuf[0]);
    va_list pArgs;

    va_start(pArgs, tszFormat);

    StringCchVPrintfEx(logBuf, cbFree, &pszEnd, &cbFree, 
        STRSAFE_NULL_ON_FAILURE, tszFormat, pArgs);
    
    g_pLog->Log(LOG, _T("%s"), logBuf);
    va_end(pArgs);
}

VOID CTestLogger::ProcessNotification(
                                      const DWORD dwCode,
                                      const void *lpData,
                                      const DWORD cbData,
                                      const DWORD dwParam
                                      )
                                      ///////////////////////////////////////////////////////////////////////////////////
                                      //
                                      ///////////////////////////////////////////////////////////////////////////////////
{
    if (RIL_NOTIFY_SIGNALQUALITY== dwCode && cbData && lpData)
    {
        Log( UI_LOG, _T( ">>>>> RSSI: %d %d %d %d %d %d" ),
            ((RILSIGNALQUALITY*)lpData)->nSignalStrength,
            ((RILSIGNALQUALITY*)lpData)->nMinSignalStrength,
            ((RILSIGNALQUALITY*)lpData)->nMaxSignalStrength,
            ((RILSIGNALQUALITY*)lpData)->dwBitErrorRate,
            ((RILSIGNALQUALITY*)lpData)->nLowSignalStrength,
            ((RILSIGNALQUALITY*)lpData)->nHighSignalStrength );

    }
    else if (RIL_NOTIFY_REGSTATUSCHANGED == dwCode && cbData && lpData) {

        Log( UI_LOG, _T( ">>>>> REG_UPDATE: %d" ), (*(DWORD*)lpData) );

    }
    else if (RIL_NOTIFY_GPRSREGSTATUSCHANGED == dwCode && cbData && lpData) {
        // Print the location update information with GPS data
        Log( UI_LOG, _T( ">>>>> GPRS_REG_UPDATE: %d" ), (*(DWORD*)lpData) );
    }
    else if (RIL_NOTIFY_GPRSCONNECTIONSTATUS == dwCode && cbData && lpData) {

        Log( UI_LOG, _T( ">>>>> GPRS_CONN_UPDATE: evt=%d, context=%d, activated=%d" ),
            ((LPRILGPRSCONTEXTACTIVATED)lpData)->dwEvent,
            ((LPRILGPRSCONTEXTACTIVATED)lpData)->dwContextID,
            ((LPRILGPRSCONTEXTACTIVATED)lpData)->fActivated );

    }
    else if (RIL_NOTIFY_SYSTEMCAPSCHANGED == dwCode && cbData && lpData) {

        Log( UI_LOG, _T( ">>>>> SYS_CAP_UPDATE: %d" ), (*(DWORD*)lpData) );

    }
}

///////////////////////////////////////////////////////////////////////////////////
// Ensure that stats are initialized to zero
///////////////////////////////////////////////////////////////////////////////////
CTestLogger::CTestLogger(
                         const LPTSTR ptszPrefix,
                         const BOOL fLogEnable = TRUE,
                         const BOOL fEnableATHook = TRUE) :
m_dwFailListCount(0),
m_failList(NULL),
m_ptszCurrTestName(NULL),
m_hmodGPS(NULL),
m_uIndent(0),
m_iStepDepth(0)
{
    if (NULL == ptszPrefix)
    {
        m_tszPrefix[0] = 0L;
    }
    else
    {
        StringCchCopy(m_tszPrefix, MAX_PREFIX_LENGTH, ptszPrefix);
    }

    memset(&g_tsStats, 0, sizeof(g_tsStats));
    m_failList = (PFAILEDTEST)new FAILEDTEST[FAIL_LIST_MAX];
    memset( m_failList, 0, sizeof( FAILEDTEST ) * FAIL_LIST_MAX );

    if (SUCCEEDED(RIL_InitializeEx(
        1,
        (RILRESULTCALLBACKEX)RadioMetrics_RIL_ResultCallback,
        (RILNOTIFYCALLBACKEX)RadioMetrics_RIL_NotifyCallback,
        RIL_NCLASS_NETWORK | RIL_NCLASS_MISC,
        (DWORD)this,
        TOOLNAME,
        &m_hRil)))
    {
        Log( LOG, _T("INFO: RIL initialized successfully.") );
    }
    else
    {
        Log( LOG, _T("INFO: RIL initialization failed.") );
    }
}

CTestLogger::~CTestLogger(void)
{
    if (m_failList)
    {
        delete[] m_failList;
        m_failList = 0;
    }

    if (SUCCEEDED(RIL_Deinitialize(m_hRil)))
    {
        Log( LOG, _T("INFO: RIL deinitialized successfully.") );
    }
    else
    {
        Log( LOG, _T("INFO: RIL deinitialized failed.") );
    }
}

VOID CTestLogger::Log( BYTE bOutputType, LPCTSTR ptszFormat, ... )
///////////////////////////////////////////////////////////////////////////////////
//  Function looks overloaded but isn't since the argumnets are slighly different
//  Calls to CTestLogger::Log() will end up here
//  Calls from base class will stay within that class not processed here
///////////////////////////////////////////////////////////////////////////////////
{
    TCHAR tszOutput[CACHE_WIDTH];
    
    // append timestamp
    SYSTEMTIME st;
    GetLocalTime(&st);
    _sntprintf_s(
        tszOutput,
        CACHE_WIDTH,
        _TRUNCATE,
        _T("%s: %02u:%02u:%02u:%02u: \n"),
        m_tszPrefix,
        st.wDay, st.wHour, st.wMinute, st.wSecond
        );

    int iLen = _tcslen(tszOutput);

    // add indent
    for (UINT u = 0; u < m_uIndent; u++) {
        *(tszOutput + iLen) = _T(' ');
        iLen ++;
    }

    // Insert incoming message into cache
    // This will always be null terminated since the last cache char is left alone and initialized to zero
    va_list vlArg;
    va_start(vlArg, ptszFormat);
    int iCount=_vsntprintf_s(tszOutput+iLen, CACHE_WIDTH - iLen - 1, _TRUNCATE, ptszFormat, vlArg);
    va_end(vlArg);

    // On oveflow we need to adjust
    if (-1 == iCount)
    {
        iCount = CACHE_WIDTH - 1;
    }
    else
    {
        iCount += iLen;
    }

    tszOutput[iCount] = 0;
    g_pKato->Log(LOG_DETAIL, TEXT("%s"), tszOutput);
}

BOOL CTestLogger::DoStep( const BOOL fCondition, const LPCTSTR ptszStepName )
///////////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////////
{
    if ( fCondition ) {
        m_uStepPass++;
        Log(LOG, TEXT("PASS Step: %s"), ptszStepName);
    } else {
        m_uStepFail++;
        Log(LOG, TEXT("FAIL Step: %s"), ptszStepName);
    }
    return fCondition;
}


///////////////////////////////////////////////////////////////////////////////////
//
//  When we start a test the edit box recieves terse dialog
//  Calling base class shoves regular stuff out to the log
//
/////////////////////////////////////////////////////////////////////////////////
VOID CTestLogger::BeginTest(const LPCTSTR ptszTestName)
{
    Log( UI, TEXT("> %s"), ptszTestName);
    // Remember the name of this test
    m_ptszCurrTestName = ptszTestName;
    m_uStepPass = m_uStepFail = 0;
}

///////////////////////////////////////////////////////////////////////////////////
//
//  Upon ending a test, the edit box receives either PASS or FALSE to display
//  Calling base class shoves regular stuff out to the log
//
/////////////////////////////////////////////////////////////////////////////////
BOOL CTestLogger::EndTest()
{
    DWORD dwPassPercent=0;
    WORD i;
    BOOL fRet = FALSE;
    if(m_uStepFail)
    {
        g_tsStats.dwFailCount++;
        if (m_dwFailListCount < FAIL_LIST_MAX)
        {
            for (i = 0; i < m_dwFailListCount; i++)
            {
                if ( !wcscmp( m_failList[i].ptszFailedTestName, m_ptszCurrTestName ))
                {
                    m_failList[i].dwNumTimeFailed++;
                    break;
                }
            }
            if (i == m_dwFailListCount)
            {
                m_failList[m_dwFailListCount].ptszFailedTestName = m_ptszCurrTestName;
                m_failList[m_dwFailListCount].dwNumTimeFailed++;
                m_dwFailListCount++;
            }
        }
        Log( UI, TEXT("> -- FAIL --") );
    }
    else
    {
        g_tsStats.dwPassCount++;

        Log( UI, TEXT( "> PASS" ) );

        fRet = TRUE;
    }
    if (!g_pMetrics)
    {
        Log(
            LOG,
            TEXT("Tests passed so far: %d out of %d (%d%%)"),
            g_tsStats.dwPassCount,
            g_tsStats.dwPassCount+g_tsStats.dwFailCount,
            (g_tsStats.dwPassCount+g_tsStats.dwFailCount) ?
            ((g_tsStats.dwPassCount * 100) + ((g_tsStats.dwPassCount+g_tsStats.dwFailCount)/2)) /(g_tsStats.dwPassCount+g_tsStats.dwFailCount) : 0
            );
    }

    return fRet;
}

VOID CTestLogger::BeginStep( const LPCTSTR ptszStepName, ... )
{

    TASSERT( m_iStepDepth < MAX_STEPDEPTH );
    if ( m_iStepDepth >= MAX_STEPDEPTH ){
        Log( LOG, _T( "Error: m_iStepDepth %d exceeds MAX_CTLOGSTEPDEPTH!" ), m_iStepDepth );
        return;
    }
    TASSERT( ptszStepName );

    va_list vlArg;
    va_start( vlArg, ptszStepName );

    TCHAR tszBuf[ MAX_LOGBUFFSIZE ] = { 0 };

    int iCount= _vsntprintf_s( tszBuf, MAX_LOGBUFFSIZE, _TRUNCATE, ptszStepName, vlArg );

    va_end( vlArg );

    if(-1==iCount)
        iCount=MAX_LOGBUFFSIZE -1;

    tszBuf[iCount] = 0;
    m_ptszStepName[m_iStepDepth] = _tcsdup( tszBuf );

    ASSERT( m_ptszStepName[m_iStepDepth] );

    Log( LOG, _T( "Begin Step: %s" ), m_ptszStepName[m_iStepDepth] );

    m_uIndent += CT_INDENT;

    m_iStepDepth++;

}

BOOL CTestLogger::EndStep( const BOOL fCondition )
{
    m_iStepDepth--;
    TASSERT( m_iStepDepth >= 0 );
    TASSERT( m_ptszStepName[m_iStepDepth] );

    BOOL b = FALSE;

    m_uIndent -= CT_INDENT;
    b = DoStep( fCondition, m_ptszStepName[m_iStepDepth] );

    if( m_ptszStepName[m_iStepDepth] )
    {
        delete m_ptszStepName[m_iStepDepth];
        m_ptszStepName[m_iStepDepth] = NULL;
    }

    return b;
}
