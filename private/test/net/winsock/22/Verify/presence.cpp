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
#include "ws2bvt.h"

TESTPROCAPI PresenceTest (UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) {
    
    /* Local variable declarations */
    WSAData                wsaData;
    int                    nError, nFamily, nType, nProto;
    SOCKET                sock = INVALID_SOCKET;
    
    // Check our message value to see why we have been called
    if (uMsg == TPM_QUERY_THREAD_COUNT) {
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0 /* DEFAULT_THREAD_COUNT */;
        return TPR_HANDLED;
    } 
    else if (uMsg != TPM_EXECUTE) {
        return TPR_NOT_HANDLED;
    }

    /* Initialize variable for this test */
    switch(lpFTE->dwUserData)
    {
    case PRES_IPV6:
        nFamily = AF_INET6;
        break;
    case PRES_IPV4:
        nFamily = AF_INET;
        break;
    default:
        nFamily = AF_UNSPEC;
        break;
    }

    nType = SOCK_STREAM;
    nProto = 0;

    nError = WSAStartup(MAKEWORD(2,2), &wsaData);
    
    if (nError) 
    {
        Log(FAIL, TEXT("WSAStartup Failed: %d"), nError);
        Log(FAIL, TEXT("Fatal Error: Cannot continue test!"));
        return TPR_FAIL;
    }

    sock = socket(nFamily, nType, nProto);

    if(sock == INVALID_SOCKET)
    {
        Log(FAIL, TEXT("socket() failed for Family = %s, Type = %s, Protocol = %s"), 
            GetFamilyName(nFamily),
            GetTypeName(nType),
            GetProtocolName(nProto));
        Log(FAIL, TEXT("***  The %s stack is probably not in the image!  ***"), 
            GetStackName(nFamily));
    }
    else
    {
        closesocket(sock);

        Log(ECHO, TEXT("***  SUCCESS: The %s stack is present!  ***"), 
            GetStackName(nFamily));
    }
    
    /* clean up */
    WSACleanup();
        
    /* End */
    return getCode();
}