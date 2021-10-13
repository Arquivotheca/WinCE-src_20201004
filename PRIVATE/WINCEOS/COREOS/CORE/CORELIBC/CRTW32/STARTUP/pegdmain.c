//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <corecrt.h>

BOOL WINAPI DllMain(HANDLE hDllHandle, DWORD dwReason, LPVOID lpReserved);

BOOL (WINAPI *_pRawDllMain)(HANDLE, DWORD, LPVOID);

BOOL WINAPI _DllMainCRTStartup(HANDLE hDllHandle, DWORD dwReason, LPVOID lpreserved) 
{
	BOOL retcode = TRUE;

	if (dwReason == DLL_PROCESS_ATTACH)
	{
		if ( retcode && _pRawDllMain )
			retcode = (*_pRawDllMain)(hDllHandle, dwReason, lpreserved);

		if ( retcode )
			_cinit();
	}
	
	if (retcode)
		retcode = DllMain(hDllHandle, dwReason, lpreserved);
	
	if (dwReason == DLL_PROCESS_DETACH)
	{
		_cexit();
        if ( _pRawDllMain )
		{
			if( (*_pRawDllMain)(hDllHandle, dwReason, lpreserved) == FALSE )
				retcode = FALSE;
		}
	}
	return retcode;
}

