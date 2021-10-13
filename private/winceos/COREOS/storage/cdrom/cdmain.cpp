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

#include "CdRom.h"
CRITICAL_SECTION g_csMain;

// /////////////////////////////////////////////////////////////////////////////
// DllMain
//
// This function is the main CDROM.DLL entry point.
//
BOOL WINAPI DllMain( HANDLE hInstance,
                     DWORD dwReason,
                     LPVOID lpReserved )
{
    switch( dwReason ) 
    {
    case DLL_PROCESS_ATTACH:

        InitializeCriticalSection(&g_csMain);

#ifdef DEBUG
        RegisterDbgZones((HMODULE)hInstance, &dpCurSettings);
#endif
        
        DisableThreadLibraryCalls((HMODULE)hInstance);
        
        DEBUGMSG(ZONE_INIT, (_T("CDROM DLL_PROCESS_ATTACH\r\n")));
        break;

    case DLL_PROCESS_DETACH:

        DeleteCriticalSection(&g_csMain);

        DEBUGMSG(ZONE_INIT, (TEXT("CDROM DLL_PROCESS_DETACH\r\n")));
        break;
    }

    return TRUE;
}



