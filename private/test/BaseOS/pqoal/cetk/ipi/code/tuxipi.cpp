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
#include <tux.h>
#include <oemglobal.h>
#include "commonUtils.h"

#define ARG_STR_IPI_FNPTR (_T("-ipifnptr"))

typedef VOID (*PFNNKSENDIPI)(DWORD dwType, DWORD dwTarget, DWORD dwCommand, DWORD dwData);

__inline BOOL InKernelMode( void )
{
    return 0 > (int)InKernelMode;
}

static DWORD DummyThread(LPVOID lpParam)
{
    volatile PBOOL pfDone = (PBOOL)lpParam;
    DWORD dwCnt = 0;

    while(FALSE == *pfDone)
    {
        if(++dwCnt  % 10000)
            Sleep(0);
    }

    return 0;
}

static DWORD DummyThreadNoSleep(LPVOID lpParam)
{
    volatile PBOOL pfDone = (PBOOL)lpParam;

    while(FALSE == *pfDone)
    {
        ; //spin
    }

    return 0;
}

static DWORD QPCThread(LPVOID lpParam)
{
    DWORD dwNumOfIterations = (DWORD)lpParam;
    DWORD dwCnt;

    for(dwCnt = 0; dwCnt < dwNumOfIterations; dwCnt++)
    {
        LARGE_INTEGER i;
        //We only care about sending the IPI, we don't care about the return value
        QueryPerformanceCounter(&i);
    }

    return 0;
}



static BOOL DoTerminateThread()
{
    const DWORD NUM_OF_THREADS = 8;
    HANDLE hThread[NUM_OF_THREADS] = {0};
    DWORD dwCnt;
    BOOL fDone = FALSE, bRC = TRUE;

    for(dwCnt = 0; bRC && dwCnt < NUM_OF_THREADS; dwCnt++)
    {
        hThread[dwCnt] = CreateThread(NULL,
                                      0,
                                      (LPTHREAD_START_ROUTINE)DummyThreadNoSleep,
                                      (LPVOID)&fDone,
                                      0,
                                      NULL);
        if(FALSE == hThread[dwCnt])
        {
            bRC = FALSE;
        }
    }


    for(dwCnt = 0; dwCnt < NUM_OF_THREADS; dwCnt++)
    {
        //Terminate thread is safe here because we know killing these thread won't leak anything, they are just spinning
        if(FALSE == TerminateThread(hThread[dwCnt], 0))
        {
            Error(TEXT("Failed to TerminateThread() -  GLE 0x%x"), GetLastError());
            bRC = FALSE;
        }
        CloseHandle(hThread[dwCnt]);
    }

    //tell those threads to exit, in case terminateion fails
    InterlockedExchange((LPLONG)&fDone,TRUE);

    return bRC;


}

static PFNNKSENDIPI GetFunctionPointer()
{
    cTokenControl tokens;
    DWORD dwRetVal = 0;

    // If no command line specified or if can't break command line into tokens, exit
    if(!(g_szDllCmdLine && tokenize((TCHAR*)g_szDllCmdLine, &tokens)))
    {
        Error (_T("Couldn't parse the command line."));
    }
    else if (!getOptionDWord(&tokens, ARG_STR_IPI_FNPTR, &dwRetVal))
    {
        Error (_T("getOptionDWord  failed...."));;
    }

    Info (_T("Function Pointer is 0x%x."), dwRetVal);

    return (PFNNKSENDIPI)dwRetVal;

}


TESTPROCAPI  IPITerminateThread(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    const DWORD NUM_OF_ITERATIONS = 2000;
    const DWORD ONE_PERCENT = (NUM_OF_ITERATIONS / 100);
    DWORD dwCnt;

    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    if(TRUE == InKernelMode())  //We cannot terminate kernel thread
    {
        Error(TEXT("This test should be run in user mode instead of kernel mode"));
        return TPR_FAIL;
    }

    for(dwCnt = 0; dwCnt < NUM_OF_ITERATIONS; dwCnt++)
    {
        if(dwCnt % ONE_PERCENT == 0)
        {
            DWORD dwProgress = (dwCnt* 100) / NUM_OF_ITERATIONS;
            Info (_T("%d percent done."), dwProgress);
        }

        if(FALSE == DoTerminateThread())
        {
            return TPR_FAIL;
        }
    }
    return TPR_PASS;
}



