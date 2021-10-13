//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/* File:    devmain.c
 *
 * Purpose: WinCE device manager
 *
 */

//
// This module is a container for the device manager functionality, most of which is
// implemented in a dll.
//
#include <windows.h>

extern int WINAPI StartDeviceManager(HINSTANCE hInst, HINSTANCE hPrevInst, LPWSTR lpCmdLine, int nCmdShow);

// This routine is the entry point for the device manager.  It simply calls the device
// management DLL's entry point.
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPWSTR lpCmdLine, int nCmdShow)
{
    int status;

    status = StartDeviceManager(hInst, hPrevInst, lpCmdLine, nCmdShow);

    return status;
}
