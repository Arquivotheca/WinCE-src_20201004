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

BYTE GetNetSamplePrefix[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xb, 0xc, 0xd, 0xe, 0xf}; 

TESTPROCAPI RasServerGetNetPrefix(UINT uMsg, 
                         TPPARAM tpParam, 
                         LPFUNCTION_TABLE_ENTRY lpFTE) 
{    
    HANDLE    hHandle=NULL;
    DWORD dwResult=ERROR_SUCCESS, dwExpectedResult=ERROR_SUCCESS, cbStatus=0;
    RASCNTL_SERVER_IPV6_NET_PREFIX PrefixValues;
    DWORD                    dwStatus = TPR_PASS;

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
        dwResult = RasIOControl(NULL, RASCNTL_SERVER_GET_IPV6_NET_PREFIX, NULL, 0, NULL, 0, &cbStatus);
        RasPrint(TEXT("RasIOControl() FAIL'ed (%d) AS EXPECTED "), dwResult);
        return (dwResult==ERROR_NOT_SUPPORTED? TPR_PASS: TPR_FAIL);
    }
    
    PrefixValues.IPV6NetPrefixCount=3;
    PrefixValues.IPV6NetPrefixBitLength=48;
    memcpy(&PrefixValues.IPV6NetPrefix, &GetNetSamplePrefix, 16);

    //
    // Set the prefix before retrieving it 
    //
    dwResult = RasIOControl(
                        hHandle, 
                        RASCNTL_SERVER_SET_IPV6_NET_PREFIX, 
                        (PUCHAR)&PrefixValues, sizeof(RASCNTL_SERVER_IPV6_NET_PREFIX), 
                        NULL, 0,
                        &cbStatus
                        );
    if(dwResult!=ERROR_SUCCESS)
    {
        RasPrint(TEXT("RASCNTL_SERVER_SET_IPV6_NET_PREFIX FAILed"));
        return TPR_SKIP;
    }

    switch(LOWORD(lpFTE->dwUserData))
    {
        case RASSERVER_INVALID_RANGE:
        case RASSERVER_VALID_RANGE:
            break;
    }    

    //
    // Call the IOCTL
    //
    dwResult = RasIOControl(
                        hHandle, 
                        RASCNTL_SERVER_GET_IPV6_NET_PREFIX, 
                        NULL, 0, 
                        (PUCHAR)&PrefixValues, sizeof(RASCNTL_SERVER_IPV6_NET_PREFIX), 
                        &cbStatus
                        );
    RasPrint(TEXT("dwResult = %d\tdwExpectedResult = %d"), dwResult, dwExpectedResult);    
    
    if (dwResult != dwExpectedResult)
    {
        dwStatus=TPR_FAIL;
    }

    if (
            (PrefixValues.IPV6NetPrefixCount != 3) ||
            (PrefixValues.IPV6NetPrefixBitLength != 48) ||
            (memcmp(&PrefixValues.IPV6NetPrefix, &GetNetSamplePrefix, 16))
        )
    {
        RasPrint(TEXT("Parameters do NOT match the expected values"));
        dwStatus=TPR_FAIL;
    }

    CleanupServer();
    return dwStatus;
}
