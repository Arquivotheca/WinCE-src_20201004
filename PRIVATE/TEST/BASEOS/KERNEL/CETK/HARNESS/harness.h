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
#ifndef __HARNESS_H__
#define __HARNESS_H__

#ifdef CONSOLE
#include <stdio.h>
#endif
#include <tchar.h>
#ifndef STANDALONE
#include <tux.h>
#include <kato.h>
#else
// Forward declaration of LPFUNCTION_TABLE_ENTRY
typedef struct _FUNCTION_TABLE_ENTRY *LPFUNCTION_TABLE_ENTRY;

// Define our ShellProc Param and TestProc Param types
typedef LPARAM  SPPARAM;
typedef LPDWORD TPPARAM;

// Shell and Test message handling procs
typedef INT (WINAPI *SHELLPROC)(UINT uMsg, SPPARAM spParam);
typedef INT (WINAPI *TESTPROC )(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE);

// SHELLPROCAPI and TESTPROCAPI
#ifdef __cplusplus
#define SHELLPROCAPI extern "C" INT __declspec(dllexport) WINAPI
#else
#define SHELLPROCAPI INT __declspec(dllexport) WINAPI
#endif
#define TESTPROCAPI  INT WINAPI

//******************************************************************************
//***** Function Table Entry Structure
//******************************************************************************

typedef struct _FUNCTION_TABLE_ENTRY {
   LPCTSTR  lpDescription; // description of test
   UINT     uDepth;        // depth of item in tree hierarchy
   DWORD    dwUserData;    // user defined data that will be passed to TestProc at runtime
   DWORD    dwUniqueID;    // uniquely identifies the test - used in loading/saving scripts
   TESTPROC lpTestProc;    // pointer to TestProc function to be called for this test
} FUNCTION_TABLE_ENTRY, *LPFUNCTION_TABLE_ENTRY;

//******************************************************************************
//***** ShellProc() Message values
//******************************************************************************

#define SPM_LOAD_DLL               1
#define SPM_UNLOAD_DLL             2
#define SPM_START_SCRIPT           3
#define SPM_STOP_SCRIPT            4
#define SPM_BEGIN_GROUP            5
#define SPM_END_GROUP              6
#define SPM_SHELL_INFO             7
#define SPM_REGISTER               8
#define SPM_EXCEPTION              9
#define SPM_BEGIN_TEST            10
#define SPM_END_TEST              11

//******************************************************************************
//***** ShellProc() Return values
//******************************************************************************

#define SPR_NOT_HANDLED            0  
#define SPR_HANDLED                1
#define SPR_SKIP                   2
#define SPR_FAIL                   3

//******************************************************************************
//***** ShellProc() Flag values
//******************************************************************************

#define SPF_UNICODE       0x00010000

//******************************************************************************
//***** TestProc() Message values
//******************************************************************************

#define TPM_EXECUTE              101
#define TPM_QUERY_THREAD_COUNT   102

//******************************************************************************
//***** TestProc() Return values
//******************************************************************************

#define TPR_SKIP                   2
#define TPR_PASS                   3
#define TPR_FAIL                   4
#define TPR_ABORT                  5

//******************************************************************************
//***** ShellProc() Structures
//******************************************************************************

// ShellProc() Structure for SPM_SHELL_INFO message
typedef struct _SPS_SHELL_INFO {
   HINSTANCE hInstance;     // Instance handle of shell.
   HWND      hWnd;          // Main window handle of shell (currently set to NULL).
   HINSTANCE hLib;          // Test Dll instance handle.
   HANDLE    hevmTerminate; // Manual event that is set by Tux to inform all
                            // tests to shutdown (currently not used).
   BOOL      fUsingServer;  // Set if Tux is connected to Tux Server.
} SPS_SHELL_INFO, *LPSPS_SHELL_INFO;

// ShellProc() Structure for SPM_REGISTER message
typedef struct _SPS_REGISTER {
   LPFUNCTION_TABLE_ENTRY lpFunctionTable;
} SPS_REGISTER, *LPSPS_REGISTER;

