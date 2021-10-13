//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <windows.h>

#include "names.h"
#include "CMMFile.h"
#include "CNfyData.h"


/*
	Functionality: Add info in the shared memory to indicate that this app was run. 
		       It would also record the time at which the app was run

*/

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPTSTR lpCmdLine, int nCmdShow )
{
	CNfyData cnf;
	SYSTEMTIME time1;
	GetLocalTime(&time1);
	cnf.AppendData(lpCmdLine, &time1);
	NKDbgPrintfW(TEXT("NFYAPP::Entry #%d, CmdLine::%s \r\n"), cnf.GetNotificationTestData()->count, lpCmdLine);

	return 0;
}