TESTPROCAPI  IPISuspendThread(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    const DWORD NUM_OF_THREADS = 8;
    const DWORD NUM_OF_ITERATIONS = 10000;
    const DWORD ONE_PERCENT = (NUM_OF_ITERATIONS / 100);
    HANDLE hThread[NUM_OF_THREADS] = {0};
    DWORD dwCnt, dwCnt2;
    BOOL fDone = FALSE, bRC = TRUE;

    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    if(TRUE == InKernelMode())  //We cannot suspend kernel thread
    {
        Error(TEXT("This test should be run in user mode instead of kernel mode"));
        return TPR_FAIL;
    }


    Info (_T("IPISuspendThread:  Going to loop for %d iterations."), NUM_OF_ITERATIONS);

    for(dwCnt = 0; bRC && dwCnt < NUM_OF_THREADS; dwCnt++)
    {
          hThread[dwCnt] = CreateThread(NULL,
                                         0,
                                        (LPTHREAD_START_ROUTINE)DummyThread,
                                        (LPVOID)&fDone,
                                         0,
                                         NULL);
          if(NULL == hThread[dwCnt])
          {
              Info (_T("IPISuspendThread:  CreateThread() failed at iteration %d"), dwCnt);
              bRC = FALSE;
          }
    }

    for(dwCnt = 0; bRC && dwCnt < NUM_OF_ITERATIONS; dwCnt++)
    {
        if(dwCnt % ONE_PERCENT == 0)
        {
             DWORD dwProgress = (dwCnt* 100) / NUM_OF_ITERATIONS;
             Info (_T("%d percent done."), dwProgress);
        }

        for(dwCnt2 = 0; dwCnt2 < NUM_OF_THREADS; dwCnt2++)
        {
            if(0 !=SuspendThread(hThread[dwCnt2]))  //SuspendThread returns the previous suspend count
            {
                Error(TEXT("Failed to SuspendThread() -  GLE 0x%x"), GetLastError());
                bRC = FALSE;
            }
        }

        for(dwCnt2 = 0; dwCnt2 < NUM_OF_THREADS; dwCnt2++)
        {
            if(1 !=ResumeThread(hThread[dwCnt2]))  //ResumeThread returns the previous suspend count
            {
                Error(TEXT("Failed to ResumeThread() -  GLE 0x%x"), GetLastError());
                bRC = FALSE;
            }
        }
    }

    InterlockedExchange((LPLONG)&fDone,TRUE);

    //Always want to wait for the child threads to exit before primary thread exits
    for(dwCnt = 0; dwCnt < NUM_OF_THREADS; dwCnt++)
    {
        if(WAIT_OBJECT_0 != WaitForSingleObject(hThread[dwCnt], INFINITE))
        {
            bRC = FALSE;
        }
        CloseHandle(hThread[dwCnt]);
    }

    return (TRUE == bRC) ? TPR_PASS : TPR_FAIL;

}

TESTPROCAPI IPIUserAddrCacheFlush(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwArray = 12345;
    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    if(TRUE == InKernelMode())
    {
        Error(TEXT("This test should be run in user mode instead of kernel mode"));
        return TPR_FAIL;
    }

    Info(TEXT("********************************************************************\r\n"));
    Info(TEXT("* This test call CacheRangeFlush() on a user-mode address.         *\r\n"));
    Info(TEXT("* If the kernel crash, there might be a bug in the OAL.            *\r\n"));
    Info(TEXT("* In a SMP environment, when we have to flush a user mode address  *\r\n"));
    Info(TEXT("* the OAL should not blindly send IPI (Inter-Processor Interrupt)  *\r\n"));
    Info(TEXT("* to other cores since the other cores might not necessary be      *\r\n"));
    Info(TEXT("* running in the same virtual address space as this core.          *\r\n"));
    Info(TEXT("* Solution 1:  When you get user mode address, ask the other       *\r\n"));
    Info(TEXT("*              cores to flush the entire cache.                    *\r\n"));
    Info(TEXT("* Solution 2:  When you get user mode address, pass the process    *\r\n"));
    Info(TEXT("*              id (ASID in ARM) to the other cores and decide      *\r\n"));
    Info(TEXT("*              whether a flush is necessary.                       *\r\n"));
    Info(TEXT("********************************************************************\r\n"));

    Info(TEXT("dwArray = %d &dwArray = 0x%x\r\n"), dwArray, &dwArray);
    CacheRangeFlush(&dwArray, 4, CACHE_SYNC_ALL);

    return TPR_PASS;
}


