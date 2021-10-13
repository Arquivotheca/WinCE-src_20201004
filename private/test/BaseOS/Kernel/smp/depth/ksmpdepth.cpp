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
#include <windows.h>
#include <tchar.h>
#include "kharness.h"
#include "ksmpdepth.h"

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO ssi;


//******************************************************************************
//***** ShellProc()
//******************************************************************************

SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam) {

   switch (uMsg) {

      case SPM_LOAD_DLL: {
         // Sent once to the DLL immediately after it is loaded.  The DLL may
         // return SPR_FAIL to prevent the DLL from continuing to load.
         PRINT(TEXT("ShellProc(SPM_LOAD_DLL, ...) called\r\n"));
         CreateLog( TEXT("ksmpbvt"));
         return SPR_HANDLED;
      }

      case SPM_UNLOAD_DLL: {
         // Sent once to the DLL immediately before it is unloaded.
         PRINT(TEXT("ShellProc(SPM_UNLOAD_DLL, ...) called\r\n"));
         CloseLog();
         return SPR_HANDLED;
      }

      case SPM_SHELL_INFO: {
         // Sent once to the DLL immediately after SPM_LOAD_DLL to give the
         // DLL some useful information about its parent shell.  The spParam
         // parameter will contain a pointer to a SPS_SHELL_INFO structure.
         // This pointer is only temporary and should not be referenced after
         // the processing of this message.  A copy of the structure should be
         // stored if there is a need for this information in the future.
         // PRINT(TEXT("ShellProc(SPM_SHELL_INFO, ...) called\r\n"));
         ssi = *(LPSPS_SHELL_INFO)spParam;
         return SPR_HANDLED;
      }

      case SPM_REGISTER: {
         // This is the only ShellProc() message that a DLL is required to
         // handle.  This message is sent once to the DLL immediately after
         // the SPM_SHELL_INFO message to query the DLL for it’s function table.
         // The spParam will contain a pointer to a SPS_REGISTER structure.
         // The DLL should store its function table in the lpFunctionTable
         // member of the SPS_REGISTER structure return SPR_HANDLED.  If the
         // function table contains UNICODE strings then the SPF_UNICODE flag
         // must be OR'ed with the return value; i.e. SPR_HANDLED | SPF_UNICODE
         // MyPrintf(TEXT("ShellProc(SPM_REGISTER, ...) called\r\n"));
         ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
         #ifdef UNICODE
            return SPR_HANDLED | SPF_UNICODE;
         #else
            return SPR_HANDLED;
         #endif
      }

      case SPM_START_SCRIPT: {
         // Sent to the DLL immediately before a script is started.  It is
         // sent to all DLLs, including loaded DLLs that are not in the script.
         // All DLLs will receive this message before the first TestProc() in
         // the script is called.
         // MyPrintf(TEXT("ShellProc(SPM_START_SCRIPT, ...) called\r\n"));
         return SPR_HANDLED;
      }

      case SPM_STOP_SCRIPT: {
         // Sent to the DLL when the script has stopped.  This message is sent
         // when the script reaches its end, or because the user pressed
         // stopped prior to the end of the script.  This message is sent to
         // all DLLs, including loaded DLLs that are not in the script.
         // MyPrintf(TEXT("ShellProc(SPM_STOP_SCRIPT, ...) called\r\n"));
         return SPR_HANDLED;
      }

      case SPM_BEGIN_GROUP: {
         // Sent to the DLL before a group of tests from that DLL is about to
         // be executed.  This gives the DLL a time to initialize or allocate
         // data for the tests to follow.  Only the DLL that is next to run
         // receives this message.  The prior DLL, if any, will first receive
         // a SPM_END_GROUP message.  For global initialization and
         // de-initialization, the DLL should probably use SPM_START_SCRIPT
         // and SPM_STOP_SCRIPT, or even SPM_LOAD_DLL and SPM_UNLOAD_DLL.
         // MyPrintf(TEXT("ShellProc(SPM_BEGIN_GROUP, ...) called\r\n"));
         return SPR_HANDLED;
      }

      case SPM_END_GROUP: {
         // Sent to the DLL after a group of tests from that DLL has completed
         // running.  This gives the DLL a time to cleanup after it has been
         // run.  This message does not mean that the DLL will not be called
         // again; it just means that the next test to run belongs to a
         // different DLL.  SPM_BEGIN_GROUP and SPM_END_GROUP allow the DLL
         // to track when it is active and when it is not active.
         // MyPrintf(TEXT("ShellProc(SPM_END_GROUP, ...) called\r\n"));
         return SPR_HANDLED;
      }

      case SPM_BEGIN_TEST: {
         // Sent to the DLL immediately before a test executes.  This gives
         // the DLL a chance to perform any common action that occurs at the
         // beginning of each test, such as entering a new logging level.
         // The spParam parameter will contain a pointer to a SPS_BEGIN_TEST
         // structure, which contains the function table entry and some other
         // useful information for the next test to execute.  If the ShellProc
         // function returns SPR_SKIP, then the test case will not execute.
         // MyPrintf(TEXT("ShellProc(SPM_BEGIN_TEST, ...) called\r\n"));
         LPSPS_BEGIN_TEST pBT = (LPSPS_BEGIN_TEST)spParam;
         return SPR_HANDLED;
      }

      case SPM_END_TEST: {
         // Sent to the DLL after a single test executes from the DLL.
         // This gives the DLL a time perform any common action that occurs at
         // the completion of each test, such as exiting the current logging
         // level.  The spParam parameter will contain a pointer to a
         // SPS_END_TEST structure, which contains the function table entry and
         // some other useful information for the test that just completed.
         // MyPrintf(TEXT("ShellProc(SPM_END_TEST, ...) called\r\n"));
         LPSPS_END_TEST pET = (LPSPS_END_TEST)spParam;

         return SPR_HANDLED;
      }

      case SPM_EXCEPTION: {
         // Sent to the DLL whenever code execution in the DLL causes and
         // exception fault.  TUX traps all exceptions that occur while
         // executing code inside a test DLL.
         // MyPrintf(TEXT("ShellProc(SPM_EXCEPTION, ...) called\r\n"));
         return SPR_HANDLED;
      }
   }

   return SPR_NOT_HANDLED;
}
