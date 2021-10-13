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

TESTPROCAPI RasServerGetParams(UINT uMsg, 
                         TPPARAM tpParam, 
                         LPFUNCTION_TABLE_ENTRY lpFTE) 
{    
    HANDLE                        hHandle=NULL;
    RASCNTL_SERVERSTATUS         *pStatus = NULL;
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
        dwResult = RasIOControl(NULL, RASCNTL_SERVER_GET_PARAMETERS, NULL, 0, NULL, 0, &cbStatus);
        RasPrint(TEXT("RasIOControl() FAIL'ed (%d) AS EXPECTED "), dwResult);
        return (dwResult==ERROR_NOT_SUPPORTED? TPR_PASS: TPR_FAIL);
    }
    
    //
    // What is the test doing?
    //
    switch(LOWORD(lpFTE->dwUserData))
    {
        case RASSERVER_ENABLED_PARAMS:
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
            
        case RASSERVER_DISABLED_PARAMS:
            //
            // Disable the server
            //
            
            dwResult = RasIOControl(NULL, RASCNTL_SERVER_DISABLE, NULL, 0, NULL, 0, &cbStatus);
            if (dwResult != ERROR_SUCCESS)
            {
                RasPrint(TEXT("FAIL'ed to disable the server (%d)"), dwResult);
                return TPR_SKIP;
            }
            break;
            
        case RASSERVER_DISPLAY:
            break;
    }    

    //
    // Call the IOCTL
    //

    //
    //    First pass is to get the size
    //
    dwResult = RasIOControl(NULL, RASCNTL_SERVER_GET_PARAMETERS, NULL, 0, NULL, 0, &cbStatus);
    if (dwResult != ERROR_BUFFER_TOO_SMALL)
    {
        RasPrint(TEXT("RASCNTL_SERVER_GET_PARAMETERS error - skip"));
        return TPR_SKIP;
    }
    else
    {
        pStatus = (RASCNTL_SERVERSTATUS *)LocalAlloc(LPTR, cbStatus);
        if (pStatus == NULL)
        {
            RasPrint(TEXT("Failed allocating memory of %i bytes. Error code = %i"), cbStatus, GetLastError());
            return TPR_SKIP;
        }
    }

    //
    //    Get server status and print out
    //
    dwResult = RasIOControl(
                            hHandle, 
                            RASCNTL_SERVER_GET_PARAMETERS, 
                            pBufIn, dwLenIn, 
                            (PUCHAR)pStatus, cbStatus,
                            &cbStatus
                            );
    RasPrint(TEXT("dwResult = %d\tdwExpectedResult = %d"), dwResult, dwExpectedResult);    
    
    switch(LOWORD(lpFTE->dwUserData))
    {
        case RASSERVER_ENABLED_PARAMS:
            if(pStatus->bEnable != 1)
            {
                RasPrint(TEXT("pStatus->bEnable != 1"));
                dwStatus = TPR_FAIL;
            }
            break;
            
        case RASSERVER_DISABLED_PARAMS:
            if(pStatus->bEnable == 1)
            {
                RasPrint(TEXT("pStatus->bEnable == 1"));
                dwStatus = TPR_FAIL;
            }
            break;
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
