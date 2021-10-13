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
#include "IcmpTest.h"

extern BOOL g_IPv6Detected;

#define MAX_BUFFER_SIZE       (sizeof(ICMP_ECHO_REPLY) + 0xfff7 + MAX_OPT_SIZE)
#define DEFAULT_BUFFER_SIZE   (0x2000 - 8)
#define DEFAULT_SEND_SIZE     32
#define DEFAULT_COUNT         4
#define DEFAULT_TTL           32
#define DEFAULT_TOS           0
#define DEFAULT_TIMEOUT       15000L
#define MIN_INTERVAL          1000L

#define BIG_SEND_SIZE         5000

#define INVALID_TIMEOUT       -1
#define INVALID_HANDLE       (HANDLE) 0x00

TESTPROCAPI IcmpAPITest (UINT uMsg, 
                         TPPARAM tpParam, 
                         LPFUNCTION_TABLE_ENTRY lpFTE) {
    
    /* Local variable declarations */
    BOOL                    fExceptionExpected = FALSE;
    BOOL                    fExceptionReceived = FALSE;
    DWORD                   dwStatus = TPR_PASS;
    DWORD                   ParseReplies = 0;
    HANDLE                  hIcmp;
    char                    *SendBuffer, *RcvBuffer;
    DWORD                   dwRcvSize;
    DWORD                   dwNumReplies=1;
    WORD                    dwSendSize = DEFAULT_SEND_SIZE;
    DWORD                   dwErrorCode;
    DWORD                   dwErrorExpected;
    int                     iErr;
    DWORD                   iCounter=0;
    
    // Send option variables and default initialization
    PIP_OPTION_INFORMATION  pSendOpts;
    UCHAR                   TTL             = DEFAULT_TTL;
    UCHAR                   *Opt            = (UCHAR *)0; 
    UCHAR                   OptLength       = 0;
    UCHAR                   TOS             = DEFAULT_TOS;
    UCHAR                   Flags           = 0;
    
    SOCKADDR_IN             saDestinationV4;
    SOCKADDR_IN6            saDestinationV6;
    SOCKADDR_IN6            saSourceV6;
    LPSOCKADDR              lpsaDest;
    
    ULONG                   ulTimeout       = DEFAULT_TIMEOUT;
    
    PICMP_ECHO_REPLY        pReplyV4;
    PICMPV6_ECHO_REPLY      pReplyV6;
    DWORD                   dwReplyLenV6=sizeof(pReplyV6);
    DWORD                   dwReplyLenV4=sizeof(pReplyV4);
    
    WSAData                 WsaData;
    
    /* Initialize variable for this test */
    // We set the source address to 0s.
    memset(&saSourceV6, 0, sizeof(SOCKADDR_IN6));
    memset(&saDestinationV4, 0, sizeof(SOCKADDR_IN));

    // Check our message value to see why we have been called
    if (uMsg == TPM_QUERY_THREAD_COUNT) {
        /* DEFAULT_THREAD_COUNT */
        ((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0;
        return TPR_HANDLED;
    } 
    else if (uMsg != TPM_EXECUTE) {
        return TPR_NOT_HANDLED;
    }
    
    //
    // If IPv6 enabled on this system? Otherwise, simply skip
    //
    if ((USE_IPv6 & lpFTE->dwUserData) && (!g_IPv6Detected))
    {
        RETAILMSG(1, (TEXT("Skipping...")));
        
        return TPR_SKIP;
    }
    
    iErr = WSAStartup(MAKEWORD(2,2), &WsaData);
    
    if (iErr) {
        RETAILMSG(1,(TEXT("WsaStartup Failed: %d"), GetLastError()));
        
        return TPR_SKIP;
    }
        
    // Create memory for send/recv buffers
    SendBuffer = (char *)LocalAlloc(LMEM_FIXED, dwSendSize);
    
    if (SendBuffer == NULL) {
        RETAILMSG(1,(_T("Failed to allocate mem for SendBuffer")));
        
        return TPR_SKIP;
    }
    
    // Calculate receive buffer size and try to allocate it.
    if (dwSendSize <= DEFAULT_SEND_SIZE) {
        dwRcvSize = DEFAULT_BUFFER_SIZE;
    } 
    else {
        dwRcvSize = MAX_BUFFER_SIZE;
    }
    
    RcvBuffer = (char *)LocalAlloc(LMEM_FIXED, dwRcvSize);
    
    if (RcvBuffer == NULL) {
        RETAILMSG(1,(_T("Failed to allocate mem for RecvBuffer")));
        
        // Since this test is skipped, free the allocated send buffer
        LocalFree(SendBuffer);
        
        return TPR_SKIP;
    }
    
    pSendOpts = (PIP_OPTION_INFORMATION)LocalAlloc(LMEM_FIXED, sizeof(IP_OPTION_INFORMATION));
    
    if (pSendOpts == NULL) {
        RETAILMSG(1,(_T("Failed to allocate mem for pSendOpts")));
        
        // Since this test is skipped, free the allocated recv buffer
        LocalFree(SendBuffer);
        LocalFree(RcvBuffer);
        
        return TPR_SKIP;
    }
    
    // Initialize send buffer
    for (; iCounter<dwSendSize; iCounter++) {
        // Make the send buffer a string of chars repetitively.
        SendBuffer[iCounter] = (char)('A' + (iCounter % 23));
    }
    
    // Initialize the send options
    pSendOpts->OptionsData  = Opt;
    pSendOpts->OptionsSize  = OptLength;
    pSendOpts->Ttl          = TTL;
    pSendOpts->Tos          = TOS;
    pSendOpts->Flags        = Flags;
        
    // Any IPvX specific variables?
    if (USE_IPv6 & lpFTE->dwUserData) {
        // Here we set up variables to invoke Icmp6SendEcho()

        hIcmp = Icmp6CreateFile();
        
        // setting the dest address to some decent value
        saDestinationV6 = g_saServerAddrV6;
                
        lpsaDest = (SOCKADDR*)&saDestinationV6;
    }
    else {
        // Here we set up variables to invoke IcmpSendEcho()
        
        // setting the dest address to some decent value
        saDestinationV4 = g_saServerAddr;

        hIcmp = IcmpCreateFile();
        
        lpsaDest = (SOCKADDR*)&saDestinationV4;
    }
    
    /* Set up environment to perform this particular test. */
    
    // Unless otherwise specified, we dont expect an error.
    dwErrorExpected    = 0;
    fExceptionExpected = FALSE;
    fExceptionReceived = FALSE;
    
    // This depends on the particular test case
    switch(LOWORD(lpFTE->dwUserData)) {
        // cases for both IPv4 and IPv6. 
    case ICMP_INVALID_HANDLE:
        hIcmp = INVALID_HANDLE;
#ifndef UNDER_CE
        // For NT, a zero handle is invalid
        dwErrorExpected = ERROR_INVALID_HANDLE;
#else
        // For CE (with NetIO), a zero handle will cause an AV exception
        fExceptionExpected = TRUE;
        dwErrorExpected = ERROR_INVALID_PARAMETER;
#endif
        break;
    case ICMP_NULL_ADDR:
        if (USE_IPv6 & lpFTE->dwUserData) {
            // for ipv6 we set the SOCKADDR_IN6 to NULL
            lpsaDest = NULL;
        }
        else {
            // for ipv4 we set the addr value to 0
            saDestinationV4.sin_addr.s_addr = 0;
            dwErrorExpected = ERROR_INVALID_NETNAME;
        }
        break;
    case ICMP_NULL_SEND_BUFFER:
        // we will send a NULL as the send buffer
        SendBuffer = NULL;
        dwErrorExpected = ERROR_NO_SYSTEM_RESOURCES;
        break;
    case ICMP_SEND_BUFFER_NO_FRAG:
        // Dont fragment the sendbuffer
        pSendOpts->Flags = IP_FLAG_DF;

        if (!(USE_IPv6 & lpFTE->dwUserData)) {
            // Only for IPv4 cases when not using localhost
            if(saDestinationV4.sin_addr.S_un.S_addr != 0x0100007F)
                dwErrorExpected = IP_PACKET_TOO_BIG;
        }
        //for the ASYNC case it will be 
        if (USE_ASYNC & lpFTE->dwUserData)
        {
            dwErrorExpected = ERROR_IO_PENDING;
        }
        // break; Intentionally no break here. We want this to fall in the case below.
    case ICMP_SEND_BUFFER_FRAG:
        // Create memory for big send/recv buffers, so that we are forced to 
        // fragment
        SendBuffer = (char *)LocalAlloc(LMEM_FIXED, BIG_SEND_SIZE);
        dwSendSize = BIG_SEND_SIZE;

        if (SendBuffer == NULL) {
            RETAILMSG(1,(_T("Failed to allocate mem for SendBuffer")));
            
            return TPR_SKIP;
        }
        
        if (dwSendSize <= DEFAULT_SEND_SIZE) {
            dwRcvSize = DEFAULT_BUFFER_SIZE;
        } 
        else {
            dwRcvSize = MAX_BUFFER_SIZE;
        }
        
        dwRcvSize = dwSendSize + sizeof(ICMPV6_ECHO_REPLY);
        RcvBuffer = (char *)LocalAlloc(LMEM_FIXED, dwRcvSize);
        
        if (RcvBuffer == NULL) {
            RETAILMSG(1,(_T("Failed to allocate mem for RecvBuffer")));
            
            // Since this test is skipped, free the allocated send buffer
            LocalFree(SendBuffer);
            
            return TPR_SKIP;
        }
        
        // Initialize send buffer
        for (; iCounter<dwSendSize; iCounter++) {
            // Make the send buffer a string of chars repetitively.
            SendBuffer[iCounter] = (char)('A' + (iCounter % 23));
        }
        
        break;
    case ICMP_SEND_BUFFER_TOO_SMALL:
        // We will give the correct instantiated send buffer, 
        // but the size is zero
        dwSendSize = 0;
        break;
    case ICMP_INVALID_SEND_LEN:
        // Setting the size of send buffer to invalid
        dwSendSize = 0;
        break;
    case ICMP_NULL_REPLY_BUFFER:
        RcvBuffer = NULL;
        dwErrorExpected = ERROR_INVALID_PARAMETER;
        break;
    case ICMP_INVALID_REPLY_LENGTH:
        dwRcvSize = 0;
        dwErrorExpected = ERROR_INVALID_PARAMETER;
        break;
    case ICMP_INVALID_SEND_OPTIONS:
        // the send options pointer is set to NULL here.
        pSendOpts = NULL;
        break;
    case ICMP_UNREACH_DESTINATION:
        // Invalid address here
        if (USE_IPv6 & lpFTE->dwUserData) {
            // for ipv6 we set the SOCKADDR_IN6 to NULL
            saDestinationV6 = saSourceV6; // saSourceV6 is all zeros. 
        }
        else {
            // for ipv4 we set the addr value to 0
            saDestinationV4.sin_addr.s_addr = 0;
        }
        dwErrorExpected = ERROR_INVALID_NETNAME;
        if (USE_IPv6 & lpFTE->dwUserData) {
            dwErrorExpected = ERROR_INVALID_NETNAME;
        }
        break;
    case ICMP_VALID_REQUEST:
        // No changes required here. The default inits are used
        break;

    case ICMP_INVALID_BUFFER:
        DWORD dwReplies;
        if (USE_IPv6 & lpFTE->dwUserData) {
            pReplyV6 = NULL;
            dwReplies = Icmp6ParseReplies(pReplyV6, dwReplyLenV6);
            if(dwReplies == 0) {
                RETAILMSG(1, (_T("No replies received. Test passing...\r\n")));
                // This is good, expected.
                dwStatus = TPR_PASS;
            }
            else {
                RETAILMSG(1, (_T("Received replies when none expected. Test failed...\r\n")));
                // Something went wrong. We shouldnt be here.
                dwStatus = TPR_FAIL;
            }
            // Test is over for this case.
            goto Cleanup;
        }
        else {
            pReplyV4 = NULL;
            dwReplies = IcmpParseReplies(pReplyV4, dwReplyLenV4);
            if (dwReplies == 0) {
                RETAILMSG(1, (_T("No replies received. Test passing...\r\n")));
                // This is good, expected.
                dwStatus = TPR_PASS;
            }
            else {
                // Something went wrong. We shouldnt be here.
                RETAILMSG(1, (_T("Received replies when none expected. Test failed...\r\n")));
                dwStatus = TPR_FAIL;
            }
            // Test is over for this case.
            goto Cleanup;
        }
        
        break;
    case ICMP_VALID_PARAMS:
        // We dont need to do anything here.
        break;
    case ICMP_INVALID_LEN:
        // We will use these lengths after call to async send echos.
        if (USE_IPv6 & lpFTE->dwUserData) {
            dwReplyLenV6 = 0;
        }
        else {
            dwReplyLenV4 = 0;
        }

        break;
    
    // We should never get here, but just in case... 
    default:
        RETAILMSG(1, 
                  (_T("Unknown Test case ID: %lu\r\n"), 
                  LOWORD(lpFTE->dwUserData)));
        
        dwStatus = TPR_SKIP;
        goto Cleanup;
    }
    
    /* Call the API with the variable we set up above */
    
    // The API to be called depends on the following:
    // 1) is IPv6 to be used?
    // 2) for IPv4, are we using the async version or normal ?
    if (USE_IPv6 & lpFTE->dwUserData) {
        // Icmp6SendEcho2():This is the only ICMPv6 API.

        HANDLE IcmpEvent6 = NULL;
        if(USE_ASYNC & lpFTE->dwUserData)
            IcmpEvent6 = CreateEvent(NULL,TRUE,FALSE,NULL);

        __try 
        {
            //This event stays signaled even if I am not waiting.
            
            dwNumReplies = Icmp6SendEcho2(
                hIcmp,
                IcmpEvent6,         // Event param NULL. 
                NULL,               // APC param unsupported on CE
                NULL,               // APC param unsupported on CE
                &saSourceV6,
                &saDestinationV6,
                SendBuffer,
                dwSendSize,
                pSendOpts,
                RcvBuffer,
                dwRcvSize,
                ulTimeout
                );
            RETAILMSG(1, (_T("Icmp6SendEcho2() returned %d\r\n"), dwNumReplies));
            //Wait for the event to be signaled, the ping should return the timeout amount plus 5 seconds.
            if(IcmpEvent6)
            {
                DWORD dwWaitStatus6 = WaitForSingleObject(IcmpEvent6, ulTimeout + 5000);

                if (dwWaitStatus6 != WAIT_OBJECT_0)
                {
                    //The test failed to recieve anything
                    dwNumReplies = 0;
                }
            }
        }
       __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
       {
           fExceptionReceived = TRUE;
           dwNumReplies = 0;
       }
       if(IcmpEvent6)
           CloseHandle(IcmpEvent6);        
    }
    else if (USE_ASYNC & lpFTE->dwUserData) {
        // IcmpSendEcho2(): This is the async version of the IcmpSendEcho() API
        //This event stays signaled even if I am not waiting.
        HANDLE IcmpEvent = CreateEvent(NULL,TRUE,FALSE,NULL);

        // only for IPv4.
        __try
        {
            
            dwNumReplies = IcmpSendEcho2(
                hIcmp,
                IcmpEvent,
                NULL,
                NULL,
                saDestinationV4.sin_addr.s_addr,
                SendBuffer,
                dwSendSize,
                pSendOpts,
                RcvBuffer,
                dwRcvSize,
                ulTimeout
                );
            RETAILMSG(1, (_T("IcmpSendEcho2() returned %d\r\n"), dwNumReplies));
            
            //Wait for the event to be signaled, the ping should return the timeout amount plus 5 seconds.
            DWORD dwWaitStatus = WaitForSingleObject(IcmpEvent, ulTimeout + 5000);
            if (dwWaitStatus != WAIT_OBJECT_0)
            {
                //The test failed to recieve anything
                dwNumReplies = 0;
            }
        

        }
       __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
       {
           fExceptionReceived = TRUE;
           dwNumReplies = 0;
       }
       CloseHandle(IcmpEvent);

    }
    else {
        // IPv4 case. Call IcmpSendEcho()
        __try
        {
            dwNumReplies = IcmpSendEcho(
                hIcmp,
                saDestinationV4.sin_addr.s_addr,
                SendBuffer,
                dwSendSize, 
                pSendOpts,
                RcvBuffer,
                dwRcvSize, 
                ulTimeout
                );
            RETAILMSG(1, (_T("IcmpSendEcho() returned %d\r\n"), dwNumReplies));
        }
       __except(EXCEPTION_EXECUTE_HANDLER)
       {
           fExceptionReceived = TRUE;
           dwNumReplies = 0;
       }

    } // else. IPv4 cases only
    
    /* Check to see if the API behaved as we expected it to */
    
    switch(LOWORD(lpFTE->dwUserData)) {
    // Putting this case at the top. Required.
    case ICMP_SEND_BUFFER_NO_FRAG:
    case ICMP_NULL_ADDR:
        // Only for IPv4 case
        if (USE_IPv6 & lpFTE->dwUserData) {
            break;
        }
    case ICMP_INVALID_HANDLE:
    case ICMP_NULL_SEND_BUFFER:
    case ICMP_INVALID_REPLY_LENGTH:
    case ICMP_NULL_REPLY_BUFFER:
    case ICMP_UNREACH_DESTINATION:
        if (dwNumReplies == 0) {
            // We didnt expect to receive any replies in this case
            // so this test is fine till now.
            
            // Just check the error value that we received. If 
            // this is also what was expected then this test is
            // a pass.
            dwErrorCode = GetLastError();
            
            if (dwErrorCode == dwErrorExpected 
                || (fExceptionExpected && fExceptionReceived))
            {
                // The error code is as expected.
                
                // Test is over for these cases so clean up
                dwStatus = TPR_PASS;
            }
            else if (fExceptionReceived)
            {
                // didn't expect this exception, so fail
                RETAILMSG(1,(_T("Unexpected exception received!"))); 
                dwStatus = TPR_FAIL;
            }
            else
            {
                // Expected a different error. This is a fail
                RETAILMSG(1,(_T("Expected error: %lu. Received %lu"), 
                    dwErrorExpected, dwErrorCode));
                dwStatus = TPR_FAIL;
            }
        }
        else {
            // We didnt expect this test to pass. We expected 0 replies
            // yet we received more than that. So this test is a fail
            
            // Before exiting we can check here to see what was returned
            // even though there shouldnt have been anything returned here.
            if(dwErrorExpected)
            {
                RETAILMSG(1,(_T("Expected error %lu, received None."), dwErrorExpected));
                dwStatus = TPR_FAIL;
            }
        }
        goto Cleanup;
    } // switch
    
    // We expected this test to pass...
    if (dwNumReplies == 0 && !(USE_ASYNC & lpFTE->dwUserData)) {
        dwErrorCode = GetLastError();
        RETAILMSG(1,(_T("Did NOT expect any error. Received error %lu"), dwErrorCode));
        
        dwStatus = TPR_FAIL;
        goto Cleanup;
    }
    else {

        // So far so good. Process to see what the response buffer looks like
        // if it contains all the expected information. Is there anything that 
        // shouldnt have been there and vice versa, is there something that 
        // should have been there but is not there?
        if (!(USE_IPv6 & lpFTE->dwUserData)) {
            PICMP_ECHO_REPLY        pReplyParseV4;
            pReplyV4 = (PICMP_ECHO_REPLY) RcvBuffer;
            pReplyParseV4 = (PICMP_ECHO_REPLY) RcvBuffer;
            
            // call Icmp6ParseReplies. Only if the Call was IcmpSendEcho2

            if (USE_ASYNC & lpFTE->dwUserData) {
                ParseReplies = IcmpParseReplies (pReplyParseV4, dwReplyLenV4);
                
                switch(LOWORD(lpFTE->dwUserData)) {
                case ICMP_INVALID_LEN:
                    if (ParseReplies == 0) {
                        RETAILMSG(1, (_T("No replies received. Test passing...\r\n")));
                        // This case should have failed, and it did. Test Pass.
                        dwStatus = TPR_PASS;
                    }
                    else {
                        RETAILMSG(1, (_T("Received replies when none expected. Test Failed...\r\n")));
                        // This test has failed.
                        dwStatus = TPR_FAIL;
                    }
                    goto Cleanup;
                case ICMP_VALID_PARAMS:
                    if (ParseReplies == 0) {
                        // This case should have passed, and it did not. Test Fail.
                        dwStatus = TPR_FAIL;
                    }
                    else {
                        // This test has failed.
                        dwStatus = TPR_PASS;
                    }
                    goto Cleanup;
                }
            }
            
            // Parse the replies received
            while (dwNumReplies--) {
                if (pReplyV4->Status == IP_SUCCESS) {
                    RETAILMSG(1, (_T("Echo size=%d "), pReplyV4->DataSize));
                    
                    if (pReplyV4->DataSize != dwSendSize) {
                        RETAILMSG(1, (_T("(sent %d) "), dwSendSize));
                    }
                    else {
                        char *sendptr, *recvptr;
                        
                        sendptr = &(SendBuffer[0]);
                        recvptr = (char *) pReplyV4->Data;
                        
                        if (!sendptr) {
                            // If either of these is null, dont compare.
                            continue;
                        }

                        int j=0;
                        for (j = 0; j < dwSendSize; j++)
                            if (*sendptr++ != *recvptr++) {
                                RETAILMSG(1, (_T("- MISCOMPARE at offset %d - "), j));
                                break;
                            }
                    }
                    
                    if (pReplyV4->RoundTripTime) {
                        RETAILMSG(1, (_T("time=%lums "), pReplyV4->RoundTripTime));
                    }
                    else {
                        RETAILMSG(1, (_T("time<1ms ")));
                    }
                    
                    RETAILMSG(1, (_T("TTL=%u\r\n"), (UINT)pReplyV4->Options.Ttl));
                }
                else {
                    RETAILMSG(1, (_T("Error %d\r\n"), pReplyV4->Status));
                }
                
                pReplyV4++;
            }
        }
        else {
            PICMPV6_ECHO_REPLY      pReplyParseV6;
            
            // V6 parsing.
            pReplyV6 = (PICMPV6_ECHO_REPLY) RcvBuffer;
            pReplyParseV6 = (PICMPV6_ECHO_REPLY) RcvBuffer;
            
            // call Icmp6ParseReplies

            ParseReplies = Icmp6ParseReplies(pReplyParseV6, dwReplyLenV6);
            
            switch(LOWORD(lpFTE->dwUserData)) {
            case ICMP_INVALID_LEN:
                if (ParseReplies == 0) {
                    RETAILMSG(1, (_T("No replies received. Test passing...\r\n")));
                    // This case should have failed, and it did. Test Pass.
                    dwStatus = TPR_PASS;
                }
                else {
                    RETAILMSG(1, (_T("Received replies when none expected. Test Failed...\r\n")));
                    // This test has failed.
                    dwStatus = TPR_FAIL;
                }
                goto Cleanup;
            case ICMP_VALID_PARAMS:
                if (ParseReplies == 0) {
                    // This case should have passed, and it did not. Test Fail.
                    dwStatus = TPR_FAIL;
                }
                else {
                    // This test has failed.
                    dwStatus = TPR_PASS;
                }
                goto Cleanup;
            }
            // Parse the replies received
            while (dwNumReplies--) {
                if (pReplyV6->Status == IP_SUCCESS) {
                    RETAILMSG(1, (_T("sent %d"), dwSendSize));
                }
                else {
                    RETAILMSG(1, (_T("Error %d\r\n"), pReplyV6->Status));
                }
                
                pReplyV6++;
            } // while
        } // else IP version
    } // dwNumReplies == 0
    
    
    /* clean up */
Cleanup:
    // close the handle
    if (INVALID_HANDLE != hIcmp)
    {
        IcmpCloseHandle(hIcmp);
    }
    
    // any allocated memory?
    if (pSendOpts) LocalFree(pSendOpts);
    if (SendBuffer) LocalFree(SendBuffer);
    if (RcvBuffer) LocalFree(RcvBuffer);
        
    /* End */
    return dwStatus;
}
