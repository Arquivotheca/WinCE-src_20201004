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
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.

#include <windows.h>
#include <tchar.h>
#include "harness.h"

// Global variables
TCHAR *g_szTestMarker = TEXT("---------------------------------------------------------------\r\n");
static TCHAR g_szBuffer[ 1024 ];
DWORD g_dwStartTime, g_dwEndTime;
CRITICAL_SECTION g_cs;
BOOL g_fCritSecInitialized = 0;
HANDLE g_hHeap=NULL;

// This file contains some helper functions related to test-pass / logging.

void CreateLog(const TCHAR* szLogName)
{
    TCHAR szLogFile[MAX_PATH];

    InitHarness();
    g_hHeap = HeapCreate( HEAP_NO_SERIALIZE, 4096, 32768);
    StringCchPrintf(szLogFile, MAX_PATH, TEXT("%s.LOG"), szLogName);
}

void CloseLog()
{
    HeapDestroy( g_hHeap);
    DeinitHarness();
}

HANDLE StartTest( const TCHAR* szText)
{
    PYGTESTDATA pTestData;
    size_t szlen = 0;
    pTestData = (PYGTESTDATA)HeapAlloc( g_hHeap, HEAP_ZERO_MEMORY, sizeof(YGTESTDATA));
    pTestData->bSuccess = TRUE;
    StringCchLength(szText, STRSAFE_MAX_CCH, &szlen);
    pTestData->szData = szText ? (LPTSTR)HeapAlloc( g_hHeap, 0, (szlen+1)*sizeof(TCHAR)) : NULL;

    if (szText)  
    {
        StringCchCopy(pTestData->szData, szlen, szText);
        LogPrint(  TEXT("START: %s\n"), szText);
    }
    return (HANDLE)pTestData;
}

BOOL FinishTest(HANDLE hTest)
{
    BOOL bSuccess;
    const YGTESTDATA* pTestData = (const YGTESTDATA*) hTest;;
    bSuccess = pTestData->bSuccess;

    if (((PYGTESTDATA)hTest)->szData) 
    {
        LogPrint( TEXT("%-5s: %s\n"), bSuccess ? TEXT("PASS") : TEXT("FAIL"), ((PYGTESTDATA)hTest)->szData);
        LogPrint( g_szTestMarker);
        HeapFree( g_hHeap, 0, ((PYGTESTDATA)hTest)->szData);
    }

    HeapFree( g_hHeap, 0, (PYGTESTDATA)hTest);
    return bSuccess;
}

void FailTest( HANDLE hTest)
{
    ((PYGTESTDATA)hTest)->bSuccess = FALSE;    
}

BOOL CritSecInitialized()
{
    return g_fCritSecInitialized; 
}

void InitCritSec()
{
    if(!CritSecInitialized())
    {
        InitializeCriticalSection(&g_cs); 
        g_fCritSecInitialized = 1; 
    }
}

HANDLE BeginStep( HANDLE hTest, const TCHAR* szText)  
{
    PYGSTEPDATA pStepData;
    size_t szlen = 0;
    ((PYGTESTDATA)hTest)->dwStepsStarted++;
    pStepData = (PYGSTEPDATA)HeapAlloc( g_hHeap, 0, sizeof(YGSTEPDATA));
    pStepData->bSuccess = FALSE;
    StringCchLength(szText, STRSAFE_MAX_CCH, &szlen);
    pStepData->szData = szText ? (LPTSTR)HeapAlloc( g_hHeap, 0, (szlen+1)*sizeof(TCHAR)) : NULL;
    if (szText)  
    {
         StringCchCopy(pStepData->szData, szlen, szText);
         LogPrint(  TEXT("BEGIN: %s\n"), szText);
     }
    return (HANDLE)pStepData;
}

void SkipStep( HANDLE hTest, HANDLE hStep)  
{                               
    ((PYGTESTDATA)hTest)->dwStepsSkipped;

    if (((PYGSTEPDATA)hStep)->szData)  
    { 
        LogPrint( TEXT(" SKIP: %s\n"), ((PYGSTEPDATA)hStep)->szData);
         HeapFree( g_hHeap, 0, ((PYGSTEPDATA)hStep)->szData);
    }

    HeapFree( g_hHeap, 0, (PYGSTEPDATA)hStep);
    return;
}

void PassStep( HANDLE hStep)  
{
  ((PYGSTEPDATA)hStep)->bSuccess = TRUE;    
}

void EndStep( HANDLE hTest, HANDLE hStep)
{
    BOOL bSuccess;
 
    ((PYGTESTDATA)hTest)->dwStepsCompleted++;
    bSuccess = ((PYGSTEPDATA)hStep)->bSuccess;

    if (!bSuccess) FailTest( hTest);

    if (((PYGSTEPDATA)hStep)->szData)  
    { 
        LogPrint( TEXT("%-5s STEP: %s\n"), bSuccess ? TEXT("PASS") : TEXT("FAIL"), ((PYGSTEPDATA)hStep)->szData);
         HeapFree( g_hHeap, 0, ((PYGSTEPDATA)hStep)->szData);
    }

    HeapFree( g_hHeap, 0, (PYGSTEPDATA)hStep);
}    

int UTL_Strlen (const char* lpString)
{
    int iLength = 0;
    while ( *lpString != 0 ) 
    {
        iLength += 1;
        lpString += 1;
    }
    return(iLength);
}

void LogDetail( const TCHAR* szFormat, ...)
{
    va_list valist;
     
    if(!CritSecInitialized())
        InitCritSec();
        
    EnterCriticalSection(& g_cs);
    va_start( valist, szFormat ); 
    StringCchVPrintf( g_szBuffer, _countof(g_szBuffer), szFormat, valist );
    va_end( valist ); 
    PRINT( TEXT("\t%s\n"), g_szBuffer);

     LeaveCriticalSection( &g_cs);
}

void LogError( const TCHAR* szText)
{
    DWORD dwErrorCode = GetLastError(); 
    LogDetail( TEXT("%s: Failed with Errorcode %ld"), szText, dwErrorCode);
}
 
void LogPrint(const TCHAR *szFormat, ... )
{
    va_list valist; 

    if(!CritSecInitialized())
        InitCritSec();
    
    EnterCriticalSection( &g_cs);
    va_start( valist, szFormat ); 
    StringCchVPrintf( g_szBuffer, _countof(g_szBuffer), szFormat, valist );
    va_end( valist );
    PRINT(g_szBuffer);

     LeaveCriticalSection( &g_cs);
}

void InitHarness()
{
    InitializeCriticalSection( &g_cs);    
    g_dwStartTime = GetTickCount();
}

void DeinitHarness()
{
    DeleteCriticalSection( &g_cs);
    g_dwEndTime = GetTickCount();
}

// Just a stub
BOOL WINAPI DllMain(
    HANDLE    /*hModule*/, 
    DWORD    /*dwReason*/,
    LPVOID    /*lpRes*/
)
{
    return TRUE;
}