// ShellProc() Structure for SPM_BEGIN_TEST message
typedef struct _SPS_BEGIN_TEST {
   LPFUNCTION_TABLE_ENTRY lpFTE;
   DWORD                  dwRandomSeed;
   DWORD                  dwThreadCount;
} SPS_BEGIN_TEST, *LPSPS_BEGIN_TEST;

// ShellProc() Structure for SPM_END_TEST message
typedef struct _SPS_END_TEST {
   LPFUNCTION_TABLE_ENTRY lpFTE;
   DWORD                  dwResult;
   DWORD                  dwRandomSeed;
   DWORD                  dwThreadCount;
   DWORD                  dwExecutionTime;
} SPS_END_TEST, *LPSPS_END_TEST;

//******************************************************************************
//***** TestProc() Structures
//******************************************************************************

// TestProc() Structure for TPM_EXECUTE message
typedef struct _TPS_EXECUTE {
   DWORD dwRandomSeed;
   DWORD dwThreadCount;
   DWORD dwThreadNumber;
} TPS_EXECUTE, *LPTPS_EXECUTE;

// TestProc() Structure for TPM_QUERY_THREAD_COUNT message
typedef struct _TPS_QUERY_THREAD_COUNT {
   DWORD dwThreadCount;
} TPS_QUERY_THREAD_COUNT, *LPTPS_QUERY_THREAD_COUNT;

//******************************************************************************
//***** Old constants defined for compatibility - DO NOT USE THESE CONSTNATS
//******************************************************************************

#define TPR_NOT_HANDLED   0
#define TPR_HANDLED       1
#define SPM_START_TESTS   SPM_BEGIN_GROUP
#define SPM_STOP_TESTS    SPM_END_GROUP
#define SHELLINFO         SPS_SHELL_INFO
#define LPSHELLINFO       LPSPS_SHELL_INFO

#endif

typedef struct _YGTESTDATA {
    DWORD dwStepsStarted;
    DWORD dwStepsCompleted;
    DWORD dwStepsSkipped;
    LPTSTR szData;
    BOOL   bSuccess;
    DWORD dwPhysStart;
} YGTESTDATA, *PYGTESTDATA;

typedef struct _YGSTEPDATA {
    LPTSTR szData;
    BOOL   bSuccess;
} YGSTEPDATA, *PYGSTEPDATA;

void InitHarness();
void DeinitHarness();
void CreateLog( LPTSTR szLogName);
void CloseLog();
HANDLE StartTest( LPTSTR szText);
BOOL   FinishTest(HANDLE hTest);
void PassStep( HANDLE hStep);
void EndStep( HANDLE hTest, HANDLE hStep);
HANDLE BeginStep( HANDLE hTest, LPTSTR szText);
void SkipStep( HANDLE hTest, HANDLE hStep);
void LogDetail( LPTSTR szFormat, ...);
void LogPrint( LPTSTR szFormat, ...);
void FailTest( HANDLE hTest);
void LogError( LPTSTR szText);
void YGComment( LPTSTR szText);
BOOL CopyFromPPSH( LPTSTR szSource, LPTSTR szDestination);


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
                          
#define CHECK CHECKTRUE
#if 1
// #ifdef DEBUG
#define CHECKTRUE(s) if (!(s)) { \
                        LogDetail( TEXT("CHECKTRUE failed at line %ld\r\n"),__LINE__); \
                        break; \
                     }
#define CHECKFALSE(s) if (s) { \
                        LogDetail( TEXT("CHECKFALSE failed at line %ld\r\n"),__LINE__); \
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
LPTSTR* CommandLineToArgv (LPCTSTR lpCmdLine, int*pNumArgs);

#ifdef UNDER_CE
#define MODE_WRITE          0x90001
#define MODE_READ           0x90002
#define MODE_READWRITE      0x90003

#define PPSH_OpenStream(s,m)       (HANDLE)U_ropen((s), m)
#define PPSH_CloseStream(h)      U_rclose( (int) h)
#define PPSH_WriteStream(s,b,l)  (BOOL)(U_rwrite( (int)(s), (unsigned char *)(b), (l)))
#define PPSH_ReadStream(s,b,l)   (int)(U_rread( (int)(s), (unsigned char *)(b), (l) ))
#endif

void LogMemory();


#endif // __HARNESS_H__
