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
#include <wininet.h>
#include <tchar.h>
#include <katoex.h>
#include <tux.h>
#include "tuxstuff.h"
#include <Pkfuncs.h>
#include <Kfuncs.h>

#ifndef UNDER_CE
#include <stdio.h>
#endif

// Global CKato logging object.  Set while processing SPM_LOAD_DLL message.
CKato *g_pKato = NULL;

// Global shell info structure.  Set while processing SPM_SHELL_INFO message.
SPS_SHELL_INFO *g_pShellInfo;

// Global critical section to be used by threaded tests if necessary.
CRITICAL_SECTION g_csProcess;

//******************************************************************************
//***** Windows CE specific code
//******************************************************************************
#ifdef UNDER_CE

#ifndef STARTF_USESIZE
#define STARTF_USESIZE     0x00000002
#endif

#ifndef STARTF_USEPOSITION
#define STARTF_USEPOSITION 0x00000004
#endif

#ifndef ZeroMemory
#define ZeroMemory(Destination,Length) memset(Destination, 0, Length)
#endif

#ifndef _vsntprintf
#define _vsntprintf(d,c,f,a) wvsprintf(d,f,a)
#endif

BOOL WINAPI DllMain(HANDLE hInstance, ULONG dwReason, LPVOID lpReserved) {
   return TRUE;
}
#endif

//******************************************************************************
VOID Debug(LPCTSTR szFormat, ...) {
   TCHAR szBuffer[1024] = TEXT("WinInetStress: ");

   va_list pArgs;
   va_start(pArgs, szFormat);
   _vsntprintf(szBuffer + 9, countof(szBuffer) - 11, szFormat, pArgs);
   va_end(pArgs);

   _tcscat(szBuffer, TEXT("\r\n"));

   OutputDebugString(szBuffer);
}

//******************************************************************************
//***** ShellProc()
//******************************************************************************

