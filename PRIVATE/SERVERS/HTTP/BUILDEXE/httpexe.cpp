//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: LISTENER.CPP
Abstract: HTTP server initialization & listener thread 
--*/


#include "httpd.h"
typedef void  (WINAPI *PFN_EXECUTE)();

#ifdef UNDER_NT
extern "C" int HttpInitializeFromExe();
#endif

int
WINAPI
WinMain(HINSTANCE hInstance,
		HINSTANCE hPrevInstance,
#ifdef UNDER_NT
		LPSTR     lpCmdLine,
#else
		LPWSTR    lpCmdLine,
#endif
		int       nCmdShow)
{
#ifdef UNDER_NT
	svsutil_Initialize();
	HttpInitializeFromExe();
#else
	PFN_EXECUTE pFunc = NULL;
	HINSTANCE hLib = LoadLibrary(L"HTTPD.DLL");

	if (!hLib) {
		RETAILMSG(1,(L"HTTPDEXE:  Httpd.dll not loaded on device, aborting execution\r\n"));
		return 1;
	}

	pFunc = (PFN_EXECUTE) GetProcAddress(hLib,L"HttpInitializeFromExe");
	if (!pFunc) {
		RETAILMSG(1,(L"HTTPDEXE:  Httpd.dll corrupt or old version, aborting execution\r\n"));
		return 1;
	}

	((PFN_EXECUTE) pFunc)();

#endif	

	Sleep(INFINITE);  // don't ever stop, must kp to end us.
	return 0;
}