TESTPROCAPI IPIQueryPerformanceCounter(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    LARGE_INTEGER i;
    const DWORD NUM_OF_ITERATIONS = 10000;
    const DWORD ONE_PERCENT = (NUM_OF_ITERATIONS / 100);

    DWORD dwCnt;
    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }


    for(dwCnt = 0; dwCnt < NUM_OF_ITERATIONS; dwCnt++)
    {
        if(dwCnt % ONE_PERCENT == 0)
        {
             DWORD dwProgress = (dwCnt* 100) / NUM_OF_ITERATIONS;
             Info (_T("%d percent done."), dwProgress);
        }

        //don't check return value, we only care about sending the IPI
        QueryPerformanceCounter(&i);
    }

    return TPR_PASS;
}


TESTPROCAPI  IPIQueryPerformanceCounterMT(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    const DWORD NUM_OF_THREADS = 8;
    const DWORD NUM_OF_ITERATIONS = 1000000;
    HANDLE hThread[NUM_OF_THREADS] = {0};
    DWORD dwCnt;
    BOOL fDone = FALSE, bRC = TRUE;

    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    Info (_T("IPIQueryPerformanceCounterMT:  Going to loop for %d iterations."), NUM_OF_ITERATIONS);

    for(dwCnt = 0; bRC && dwCnt < NUM_OF_THREADS; dwCnt++)
    {
          hThread[dwCnt] = CreateThread(NULL,
                                         0,
                                        (LPTHREAD_START_ROUTINE)QPCThread,
                                        (LPVOID)NUM_OF_ITERATIONS,
                                         0,
                                         NULL);
          if(FALSE == hThread[dwCnt])
          {
              bRC = FALSE;
          }
    }

    //Always want to wait for the child threads to exit before primary thread exits
    for(dwCnt = 0; dwCnt < NUM_OF_THREADS; dwCnt++)
    {
        if(WAIT_OBJECT_0 != WaitForSingleObject(hThread[dwCnt], INFINITE))
        {
            bRC = FALSE;
        }
        //The thread can only return 1 value, so we don't have to check the threads' exit code
        CloseHandle(hThread[dwCnt]);
    }

    return (TRUE == bRC) ? TPR_PASS : TPR_FAIL;
}


//
//  Use core #2 as an example to see whether this BSP supports powering off cores
//
static BOOL CanPowerOffSecondaryCore()
{
    BOOL bRetVal;
    //Power it on first in case it's off
    CePowerOnProcessor(2);

    Sleep(1000);
    bRetVal = CePowerOffProcessor(2,1);

    if(bRetVal)
    {
        Sleep(1000);
        bRetVal = CePowerOnProcessor(2);  //If we successfully power-off the processor, power it back on
    }

  return bRetVal;
}

//
//  Turns the specified core on/off, if the core is not ready retry MAX_COUNT
//
static BOOL SetSecondaryCorePowerState(const DWORD coreId, const BOOL powerOn)
{
    BOOL   bRetVal = TRUE;
    DWORD  dwLastError = 0;
    DWORD  count = 0;
    const  DWORD MAX_COUNT = 10;

    do
    {
        if (powerOn)
        {
            bRetVal = CePowerOnProcessor(coreId);
        }
        else
        {
            bRetVal = CePowerOffProcessor(coreId, 1);
        }

        if (!bRetVal)
        {
            dwLastError = GetLastError();
            // Relinquish the thread quantum to allow the core to complete its power state transition.
            Sleep(0); 
        }
    } while (!bRetVal && dwLastError == ERROR_NOT_READY && count++ < MAX_COUNT);

  return bRetVal;
}

TESTPROCAPI  IPIPowerOffOnCores1(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwNumOfCores = CeGetTotalProcessors();
    BOOL bRC = TRUE;
    DWORD dwCnt;

    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    if(FALSE == InKernelMode())
    {
        Error(TEXT("This test should be run in kernel mode instead of user mode"));
        return TPR_FAIL;
    }

    //Try to power off primary core, this should not work
    if(TRUE == CePowerOffProcessor(1, 1))
    {
        Error(TEXT("CePowerOffProcessor(1, 0)  returned TRUE"));
        return TPR_FAIL;
    }
    else if(TRUE == CePowerOnProcessor(1))
    {
        Error(TEXT("CePowerOnProcessor(1) returned TRUE"));
        return TPR_FAIL;
    }

    if(FALSE == CanPowerOffSecondaryCore())
    {
        Info(TEXT("This BSP does not support powering off secondary cores"));
        return TPR_SKIP;
    }

    //Now try to power on /off the secondary processor(s), toggle them one at a time
    for(dwCnt = 2; bRC && dwCnt <= dwNumOfCores; dwCnt++)
    {
        Info(TEXT("Powering Off Core %d"), dwCnt);
        if(FALSE == CePowerOffProcessor(dwCnt, 1))
        {
            Error(TEXT("CePowerOffProcessor(%d, 1) returned FALSE.  GetLastError() = 0x%x"), dwCnt, GetLastError());
            bRC = FALSE;
        }
        Sleep(1000);
        Info(TEXT("Powering On Core %d"), dwCnt);
        if(bRC && FALSE == CePowerOnProcessor(dwCnt))
        {
            Error(TEXT("CePowerOnProcessor(%d, 1) returned FALSE.  GetLastError() = 0x%x"), dwCnt, GetLastError());
            bRC = FALSE;
        }
    }

    return (TRUE == bRC) ? TPR_PASS : TPR_FAIL;
}


