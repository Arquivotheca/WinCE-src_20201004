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
#include <stdio.h>
#include <devinfo.h>
#include "..\bootStress.h"

static bool RecordBootTime(TCHAR* szVirtualRelease);
static bool RecordTotalBootTime(TCHAR*szLastTickFullPath, TCHAR* szBootTestFullPath );
static bool PrintTestResult(TCHAR* szBootTestFullPath);


static BOOL RunTest()
{
    BOOL fOk = FALSE;
    DWORD toRun = 0;
    DWORD dwTotalRan = 0;

    NKDbgPrintfW(L"**************start Bootstress.exe\r\n");
    //-------------------------------------------------------------------------
    //update number of runs
    //-------------------------------------------------------------------------
    
    FILE* pTimesRunFile = NULL;
    errno_t err;

    err = _wfopen_s( &pTimesRunFile, FILE_TOTAL_RAN, L"r" );
    if ((err != 0) || (pTimesRunFile == NULL))
    {
        NKDbgPrintfW(L"Bootstress.exe: Failed to open bootStressTotalRan.txt\r\n");
        goto cleanUp;
    }
    fwscanf_s( pTimesRunFile, L"%lu", &dwTotalRan );
    fclose( pTimesRunFile );
    pTimesRunFile = NULL;
    dwTotalRan++;

    err =  _wfopen_s( &pTimesRunFile, FILE_TOTAL_RAN, L"w" );
    if ((err != 0) || (pTimesRunFile == NULL))
    {
        NKDbgPrintfW(L"Bootstress.exe: Failed to open bootStressTotalRan.txt\r\n");
        goto cleanUp;
    }
    fwprintf_s( pTimesRunFile, L"%lu", dwTotalRan );
    fclose( pTimesRunFile );
    pTimesRunFile = NULL;
        
    FILE *pBootTestFile = NULL;
    err =  _wfopen_s( &pBootTestFile, FILE_BOOTTEST, L"r" );
    if ((err != 0) || (pBootTestFile == NULL))
    {
        NKDbgPrintfW(L"Bootstress.exe: Failed to open bootStress.txt\r\n");
        goto cleanUp;
    }
    fwscanf_s( pBootTestFile, L"%lu", &toRun );
    fclose( pBootTestFile );
    pBootTestFile = NULL;

    //-------------------------------------------------------------------------
    //Update total boot time (TotalBootTime = Shutdown + Download + Boot time)
    //-------------------------------------------------------------------------
    
    bool bReturn = RecordTotalBootTime(FILE_LASTTICK, FILE_BOOTTEST);
    if (!bReturn)
    {
        goto cleanUp;
    }

          
    if (toRun == 0) 
    {
        return 0;
    }
    else if (dwTotalRan >= toRun) //test completed
    {
        bool bReturn = PrintTestResult(FILE_BOOTTEST);
        if (!bReturn)
        {
            goto cleanUp;
        }
    }
    else  // continue test. 
    {
        
        NKDbgPrintfW(L"Bootstress.exe: Reboot device\r\n");
        reboot();
        NKDbgPrintfW(L"Bootstress.exe: Reboot failed\r\n");
    }
    
    fOk = TRUE;
    return fOk;

cleanUp:
    NKDbgPrintfW(L"**************Failed to execute Bootstress.exe\r\n");
    return fOk;
}

//-----------------------------------------------------------------------------
//Calculates the time difference between two resets.
// TotalBootTime = Shutdown + Download + Boot time
//-----------------------------------------------------------------------------
static bool RecordTotalBootTime(TCHAR*szLastTickFullPath, TCHAR* szBootTestFullPath )
{
     //Record the time between now and reset command is issued
    DWORD dwLastTickHigh;
    DWORD dwLastTickLow;
    errno_t err;

    
    FILE* pLastTickFile = NULL;
    err = _wfopen_s(&pLastTickFile, szLastTickFullPath, L"r" );
    if ((err != 0) || (pLastTickFile == NULL))
    {
        NKDbgPrintfW(L"Bootstress.exe: RecordTotalBootTime Failed to open bootStressLastTick.txt \r\n");
        return false;
    }
    fwscanf_s( pLastTickFile, L"%lu", &dwLastTickHigh );
    fwscanf_s( pLastTickFile, L"%lu", &dwLastTickLow );
    fclose( pLastTickFile );
    pLastTickFile = NULL;

    FILETIME ftCurTime; //This structure is a 64-bit value representing the number of 100-nanosecond 
                                    //intervals since January 1, 1601. 1 second = 1000000000 nano seconds


    GetCurrentFT( &ftCurTime ); // GetCurrentFT returns value based in UTC unit. 
                                            // 1 second = 10000000 UTC unit 
                                            // 1 millisecond = 10000 UTC unit
    
    ULARGE_INTEGER ulCurTime, ulLastTime;
    ulCurTime.LowPart  = ftCurTime.dwLowDateTime;
    ulCurTime.HighPart  = ftCurTime.dwHighDateTime;
    ulLastTime.LowPart  = dwLastTickLow;
    ulLastTime.HighPart  = dwLastTickHigh;
    
    err = _wfopen_s( &pLastTickFile, szLastTickFullPath, L"w" );
    if ((err != 0) ||(pLastTickFile == NULL))
    {
        NKDbgPrintfW(L"Bootstress.exe: RecordTotalBootTime Failed to open bootStressLastTick.txt \r\n");
        return false;
    }
    fwprintf_s( pLastTickFile, L"%lu", ulCurTime.HighPart );
    fwprintf_s( pLastTickFile, L" %lu", ulCurTime.LowPart );
    fclose( pLastTickFile );
    pLastTickFile =NULL;

    FILE* pBootTestFile = NULL;
    err = _wfopen_s( &pBootTestFile, szBootTestFullPath, L"a" );
    if ((err != 0) ||(pBootTestFile == NULL))
    {
        NKDbgPrintfW(L"Bootstress.exe: RecordTotalBootTime Failed to open bootStress.txt \r\n");
        return false;
    }

    // 1 millisecond = 10000 UTC unit
    // convert time difference in milliseconds  
    DWORD dwFullBootTimeMs = (DWORD)((ulCurTime.QuadPart - ulLastTime.QuadPart)/10000);
    fwprintf_s( pBootTestFile, L" %lu", dwFullBootTimeMs );
    fclose( pBootTestFile );
    pBootTestFile = NULL;
    return true;
}


