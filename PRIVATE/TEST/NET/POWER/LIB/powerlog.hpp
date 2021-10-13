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
#ifndef __POWERLOG_HPP_
#define __POWERLOG_HPP_

#pragma once

#include <tux.h>
#include "cmnprint.h"
#include <assert.h>

namespace ce {
namespace qa {

// the log is written to memory until flushed
// this prevents file writes associated with the log from interfering with the tests

#define MAX_BUFFER_SIZE 1024*1024
#define DEFAULT_LOG_FILE TEXT("\\powerqa.txt")

#define MAX_LOG_SIZE MAX_PRINT_CHARS

extern TCHAR g_szLogFileName[MAX_PATH];

class PowerLog
{
private:
    TCHAR* szBuffer;
    DWORD dwBufferSize;    
    DWORD dwLogBytes;    

    FILETIME sTestStartTime;

   PowerLog(TCHAR* szLogFileName = DEFAULT_LOG_FILE, DWORD dwBufSize=MAX_BUFFER_SIZE) 
        :dwLogBytes(0)
    {
        if (dwBufferSize <= 0)
            dwBufferSize = MAX_BUFFER_SIZE;
        szBuffer = new TCHAR[dwBufSize];
    }

    ~PowerLog()
    {
        delete [] szBuffer;
    }  

    PowerLog(const PowerLog &src);
    PowerLog &operator = (const PowerLog &src);

    static PowerLog* psPowerSingletonInstance;
        
public:
 
    static PowerLog* GetInstance() 
    {
        if (!psPowerSingletonInstance)
        {
            if (g_szLogFileName)
                psPowerSingletonInstance = new PowerLog(g_szLogFileName);
            else
                psPowerSingletonInstance = new PowerLog();            
        }
        assert (psPowerSingletonInstance);
        return psPowerSingletonInstance;
    }
        
    static void DeleteInstance()
    {
        if (psPowerSingletonInstance)
        {
            psPowerSingletonInstance->Flush();
            delete psPowerSingletonInstance;
        }
    }
    
    void LogMarker(const TCHAR *tszFormat, ...)
    {
        TCHAR tszLog[MAX_LOG_SIZE];

        va_list vl;
        va_start(vl, tszFormat);

        _vsntprintf(tszLog, MAX_LOG_SIZE, tszFormat, vl);
        tszLog[MAX_LOG_SIZE - 1] = 0;
        va_end(vl);

        LogTime();
        Log(TEXT("MARKER: \"%s\" \r\n"), tszLog);
    }

    
    void LogTestStart(LPCTSTR szDesc, UINT uThreads, UINT uSeed)
    {
        LogTime();
        Log(TEXT("BEGIN TEST: \"%s\" Threads=%u, Seed=%u \r\n"), szDesc, uThreads, uSeed);
        Flush();
    }

    void LogTestStart(LPCTSTR szDesc)
    {
        LogTime();
        Log(TEXT("BEGIN TEST: \"%s\" \r\n"), szDesc);        
        Flush();
    }


    void LogTestEnd(DWORD dwResult)
    {
        LogTime();
        switch (dwResult)
        {
            case TPR_SKIP:
                Log(TEXT("END TEST: SKIPPED \r\n"));
                break;
            case TPR_PASS:
                Log(TEXT("END TEST: PASSED \r\n"));
                break;
            case TPR_FAIL:
                Log(TEXT("END TEST: FAILED \r\n"));
                break;
            default:
                Log(TEXT("END TEST: ABORTED \r\n"));
                break;
        }

        Flush();
    }

    // Get time since start of test in 2 bytes in multiples of 100 nanosecond
    DWORD GetLogTime()
    {
        FILETIME ft;
        SYSTEMTIME st;

        GetSystemTime(&st);
        SystemTimeToFileTime(&st,&ft);

        INT64 i64Time = (INT64) (ft.dwLowDateTime + ((INT64) ft.dwHighDateTime << (INT64) 32)) - 
                    (INT64) (sTestStartTime.dwLowDateTime + ((INT64) sTestStartTime.dwHighDateTime << (INT64) 32));

        i64Time /= (INT64) 10000;
        return (DWORD)(0x00000000FFFFFFFF & i64Time);
    }

          
    void LogTime()
    {     
        Log(TEXT("%09.1f  "), GetLogTime()/100.0f);
            
    }

    void Log(const TCHAR *tszFormat, ...)
    {
        TCHAR tszLog [MAX_LOG_SIZE];

        va_list vl;
        va_start(vl, tszFormat);

        _vsntprintf(tszLog, MAX_LOG_SIZE, tszFormat, vl);
        tszLog[MAX_LOG_SIZE - 1] = 0;
        va_end(vl);
   
        DWORD dwBytes = _tcslen(tszLog) * sizeof(TCHAR);
        if (dwBytes + dwLogBytes > MAX_BUFFER_SIZE)
        {
           Flush();
           return;
        }

        _tcscat((TCHAR*)szBuffer, tszLog);
        dwLogBytes += dwBytes;
    }

    void Flush()
    {
         TCHAR seps[] = TEXT("\r\n");
         TCHAR *token = _tcstok((LPTSTR)szBuffer, seps);
         while (token)
         {
            if(_tcslen(token) > MAX_PRINT_CHARS - 1)
            { 
                TCHAR *largeToken = token;
                do
                {
                    TCHAR chunk[MAX_PRINT_CHARS];
                    _tcsncpy(chunk, largeToken, MAX_PRINT_CHARS -1);
                    chunk[MAX_PRINT_CHARS - 1] = 0;                
                    CmnPrint(PT_LOG, TEXT("%s"), (LPCTSTR)chunk);
                    largeToken = largeToken + MAX_PRINT_CHARS - 1;            
                }
                while(largeToken && _tcslen(largeToken) > MAX_PRINT_CHARS - 1);

                if (largeToken)
                    CmnPrint(PT_LOG, TEXT("%s"), (LPCTSTR)largeToken);

                token = _tcstok(NULL, seps);                
                continue;
            }
            
            CmnPrint(PT_LOG, TEXT("%s"), (LPCTSTR)token);
            token = _tcstok(NULL, seps);
          }        

          DWORD cBytesWritten = 0;
          dwLogBytes = 0;
          szBuffer[0] = 0;
    }    
};


}
}

#endif
