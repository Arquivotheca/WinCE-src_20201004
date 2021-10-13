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

#include <windows.h>
#include <tchar.h>
#include "baseapp.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInst, LPWSTR lpCmdLine, int nCmdShow)
{
    DWORD  dwWait;
    HANDLE hEvent;


    hEvent = CreateEvent( NULL,  // sec attr
                          TRUE,  // manual reset
                          FALSE, // initial state reset
                          SZ_EVENT_NAME);


    dwWait = WaitForSingleObject(hEvent, INFINITE);

    return( PROCESS_EXIT_CODE); 
}
