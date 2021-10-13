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
extern BOOL bExpectedFail;

TESTPROCAPI RasServerEnable(UINT uMsg, 
                         TPPARAM tpParam, 
                         LPFUNCTION_TABLE_ENTRY lpFTE) 
{    
    HANDLE                        hHandle=NULL;
    RASCNTL_SERVERSTATUS         *pStatus = NULL;
    RASCNTL_SERVERLINE           *pLine    = NULL;
    DWORD                        cbStatus = 0,
                                dwResult = 0,
                                dwSize     = 0,
                                dwExpectedResult = ERROR_SUCCESS,
                                dwResultTemp = 0;

    PBYTE                         pBufIn=NULL;    
    PBYTE                         pBufOut=NULL;
    DWORD                        dwLenIn = 0;    
    DWORD                        dwLenOut = 0;

    DWORD                        dwStatus = TPR_PASS;

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

    if(bExpectedFail)
    {
        //
        // Try the IOCTL here. This should fail with ERROR_NOT_SUPPORTED
        //
        dwResult = RasIOControl(NULL, RASCNTL_SERVER_ENABLE, NULL, 0, NULL, 0, &cbStatus);
        RasPrint(TEXT("RasIOControl() FAIL'ed (%d) AS EXPECTED "), dwResult);
        return (dwResult==ERROR_NOT_SUPPORTED? TPR_PASS: TPR_FAIL);
    }

    //
    // Disable the server before starting this test
    //
    dwResult = RasIOControl(NULL, RASCNTL_SERVER_DISABLE, NULL, 0, NULL, 0, &cbStatus);
    if (dwResult != ERROR_SUCCESS)
    {
        RasPrint(TEXT("FAIL'ed to disable the server (%d)"), dwResult);
        return TPR_SKIP;
    }
    else 
    {
        RasPrint(TEXT("Disabled RAS server"));
    }

    //
    // What is the test doing?
    //
    switch(LOWORD(lpFTE->dwUserData))
    {
        case RASSERVER_ALREADY_ENABLED:
            //
            // Enable the server here
            //

            dwResult = RasIOControl(NULL, RASCNTL_SERVER_ENABLE, NULL, 0, NULL, 0, &cbStatus);
            if (dwResult != ERROR_SUCCESS)
            {
                RasPrint(TEXT("Enable already Enabled server test failed (%i)"), dwResult);
                return TPR_SKIP;
            }    
            break;
            
        case RASSERVER_ENABLE:
            //
            // Enable once disabled server. 
            //
            break;
    }    

    //
    // Call the IOCTL
    //
    dwResult = RasIOControl(
                            hHandle, 
                            RASCNTL_SERVER_ENABLE, 
                            pBufIn, dwLenIn, 
                            pBufOut, dwLenOut, 
                            &cbStatus
                            );
    RasPrint(TEXT("dwResult = %d\tdwExpectedResult = %d"), dwResult, dwExpectedResult);    
    
    //
    //  Verify the server is disabled by checking the bEnable flag
    //
    //    First pass is to get the size
    //
    dwResultTemp = RasIOControl(NULL, RASCNTL_SERVER_GET_STATUS, NULL, 0, NULL, 0, &dwSize);
    if (dwResultTemp != ERROR_BUFFER_TOO_SMALL)
    {
        RasPrint(TEXT("RasIOControl failed when pBufOut = NULL. Error code = %i"), dwResultTemp);
        return TPR_SKIP;
    }
    else
    {
        pStatus = (RASCNTL_SERVERSTATUS *)LocalAlloc(LPTR, dwSize);
        if (pStatus == NULL)
        {
            RasPrint(TEXT("LocalAlloc(pStatus) FAILed"));
            return TPR_SKIP;
        }
    }
    
    dwResultTemp = RasIOControl(NULL, RASCNTL_SERVER_GET_STATUS, NULL, 0, (PUCHAR)pStatus, dwSize, &dwSize);
    if (dwResultTemp != 0)
    {
        RasPrint(TEXT("RASCNTL_SERVER_GET_STATUS FAILed (%i)"), dwResultTemp);
        return TPR_SKIP;
    }
    else
    {
        //
        // Did we expect this case to be a pass?
        // 
        if (dwResult != dwExpectedResult)
        {
            //
            // FAILed
            // 
            dwStatus = TPR_FAIL;
        }
        else
        {
            //
            // This should have passed
            //
            if(pStatus->bEnable)
            {
                dwStatus = TPR_PASS;
            }
            else
            {
                //
                // bEnable != 1. This is a problem
                //
                dwStatus = TPR_FAIL;
                RasPrint(TEXT("pStatus->bEnable != 1"));
            }            
        }
    }

    if(pBufIn)    
        LocalFree(pBufIn);
    
    if(pBufOut) 
        LocalFree(pBufOut);

    if(pStatus)
        LocalFree(pStatus);

    CleanupServer();
    return dwStatus;
}
