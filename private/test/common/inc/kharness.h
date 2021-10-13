//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
//
/*****************************************************************************

  KHarness.h - header file for kernel test harness

*****************************************************************************/

#pragma once

#include <tchar.h>
#include <tux.h>

#ifdef CONSOLE
#include <stdio.h>
#endif
#ifndef STANDALONE
#include <kato.h>
#endif

typedef struct _YGTESTDATA {
    BOOL    bSuccess;
    DWORD   dwStepsStarted;
    DWORD   dwStepsCompleted;
    DWORD   dwStepsSkipped;
    DWORD   dwStartTick;
    DWORD   dwStartPMem;
    DWORD   dwStartVMem;
    LPTSTR  szData;
} YGTESTDATA, *PYGTESTDATA;

typedef struct _YGSTEPDATA {
    BOOL   bSuccess;
    LPTSTR szData;
} YGSTEPDATA, *PYGSTEPDATA;

void    CreateLog( LPTSTR szLogName);
void    CloseLog();
void    InitHarness();
void    DeinitHarness();
HANDLE  StartTest( LPTSTR szText);
BOOL    FinishTest(HANDLE hTest);
void    PassStep( HANDLE hStep);
void    EndStep( HANDLE hTest, HANDLE hStep);
HANDLE  BeginStep( HANDLE hTest, LPTSTR szText);
void    SkipStep( HANDLE hTest, HANDLE hStep);
void    LogDetail( LPTSTR szFormat, ...);
void    LogPrint( LPTSTR szFormat, ...);
void    FailTest( HANDLE hTest);
void    LogError( LPTSTR szText);
LPTSTR* CommandLineToArgv (LPCTSTR lpCmdLine, int*pNumArgs);
void    LogMemory();

#ifdef CONSOLE
#define PRINT _tprintf
#else
#define PRINT NKDbgPrintfW
#endif

SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam);

#define BEGINSTEP(hTest,hStep,szText)  \
                                hStep = BeginStep(hTest, szText); \
                                if ( ((PYGTESTDATA)(hTest))->bSuccess) { \
                                    do {

#define ENDSTEP(hTest,hStep) \
                                        PassStep( hStep);   \
                                    } while(FALSE); \
                                    EndStep( hTest, hStep); \
                                } else { \
                                    SkipStep( hTest, hStep); \
                                }

#define BEGINSTEP_FORCE(hTest, hStep, szTest) \
                                hStep = BeginStep( hTest, szTest); \
                                    do {

#define ENDSTEP_FORCE(hTest, hStep) \
                                        PassStep(hStep); \
                                    } while (FALSE); \
                                EndStep(hTest, hStep)


#define CHECK CHECKTRUE
#define CHECKTRUE(s) if ( !(s) ) { \
                        LogDetail( TEXT("CHECKTRUE( %s ) failed at line %ld (tid=0x%x, GLE=0x%x)\r\n"),TEXT(#s),__LINE__, GetCurrentThreadId(), GetLastError() ); \
                        break; \
                     }
#define CHECKFALSE(s) if ( (s) ) {  \
                        LogDetail( TEXT("CHECKFALSE( %s ) failed at line %ld (tid=0x%x, GLE=0x%x)\r\n"),TEXT(#s),__LINE__, GetCurrentThreadId(), GetLastError() ); \
                        break; \
                      }
#define CHECKTRUE1(msg,s) if (!(s)) { \
                        LogDetail( TEXT("CHECKTRUE1( %s ) failed at line %ld (tid=0x%x, GLE=0x%x\r\n"), TEXT(#s),__LINE__, GetCurrentThreadId(), GetLastError() ); \
                        LogDetail( TEXT("==> %s\r\n"),msg ); \
                        break; \
                      }
#define CHECKFALSE1(msg,s) if (s) { \
                        LogDetail( TEXT("CHECKFALSE1( %s ) failed at line %ld (tid=0x%x, GLE=0x%x\r\n"), TEXT(#s),__LINE__, GetCurrentThreadId(), GetLastError() ); \
                        LogDetail( TEXT("%s"), msg); \
                        break; \
                      }


#define CHECKHANDLE( h ) CHECKTRUE( (h) && (h)!=INVALID_HANDLE_VALUE )
#define CHECKBADHANDLE( h ) CHECKTRUE (h==NULL || h==INVALID_HANDLE_VALUE )