TESTPROCAPI  IPIPowerOffOnCores2(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwNumOfCores = CeGetTotalProcessors();
    const DWORD NUM_OF_ITERATIONS = 5000;
    BOOL bRC = TRUE;
    DWORD dwCnt;
    const BOOL  POWER_ON  = TRUE;
    const BOOL  POWER_OFF = FALSE;

    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    if(FALSE == InKernelMode())
    {
        Error(TEXT("This test should be run in kernel mode instead of user mode"));
        return TPR_FAIL;
    }

    //Try to power off primary core, this should not work
    if(TRUE == CePowerOffProcessor(1, 1))
    {
        Error(TEXT("CePowerOffProcessor(1, 0)  returned TRUE"));
        return TPR_FAIL;
    }
    else if(TRUE == CePowerOnProcessor(1))
    {
        Error(TEXT("CePowerOnProcessor(1) returned TRUE"));
        return TPR_FAIL;
    }

    if(FALSE == CanPowerOffSecondaryCore())
    {
        Info(TEXT("This BSP does not support powering off secondary cores"));
        return TPR_SKIP;
    }

    //Now try to power on /off the secondary processor(s), toggle them one at a time
    for(DWORD i = 0; bRC && i < NUM_OF_ITERATIONS; i++)
    {
        //Power off all secondary processors
        for(dwCnt = 2; bRC && dwCnt <= dwNumOfCores; dwCnt++)
        {
            if(FALSE == SetSecondaryCorePowerState(dwCnt, POWER_OFF))
            {
                Error(TEXT("CePowerOffProcessor(dwCnt, 1) returned FALSE"));
                bRC = FALSE;
            }
        }

        //Power on all secondary processors
        for(dwCnt = 2; bRC && dwCnt <= dwNumOfCores; dwCnt++)
        {
            if(FALSE == SetSecondaryCorePowerState(dwCnt, POWER_ON))
            {
                Error(TEXT("CePowerOnProcessor(dwCnt) returned FALSE"));
                bRC = FALSE;
            }
        }
    }

    if(FALSE == bRC)
    {
        //If there is an error, we'll try to turn all the cores back on.....
        for(dwCnt = 2; dwCnt <= dwNumOfCores; dwCnt++)
        {
            CePowerOnProcessor(dwCnt);
        }
    }

    return (TRUE == bRC) ? TPR_PASS : TPR_FAIL;
}


//In CE 7.0, processor numbers are from 1 to MAX_NUM_PROCESSOR,
//skip array entry 0 and use index 1 to MAX_NUM_PROCESSOR  inclusive
#define MAX_NUM_PROCESSOR    64
DWORD g_dwProcArray[MAX_NUM_PROCESSOR+1];

VOID  IPICallBack(DWORD dwProcNumber)
{
  if(0== dwProcNumber || dwProcNumber > MAX_NUM_PROCESSOR)
      return;

  InterlockedIncrement((LPLONG)(&g_dwProcArray[dwProcNumber]));

}


