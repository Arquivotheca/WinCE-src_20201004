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
#include <tchar.h>
#include "basebvt.h"
#include "task.h"
#include "baseapp.h"


/*****************************************************************************
 *
 *    Description: 
 *
 *       BaseTaskTest tests Process and thread creation and Termination. 
 *
 *    Need to have the baseApp.exe built in the release dir for this test.
 *
 *****************************************************************************/

TESTPROCAPI 
BaseTaskTest(
    UINT uMsg, 
    TPPARAM /*tpParam*/, 
    LPFUNCTION_TABLE_ENTRY /*lpFTE*/
    ) 
{
    HANDLE hTest=NULL, hStep=NULL;

    if (uMsg != TPM_EXECUTE) {
        return TPR_NOT_HANDLED;
    }
    hTest = StartTest( TEXT("Base Task test") );
    
    hStep = BEGINSTEP( hTest, hStep, TEXT("Process Creation Test"));

    CHECKTRUE(TestProcessCreation());

    ENDSTEP( hTest, hStep);

    hStep = BEGINSTEP( hTest, hStep, TEXT("Thread Creation Test"));

    CHECKTRUE(TestThreadCreation());

    ENDSTEP( hTest, hStep);

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}



/*****************************************************************************
*
*   Name    : TestProcessCreation
*
*   Purpose : Create an event
*             Form the CommandLine with event handle as an argument
*             Invoke the w32child.exe with Cmdline
*             Sleep for sometime, so that child gets chance to get into action
*             Set this event so that child gets out of wait
*             Wait for child to terminate with status code
*             Check if child did its job, by looking at its exitcode
*             Close Event handle and child process handle
*
*
*    Input  : variation number
*
*
*
*   Exit    : none
*
*   Calls   : CreateEvent,sprintf,CreateProcess,GetCurrentProcess
*             GetCurrentProcessId,SetEvent,WaitForSingleObject,GetExitCodeProcess
*             CloseHandle
*
*
*****************************************************************************/


BOOL TestProcessCreation()
{

    BOOL                 bRc = TRUE;
    HANDLE                 hCurrProc=NULL, hEvent=NULL;
    DWORD               dwWait=0;
    DWORD                 dwProcId=0;
    DWORD                 dwChildRC=0;
    PROCESS_INFORMATION ProcessInfo = { 0 };

// dirty the id and child status, so that it can be checked later

    dwProcId = 420L;
    dwChildRC= 420L;


    hEvent = YCreateEvent( NULL,  // sec attr
                           TRUE,  // manual reset
                           FALSE, // initial state reset
                           SZ_EVENT_NAME);

    if (! hEvent)  {
        return(FALSE);
                
    }



    if (!YCreateProcess( CHILD_EXE_CMD_LINE,       // Application Name
                         NULL,                    // CommandLine
                         NULL,                    // Process Attr's
                         NULL,                    // Thread Attr's
                         FALSE,                    // Inherit Handle
                         0,                        // Creation Flags
                         NULL,                    // Environ
                         NULL,                    // Cur Dir
                         NULL,             // StartupInfo
                         &ProcessInfo ))        // ProcessInfo 
    {
        LogDetail ( TEXT("Make sure the Application is present in the release dir"));
        bRc = FALSE;
        goto EXIT;    
    }

    Sleep(100);

    hCurrProc = YGetCurrentProcess();

    LogDetail ( TEXT("Process Handle is :%lx\n"), hCurrProc );

    dwProcId = YGetCurrentProcessId( );

    LogDetail ( TEXT("Process Id is :%lx\n"), dwProcId );

    LogDetail ( TEXT("Child Process' Id is :%lx:\n"), ProcessInfo.dwProcessId );

    LogDetail ( TEXT("Waiting for child process to finish, allow child to print, setting event..\n" ));

    if (!YSetEvent(hEvent))  {
        bRc = FALSE;
    }

    dwWait = YWaitForSingleObject(ProcessInfo.hProcess, INFINITE);

    YGetExitCodeProcess( ProcessInfo.hProcess, &dwChildRC );

    if (!YCloseHandle(ProcessInfo.hProcess))  {
        bRc = FALSE;
    }
    if (!YCloseHandle(ProcessInfo.hThread))  {
        bRc = FALSE;
    }

EXIT:
    if (hEvent)  {
        if (!YCloseHandle(hEvent))  {
            bRc = FALSE;
        }
    }
    return( bRc && (dwChildRC == PROCESS_EXIT_CODE) && (dwWait == WAIT_OBJECT_0));
}


/*****************************************************************************
*
*   Name    : TestThreadCreation
*
*   Purpose : Create an event
*             Invoke the thread function with thread argument
*             Sleep for sometime, so that thread gets chance to get into action
*             Set this event so that thread gets out of wait
*             Wait for thread to terminate with status code
*             Check if thread did its job, by looking at its exitcode
*             Close Event handle and child thread handle
*
*
*
*
*
*   Exit    : none
*
*   Calls   : CreateEvent,CreateThread,SetEvent,WaitForSingleObject
*             GetExitCodeThread,CloseHandle
*
*
*****************************************************************************/


BOOL TestThreadCreation()
{
    BOOL    bRc = TRUE;
    HANDLE  hEvent = NULL,hThread = NULL;
    DWORD   dwWait=0,dwThreadId=0,dwThreadRc=0;


    hEvent = CreateEvent(NULL,   // sec attr
                         TRUE,   // manual reset
                         FALSE, // initial state reset
                         NULL);

    if (!hEvent)  {
        return(FALSE);
        
    }

    hThread = CreateThread(NULL,           // sec attr
                           0,              // stack size same as creater's stack
                              (LPTHREAD_START_ROUTINE)ThreadFunction, // function to be called as thread
                              &hEvent,         // thread arg
                               0,              // no extra creation flags
                            &dwThreadId);    // pointer to get thread id back

    if (!dwThreadId)  {
        bRc = FALSE;
        goto EXIT;
    }

    Sleep(100);

    if (!SetEvent(hEvent))  {
        bRc = FALSE;
    }

    dwWait = YWaitForSingleObject(hThread, INFINITE);

    if (!YGetExitCodeThread( hThread, &dwThreadRc ))  {
        bRc = FALSE;
    }

    if (!YCloseHandle(hThread))  {
        bRc = FALSE;
    }
EXIT:
    if (hEvent)  {
        if (!YCloseHandle(hEvent))  {
            bRc = FALSE;
        }
    }
    return( bRc && (dwThreadRc == THREAD_EXIT_CODE) && (dwWait == WAIT_OBJECT_0));

}




/*****************************************************************************
*
*   Name    : ThreadFunction
*
*   Purpose : wait for calling thread to signal an event
*             set the exit status code to success
*             exit thread
*
*
*
*    Input  : thread argument(pointer to an event handle)
*
*
*
*   Exit    : none
*
*   Calls   : WaitForSingleObject, ExitThread
*
*
*
*****************************************************************************/






DWORD ThreadFunction(PHANDLE phEvent)
{

    DWORD dwWait=0;
    DWORD dwStatus=0;

    // dwStatus init to 0

    dwStatus = THREAD_EXIT_CODE;

    dwWait = YWaitForSingleObject(*phEvent, INFINITE);

    if ( dwWait != WAIT_OBJECT_0) {
        dwStatus = 0xFFFFFFFF;
    }

    ExitThread(dwStatus);
    return(0);    // Should never reach here !!!
    
}
