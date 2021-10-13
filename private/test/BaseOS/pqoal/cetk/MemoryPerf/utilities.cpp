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


// ***************************************************************************************************************
//
// These functions are a collection of low level utilities
//
// ***************************************************************************************************************


#include "memoryPerf.h"
#include "globals.h"

#pragma warning( disable: 25068 )   // prefast does not like StringCchVPrintfA but it's use is OK here


// ***************************************************************************************************************
static HANDLE ghLogFile = INVALID_HANDLE_VALUE;
static int    giLogPath = 0;
static HANDLE ghCSVFile = INVALID_HANDLE_VALUE;


// ***************************************************************************************************************
// global initialization 
// ***************************************************************************************************************

void
MemPerfMiscInit()
{
    {   // Initialize misc globals
        gtGlobal.gbInitialized = false;
        gtGlobal.giUseQPC = 0;
        gtGlobal.gliFreq.QuadPart = 1000;
        gtGlobal.gliOrigFreq.QuadPart = gtGlobal.gliFreq.QuadPart;
        gtGlobal.gdwCountFreqChanges = 0;
        gtGlobal.gdTickScale2us = 1000000.0/gtGlobal.gliFreq.QuadPart;
        gtGlobal.gdTotalTestLoopUS = 0;
        gtGlobal.bExtraPrints = false;
        gtGlobal.bConsole2 = false;        // tee to the debug output window as well as a log file.
        gtGlobal.bWarnings = true;
        gtGlobal.giCEPriorityLevel = 224;   // assume power management is on and needs to run
        gtGlobal.giRescheduleSleep = 0;     // assume special sleep mode to compat power management is needed
    }
    
    UINT uiRand = 0;

    if ( 0 != QueryPerformanceFrequency( &gtGlobal.gliFreq ) ) 
    {
        // QueryPerformance Counter is seldom implemented in Windows Mobile.  But it is being implemented in quite a few 7 devices.
        gtGlobal.gliOrigFreq.QuadPart = gtGlobal.gliFreq.QuadPart;
        LARGE_INTEGER liStartTime, liStopTime;
        liStartTime.QuadPart = -1;
        liStopTime.QuadPart = -1;
        if ( 0 != QueryPerformanceCounter( &liStartTime ) )
        {
            volatile int x = 0;
            for( int i=0; i<1000; i++ )
            {
                rand_s(&uiRand);
                x += uiRand;
            }
            QueryPerformanceCounter( &liStopTime );
            if ( liStopTime.QuadPart > liStartTime.QuadPart )
            {
                if ( gtGlobal.gliFreq.QuadPart >= 1000000 )
                {   // All WM7 devices should have pretty good performance counters, but some might not
                    gtGlobal.giUseQPC = 1;
                }
                else if ( gtGlobal.gliFreq.QuadPart >= 10000 )
                {   
                    gtGlobal.giUseQPC = 2;
                }
                // otherwise user normal GetTickCount which is 1 ms
                NKDbgPrintfW( L"!QPC Time to call %d rand_s()'s took %I64d ticks at freq %I64d.\r\n", 
                    i, liStopTime.QuadPart - liStartTime.QuadPart, gtGlobal.gliFreq.QuadPart );
                if ( gtGlobal.gliFreq.QuadPart < 10000 )
                {   // really the else part from above
                    gtGlobal.gliFreq.QuadPart = 1000;  // will use ms timer tick.
                }
            }
            else
            {
                NKDbgPrintfW( L"!QPC Time is STATIC!  e.g. to call %d rand_s()'s took %I64d ticks at freq %I64d.\r\n", i, liStopTime.QuadPart - liStartTime.QuadPart, gtGlobal.gliFreq.QuadPart );
                gtGlobal.gliFreq.QuadPart = 1000;  // will use ms timer tick.
            }
        }
        else
        {
            NKDbgPrintfW( L"!QFC fails but QPC Frequency is %I64d.\r\n",  gtGlobal.gliFreq.QuadPart );
            gtGlobal.gliFreq.QuadPart = 1000;  // will use ms timer tick.
        }
    }
    else
    {
        gtGlobal.gliFreq.QuadPart = 1000;  // will use ms timer tick.
    }

    gtGlobal.gdTickScale2us = 1000000.0 / gtGlobal.gliFreq.QuadPart;
    gtGlobal.gbInitialized = true;

    {
        giLogPath = 0;
        // For TUX, the log file is replaced by the KATO log
        if ( NULL == g_pKato )
        {
            // try to open a log file, otherwise fall-back to debug output
            if ( INVALID_HANDLE_VALUE == ghLogFile ) 
            {   
                giLogPath = 1;
                ghLogFile = CreateFile( L"\\Release\\MemoryPerfExe.Log", GENERIC_WRITE, FILE_SHARE_READ, NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL  );
                if ( INVALID_HANDLE_VALUE == ghLogFile ) 
                    ghLogFile = CreateFile( L"\\Release\\MemoryPerfExe.Log", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL  );
            }
            if (INVALID_HANDLE_VALUE == ghLogFile)
            {
                giLogPath = 2;
                ghLogFile = CreateFile( L"\\Storage Card\\MemoryPerfExe.Log", GENERIC_WRITE, FILE_SHARE_READ, NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
                if ( INVALID_HANDLE_VALUE == ghLogFile ) 
                    ghLogFile = CreateFile( L"\\Storage Card\\MemoryPerfExe.Log", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
            }
            if ( INVALID_HANDLE_VALUE == ghLogFile ) 
            {   
                giLogPath = 3;
                ghLogFile = CreateFile( L"\\My Documents\\MemoryPerfExe.Log", GENERIC_WRITE, FILE_SHARE_READ, NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL  );
                if ( INVALID_HANDLE_VALUE == ghLogFile ) 
                    ghLogFile = CreateFile( L"\\My Documents\\MemoryPerfExe.Log", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL  );
            }
            if ( INVALID_HANDLE_VALUE == ghLogFile ) 
            {   
                giLogPath = 4;
                ghLogFile = CreateFile( L"\\MemoryPerfExe.Log", GENERIC_WRITE, FILE_SHARE_READ, NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
                if ( INVALID_HANDLE_VALUE == ghLogFile ) 
                    ghLogFile = CreateFile( L"\\MemoryPerfExe.Log", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
            }
            if ( INVALID_HANDLE_VALUE == ghLogFile ) 
            {
                giLogPath = 5;
            }
        }
        #if defined(DEBUG) || defined(_DEBUG)
            const char strCompile[] = "DEBUG";
        #else
            const char strCompile[] = "Retail";
        #endif
        SYSTEMTIME sLocalTime = {0};
        GetLocalTime( &sLocalTime );
        if ( !( 0==sLocalTime.wMonth && 0==sLocalTime.wDay && 0==sLocalTime.wYear ) )
        {
            LogPrintf( "MemoryPerf %s Starting on %2d/%02d/%4d %2d:%02d:%02d with QPF %ld\r\n\r\n",
                strCompile, 
                sLocalTime.wMonth, sLocalTime.wDay, sLocalTime.wYear, 
                sLocalTime.wHour, sLocalTime.wMinute, sLocalTime.wSecond, 
                gtGlobal.gliFreq.QuadPart );
        }
    }
}


// ***************************************************************************************************************
// close things open above
// ***************************************************************************************************************

void MemPerfClose(void)
{
    if ( INVALID_HANDLE_VALUE != ghLogFile )
    {
        #if defined(DEBUG) || defined(_DEBUG)
            const char strCompile[] = "DEBUG";
        #else
            const char strCompile[] = "Retail";
        #endif
        SYSTEMTIME sLocalTime = {0};
        GetLocalTime( &sLocalTime );
        if ( !( 0==sLocalTime.wMonth && 0==sLocalTime.wDay && 0==sLocalTime.wYear ) )
        {
            LogPrintf( "MemoryPerf %s Exiting on %2d/%02d/%4d %2d:%02d:%02d with QPF %ld\r\n\r\n",
                strCompile, 
                sLocalTime.wMonth, sLocalTime.wDay, sLocalTime.wYear, 
                sLocalTime.wHour, sLocalTime.wMinute, sLocalTime.wSecond, 
                gtGlobal.gliFreq.QuadPart );
        }
        if ( CloseHandle( ghLogFile ) )
        {
            ghLogFile = INVALID_HANDLE_VALUE;
        }
    }
    if ( INVALID_HANDLE_VALUE != ghCSVFile && CloseHandle( ghCSVFile ) )
    {
        ghCSVFile = INVALID_HANDLE_VALUE;
    }
}


// ***************************************************************************************************************
// Workhorse logging function
// ***************************************************************************************************************

void
LogPrintf(
    __format_string const char* format,
    ...
    )
{
    va_list       arglist;
    const size_t  MAX_BUFFER = 1000;
    char          sz[MAX_BUFFER];
    wchar_t       wsz[MAX_BUFFER];
    HRESULT       hr = S_OK;

    va_start(arglist, format);
    hr = StringCchVPrintfA(sz, MAX_BUFFER, format, arglist);
    va_end(arglist);

    if (SUCCEEDED(hr))
    {
        if ( NULL != g_pKato )
        {
            // normally, output goes to KATO for TUX tests
            swprintf_s( wsz, MAX_BUFFER, L"%S", sz );   // first convert to wide characters
            g_pKato->Log( LOG_DETAIL, L"%s", wsz );     // output and avoid interpreting % characters that have already been handled by StringCchVPrintfA above
        }
        else
        {
            // should this ever get used by a non-tux executable, allow ghLogFile and debug output to take over
            DWORD dwLength = strlen( sz );
            DWORD dwWritten = 0;
            if ( INVALID_HANDLE_VALUE == ghLogFile || 0 == WriteFile( ghLogFile, sz, dwLength, &dwWritten, NULL ) || dwLength != dwWritten || gtGlobal.bConsole2 )
            {
                // this has a limit of 256 bytes (128 wide characters)
                NKDbgPrintfW( L"%S", sz );
            }
        }
        if ( INVALID_HANDLE_VALUE != ghCSVFile )
        {
            // Tee log info to csv file to add context for the numbers shown there
            // we need to convert text so single fields do not get split into separate fields by MS Excel
            if ( 0 == strncmp( sz, "\r\n", 2) )
            {
                DWORD dwWritten = 0;
                WriteFile( ghCSVFile, sz, 1, &dwWritten, NULL );
            }
            else
            {
                const int ciMaxCSVBuffer = MAX_BUFFER+20;   // extra room for some extra characters
                char szCSV[ciMaxCSVBuffer];
                char * pszCSV = szCSV;
                const char * psz    = sz;
                int iChars = 1;
                *pszCSV++ = '"';
                while ( *psz && iChars < (ciMaxCSVBuffer-4) )
                {
                    char c = *psz++;
                    switch( c )
                    {
                    case '\r':
                        *pszCSV++ = '"';
                        *pszCSV++ = c;
                        iChars += 2;
                        break;
                    case '\n':
                        *pszCSV++ = c;
                        iChars++;
                        if ( *psz )
                        {
                            *pszCSV++ = '"';
                            iChars++;
                        }
                        break;
                    case '"':
                        *pszCSV++ = '\'';
                        iChars += 1;
                        break;
                    default:
                        *pszCSV++ = c;
                        iChars += 1;
                        break;
                    }
                }
                if ( '"' != *(pszCSV-1) && '\n' != *(pszCSV-1) )
                {
                    *pszCSV++ = '"';
                    iChars++;
                }

                *pszCSV++ = 0;
                DWORD dwWritten = 0;
                WriteFile( ghCSVFile, szCSV, iChars, &dwWritten, NULL );
            }
        }
    }
}


// ***************************************************************************************************************
// a simple utility calculation
// ***************************************************************************************************************

int iRoundUp( const int ciNum, const int ciModulo )
{
    return ((ciNum + ciModulo -1 ) / ciModulo) * ciModulo;
}


// ***************************************************************************************************************
// open the CSV file, ideally in the same directory as the log file
// ***************************************************************************************************************

bool bCSVOpen()
{
    // try to open the CSV file on the same path as the log file
    // base name is either MemoryPerf.csv (Tux) or MemoryPerfExe.csv (Exe)
    WCHAR wstrBaseName[40] = {0};
    swprintf_s( wstrBaseName, 40, L"%s", (NULL == g_pKato) ? L"MemoryPerfExe.csv" : L"MemoryPerf.csv" );
    WCHAR wstrCSVFileName[MAX_PATH] = {0};
    bool bErrMsg = false;
    switch( giLogPath )
    {
    default:    // fall thru to search all paths if we don't know the path to the log file.
        LogPrintf( "Info: Unknown Log file path, so searching path list for a place to put MemoryPerf.CSV\r\n" );
        bErrMsg = true;
        __fallthrough;
    case 0: case 1:
        swprintf_s( wstrCSVFileName, MAX_PATH, L"\\Release\\%s", wstrBaseName );
        ghCSVFile = CreateFile(  wstrCSVFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL  );
        if ( INVALID_HANDLE_VALUE == ghCSVFile ) 
            ghCSVFile = CreateFile(  wstrCSVFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL  );
        if ( INVALID_HANDLE_VALUE != ghCSVFile )
        {
            LogPrintf( "Info: CSV file opened on path %S\r\n",  wstrCSVFileName );
            break;
        }
        // otherwise fall thru and try to put it someplace
        if ( !bErrMsg )
        {
            LogPrintf( "Error: opening CSV file on path \\Release to match log file path\r\n" );
            bErrMsg = true;
        }
        __fallthrough;
    case 2:
        swprintf_s( wstrCSVFileName, MAX_PATH, L"\\Storage Card\\%s", wstrBaseName );
        ghCSVFile = CreateFile( wstrCSVFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
        if ( INVALID_HANDLE_VALUE == ghCSVFile ) 
            ghCSVFile = CreateFile(wstrCSVFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
        if ( INVALID_HANDLE_VALUE != ghCSVFile )
        {
            LogPrintf( "Info: CSV file opened on path %S\r\n",  wstrCSVFileName );
            break;
        }
        // otherwise fall thru and try to put it someplace
        if ( !bErrMsg )
        {
            LogPrintf( "Error: opening CSV file on path \\Storage Card to match log file path\r\n" );
            bErrMsg = true;
        }
        __fallthrough;
    case 3:
        swprintf_s( wstrCSVFileName, MAX_PATH, L"\\My Documents\\%s", wstrBaseName );
        ghCSVFile = CreateFile( wstrCSVFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL  );
        if ( INVALID_HANDLE_VALUE == ghCSVFile ) 
            ghCSVFile = CreateFile( wstrCSVFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL  );
        if ( INVALID_HANDLE_VALUE != ghCSVFile )
        {
            LogPrintf( "Info: CSV file opened on path %S\r\n",  wstrCSVFileName );
            break;
        }
        // otherwise fall thru and try to put it someplace
        if ( !bErrMsg )
        {
            LogPrintf( "Error: opening CSV file on path \\My Documents to match log file path\r\n" );
            bErrMsg = true;
        }
        __fallthrough;
    case 4:
        swprintf_s( wstrCSVFileName, MAX_PATH, L"\\%s", wstrBaseName );
        ghCSVFile = CreateFile( wstrCSVFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
        if ( INVALID_HANDLE_VALUE == ghCSVFile ) 
            ghCSVFile = CreateFile( wstrCSVFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
        if ( INVALID_HANDLE_VALUE != ghCSVFile )
        {
            LogPrintf( "Info: CSV file opened on path %S\r\n",  wstrCSVFileName );
            break;
        }
        if ( !bErrMsg )
        {
            LogPrintf( "Error: opening CSV file on path \\ to match log file path\r\n" );
            bErrMsg = true;
        }
    }

    if ( INVALID_HANDLE_VALUE != ghCSVFile )
    {
        #if defined(DEBUG) || defined(_DEBUG)
            const char strCompile[] = "DEBUG";
        #else
            const char strCompile[] = "Retail";
        #endif
        SYSTEMTIME sLocalTime = {0};
        GetLocalTime( &sLocalTime );
        if ( !( 0==sLocalTime.wMonth && 0==sLocalTime.wDay && 0==sLocalTime.wYear ) )
        {
            CSVPrintf( "\"MemoryPerf %s Starting on %2d/%02d/%4d %2d:%02d:%02d\"\r\n\r\n",
                strCompile, sLocalTime.wMonth, sLocalTime.wDay, sLocalTime.wYear, sLocalTime.wHour, sLocalTime.wMinute, sLocalTime.wSecond );
        }
        return true;
    }
    LogPrintf( "Error: Unable to open MemoryPerf.csv\r\n" );
    return false;
}


// ***************************************************************************************************************
// printf destined for the CSV file
// ***************************************************************************************************************

void
CSVPrintf(
    __format_string const char* format,
    ...
    )
{
    va_list       arglist;
    const size_t  MAX_BUFFER = 1024;
    char          sz[MAX_BUFFER];
    HRESULT       hr = S_OK;

    if ( INVALID_HANDLE_VALUE != ghCSVFile )
    {
        va_start(arglist, format);
        hr = StringCchVPrintfA(sz, MAX_BUFFER, format, arglist);
        va_end(arglist);

        if (SUCCEEDED(hr))
        {
            DWORD dwLength = strlen( sz );
            DWORD dwWritten = 0;
            if ( 0 == WriteFile( ghCSVFile, sz, dwLength, &dwWritten, NULL ) || dwLength != dwWritten )
            {
                // this has a limit of 256 bytes (128 wide characters)
                NKDbgPrintfW( L"Error: CSV output failed for %S", sz );
            }
        }
    }
}