BOOL CheckIPIArray(DWORD dwType, DWORD dwTarget, BOOL bCurrentResult[])
{
  BOOL bRC = TRUE;
  DWORD dwCurrentProcNumber = GetCurrentProcessorNumber();  //Since we have thread affinity set, we should be OK to call this
  DWORD dwNumOfProc = CeGetTotalProcessors();
  DWORD dwCnt;

  if(0==dwCurrentProcNumber)
  {
      Error(TEXT("ERROR:  0==dwCurrentProcNumber"));      //We expect we have thread affinity set, but guess we didn't...
      return FALSE;
  }

  switch (dwType)
  {
    case IPI_TYPE_ALL_BUT_SELF:
    {
        Info(TEXT("CheckIPIArray:  dwType = IPI_TYPE_ALL_BUT_SELF"));
        for(dwCnt = 0; dwCnt <= MAX_NUM_PROCESSOR; dwCnt++)
        {
            if((dwCnt > 0 && dwCnt <= dwNumOfProc)  && dwCnt != dwCurrentProcNumber)
            {
                //We sent IPI to every other core in the system, we expect the following index to have an entry of 1
                if(g_dwProcArray[dwCnt] != 1)
                {
                    Error(TEXT("g_dwProcArray[%d] != 1"), dwCnt);     //We sent IPI to every other core, but we didn't see anything in our array....
                    bRC = FALSE;
                }
                else
                {
                    Info(TEXT("  g_dwProcArray[%d] = %d"),dwCnt, g_dwProcArray[dwCnt]);
                }
            }
            else if(g_dwProcArray[dwCnt] != 0)  //We didn't send IPI to ourself, but something wrote to our array entry..
            {
                Error(TEXT("g_dwProcArray[%d] != 0"), dwCnt);
                bRC = FALSE;
            }
        }
    }
        break;
    case IPI_TYPE_SPECIFIC_CPU:
    {
        DWORD dwNumOfNewEntry = 0;
        Info(TEXT("CheckIPIArray:  dwType = IPI_TYPE_SPECIFIC_CPU, dwTarget = %d"), dwTarget);
        for(dwCnt = 0; dwCnt <= MAX_NUM_PROCESSOR; dwCnt++)
        {
            if(g_dwProcArray[dwCnt] != 0)  //We just received an IPI from this processor
            {
                Info(TEXT("g_dwProcArray[%d] = %d"), dwCnt,g_dwProcArray[dwCnt]);
                dwNumOfNewEntry++;
                if(bCurrentResult[dwCnt]!= 0)
                {
                    Error(TEXT("Already received IPI from this processor in the previous iteration....bCurrentResult[%d] = %d"), dwCnt,bCurrentResult[dwCnt]);
                    bRC = FALSE;
                }
                (bCurrentResult[dwCnt])++;
            }
         }
         if(1 != dwNumOfNewEntry)
         {
            Error(TEXT("%d  processors signalled IPI"), dwNumOfNewEntry);
            bRC = FALSE;
         }
    }
        break;
    default:
    {
        Error(TEXT("ERROR:  Unknown dwType:  %d"), dwType);
    }
        break;
  }

  return bRC;

}

//
//   !readex {,,kernel.dll}g_pNKGlobal->pfnNKSendIPI    will give you the value you want
//
TESTPROCAPI  IPISendIPISpecificCPU(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwNumOfCores = CeGetTotalProcessors();
    BOOL bCurrentResult[MAX_NUM_PROCESSOR+1] = {0};
    PFNNKSENDIPI pfnNkSendIPI = NULL;

    BOOL bRC = FALSE;

    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    if(FALSE == InKernelMode())
    {
        Error(TEXT("This test should be run in kernel mode instead of user mode"));
        return TPR_FAIL;
    }
    else if(NULL == (pfnNkSendIPI = GetFunctionPointer()))
    {
        Error(TEXT("Fucntion Pointer to SendIPI is NULL"));
        return TPR_SKIP;
    }

    //TODO:  Lock page, so we won't page fault during IPI processing

    //We'll set our thread affinity to each core
    for(DWORD i = 0; i < dwNumOfCores; i++)
    {
        if(FALSE == CeSetThreadAffinity(GetCurrentThread(), i))
        {
            Error(TEXT("CeSetThreadAffinity() Failed"));
            goto Error;
        }
        memset(&bCurrentResult[0],0,sizeof(bCurrentResult));
        //Try to send IPI to each core (NOTE:  looking at the IPI code, the processor number are 0 to dwNumofCores-1, inclusive)
        Info(TEXT("**************************************************************************"));
        for(DWORD j = 0; j < dwNumOfCores; j++)
        {
            if(j!=i)  //We don't want to send IPI to ourself
            {
                Info(TEXT("Core %d is going to send IPI to core %d "), i, j);
                memset(&g_dwProcArray[0], 0, sizeof(g_dwProcArray));  //Clear the global
                (pfnNkSendIPI)(IPI_TYPE_SPECIFIC_CPU, j, IPI_TEST_CALL_FUNCTION_PTR, (DWORD)IPICallBack);
                if(FALSE == CheckIPIArray(IPI_TYPE_SPECIFIC_CPU, j, bCurrentResult))
                {
                    goto Error;
                }
            }
        }
    }

    bRC = TRUE;

Error:

    CeSetThreadAffinity(GetCurrentThread(), 0);  //Undo thread affinity

    return (TRUE == bRC) ? TPR_PASS : TPR_FAIL;
}


