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
#ifndef __HARNESS_H__
#define __HARNESS_H__

#include <tchar.h>
#include <tux.h>
#include <kato.h>

typedef struct _YGTESTDATA {
    DWORD dwStepsStarted;
    DWORD dwStepsCompleted;
    DWORD dwStepsSkipped;
    LPTSTR szData;
    BOOL   bSuccess;
} YGTESTDATA, *PYGTESTDATA;

typedef struct _YGSTEPDATA {
    LPTSTR szData;
    BOOL   bSuccess;
} YGSTEPDATA, *PYGSTEPDATA;

void CreateLog( const TCHAR* szLogName);
void CloseLog();
void InitHarness();
void DeinitHarness();
HANDLE StartTest( const TCHAR* szText);
BOOL FinishTest(HANDLE hTest);
void PassStep( HANDLE hStep);
void EndStep( HANDLE hTest, HANDLE hStep);
HANDLE BeginStep( HANDLE hTest, const TCHAR* szText);
void SkipStep( HANDLE hTest, HANDLE hStep);
void LogDetail( const TCHAR* szFormat, ...);
void LogPrint( const TCHAR* szFormat, ...);
void FailTest( HANDLE hTest);
void LogError( const TCHAR* szText);

#define PRINT NKDbgPrintfW

SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam);

#define BEGINSTEP(hTest,hStep,szText)  \
                                hStep = BeginStep(hTest, szText); \
                                if ( ((PYGTESTDATA)(hTest))->bSuccess) { \
                                    for(;;) {        
#define ENDSTEP(hTest,hStep) \
                                        PassStep( hStep);   \
                                        break; \
                                    } \
                                    EndStep( hTest, hStep);    \
                                } else { \
                                    SkipStep( hTest, hStep); \
                                }
                          
#define CHECK CHECKTRUE
#ifdef DEBUG
#define CHECKTRUE(s) if (!(s)) { \
                        LogDetail( TEXT("CHECKTRUE failed at line %ld"),__LINE__); \
                        break; \
                     }
#define CHECKFALSE(s) if (s) {    \
                        LogDetail( TEXT("CHECKFALSE failed at line %ld"),__LINE__); \
                        break; \
                      }
#else
#define CHECKTRUE(s) if (!(s)) break;
#define CHECKFALSE(s) if (s) break;
#endif

#define CHECKTRUE1(msg,s) if (!(s)) { \
                        LogDetail( TEXT("%s"), msg); \
                        break; \
                      }
#define CHECKFALSE1(msg,s) if (s) { \
                        LogDetail( TEXT("%s"), msg); \
                        break; \
                      }
#define CHECKTRUE2(msg,arg,s) if (!(s)) { \
                        LogDetail( msg, arg); \
                        break; \
                      }
#define CHECKFALSE2(msg,arg,s) if (s) { \
                        LogDetail( msg, arg); \
                        break; \
                      }
#define CHECKTRUE3(msg,arg1,arg2,s) if (!(s)) { \
                        LogDetail( msg, arg1, arg2); \
                        break; \
                      }
#define CHECKFALSE3(msg,arg1,arg2,s) if (s) { \
                        LogDetail( msg, arg1, arg2, arg3); \
                        break; \
                      }
#define CHECKTRUE4(msg,arg1,arg2,arg3, s) if (!(s)) { \
                        LogDetail( msg, arg1, arg2, arg3); \
                        break; \
                      }
#define CHECKFALSE4(msg,arg1,arg2,arg3,s) if (s) { \
                        LogDetail( msg, arg1, arg2, arg3); \
                        break; \
                      }

#endif // __HARNESS_H__
