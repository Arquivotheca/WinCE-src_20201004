//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <windows.h>
#include <tchar.h>
#include <stressutils.h>


/////////////////////////////////////////////////////////////////////////////////////
const TCHAR gszClassName[] = TEXT("s2_rotate");
int g_nTimeToRotate = 60;   //default to rotating every 60 seconds

/////////////////////////////////////////////////////////////////////////////////////
HINSTANCE g_hInst = NULL;

/////////////////////////////////////////////////////////////////////////////////////
void DoMsgPump(DWORD dwMilliSeconds)
{
	// Main message loop:
	MSG msg;
	DWORD dwStart = GetTickCount();

	while (GetTickCount() < dwStart + dwMilliSeconds)
	{
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
BOOL InitializeStressModule (
							/*[in]*/ MODULE_PARAMS* pmp, 
							/*[out]*/ UINT* pnThreads
							)
{
    // This test will use three threads
	*pnThreads = 3;

	InitializeStressUtils (
							_T("s2_rotate"),									// Module name to be used in logging
							LOGZONE(SLOG_SPACE_GDI, SLOG_FONT | SLOG_RGN),	// Logging zones used by default
							pmp												// Forward the Module params passed on the cmd line
							);

	srand(GetTickCount());

	TCHAR tsz[MAX_PATH];

	GetModuleFileName((HINSTANCE) g_hInst, tsz, MAX_PATH);
	tsz[MAX_PATH-1] = 0;

	LogComment(_T("Module File Name: %s"), tsz);
	
	return TRUE;
}



/////////////////////////////////////////////////////////////////////////////////////
UINT DoStressIteration (
						/*[in]*/ HANDLE hThread, 
						/*[in]*/ DWORD dwThreadId, 
						/*[in,out]*/ LPVOID pv /*unused*/)
{
	DEVMODE devMode;

	memset(&devMode,0x00,sizeof(devMode));
	devMode.dmSize=sizeof(devMode);

	switch(rand() % 4)
	{
	case 0:
		devMode.dmDisplayOrientation = DMDO_0;
		break;
	case 1:
		devMode.dmDisplayOrientation = DMDO_90;
		break;
	case 2:
		devMode.dmDisplayOrientation = DMDO_180;
		break;
	case 3:
		devMode.dmDisplayOrientation = DMDO_270;
		break;
	}
	
	devMode.dmFields = DM_DISPLAYORIENTATION;
	ChangeDisplaySettingsEx(NULL,&devMode,NULL,CDS_RESET,NULL);

	// Back off for some number of seconds
	DoMsgPump(g_nTimeToRotate * 1000);

	return CESTRESS_PASS;
}



/////////////////////////////////////////////////////////////////////////////////////
DWORD TerminateStressModule (void)
{
	return ((DWORD) -1);
}



///////////////////////////////////////////////////////////
BOOL WINAPI DllMain(
					HANDLE hInstance, 
					ULONG dwReason, 
					LPVOID lpReserved
					)
{
	g_hInst = static_cast<HINSTANCE>(hInstance);
	return TRUE;
}
