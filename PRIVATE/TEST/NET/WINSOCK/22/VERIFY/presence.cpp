//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "ws2bvt.h"

TESTPROCAPI PresenceTest (UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE) {
	
	/* Local variable declarations */
	WSAData				wsaData;
	int					nError, nFamily, nType, nProto;
	SOCKET				sock = INVALID_SOCKET;
	
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
		//dwStatus = TPR_FAIL;
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