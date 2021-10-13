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
#include <windows.h>
#include <tchar.h>
#include "harness.h"

TCHAR *szTestMarker = TEXT("---------------------------------------------------------------\n");
static TCHAR szBuffer[ 1024 ] = { 0 };

TCHAR **szTestArgv=NULL;
int nTestArgc = 0;
DWORD dwStartTime = 0, dwEndTime = 0;
CRITICAL_SECTION cs = { 0 };

#ifdef __cplusplus
extern "C" {
#endif
    HINSTANCE   g_hInstance = { 0 };
#ifdef __cplusplus
}
#endif

#ifdef STANDALONE
DWORD dwFailTotal=0;
DWORD dwPassTotal=0;
DWORD dwTotalTests=0;
BOOL  *bRunTest= NULL;
DWORD dwRandomSeed=0;
#else
HKATO hKato;
#endif

#ifdef UNDER_CE
HANDLE hPPSH=NULL;
#endif

HANDLE hHeap=NULL;
//******************************************************************************
void CreateLog(LPTSTR szLogName)
{
    TCHAR szLogFile[MAX_PATH] = { 0 };

    InitHarness();
    hHeap = HeapCreate( HEAP_NO_SERIALIZE, 4096, 32768);
    wsprintf( szLogFile, TEXT("%s.LOG"), szLogName);
    
#ifdef STANDALONE
#else
    hKato = KatoCreate( szLogName);
#endif
#ifdef UNDER_CE
    hPPSH = PPSH_OpenStream( szLogFile, MODE_WRITE);
#endif

}

void CloseLog()
{
    HeapDestroy( hHeap);
#ifdef STANDALONE
#else
    KatoDestroy( hKato);
#endif
#ifdef UNDER_CE
    PPSH_CloseStream( hPPSH);
#endif
    DeinitHarness();
}

HANDLE StartTest( LPTSTR szText)
{
    PYGTESTDATA pTestData = NULL;
    MEMORYSTATUS ms = { 0 };


    pTestData = (PYGTESTDATA)HeapAlloc( hHeap, HEAP_ZERO_MEMORY, sizeof(YGTESTDATA));
    pTestData->bSuccess = TRUE;
    pTestData->szData = szText ? (LPTSTR)HeapAlloc( hHeap, 0, (_tcslen( szText)+1)*sizeof(TCHAR)) : NULL;
    ms.dwLength = sizeof(MEMORYSTATUS);
    GlobalMemoryStatus( &ms);
    pTestData->dwPhysStart = ms.dwAvailPhys;
    if (szText)  {
        _tcscpy( pTestData->szData, szText);
        LogPrint(  TEXT("START: %s\n"), szText);
    }
    return (HANDLE)pTestData;
}

BOOL FinishTest(HANDLE hTest)
{
    BOOL bSuccess = FALSE;
    PYGTESTDATA pTestData = NULL;
    MEMORYSTATUS ms = { 0 };

    pTestData = (PYGTESTDATA)hTest;
    bSuccess = pTestData->bSuccess;
#ifdef STANDALONE
    if (bSuccess) {
        dwPassTotal++;      
    } else {
        dwFailTotal++;
    }
    LogPrint( TEXT("\tNumber of steps = %ld "), pTestData->dwStepsStarted);
    LogPrint( TEXT("Steps Completed = %ld "), pTestData->dwStepsCompleted);
    LogPrint( TEXT("Steps Skipped   = %ld\r\n"), pTestData->dwStepsSkipped);
#endif

    if (((PYGTESTDATA)hTest)->szData) {
        LogPrint( TEXT("%-5s: %s\n"), bSuccess ? TEXT("PASS") : TEXT("FAIL"), ((PYGTESTDATA)hTest)->szData);
        HeapFree( hHeap, 0, ((PYGTESTDATA)hTest)->szData);
    }
    ms.dwLength = sizeof(MEMORYSTATUS);
    GlobalMemoryStatus( &ms);
    LogPrint( TEXT("Memory Difference after running test = %ld\r\n"), pTestData->dwPhysStart-ms.dwAvailPhys);
    LogPrint( szTestMarker);
    HeapFree( hHeap, 0, (PYGTESTDATA)hTest);
    return bSuccess;
}

void FailTest( HANDLE hTest)
{
    ((PYGTESTDATA)hTest)->bSuccess = FALSE; 
}

