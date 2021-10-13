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
#include "ksmplib.h"


//
//  Verify CeSetProcessAffinity() and KData field
//
TESTPROCAPI
SMP_DEPTH_KDATA_1(
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

    RestoreInitialCondition();

    hTest = StartTest( NULL);

    BEGINSTEP(hTest, hStep, TEXT("Checking number of processors"));
        DWORD dwNumOfProcessors = 0;
        dwNumOfProcessors = CeGetTotalProcessors();
        CHECKTRUE(dwNumOfProcessors == __GetUserKData(KDATA_TOTAL_PROCESSOR_OFFSET));
        CHECKTRUE(dwNumOfProcessors > 1); //This is a SMP-capable system
    ENDSTEP(hTest,hStep);

    BEGINSTEP(hTest, hStep, TEXT("Setting process affinity to 0 - no affinity"));
        CHECKTRUE(CeSetProcessAffinity( GetCurrentProcess(),0));
    ENDSTEP(hTest,hStep);

    BEGINSTEP(hTest, hStep, TEXT("Setting process affinity to 1 - only run on core 1"));
        CHECKTRUE(CeSetProcessAffinity( GetCurrentProcess(),1));
        CHECKTRUE(1==GetCurrentProcessorNumber());
        CHECKTRUE(1==__GetUserKData(PCB_CURRENT_PROCESSOR_OFFSET));
    ENDSTEP(hTest,hStep);

    BEGINSTEP(hTest, hStep, TEXT("Setting process affinity to 2 - only run on core 2"));
        CHECKTRUE(CeSetProcessAffinity( GetCurrentProcess(),2));
        CHECKTRUE(2==GetCurrentProcessorNumber());
        CHECKTRUE(2==__GetUserKData(PCB_CURRENT_PROCESSOR_OFFSET));
    ENDSTEP(hTest,hStep);

    CeSetProcessAffinity( GetCurrentProcess(),0);
    RestoreInitialCondition();

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}


//
//  Verify CeGetTotalProcessors() / KData field / GetSystemInfo()
//
TESTPROCAPI
SMP_DEPTH_KDATA_2(
    UINT uMsg,
    TPPARAM tpParam,
    LPFUNCTION_TABLE_ENTRY lpFTE
)
{
    if (uMsg != TPM_EXECUTE){
        return TPR_NOT_HANDLED;
    }

    RestoreInitialCondition();

    HANDLE hTest=NULL, hStep=NULL;

    hTest = StartTest( NULL);

    BEGINSTEP(hTest, hStep, TEXT("Checking number of processors"));
        SYSTEM_INFO si = {0};
        DWORD dwApiReturn = CeGetTotalProcessors();
        DWORD dwKdataReturn = __GetUserKData(KDATA_TOTAL_PROCESSOR_OFFSET);
        GetSystemInfo(&si);

        LogDetail(TEXT("dwApiReturn = %d\r\n"), dwApiReturn);
        LogDetail(TEXT("dwKdataReturn = %d\r\n"), dwKdataReturn);
        LogDetail(TEXT("si.dwNumberOfProcessors = %d\r\n"), si.dwNumberOfProcessors);

        //We expect:  dwApiReturn == dwKdataReturn == si.dwNumberOfProcessors
        CHECKTRUE(dwApiReturn == dwKdataReturn);
        CHECKTRUE(dwApiReturn ==si.dwNumberOfProcessors);
    ENDSTEP(hTest,hStep);

    RestoreInitialCondition();

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}


//
//  Verify CeSetThreadAffinity() and KData field
//
TESTPROCAPI
SMP_DEPTH_KDATA_3(
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

    RestoreInitialCondition();

    hTest = StartTest( NULL);

    BEGINSTEP(hTest, hStep, TEXT("Checking number of processors"));
        DWORD dwNumOfProcessors = 0;
        dwNumOfProcessors = CeGetTotalProcessors();
        CHECKTRUE(dwNumOfProcessors == __GetUserKData(KDATA_TOTAL_PROCESSOR_OFFSET));
        CHECKTRUE(dwNumOfProcessors > 1); //This is a SMP-capable system
    ENDSTEP(hTest,hStep);

    BEGINSTEP(hTest, hStep, TEXT("Setting thread affinity to 0 - no affinity"));
        CHECKTRUE(CeSetThreadAffinity( GetCurrentThread(),0));
    ENDSTEP(hTest,hStep);

    BEGINSTEP(hTest, hStep, TEXT("Setting thread affinity to 1 - only run on core 1"));
        CHECKTRUE(CeSetThreadAffinity( GetCurrentThread(),1));
        CHECKTRUE(1==GetCurrentProcessorNumber());
        CHECKTRUE(1==__GetUserKData(PCB_CURRENT_PROCESSOR_OFFSET));
    ENDSTEP(hTest,hStep);

    BEGINSTEP(hTest, hStep, TEXT("Setting thread affinity to 2 - only run on core 2"));
        CHECKTRUE(CeSetThreadAffinity( GetCurrentThread(),2));
        CHECKTRUE(2==GetCurrentProcessorNumber());
        CHECKTRUE(2==__GetUserKData(PCB_CURRENT_PROCESSOR_OFFSET));
    ENDSTEP(hTest,hStep);


    RestoreInitialCondition();

    return FinishTest(hTest) ? TPR_PASS : TPR_FAIL;
}
