//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/*++

Module Name:

    init.c

--*/

#include <guiddef.h>
#include <windows.h>
#ifdef KCOREDLL
#include <kerncmn.h>
#include <apicall.h>
#endif KCOREDLL

#ifdef DEBUG
#define ZONE_INIT       DEBUGZONE(0)
#define ZONE_LIST       DEBUGZONE(1)
#define ZONE_MSG        DEBUGZONE(2)
#define ZONE_OBJECTS    DEBUGZONE(3)
#define ZONE_TRANS      DEBUGZONE(4)
#define ZONE_LOC        DEBUGZONE(5)
#define ZONE_PARAM      DEBUGZONE(6)
#define ZONE_PSL        DEBUGZONE(8)
#define ZONE_HCALL      DEBUGZONE(9)
#define ZONE_CALLS      DEBUGZONE(10)
#define ZONE_MISC       DEBUGZONE(11)
#define ZONE_ALLOC      DEBUGZONE(12)
#define ZONE_FUNCTION   DEBUGZONE(13)
#define ZONE_FUNC       ZONE_FUNCTION
#define ZONE_WARN       DEBUGZONE(14)
#define ZONE_ERROR      DEBUGZONE(15)
#endif

// Debug Zones.
//
#ifdef DEBUG
DBGPARAM dpCurSettings = {
    TEXT("Tapi32"), {
        TEXT("Init"),TEXT("Lists"),TEXT("Messages"),TEXT("Objects"),
        TEXT("Translate"),TEXT("Location"),TEXT("Parameters"),TEXT(""),
        TEXT("PSL"),TEXT("Call Handles"),TEXT("Call State"),TEXT("Misc"),
        TEXT("Alloc"),TEXT("Function"),TEXT("Warning"),TEXT("Error") },
    0
}; 
#endif

BOOL WINAPI
DllMain (
    HANDLE  hinstDLL,
    DWORD   Op,
    LPVOID  lpvReserved
    )
{
    switch (Op) {
        case DLL_PROCESS_ATTACH :
            DEBUGREGISTER(hinstDLL);
            DEBUGMSG(ZONE_FUNCTION|ZONE_INIT, (TEXT("TAPI:DllMain(DLL_PROCESS_ATTACH)\n")));
            DisableThreadLibraryCalls ((HMODULE)hinstDLL);
            break;
            
    case DLL_PROCESS_DETACH :
        DEBUGMSG(ZONE_FUNCTION|ZONE_INIT, (TEXT("TAPI:DllMain(DLL_PROCESS_DETACH)\n")));
        break;

    case DLL_THREAD_DETACH :
    case DLL_THREAD_ATTACH :
    default :
        break;
    }
    return TRUE;
}

