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

TESTPROCAPI RasServerUserDeleteCredentials(UINT uMsg, 
                         TPPARAM tpParam, 
                         LPFUNCTION_TABLE_ENTRY lpFTE) 
{    
    DWORD                    dwStatus = TPR_PASS,
                            cbStatus = 0,
                            dwResult = 0,
                            iLine = 0,
                            dwExpectedResult = ERROR_SUCCESS;
    HANDLE                    hHandle=NULL;
    RASCNTL_SERVERUSERCREDENTIALS Credentials;
    PBYTE                    pBufIn=(PUCHAR)&Credentials;    
    PBYTE                    pBufOut=NULL;
    DWORD                    dwLenIn = 0;    
    DWORD                    dwLenOut = 0;

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
        dwResult = RasIOControl(NULL, RASCNTL_SERVER_USER_DELETE_CREDENTIALS, NULL, 0, NULL, 0, &cbStatus);
        RasPrint(TEXT("RasIOControl() FAIL'ed (%d) AS EXPECTED "), dwResult);
        return (dwResult==ERROR_NOT_SUPPORTED? TPR_PASS: TPR_FAIL);
    }
    
    //    
    // Initialize the credential struct to delete and then add this valid user
    //
    _tcscpy_s(Credentials.tszUserName, TEXT("Test"));
    _tcscpy_s(Credentials.tszDomainName, TEXT(""));
    Credentials.cbPassword = WideCharToMultiByte (CP_OEMCP, 0,
                                                TEXT("Test"), 5,
                                                (char*)Credentials.password, 
                                                sizeof(Credentials.password), 
                                                NULL, NULL);

    dwResult = RasIOControl(NULL, RASCNTL_SERVER_USER_SET_CREDENTIALS, 
                            (PUCHAR)&Credentials, sizeof(Credentials), 
                            NULL, 0, &cbStatus);
    if (dwResult != ERROR_SUCCESS)
    {
        RasPrint(TEXT("Unable to add user: Test"));
        return TPR_SKIP;
    }

    switch(LOWORD(lpFTE->dwUserData)) 
    {
        case RASSERVER_USER_EXISTS:
            break;
            
        case RASSERVER_USER_NOT_EXISTS:
            _tcsncpy_s(Credentials.tszUserName, TEXT("TestTestTest"), UNLEN+1);
            dwExpectedResult=ERROR_BAD_USERNAME;
            break;
            
        case RASSERVER_USER_INTL_NAME:

            RasPrint(TEXT("Skipping RASSERVER_USER_INTL_NAME..."));
            return TPR_SKIP;
    }

    //
    // Call the IOCTL now
    // 
    dwResult = RasIOControl(hHandle, 
                            RASCNTL_SERVER_USER_DELETE_CREDENTIALS, 
                            pBufIn, sizeof(Credentials), 
                            pBufOut, dwLenOut, 
                            &cbStatus);
    //
    // Did it pass?
    //
    RasPrint(TEXT("dwResult = %d\tdwExpectedResult = %d"), dwResult, dwExpectedResult);    

    if (dwExpectedResult != dwResult)
    {
        // Test Failed.
        dwStatus = TPR_FAIL;
    }

    return dwStatus;
}
