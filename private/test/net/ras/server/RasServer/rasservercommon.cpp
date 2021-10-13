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
#include "RasServerTest.h"

#define INVALID_POINTER_INT 0xabcd
extern BOOL bExpectedFail;
 
TESTPROCAPI RasServerCommon(UINT uMsg, 
                         TPPARAM tpParam, 
                         LPFUNCTION_TABLE_ENTRY lpFTE) 
{    
    DWORD                    dwStatus = TPR_PASS;
    HANDLE                    hHandle=INVALID_HANDLE_VALUE;
    DWORD dwResult=ERROR_SUCCESS, dwExpectedResult=ERROR_SUCCESS;
    INT    i = INVALID_POINTER_INT;
    PBYTE pBufIn=(PUCHAR)&i;
    PBYTE pBufOut=(PUCHAR)&i;
    DWORD dwLenIn=0, dwLenOut=0, cbStatus=0;

    // Check our message value to see why we have been called
    if (uMsg == TPM_QUERY_THREAD_COUNT) 
    {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return TPR_HANDLED;
    } 
    else if (uMsg != TPM_EXECUTE) 
    {
        return TPR_NOT_HANDLED;
    }

    //
    // Disable the server before starting this test
    //
    dwResult = RasIOControl(NULL, RASCNTL_SERVER_DISABLE, NULL, 0, NULL, 0, &cbStatus);
    if (dwResult != ERROR_SUCCESS)
    {
        if(bExpectedFail)
        {
            RasPrint(TEXT("RasIOControl() FAIL'ed (%d) AS EXPECTED "), dwResult);
            return (dwResult==ERROR_NOT_SUPPORTED? TPR_PASS: TPR_FAIL);
        }

        RasPrint(TEXT("FAIL'ed to disable the server (%d)"), dwResult);
        return TPR_SKIP;
    }

    switch(LOWORD(lpFTE->dwUserData))
    {
        case RASSERVER_INVALID_HANDLE:
            break;

        case RASSERVER_INVALID_IN_BUF:
            break;

        case RASSERVER_INVALID_OUT_BUF:
            break;

        case RASSERVER_INVALID_IN_LEN:
        case RASSERVER_INVALID_OUT_LEN:
            RasPrint(TEXT("Skipping Invalid Input/Output Test ..."));
            return TPR_SKIP;
    }

    //
    // We will just call RasServer Enable routine to see if the invalid cases
    // above work
    //
    dwResult = RasIOControl(
                        hHandle, 
                        RASCNTL_SERVER_ENABLE, 
                        pBufIn, dwLenIn, 
                        pBufOut, dwLenOut, 
                        &cbStatus
                        );
    RasPrint(TEXT("dwResult = %d\tdwExpectedResult = %d"), dwResult, dwExpectedResult);    

    if(dwResult != dwExpectedResult)
    {
        return dwStatus=TPR_FAIL;
    }
    
    return dwStatus;
}