TESTPROCAPI IPISendIPIAllButSelf(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    DWORD dwNumOfCores = CeGetTotalProcessors();
    PFNNKSENDIPI pfnNkSendIPI = NULL;

    BOOL bRC = FALSE;

    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    if(FALSE == InKernelMode())
    {
        Error(TEXT("This test should be run in kernel mode instead of user mode"));
        return TPR_FAIL;
    }
    else if(NULL == (pfnNkSendIPI = GetFunctionPointer()))
    {
        Error(TEXT("Fucntion Pointer to SendIPI is NULL"));
        return TPR_SKIP;
    }



    //TODO:  Lock page, so we won't page fault during IPI processing

    //We'll set our thread affinity to each core
    for(DWORD i = 1; i <= dwNumOfCores; i++)
    {
        if(FALSE == CeSetThreadAffinity(GetCurrentThread(), i))
        {
            Error(TEXT("CeSetThreadAffinity() Failed"));
            goto Error;
        }
        Info(TEXT("**************************************************************************"));
        Info(TEXT("Core %d is going to send IPI to all other CPUs "), i);
        memset(&g_dwProcArray[0], 0, sizeof(g_dwProcArray));  //Clear the global
        (pfnNkSendIPI)(IPI_TYPE_ALL_BUT_SELF, 0, IPI_TEST_CALL_FUNCTION_PTR, (DWORD)IPICallBack);

        if(FALSE == CheckIPIArray(IPI_TYPE_ALL_BUT_SELF, 0, NULL))
        {
            goto Error;
        }
    }

    bRC = TRUE;

Error:

    CeSetThreadAffinity(GetCurrentThread(), 0);  //Undo thread affinity

    return (TRUE == bRC) ? TPR_PASS : TPR_FAIL;
}

TESTPROCAPI  IPIUsage(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
    if (uMsg != TPM_EXECUTE)
    {
        return TPR_NOT_HANDLED;
    }

    Info(TEXT("**************************************************************************"));
    Info(TEXT("Test Usage:                                                             "));
    Info(TEXT("------------------------------------------------------------------------"));
    Info(TEXT("Test 2000 to 2004 should be run in user-mode (ie without the -n flag)   "));
    Info(TEXT("Test 3000 to 3003 should be run in kernel-mode (ie with the -n flag)    "));
    Info(TEXT("                                                                        "));
    Info(TEXT("Test 3002 and 3003 requires the value of g_pNKGlobal->pfnNKSendIPI      "));
    Info(TEXT("To obtain the value of g_pNKGlobal->pfnNKSendIPI                        "));
    Info(TEXT("1) Make sure you have the appropriate kernel.pdb and kernel.dll in your "));
    Info(TEXT("   flat release directory.                                              "));
    Info(TEXT("2) Break into the debugger.                                             "));
    Info(TEXT("3) Run: \"!readex {,,kernel.dll}g_pNKGlobal->pfnNKSendIPI\"  in your    "));
    Info(TEXT("   TargetControl Window.                                                "));
    Info(TEXT("       Sample Output:                                                   "));
    Info(TEXT("       Windows CE>!readex {,,kernel.dll}g_pNKGlobal->pfnNKSendIPI       "));
    Info(TEXT("       Refreshing system data ...                                       "));
    Info(TEXT("       Reading system memory info ...                                   "));
    Info(TEXT("       Populating process list ...                                      "));
    Info(TEXT("       Reading general device info ...                                  "));
    Info(TEXT("       Reading system memory info ...                                   "));
    Info(TEXT("                                                                        "));
    Info(TEXT("       Address    : Value                                               "));
    Info(TEXT("       0x83bd9110 : 0x8013AE10                                          "));
    Info(TEXT("4) Resume the device (from debug state)                                 "));
    Info(TEXT("5) Run the tests:                                                       "));
    Info(TEXT("      s tux -o -d oaltestipi -n -x3002-3003 -c\"-ipifnptr 0x8013ae10\"  "));
    Info(TEXT("**************************************************************************"));

    return TPR_PASS;
}