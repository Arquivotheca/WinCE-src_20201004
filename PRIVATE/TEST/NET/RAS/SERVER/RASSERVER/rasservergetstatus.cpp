//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "RasServerTest.h"
extern BOOL bExpectedFail;

TESTPROCAPI RasServerGetStatus(UINT uMsg, 
						 TPPARAM tpParam, 
						 LPFUNCTION_TABLE_ENTRY lpFTE) 
{	
	HANDLE	hHandle=NULL;
	DWORD dwStatus = TPR_PASS;
	DWORD	dwResult=ERROR_SUCCESS, dwExpectedResult=ERROR_SUCCESS, cbStatus=ERROR_SUCCESS;
	DWORD	cDevices=0;
	RASCNTL_SERVERSTATUS *pStatus	= NULL;
	DWORD dwSize = sizeof(RASCNTL_SERVERSTATUS);
	
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
		dwResult = RasIOControl(NULL, RASCNTL_SERVER_GET_STATUS, NULL, 0, NULL, 0, &cbStatus);
		RasPrint(TEXT("RasIOControl() FAIL'ed (%d) AS EXPECTED "), dwResult);
		return (dwResult==ERROR_NOT_SUPPORTED? TPR_PASS: TPR_FAIL);
	}
	
	//
	// Get the size first
	//
	dwResult = RasIOControl(NULL, RASCNTL_SERVER_GET_STATUS, NULL, 0, NULL, 0, &dwSize);

	if (dwResult != ERROR_BUFFER_TOO_SMALL)
	{
		RasPrint(TEXT("RASCNTL_SERVER_GET_STATUS failed to get size: %d"), GetLastError());
		return TPR_SKIP;
	}
	else
	{
		pStatus = (RASCNTL_SERVERSTATUS *)LocalAlloc(LPTR, dwSize);
		if (pStatus == NULL)
		{
			RasPrint(TEXT("RASCNTL_SERVER_GET_STATUS failed to allocate memory: %d"), GetLastError());
			return TPR_SKIP;
		}
	}
	switch(LOWORD(lpFTE->dwUserData))
	{
		case RASSERVER_ENABLED_SERVER:
			//
			// Enable the server
			//
			dwResult = RasIOControl(NULL, RASCNTL_SERVER_ENABLE, NULL, 0, NULL, 0, &cbStatus);
			if (dwResult != ERROR_SUCCESS)
			{
				RasPrint(TEXT("FAIL'ed to enable the server (%d)"), dwResult);
				return TPR_SKIP;
			}
			else 
			{
				RasPrint(TEXT("Enabled RAS server"));
			}
			break;
			
		case RASSERVER_DISABLED_SERVER:
			//
			// Disable the server
			//
			dwResult = RasIOControl(NULL, RASCNTL_SERVER_DISABLE, NULL, 0, NULL, 0, &cbStatus);
			if (dwResult != ERROR_SUCCESS)
			{
				RasPrint(TEXT("FAIL'ed to disable the server (%d)"), dwResult);
				return TPR_SKIP;
			}
			else 
			{
				RasPrint(TEXT("Disabled RAS server"));
			}
			break;
	}
	
	//
	// Call the IOCTL
	//
	dwResult = RasIOControl(
						hHandle, 
						RASCNTL_SERVER_GET_STATUS, 
						NULL, 0,
						(PUCHAR)pStatus, dwSize,
						&cbStatus
						);
	RasPrint(TEXT("dwResult = %d\tdwExpectedResult = %d"), dwResult, dwExpectedResult);

	if(dwResult != dwExpectedResult)
	{
		dwStatus=TPR_FAIL;
	}
	else if (dwResult == ERROR_SUCCESS)
	{
		switch(LOWORD(lpFTE->dwUserData))
		{
			case RASSERVER_ENABLED_SERVER:
				if (
					pStatus->bEnable != 1
					)
				{
					RasPrint(TEXT("Parameters don't match."));
					dwStatus=TPR_FAIL;
				}
				break;
				
			case RASSERVER_DISABLED_SERVER:
				if (
					pStatus->bEnable == 1
					)
				{
					RasPrint(TEXT("Parameters don't match."));
					dwStatus=TPR_FAIL;
				}
				break;
		}
	}

	if(pStatus)
		LocalFree(pStatus);

	CleanupServer();
	return dwStatus;
}
