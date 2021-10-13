//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
//-***********************************************************************
//
//  This file contains all of the code that sits under an IP interface
//  and does the necessary multiplexing/demultiplexing for WAN connections.
//
//-***********************************************************************

#include "windows.h"
#include "cxport.h"
#include "types.h"

//  PPP Include Files

#include "protocol.h"
#include "ppp.h"
#include "mac.h"
#include "crypt.h"
#include "pppserver.h"
#include "auth.h"
#include "debug.h"

#include <ndistapi.h>

//
// PPP Context List
//

extern  PPP_CONTEXT     *pppContextList;
extern  CRITICAL_SECTION v_ListCS;
extern  HANDLE           v_hInstDLL;

#ifdef DEBUG
DBGPARAM dpCurSettings =
{
    TEXT("PPP"),
    {
        TEXT("Init"),TEXT("Mac"),TEXT("Lcp"),TEXT("Auth"),
        TEXT("Ncp"),TEXT("IpCp"),TEXT("Ras"),TEXT("PPP"),
        TEXT("Timing"),TEXT("Trace"),TEXT("Lock"),TEXT("Event"),
        TEXT("Alloc"),TEXT("Function"),TEXT("Warning"),TEXT("Error")
    },
    0x0C000
};
#endif

BOOL            g_bPPPInitialized = FALSE;

HMODULE         g_hCoredllModule;
pfnPostMessageW g_pfnPostMessageW;

HMODULE         g_hDebugLogModule;
BOOL (*g_pfnLogTxNdisPacket)(PWSTR wszProtocolName, PWSTR wszStreamName, PWSTR PacketType, struct _NDIS_PACKET *Packet);
BOOL (*g_pfnLogTxNdisWanPacket)(PWSTR wszProtocolName, PWSTR wszStreamName, PWSTR PacketType, struct _NDIS_WAN_PACKET *Packet);
BOOL (*g_pfnLogRxContigPacket)(PWSTR wszProtocolName, PWSTR wszStreamName, PWSTR PacketType, PBYTE pPacket, DWORD cbPacket);


BOOL
GetGWEProcAddresses(void)
{
	BOOL bOk = FALSE;

	g_hCoredllModule = LoadLibrary(TEXT("coredll.dll"));
	if (g_hCoredllModule)
	{
		g_pfnPostMessageW = (pfnPostMessageW)GetProcAddress(g_hCoredllModule, TEXT("PostMessageW"));
		if (g_pfnPostMessageW == NULL)
		{
			RETAILMSG(1, (TEXT("PPP: ERROR: Can't find PostMessageW in coredll.dll!\n")));
			FreeLibrary(g_hCoredllModule);
		}
		else
		{
			bOk = TRUE;
		}
	}
	else
	{
		RETAILMSG(1, (TEXT("PPP: ERROR: LoadLibrary of coredll.dll failed!\n")));
	}
	return bOk;
}

DWORD
LoadLoggingExtension(void)
//
// Get function hooks into netlog.dll if present, to support logging
// packets being sent/received by PPP.
//
{
	DWORD dwResult = NO_ERROR;

	if (g_hDebugLogModule == NULL)
	{
		dwResult = CXUtilGetProcAddresses(TEXT("netlog.dll"), &g_hDebugLogModule,
						TEXT("LogTxNdisPacket"),    &g_pfnLogTxNdisPacket,
						TEXT("LogTxNdisWanPacket"), &g_pfnLogTxNdisWanPacket,
						TEXT("LogRxContigPacket"),  &g_pfnLogRxContigPacket,
						NULL);
	}

	return dwResult;
}

DWORD
UnloadLoggingExtension(void)
//
// Release function hooks into netlog.dll if they were previously hooked.
//
{
	DWORD dwResult = NO_ERROR;

	if (g_hDebugLogModule)
	{
		FreeLibrary(g_hDebugLogModule);
		g_hDebugLogModule = NULL;
		g_pfnLogTxNdisPacket = NULL;
		g_pfnLogTxNdisWanPacket = NULL;
		g_pfnLogRxContigPacket = NULL;
	}

	return dwResult;
}

BOOL __stdcall
DllEntry( HANDLE  hinstDLL, DWORD   Op, LPVOID  lpvReserved )
{
    switch (Op)
    {
    case DLL_PROCESS_ATTACH :
		DEBUGREGISTER(hinstDLL);
        v_hInstDLL = hinstDLL;
        DisableThreadLibraryCalls ((HMODULE)hinstDLL);
		InitializeCriticalSection( &v_ListCS );
		break;

    case DLL_PROCESS_DETACH:
		// PPP never unloads from device.exe in the current
		// architecture. Properly implementing support for unloading
		// would take some effort...
		ASSERT(FALSE);
        break;

    default :
        break;
    }
    return TRUE;
}

BOOL
PPPInitialize (
    IN TCHAR *szRegPath)
//
//  Called at boot time by the device.exe driver loader if
//  the Builtin/PPP registry key values are configured like this:
//    [HKEY_LOCAL_MACHINE\Drivers\BuiltIn\PPP]
//      "Dll"="PPP.Dll"
//      ; Must load after NDIS
//      "Order"=dword:3
//      "Keep"=dword:1
//      "Entry"="PPPInitialize"
//
//  This function performs boot time initialization of PPP.
//  This is done here rather than DllEntry because of potential
//  deadlocks that can occur because DllEntry runs with the loader
//  critical section held.
//
//  Return TRUE if successful, FALSE if an error occurs.
//
{
	extern BOOL GetIPProcAddresses(void);

	if (g_bPPPInitialized == FALSE)
	{
		// Initialize add/delete interface hooks into IP
		if (!GetIPProcAddresses())
			return FALSE;

		LoadLoggingExtension();

		// OK if this fails, code will handle gracefully
		GetGWEProcAddresses();

		// Initialize the NDIS interface
		pppMac_Initialize();

		// Initialize hooks to EAP
		(void)rasEapGetDllFunctionHooks();

		// Initialize PPP Server
		PPPServerInitialize();

		g_bPPPInitialized = TRUE;
	}

	return TRUE;
}

