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

#define INVALID_USER_NAME TEXT("~!@#$%^&*()_+")
#define NULL_USER_NAME TEXT("")
#define LONG_USER_NAME TEXT("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz")

TESTPROCAPI RasServerUserSetCredentials(UINT uMsg, 
                         TPPARAM tpParam, 
                         LPFUNCTION_TABLE_ENTRY lpFTE) 
{    
    HANDLE                     hHandle = NULL;
    RASCNTL_SERVERUSERCREDENTIALS Credentials;
    DWORD                 cbStatus    = 0,
                         dwResult    = 0,
                         dwExpectedResult = ERROR_SUCCESS,
                         iLine        = 0;
    
    PBYTE                         pBufIn=(PUCHAR)&Credentials;    
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
        dwResult = RasIOControl(NULL, RASCNTL_SERVER_USER_SET_CREDENTIALS, NULL, 0, NULL, 0, &cbStatus);
        RasPrint(TEXT("RasIOControl() FAIL'ed (%d) AS EXPECTED "), dwResult);
        return (dwResult==ERROR_NOT_SUPPORTED? TPR_PASS: TPR_FAIL);
    }
    
    //
    // Initialize the Credentials structure with valid information
    //
    _tcsncpy_s(Credentials.tszUserName, TEXT("Test"), UNLEN+1);
    _tcsncpy_s(Credentials.tszDomainName, TEXT(""), DNLEN+1);
    Credentials.cbPassword = WideCharToMultiByte (CP_OEMCP, 0,
                                TEXT("Test"), 5,
                                (char*)Credentials.password, sizeof(Credentials.password), NULL, NULL );


    switch(LOWORD(lpFTE->dwUserData))
    {            
        case RASSERVER_INVALID_USER_NAME:
            memset(&Credentials.tszUserName, 0, UNLEN+1);
            _tcsncpy_s(Credentials.tszUserName, INVALID_USER_NAME, UNLEN+1);
            dwExpectedResult=ERROR_BAD_USERNAME;
            break;

        case RASSERVER_NULL_USER_NAME:
            memset(&Credentials.tszUserName, 0, UNLEN+1);
            _tcsncpy_s(Credentials.tszUserName, NULL_USER_NAME, UNLEN+1);
            dwExpectedResult=ERROR_INVALID_PARAMETER;
            break;
        
        case RASSERVER_LONG_USER_NAME:
            memset(&Credentials.tszUserName, 0, UNLEN+1);
            //
            // The following is a test for buffer over run. As a result, prefast complains
            //
            _tcscpy_s(Credentials.tszUserName, LONG_USER_NAME);
            break;
            
        case RASSERVER_LONG_PASSWORD:
            memset(&Credentials.password, 0, PWLEN);
            Credentials.cbPassword = WideCharToMultiByte (CP_OEMCP, 0,
                                LONG_USER_NAME, wcslen(LONG_USER_NAME),
                                (char*)Credentials.password, 5, NULL, NULL);
            break;
            
        case RASSERVER_INVALID_PASSWORD:
            Credentials.cbPassword = PWLEN + 5;
            break;
            
        case RASSERVER_INTL_USERNAMES:
            RasPrint(TEXT("Skipping RASSERVER_INTL_USERNAMES..."));
            return TPR_SKIP;

    }

    //
    // Call the IOCTL
    //
    dwResult = RasIOControl(hHandle, 
                            RASCNTL_SERVER_USER_SET_CREDENTIALS, 
                            pBufIn, sizeof(Credentials), 
                            pBufOut, dwLenOut, 
                            &cbStatus);

    RasPrint(TEXT("dwResult = %d\tdwExpectedResult = %d"), dwResult, dwExpectedResult);    

    if (dwResult != dwExpectedResult)
    {
        dwStatus=TPR_FAIL;
    }

    //
    // Did the test pass or fail?
    //
    
    return dwStatus;
}