SHELLPROCAPI ShellProc(UINT uMsg, SPPARAM spParam) {

   switch (uMsg) {

      //------------------------------------------------------------------------
      // Message: SPM_LOAD_DLL
      //
      // Sent once to the DLL immediately after it is loaded.  The spParam
      // parameter will contain a pointer to a SPS_LOAD_DLL structure.  The DLL
      // should set the fUnicode member of this structre to TRUE if the DLL is
      // built with the UNICODE flag set.  By setting this flag, Tux will ensure
      // that all strings passed to your DLL will be in UNICODE format, and all
      // strings within your function table will be processed by Tux as UNICODE.
      // The DLL may return SPR_FAIL to prevent the DLL from continuing to load.
      //------------------------------------------------------------------------

      case SPM_LOAD_DLL: {
         Debug(TEXT("ShellProc(SPM_LOAD_DLL, ...) called"));

         // If we are UNICODE, then tell Tux this by setting the following flag.
         #ifdef UNICODE
            ((LPSPS_LOAD_DLL)spParam)->fUnicode = TRUE;
         #endif

         // Get/Create our global logging object.
         g_pKato = (CKato*)KatoGetDefaultObject();

         // Initialize our global critical section.
         InitializeCriticalSection(&g_csProcess);

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_UNLOAD_DLL
      //
      // Sent once to the DLL immediately before it is unloaded.
      //------------------------------------------------------------------------

      case SPM_UNLOAD_DLL: {
         Debug(TEXT("ShellProc(SPM_UNLOAD_DLL, ...) called"));

         // This is a good place to delete our global critical section.
         DeleteCriticalSection(&g_csProcess);

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_SHELL_INFO
      //
      // Sent once to the DLL immediately after SPM_LOAD_DLL to give the DLL
      // some useful information about its parent shell and environment.  The
      // spParam parameter will contain a pointer to a SPS_SHELL_INFO structure.
      // The pointer to the structure may be stored for later use as it will
      // remain valid for the life of this Tux Dll.  The DLL may return SPR_FAIL
      // to prevent the DLL from continuing to load.
      //------------------------------------------------------------------------

      case SPM_SHELL_INFO: {
         Debug(TEXT("ShellProc(SPM_SHELL_INFO, ...) called"));

         // Store a pointer to our shell info for later use.
         g_pShellInfo = (LPSPS_SHELL_INFO)spParam;

         // Display our Dlls command line if we have one.
         if (g_pShellInfo->szDllCmdLine && *g_pShellInfo->szDllCmdLine) {
#ifdef UNDER_CE
            Debug(TEXT("Command Line: \"%s\"."), g_pShellInfo->szDllCmdLine);
#else
            MessageBox(g_pShellInfo->hWnd, g_pShellInfo->szDllCmdLine,
                       TEXT("wininetstress.DLL Command Line Arguments"), MB_OK);
#endif UNDER_CE
         }

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_REGISTER
      //
      // This is the only ShellProc() message that a DLL is required to handle
      // (except for SPM_LOAD_DLL if you are UNICODE).  This message is sent
      // once to the DLL immediately after the SPM_SHELL_INFO message to query
      // the DLL for it's function table.  The spParam will contain a pointer to
      // a SPS_REGISTER structure.  The DLL should store its function table in
      // the lpFunctionTable member of the SPS_REGISTER structure.  The DLL may
      // return SPR_FAIL to prevent the DLL from continuing to load.
      //------------------------------------------------------------------------

      case SPM_REGISTER: {
         Debug(TEXT("ShellProc(SPM_REGISTER, ...) called"));
         ((LPSPS_REGISTER)spParam)->lpFunctionTable = g_lpFTE;
         #ifdef UNICODE
            return SPR_HANDLED | SPF_UNICODE;
         #else
            return SPR_HANDLED;
         #endif
      }

      //------------------------------------------------------------------------
      // Message: SPM_START_SCRIPT
      //
      // Sent to the DLL immediately before a script is started.  It is sent to
      // all Tux DLLs, including loaded Tux DLLs that are not in the script.
      // All DLLs will receive this message before the first TestProc() in the
      // script is called.
      //------------------------------------------------------------------------

      case SPM_START_SCRIPT: {
         Debug(TEXT("ShellProc(SPM_START_SCRIPT, ...) called"));
         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_STOP_SCRIPT
      //
      // Sent to the DLL when the script has stopped.  This message is sent when
      // the script reaches its end, or because the user pressed stopped prior
      // to the end of the script.  This message is sent to all Tux DLLs,
      // including loaded Tux DLLs that are not in the script.
      //------------------------------------------------------------------------

      case SPM_STOP_SCRIPT: {
         Debug(TEXT("ShellProc(SPM_STOP_SCRIPT, ...) called"));
         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_BEGIN_DLL
      //
      // Sent to the DLL before a group of tests from that DLL is about to be
      // executed.  This gives the DLL a time to initialize or allocate data for
      // the tests to follow.  Only the DLL that is next to run receives this
      // message.  The prior DLL, if any, will first receive a SPM_END_DLL
      // message.  For global initialization and de-initialization, the DLL
      // should probably use SPM_START_SCRIPT and SPM_STOP_SCRIPT, or even
      // SPM_LOAD_DLL and SPM_UNLOAD_DLL.
      //
      // The DLL may return SPR_FAIL to cause all tests in this dll to be
      // skipped.
      //------------------------------------------------------------------------

      case SPM_BEGIN_DLL: {
         Debug(TEXT("ShellProc(SPM_BEGIN_DLL, ...) called"));
         g_pKato->BeginLevel(0, TEXT("BEGIN DLL: wininetstress.DLL"));
         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_END_DLL
      //
      // Sent to the DLL after a group of tests from that DLL has completed
      // running.  This gives the DLL a time to cleanup after it has been run.
      // This message does not mean that the DLL will not be called again to run
      // tests; it just means that the next test to run belongs to a different
      // DLL.  SPM_BEGIN_DLL and SPM_END_DLL allow the DLL to track when it
      // is active and when it is not active.
      //------------------------------------------------------------------------

      case SPM_END_DLL: {
         Debug(TEXT("ShellProc(SPM_END_DLL, ...) called"));
         g_pKato->EndLevel(TEXT("END DLL: wininetstress.DLL"));
         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_BEGIN_TEST
      //
      // Sent to the DLL immediately before a test executes.  This gives the DLL
      // a chance to perform any common action that occurs at the beginning of
      // each test, such as entering a new logging level.  The spParam parameter
      // will contain a pointer to a SPS_BEGIN_TEST structure, which contains
      // the function table entry and some other useful information for the next
      // test to execute.  If the ShellProc function returns SPR_SKIP, then the
      // test case will not execute.
      //------------------------------------------------------------------------

      case SPM_BEGIN_TEST: {
         Debug(TEXT("ShellProc(SPM_BEGIN_TEST, ...) called"));

         // Start our logging level.
         LPSPS_BEGIN_TEST pBT = (LPSPS_BEGIN_TEST)spParam;
         g_pKato->BeginLevel(pBT->lpFTE->dwUniqueID,
                             TEXT("BEGIN TEST: \"%s\", Threads=%u, Seed=%u"),
                             pBT->lpFTE->lpDescription, pBT->dwThreadCount,
                             pBT->dwRandomSeed);

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_END_TEST
      //
      // Sent to the DLL after a single test executes from the DLL.  This gives
      // the DLL a time perform any common action that occurs at the completion
      // of each test, such as exiting the current logging level.  The spParam
      // parameter will contain a pointer to a SPS_END_TEST structure, which
      // contains the function table entry and some other useful information for
      // the test that just completed.
      //------------------------------------------------------------------------

      case SPM_END_TEST: {
         Debug(TEXT("ShellProc(SPM_END_TEST, ...) called"));

         // End our logging level.
         LPSPS_END_TEST pET = (LPSPS_END_TEST)spParam;
         g_pKato->EndLevel(TEXT("END TEST: \"%s\", %s, Time=%u.%03u"),
                           pET->lpFTE->lpDescription,
                           pET->dwResult == TPR_SKIP ? TEXT("SKIPPED") :
                           pET->dwResult == TPR_PASS ? TEXT("PASSED") :
                           pET->dwResult == TPR_FAIL ? TEXT("FAILED") : TEXT("ABORTED"),
                           pET->dwExecutionTime / 1000, pET->dwExecutionTime % 1000);

         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_EXCEPTION
      //
      // Sent to the DLL whenever code execution in the DLL causes and exception
      // fault.  By default, Tux traps all exceptions that occur while executing
      // code inside a Tux DLL.
      //------------------------------------------------------------------------

      case SPM_EXCEPTION: {
         Debug(TEXT("ShellProc(SPM_EXCEPTION, ...) called"));
         g_pKato->Log(LOG_EXCEPTION, TEXT("Exception occurred!"));
         return SPR_HANDLED;
      }

      //------------------------------------------------------------------------
      // Message: SPM_SETUP_GROUP
      //
      // Sent to the DLL whenever a new group of tests is about to run.
      // A 'group' is defined as a series of tests that are listed after a
      // FUNCTION_TABLE_ENTRY with lpTestProc = NULL.
      //
      // The DLL may return SPR_FAIL to cause all tests in this group to be
      // skipped.
      //------------------------------------------------------------------------

      case SPM_SETUP_GROUP: {
         LPSPS_SETUP_GROUP pSG = (LPSPS_SETUP_GROUP) spParam;
         Debug(TEXT("ShellProc(SPM_SETUP_GROUP, ...) called"));
         break;
      }

      //------------------------------------------------------------------------
      // Message: SPM_TEARDOWN_GROUP
      //
      // Sent to the DLL whenever a group of tests has finished running. See
      // the handler for SPM_SETUP_GROUP for more information about this grouping.
      //
      // The DLL's return value is ignored.
      //------------------------------------------------------------------------

      case SPM_TEARDOWN_GROUP: {
         LPSPS_TEARDOWN_GROUP pTG = (LPSPS_TEARDOWN_GROUP) spParam;
         Debug(TEXT("ShellProc(SPM_TEARDOWN_GROUP, ...) called"));
         // Debug(_T("Results:"));
         // Debug(_T("Passed:  %u"), pTG->dwGroupTestsPassed);
         // Debug(_T("Failed:  %u"), pTG->dwGroupTestsFailed);
         // Debug(_T("Skipped: %u"), pTG->dwGroupTestsSkipped);
         // Debug(_T("Aborted: %u"), pTG->dwGroupTestsAborted);
         // Debug(_T("Time:    %u"), pTG->dwGroupExecutionTime);

         return SPR_HANDLED;
      }
   }

   return SPR_NOT_HANDLED;
}


bool DeleteCacheEntry(TCHAR                         szCompleteUrl[URL_LENGTH], 
                      LPINTERNET_CACHE_ENTRY_INFO   lpCacheEntryInfo,
                      DWORD                         dwCacheEntryInfoBufferSize)
{   
    if (!DeleteUrlCacheEntry(szCompleteUrl))
    {
        g_pKato->Log(LOG_FAIL, TEXT("Deleting the cache entry failed, %d"), GetLastError());
        return FALSE;
    }
    
    dwCacheEntryInfoBufferSize = BUFFER;
    // verify the delete worked; if GetUrlCacheEntryInfo returns TRUE then we enter the if block, which means a FAIL for the test
    if (GetUrlCacheEntryInfo(szCompleteUrl, lpCacheEntryInfo, &dwCacheEntryInfoBufferSize))
    {
        g_pKato->Log(LOG_FAIL, TEXT("The delete operation apparently succeeded, but the entry is still in the cache, %d"), GetLastError());
        return FALSE;
    }
    
    return TRUE;
}


bool GetDeviceName(TCHAR* pszDeviceName, DWORD dwBufLen)
{
//	KernelMode_t kmode;
	DWORD dwBytesReturned, dwIn = SPI_GETBOOTMENAME;

	if (!KernelIoControl(IOCTL_HAL_GET_DEVICE_INFO, &dwIn, sizeof(DWORD), pszDeviceName, dwBufLen * sizeof(TCHAR), &dwBytesReturned))
	{
		return FALSE;
	}

	return TRUE;
}


void HandleCleanup(HINTERNET openHandle, HINTERNET openUrlHandle)
{
        // call InternetCloseHandle to close the handle created by InternetOpenUrl or InternetConnect
        if (openUrlHandle != NULL)
	    {
		    InternetCloseHandle(openUrlHandle);
            g_pKato->Log(LOG_DETAIL, TEXT("Closing InternetOpenUrl or InternetConnect handle"));
	    }

        // call InternetCloseHandle to close the handle created by InternetOpen
	    if (openHandle != NULL)
	    {
		    InternetCloseHandle(openHandle);
            g_pKato->Log(LOG_DETAIL, TEXT("Closing InternetOpen handle"));
	    }
}


int LocalDirectorySetup(TCHAR *localDir) // create a local directory to store test files
{
        if (!CreateDirectory(localDir, NULL))
        {
            if (GetLastError() != ERROR_ALREADY_EXISTS)
            {
                g_pKato->Log(LOG_FAIL, TEXT("Creating the the local directory %s failed, %d"), localDir, GetLastError());
                return 0;
            }
        }

        g_pKato->Log(LOG_DETAIL, TEXT("Local directory %s will be used for storing test files"), localDir);
        return 1;
}


int RemoteDirectorySetup(HINTERNET connectHandle, TCHAR* rootUploadDir, TCHAR* deviceUploadDir, TCHAR* uniqueUploadDir) // create some directories on the server for the upload tests
{
    char            asciiErrorMsgString[MAX_PATH],
                    asciiExpectedErrorContent[MAX_PATH];
    const LPCTSTR   expectedErrorBodyFormat = TEXT("%s%s%s");
    const LPCTSTR   uniqueDirFormat = TEXT("date%d-%d-%d__UTC%d-%d-%d__threadID%x__random%d");
    const TCHAR     expectedErrorBodyEnd[] = TEXT(": Cannot create a file when that file already exists. ");
    const TCHAR     expectedErrorBodyStart[] = TEXT("550 ");
    DWORD           currentThread, errorMsg;
    DWORD           cLen = MAX_PATH;
    DWORD           errorMsgLength = MAX_PATH;
    HRESULT         expectedErrorContentResult,
                    uniqueUploadDirResult;
    int             compareError, 
                    converter, 
                    converterTwo,
                    i,
                    randomNum;
    TCHAR           errorMsgString[MAX_PATH], 
                    expectedErrorContent[MAX_PATH];
    size_t          cchDest = MAX_PATH;
    SYSTEMTIME      timeStamp;
     
    
    //--------------------------------------------------------------------------
    // get the device name   
    if (!GetDeviceName(deviceUploadDir, cLen))
    {
        g_pKato->Log(LOG_FAIL, TEXT("Could not get the device name, %d"), GetLastError());
        return 0;
    }   
    
    //--------------------------------------------------------------------------
    // get into the root upload directory
    if (!FtpSetCurrentDirectory(connectHandle, rootUploadDir))
    {
        g_pKato->Log(LOG_FAIL, TEXT("Changing the directory failed, %d"), GetLastError());
        return 0;
    }

    //--------------------------------------------------------------------------
    // create the device name directory; if it already exists we fail gracefully and continue    
    if (!FtpCreateDirectory(connectHandle, deviceUploadDir))
    {
        // creation failed for some reason; hopefully just because it already existed, but we don't yet know for sure
        g_pKato->Log(LOG_DETAIL, TEXT("Couldn't create device name directory,%d"), GetLastError());
        
        //--------------------------------------------------------------------------
        // get the extended error information - this will be used to determine if the directory already exists        
        if (!InternetGetLastResponseInfo(&errorMsg, errorMsgString, &errorMsgLength))
        {
            g_pKato->Log(LOG_DETAIL, TEXT("Couldn't get extended error information, %d"), GetLastError());
            return 0;
        }
        
        errorMsgString[errorMsgLength - 1] = '\0'; // shorten the error string to just the length of the error message we received
        g_pKato->Log(LOG_DETAIL, TEXT("Extended error information: %s"), errorMsgString);
       
        //--------------------------------------------------------------------------
        // build the string containing the "directory already exists" error message to compare with the real error message       
        expectedErrorContentResult = StringCchPrintf(expectedErrorContent, cchDest, expectedErrorBodyFormat, expectedErrorBodyStart, deviceUploadDir, expectedErrorBodyEnd);
        if (!SUCCEEDED(expectedErrorContentResult))
        {
            g_pKato->Log(LOG_FAIL, TEXT("Could not construct the string containing the expected error message to compare with, %d"), GetLastError());
            return 0;
        }
        g_pKato->Log(LOG_DETAIL, TEXT("Expected Content: %s"), expectedErrorContent);

        //--------------------------------------------------------------------------
        // compare the expected error and the actual error message
        //
        // convert expectedErrorContent to ASCII so the strings are in the same format
        converter = wcstombs(asciiExpectedErrorContent, expectedErrorContent, MAX_PATH);

        // now convert errorMsgString to ASCII
        converterTwo = wcstombs(asciiErrorMsgString, errorMsgString, MAX_PATH);

        // do the actual compare   
        compareError = strcmp(asciiExpectedErrorContent, asciiErrorMsgString);
        
        if (compareError != 0)
        {
            // the errors didn't match, so directory creation failed for some reason other than the directory already exists; end the test
            g_pKato->Log(LOG_DETAIL, TEXT("Error messages did not match. This means directory creation failed for an unexpected reason"));
            return 0;
        }
        g_pKato->Log(LOG_DETAIL, TEXT("Error messages match. The device name directory already exists"));       
    }

    else
    {
        // the device name directory doesn't exist and was created without error
        g_pKato->Log(LOG_DETAIL, TEXT("Created device name directory"));
    }

    //--------------------------------------------------------------------------
    // get the current thread ID to help uniquely identify test run
    currentThread = GetCurrentThreadId();
    
    //--------------------------------------------------------------------------
    // generate random 5-digit number sequence to help uniquely identify test run
    // srand seeds using number of milliseconds since boot; random number from 10th loop is used   
    srand(GetTickCount());    
    for (i = 0; i < 10; i++)
    {
        randomNum = rand();
    }

    //--------------------------------------------------------------------------
    // generate time stamp to help uniquely identify test run
    GetSystemTime(&timeStamp);
  
    //--------------------------------------------------------------------------
    // combine timeStamp, currentThread, randomNum, into one string
    uniqueUploadDirResult = StringCchPrintf(uniqueUploadDir, cchDest, uniqueDirFormat, timeStamp.wYear, timeStamp.wMonth, timeStamp.wDay, timeStamp.wHour, timeStamp.wMinute, timeStamp.wSecond, currentThread, randomNum);
    if (!SUCCEEDED(uniqueUploadDirResult))
    {
        g_pKato->Log(LOG_FAIL, TEXT("Could not construct the unique upload path string, %d"), GetLastError());
        return 0;
    }

    //--------------------------------------------------------------------------
    // get into the devicename upload directory
    if (!FtpSetCurrentDirectory(connectHandle, deviceUploadDir))
    {
        g_pKato->Log(LOG_FAIL, TEXT("Changing the directory failed, %d"), GetLastError());
        return 0;
    }

    //--------------------------------------------------------------------------
    // create a subdir in the devicename dir using the new combined string
    if (!FtpCreateDirectory(connectHandle, uniqueUploadDir))
    {
        g_pKato->Log(LOG_DETAIL, TEXT("Couldn't create directory with thread ID name, %d"), GetLastError());
        
        // get the extended error information
        if (!InternetGetLastResponseInfo(&errorMsg, errorMsgString, &errorMsgLength))
        {
            g_pKato->Log(LOG_DETAIL, TEXT("Couldn't get extended error information, %d"), GetLastError());
            return 0;
        }
        g_pKato->Log(LOG_DETAIL, TEXT("Extended error information: %s"), errorMsgString);
        return 0;
    }
   
    //--------------------------------------------------------------------------
    // if we made it here, the unique directory/directories were created successfully 
    return 1;
}