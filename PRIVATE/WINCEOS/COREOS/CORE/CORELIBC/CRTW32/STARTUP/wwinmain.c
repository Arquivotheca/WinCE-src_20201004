//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <corecrt.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hInstancePrev, LPWSTR lpszCmdLine, int nCmdShow);

void WINAPI wWinMainCRTStartup(HINSTANCE hInstance, HINSTANCE hInstancePrev, LPWSTR lpszCmdLine, int nCmdShow) {

    int retcode;

    _try {
        
        _cinit(); /* Initialize C's data structures */
    
	retcode = wWinMain(hInstance, hInstancePrev, lpszCmdLine, nCmdShow);
     }
    __except ( _XcptFilter(GetExceptionCode(), GetExceptionInformation()) ) {
             /*
             * Should never reach here unless UnHandled Exception Filter does
	     * not pass the exception to the debugger
             */

        retcode = GetExceptionCode();

	exit(retcode);

    } /* end of try - except */

    exit(retcode);
}