HANDLE BeginStep( HANDLE hTest, LPTSTR szText)  
{
    PYGSTEPDATA pStepData = NULL;

    ((PYGTESTDATA)hTest)->dwStepsStarted++;
    pStepData = (PYGSTEPDATA)HeapAlloc( hHeap, 0, sizeof(YGSTEPDATA));
    pStepData->bSuccess = FALSE;
    pStepData->szData = szText ? (LPTSTR)HeapAlloc( hHeap, 0, (_tcslen( szText)+1)*sizeof(TCHAR)) : NULL;
    if (szText)  {
        _tcscpy( pStepData->szData, szText);
        LogPrint(  TEXT("BEGIN: %s\r\n"), szText);
    }
    return (HANDLE)pStepData;
}

void SkipStep( HANDLE hTest, HANDLE hStep)  
{                              
    
    ((PYGTESTDATA)hTest)->dwStepsSkipped;


    if (((PYGSTEPDATA)hStep)->szData)  {
        LogPrint( TEXT(" SKIP: %s\n"), ((PYGSTEPDATA)hStep)->szData);
        HeapFree( hHeap, 0, ((PYGSTEPDATA)hStep)->szData);
    }
    HeapFree( hHeap, 0, (PYGSTEPDATA)hStep);
    return;
}

void PassStep( HANDLE hStep)  
{
  ((PYGSTEPDATA)hStep)->bSuccess = TRUE;    
}

void EndStep( HANDLE hTest, HANDLE hStep)
{
    BOOL bSuccess = FALSE;
 
    ((PYGTESTDATA)hTest)->dwStepsCompleted++;
    bSuccess = ((PYGSTEPDATA)hStep)->bSuccess;

    if (!bSuccess) FailTest( hTest);

    if (((PYGSTEPDATA)hStep)->szData)  {
        LogPrint( TEXT("%-5s STEP: %s\r\n"), bSuccess ? TEXT("PASS") : TEXT("FAIL"), ((PYGSTEPDATA)hStep)->szData);
        HeapFree( hHeap, 0, ((PYGSTEPDATA)hStep)->szData);
    }
    HeapFree( hHeap, 0, (PYGSTEPDATA)hStep);
}   


void LogDetail( LPTSTR szFormat, ...)
{
    DWORD gle=GetLastError();
    va_list valist;

    EnterCriticalSection(& cs);
    va_start( valist, szFormat );
    wvsprintf( szBuffer, szFormat, valist );
    va_end( valist );


#ifndef STANDALONE
    KatoComment( hKato, 0, szBuffer);
#else
    PRINT( TEXT("\t%s\n"), szBuffer);
#endif
#ifdef UNDER_CE    
    wcstombs( (char *)szBuffer, szBuffer, _tcslen(szBuffer));   
    PPSH_WriteStream( hPPSH, "\t", 1);
    PPSH_WriteStream( hPPSH, szBuffer, _tcslen(szBuffer));
    PPSH_WriteStream( hPPSH, "\n", 1);
#endif
    LeaveCriticalSection( &cs);
    SetLastError(gle);
}

void LogError( LPTSTR szText)
{
    DWORD dwErrorCode = GetLastError();     
    LogDetail( TEXT("%s: Failed with Errorcode %ld\r\n"), szText, dwErrorCode);
    SetLastError(dwErrorCode);
}

//******************************************************************************
void LogPrint(TCHAR *szFormat, ... )
{
    va_list valist;

    EnterCriticalSection( &cs);
    va_start( valist, szFormat );
    wvsprintf( szBuffer, szFormat, valist );
    va_end( valist );

    PRINT(szBuffer);
#ifdef UNDER_CE
    wcstombs( (char *)szBuffer, szBuffer, _tcslen(szBuffer));
    PPSH_WriteStream( hPPSH, szBuffer, _tcslen(szBuffer));
#endif
    LeaveCriticalSection( &cs);
}

void InitHarness()
{
    InitializeCriticalSection( &cs);
    dwStartTime = GetTickCount();
}

void DeinitHarness()
{
    DeleteCriticalSection( &cs);
    dwEndTime = GetTickCount();
}

void LogMemory()
{
    MEMORYSTATUS ms = { 0 };
    
    ms.dwLength = sizeof( MEMORYSTATUS);
    GlobalMemoryStatus( &ms);
    
    LogDetail( TEXT("Total Physical = %ld Available Physical = %ld\r\n"), ms.dwTotalPhys, ms.dwAvailPhys);
}   