//-----------------------------------------------------------------------------
//Print test result at the end of test
//Test result contains number of reboot, average boot time and standard deviation
//-----------------------------------------------------------------------------
static bool PrintTestResult(TCHAR* szBootTestFullPath)
{
    //done - calculate average TickCounts
       FILE *pBootTestFile = NULL;
    errno_t err;
    err = _wfopen_s( &pBootTestFile, szBootTestFullPath, L"r" );
       if ((err != 0) || (pBootTestFile == NULL))
       {
            NKDbgPrintfW(L"Bootstress.exe: PrintTestResult Failed to open bootStress.txt \r\n");
            return false;
        }

        DWORD dwTotalRan;
        fseek( pBootTestFile, 0, SEEK_SET );
        fwscanf_s( pBootTestFile, L"%lu", &dwTotalRan );

        NKDbgPrintfW(L"********************************\r\n");
        NKDbgPrintfW(L"Reboot test Complete:\r\n");
        NKDbgPrintfW(L"Open BootStress.txt to view the time recorded for shutdown+download+boot:\r\n");
            
        NKDbgPrintfW(L"Download plus Boot Time for each test:\r\n");
        DWORD dwTotalBootTimeMS = 0;
        for (DWORD i=0; i < dwTotalRan; i++)
        {
            DWORD dwCurBootTimeMS = 0;
            fwscanf_s( pBootTestFile, L"%lu", &dwCurBootTimeMS);
            NKDbgPrintfW(L"%lu seconds \r\n", dwCurBootTimeMS/1000);
            dwTotalBootTimeMS += dwCurBootTimeMS;
        }
        
        double mean; // mean is average boot time in seconds
        mean = dwTotalBootTimeMS/dwTotalRan/1000;
        
        double totalVariance = 0;
        fseek( pBootTestFile, 0, SEEK_SET );
        fwscanf_s( pBootTestFile, L"%lu", &dwTotalRan );
        for (DWORD i=0; i < dwTotalRan; i++)
        {
            DWORD dwCurBootTimeMS = 0;
         fwscanf_s( pBootTestFile, L"%lu", &dwCurBootTimeMS);
         totalVariance += (dwCurBootTimeMS/1000 - mean)*(dwCurBootTimeMS/1000 - mean);
        }
        
        double variance = 0;
        variance = totalVariance/dwTotalRan;
        
        double stdDev = sqrt(variance);
                   
        double percentage = 0;
        if (mean > 0)
        {
            percentage = stdDev/mean*100.0;
        }
        
        wchar_t buffer[32];
        wchar_t buffer2[32];
        wchar_t buffer3[32];
        StringCbPrintfW(buffer, 32, L"%.2f", mean);
        StringCbPrintfW(buffer2, 32, L"%.2f", stdDev);
        StringCbPrintfW(buffer3, 32, L"%.2f", percentage);
        //output results
      
        NKDbgPrintfW(L"Test ran %d times\r\n", dwTotalRan);
        NKDbgPrintfW(L"Average Boot Time  = %s seconds\r\n", buffer);
        NKDbgPrintfW(L"Standard Deviation  = %s seconds (%s%%)\r\n", buffer2, buffer3);
        NKDbgPrintfW(L"********************************\r\n");

        fclose( pBootTestFile );
        pBootTestFile = NULL;

    return true;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nShowCmd)
{
    UNREFERENCED_PARAMETER(nShowCmd);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(hInstance);
    
    RunTest();
    return 0;
}

