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
#include <ksmplib.h>
#include "kharness.h"


//
//  Backward Compatibility Test #1, 2
//      Make sure:  For CE7, we can have multiple threads (from the same process) running at the same time
//                  For CE6 and before, only 1 thread can be active any given time
//
TESTPROCAPI
SMP_DEPTH_BC_1(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if (uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }
    else if(FALSE == IsSMPCapable()){
        LogDetail(TEXT("This test should only be run on platforms with more than 1 core....Skipping\r\n"));
        return TPR_SKIP;
    }

    HANDLE hTest=NULL, hStep=NULL;
    HANDLE hProcess = NULL;
    LPCWSTR pszFileNameArray[] = {TEXT("KSMP_BC_P1_CE5.EXE"),
                                  TEXT("KSMP_BC_P1_CE6.EXE"),
                                  TEXT("KSMP_BC_P1_CE7.EXE"),
                                  TEXT("KSMP_BC_P2_CE5.EXE"),
                                  TEXT("KSMP_BC_P2_CE6.EXE")};


    RestoreInitialCondition();

    hTest = StartTest(NULL);


    BEGINSTEP( hTest, hStep, TEXT("Begin backward compatibility test"));
        DWORD dwArrayIndex = lpFTE->dwUserData;
        CHECKTRUE(dwArrayIndex < _countof(pszFileNameArray));
        LPCWSTR pszFileName = pszFileNameArray[dwArrayIndex];
        LogDetail(TEXT("Creating process %s...\r\n"),pszFileName);
        PROCESS_INFORMATION pi = {0};

        CHECKTRUE(CreateProcess(pszFileName, NULL, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi));
        hProcess = pi.hProcess;
        CloseHandle(pi.hThread);  //We don't need the thread handle of the primary thread
    ENDSTEP(hTest,hStep);

    BEGINSTEP( hTest, hStep, TEXT("Waiting for process to exit"));
        CHECKTRUE(DoWaitForProcess(&hProcess,1,TPR_PASS));
    ENDSTEP(hTest,hStep);

    if(NULL != hProcess){
        CloseHandle(hProcess);
        hProcess = NULL;
    }

    RestoreInitialCondition();

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}