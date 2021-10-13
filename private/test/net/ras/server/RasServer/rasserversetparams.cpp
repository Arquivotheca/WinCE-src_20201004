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

TESTPROCAPI RasServerSetParams(UINT uMsg, 
                         TPPARAM tpParam, 
                         LPFUNCTION_TABLE_ENTRY lpFTE) 
{    
    HANDLE                    hHandle=NULL;
    RASCNTL_SERVERSTATUS         *pStatus = NULL;
    DWORD                    dwStatus = TPR_PASS;
    DWORD                     dwResult=ERROR_SUCCESS;
    DWORD                     dwExpectedResult=ERROR_SUCCESS;
    PBYTE                    pBufIn=NULL;    
    PBYTE                    pBufOut=NULL;
    DWORD                    dwLenIn = 0;    
    DWORD                    dwLenOut = 0;
    DWORD                     cbStatus = 0;

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
        dwResult = RasIOControl(NULL, RASCNTL_SERVER_SET_PARAMETERS, NULL, 0, NULL, 0, &cbStatus);
        RasPrint(TEXT("RasIOControl() FAIL'ed (%d) AS EXPECTED "), dwResult);
        return (dwResult==ERROR_NOT_SUPPORTED? TPR_PASS: TPR_FAIL);
    }
    
    pStatus=(RASCNTL_SERVERSTATUS*)LocalAlloc(LPTR, sizeof(RASCNTL_SERVERSTATUS));
    if(pStatus==NULL)
    {
        RasPrint(TEXT("LocalAlloc(pStatus) Failed"));
        return TPR_SKIP;
    }

    switch(LOWORD(lpFTE->dwUserData))
    {
        case RASSERVER_SET_VALID_PARAMS:
            //
            // Init the structure with dummy but valid values
            //
            pStatus->bEnable = FALSE;                    // Should not be changed
            pStatus->bmFlags = 0xff;
            pStatus->bUseDhcpAddresses = TRUE;
            pStatus->dwStaticIpAddrStart = 0;
            pStatus->dwStaticIpAddrCount = 100;
            pStatus->bmAuthenticationMethods = RASEO_ProhibitCHAP;  
            pStatus->dwNumLines = 100;                    // Should not be changed
            
            break;
            
        case RASSERVER_SET_INVALID_PARAMS:
            pStatus=NULL;
            dwExpectedResult=ERROR_INVALID_PARAMETER;
            break;
    }    

    //
    // Call the IOCTL
    //
    dwResult = RasIOControl(
                        hHandle, 
                        RASCNTL_SERVER_SET_PARAMETERS, 
                        (PUCHAR)pStatus, sizeof(RASCNTL_SERVERSTATUS), 
                        pBufOut, dwLenOut,
                        &cbStatus
                        );
    RasPrint(TEXT("dwResult = %d\tdwExpectedResult = %d"), dwResult, dwExpectedResult);    

    RASCNTL_SERVERSTATUS Status;
    memset(&Status, 0, sizeof(RASCNTL_SERVERSTATUS));
    DWORD dwResultNew = RasIOControl(NULL, RASCNTL_SERVER_GET_PARAMETERS, NULL, 0, (PUCHAR)&Status, sizeof(Status), &cbStatus);

    if ((Status.bEnable)                            ||
        (Status.bmFlags != 0xff)                    ||
        !(Status.bUseDhcpAddresses)                ||
        (Status.dwStaticIpAddrStart != 0)            || 
        (Status.dwStaticIpAddrCount != 100)        ||
        (Status.bmAuthenticationMethods != RASEO_ProhibitCHAP)||
        (Status.dwNumLines == 100))
    {
        RasPrint(TEXT("Parameters do not match"));
        dwStatus=TPR_FAIL;
    }

    if(dwResult != dwExpectedResult)
    {
        dwStatus=TPR_FAIL;
    }

    if(pStatus)
        LocalFree(pStatus);

    CleanupServer();    
    return dwStatus;
}
