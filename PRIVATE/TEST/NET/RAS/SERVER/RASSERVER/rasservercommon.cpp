//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "RasServerTest.h"

#define INVALID_POINTER_INT 0xabcd
extern BOOL bExpectedFail;
 
TESTPROCAPI RasServerCommon(UINT uMsg, 
						 TPPARAM tpParam, 
						 LPFUNCTION_TABLE_ENTRY lpFTE) 
{	
	DWORD					dwStatus = TPR_PASS;
	HANDLE					hHandle=INVALID_HANDLE_VALUE;
	DWORD dwResult=ERROR_SUCCESS, dwExpectedResult=ERROR_SUCCESS;
	INT	i = INVALID_POINTER_INT;
	PBYTE pBufIn=(PUCHAR)&i;
	PBYTE pBufOut=(PUCHAR)&i;
	DWORD dwLenIn=0, dwLenOut=0, cbStatus=0;

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

	//
	// Disable the server before starting this test
	//
	dwResult = RasIOControl(NULL, RASCNTL_SERVER_DISABLE, NULL, 0, NULL, 0, &cbStatus);
	if (dwResult != ERROR_SUCCESS)
	{
		if(bExpectedFail)
		{
			RasPrint(TEXT("RasIOControl() FAIL'ed (%d) AS EXPECTED "), dwResult);
			return (dwResult==ERROR_NOT_SUPPORTED? TPR_PASS: TPR_FAIL);
		}

		RasPrint(TEXT("FAIL'ed to disable the server (%d)"), dwResult);
		return TPR_SKIP;
	}

	switch(LOWORD(lpFTE->dwUserData))
	{
		case RASSERVER_INVALID_HANDLE:
			break;
			
		case RASSERVER_INVALID_IN_BUF:
			break;
			
		case RASSERVER_INVALID_IN_LEN:
			dwLenIn=-1;
			break;
			
		case RASSERVER_INVALID_OUT_BUF:
			break;
			
		case RASSERVER_INVALID_OUT_LEN:
			dwLenOut=-1;
			break;
	}

	//
	// We will just call RasServer Enable routine to see if the invalid cases
	// above work
	//
	dwResult = RasIOControl(
						hHandle, 
						RASCNTL_SERVER_ENABLE, 
						pBufIn, dwLenIn, 
						pBufOut, dwLenOut, 
						&cbStatus
						);
	RasPrint(TEXT("dwResult = %d\tdwExpectedResult = %d"), dwResult, dwExpectedResult);	

	if(dwResult != dwExpectedResult)
	{
		return dwStatus=TPR_FAIL;
	}
	
	return dwStatus;
}
