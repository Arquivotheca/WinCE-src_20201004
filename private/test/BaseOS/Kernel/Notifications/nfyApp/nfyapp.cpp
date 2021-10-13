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
	UNREFERENCED_PARAMETER(nCmdShow);
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(hInstance);
	CNfyData cnf;
	SYSTEMTIME time1;
	GetLocalTime(&time1);
	cnf.AppendData(lpCmdLine, &time1);
	NKDbgPrintfW(TEXT("NFYAPP::Entry #%d, CmdLine::%s \r\n"), cnf.GetNotificationTestData()->count, lpCmdLine);

	return 0;
}
