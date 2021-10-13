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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
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

#define BIG_SEND_SIZE		  5000

#define INVALID_TIMEOUT		  -1		
#define INVALID_HANDLE  (HANDLE) 0x00

TESTPROCAPI IcmpAPITest (UINT uMsg, 
						 TPPARAM tpParam, 
						 LPFUNCTION_TABLE_ENTRY lpFTE) {
	
	/* Local variable declarations */
	DWORD					dwStatus = TPR_PASS;
	DWORD					ParseReplies = 0;
	HANDLE					hIcmp;
	char					*SendBuffer, *RcvBuffer;
    DWORD					dwRcvSize;
    DWORD					dwNumReplies=1;
	WORD					dwSendSize		= DEFAULT_SEND_SIZE;
    DWORD					dwErrorCode;
    DWORD					dwErrorExpected;
	int						iErr;
	DWORD					iCounter=0;
	
   	// Send option variables and default initialization
    PIP_OPTION_INFORMATION	pSendOpts;
	UCHAR					TTL				= DEFAULT_TTL;
    UCHAR					*Opt			= (UCHAR *)0; 
    UINT					OptLength		= 0;
    UCHAR					TOS				= DEFAULT_TOS;
    UCHAR					Flags			= 0;
	
	SOCKADDR_IN				saDestinationV4;
	SOCKADDR_IN6			saDestinationV6;
	SOCKADDR_IN6			saSourceV6;
	LPSOCKADDR				lpsaDest;
	
    ULONG					ulTimeout		= DEFAULT_TIMEOUT;
	
	PICMP_ECHO_REPLY		pReplyV4;
	PICMPV6_ECHO_REPLY		pReplyV6;
	DWORD					dwReplyLenV6=sizeof(pReplyV6);
	DWORD					dwReplyLenV4=sizeof(pReplyV4);
	
	WSAData					WsaData;
	
	/* Initialize variable for this test */
	
    // Check our message value to see why we have been called
    if (uMsg == TPM_QUERY_THREAD_COUNT) {
		((LPTPS_QUERY_THREAD_COUNT)tpParam)->dwThreadCount = 0 /* DEFAULT_THREAD_COUNT */;
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
    pSendOpts->OptionsData	= Opt;
    pSendOpts->OptionsSize	= OptLength;
    pSendOpts->Ttl			= TTL;
    pSendOpts->Tos			= TOS;
    pSendOpts->Flags		= Flags;
		
	// Any IPvX specific variables?
	if (USE_IPv6 & lpFTE->dwUserData) {
		// Here we set up variables to invoke Icmp6SendEcho()

		hIcmp = Icmp6CreateFile();
		
		// setting the dest address to some decent value
		saDestinationV6 = g_saServerAddrV6;
		
		// We set the source address to 0s.
		memset(&saSourceV6, 0, sizeof(SOCKADDR_IN6));
		
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
	dwErrorExpected = 0;
	
	// This depends on the particular test case
	switch(LOWORD(lpFTE->dwUserData)) {	
		// cases for both IPv4 and IPv6. 
	case ICMP_INVALID_HANDLE:
		hIcmp = INVALID_HANDLE;
#ifndef UNDER_CE
		// For NT, the handle is actually valid, so it fails.
		dwErrorExpected = 6;
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
			dwErrorExpected = ERROR_INVALID_PARAMETER;
		}
		break;
	case ICMP_NULL_SEND_BUFFER:
		// we will send a NULL as the send buffer
		SendBuffer = NULL;
		break;
	case ICMP_SEND_BUFFER_NO_FRAG:
		// Dont fragment the sendbuffer
		pSendOpts->Flags = IP_FLAG_DF;

		if (!(USE_IPv6 & lpFTE->dwUserData)) {
			// Only for IPv4 cases
			dwErrorExpected = IP_PACKET_TOO_BIG;
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
		if ((!(USE_IPv6 & lpFTE->dwUserData))) {
			if (USE_ASYNC & lpFTE->dwUserData) {
				dwErrorExpected = 998;
			}
			else {
				dwErrorExpected = 87;
			}
		}
		else {
			dwErrorExpected = 998;
		}
		break;
	case ICMP_INVALID_REPLY_LENGTH:
		dwRcvSize = 0;
		if ((!(USE_IPv6 & lpFTE->dwUserData))) {
			dwErrorExpected = 122;
		}
		else {
			dwErrorExpected = 122;
		}
		break;
	case ICMP_INVALID_SEND_OPTIONS:
		// the send options pointer is set to NULL here.
		pSendOpts = NULL;
		break;
	case ICMP_INVALID_TIMEOUT:
		// Timeout is set to some invalid value
		ulTimeout = INVALID_TIMEOUT;
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
		dwErrorExpected = ERROR_INVALID_PARAMETER;
		if (USE_IPv6 & lpFTE->dwUserData) {
			dwErrorExpected = 11015;
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
		RETAILMSG(1, (_T("Unknown Test case ID: %d\r\n"), LOWORD(lpFTE->dwUserData)));
		
		dwStatus = TPR_SKIP;
		goto Cleanup;
	}
	
	/* Call the API with the variable we set up above */
	
	// The API to be called depends on the following:
	// 1) is IPv6 to be used?
	// 2) for IPv4, are we using the async version or normal ?
	if (USE_IPv6 & lpFTE->dwUserData) {
		// Icmp6SendEcho2():This is the only ICMPv6 API.
		dwNumReplies = Icmp6SendEcho2(
			hIcmp,
			NULL,				// Event param NULL. 
			NULL,				// APC param unsupported on CE
			NULL,				// APC param unsupported on CE
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
	}
	else if (USE_ASYNC & lpFTE->dwUserData) {
		// IcmpSendEcho2(): This is the async version of the IcmpSendEcho() API
		// only for IPv4.
		dwNumReplies = IcmpSendEcho2(
			hIcmp,
			NULL,
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
	}
	else {
		// IPv4 case. Call IcmpSendEcho()
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
#ifndef UNDER_CE
	case ICMP_INVALID_HANDLE:
#endif
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
			
			if (dwErrorCode == dwErrorExpected) {
				// The error code is as expected.
				
				// Test is over for these cases so clean up
				dwStatus = TPR_PASS;	
			}
			else {
				// Expected a different error. This is a fail
				RETAILMSG(1,(_T("Expected error: %d. Received %d"), 
					dwErrorExpected, dwErrorCode));
				dwStatus = TPR_FAIL;
			}
		}
		else {
			// We didnt expect this test to pass. We expected 0 replies
			// yet we received more than that. So this test is a fail
			
			// Before exiting we can check here to see what was returned
			// even though there shouldnt have been anything returned here.
			RETAILMSG(1,(_T("Expected error %d, received None."), dwErrorExpected));
			dwStatus = TPR_FAIL;
		}
		goto Cleanup;
	} // switch	
	
	// We expected this test to pass...
	if (dwNumReplies == 0) {
		dwErrorCode = GetLastError();
		RETAILMSG(1,(_T("Did NOT expect any error. Received error %d"), dwErrorCode));
		
		dwStatus = TPR_FAIL;
		goto Cleanup;
	}
	else {
		// So far so good. Process to see what the response buffer looks like
		// if it contains all the expected information. Is there anything that 
		// shouldnt have been there and vice versa, is there something that 
		// should have been there but is not there?
		if (!(USE_IPv6 & lpFTE->dwUserData)) {
			PICMP_ECHO_REPLY		pReplyParseV4;
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
			PICMPV6_ECHO_REPLY		pReplyParseV6;
			
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
	
	IcmpCloseHandle(hIcmp);
	
	// any allocated memory?
	if (pSendOpts) LocalFree(pSendOpts);
	if (SendBuffer) LocalFree(SendBuffer);
	if (RcvBuffer) LocalFree(RcvBuffer);
		
	/* End */
	return dwStatus;
}
