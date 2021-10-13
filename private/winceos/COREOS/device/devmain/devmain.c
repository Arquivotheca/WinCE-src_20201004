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
#ifndef SHIP_BUILD
//
// These defines must match the ZONE_* defines in devmgr.h
//
#define DBG_ERROR      1
#define DBG_WARNING    2
#define DBG_FUNCTION   4
#define DBG_INIT       8
#define DBG_BUILTIN    16
#define DBG_ACTIVE     64
#define DBG_RESMGR     128
#define DBG_FSD        256
#define DBG_DYING      512
#define DBG_BOOTSEQ     1024
#define DBG_PNP         2048
#define DBG_SERVICES    4096
#define DBG_IO          0x2000
#define DBG_FILE        0x4000

DBGPARAM dpCurSettings = {
    TEXT("DEVLOAD"), {
    TEXT("Errors"),TEXT("Warnings"),TEXT("Functions"),TEXT("Initialization"),
    TEXT("Built-In Devices"),TEXT("Undefined"),TEXT("Active Devices"),TEXT("Resource Manager"),
    TEXT("File System Drvr"),TEXT("Dying Devs"),TEXT("Boot Sequence"),TEXT("PnP"),
    TEXT("Services"),TEXT("IO"),TEXT("FILE"),TEXT("Undefined") },
    DBG_ERROR 
};
#endif  // SHIP_BUILD

int WINAPI  DevMainEntry(HINSTANCE hInst)
{
    int status;
    status = StartDeviceManager(hInst, NULL, NULL, 0);

    return status;
}
extern BOOL WINAPI
DllEntry(HINSTANCE DllInstance, INT Reason, LPVOID Reserved)
{
    UNREFERENCED_PARAMETER(Reserved);

    switch(Reason) {
    case DLL_PROCESS_ATTACH:
        DEBUGREGISTER(DllInstance);
        DEBUGMSG(DBG_INIT,(TEXT("devmain DLL_PROCESS_ATTACH\r\n")));
        DisableThreadLibraryCalls((HMODULE) DllInstance);
        break;
    case DLL_PROCESS_DETACH:
        DEBUGMSG(DBG_INIT,(TEXT("devmain DLL_PROCESS_DETACH\r\n")));
        break;
    };
    return TRUE;
}

