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
/*++


Module Name:

    kdinit.c

Abstract:

    This module implements the initialization for the portable kernel debgger.

Revision History:

--*/

#include "kdp.h"
#include "kdfilter.h"

DBGPARAM dpCurSettings = {
        L"KdStub",
        {
            L"Init",                // 0x0001
            L"Trap",                // 0x0002
            L"KdApi",               // 0x0004
            L"Dbg - kdExt",         // 0x0008
            L"SW BP alloc",         // 0x0010
            L"BP generic",          // 0x0020
            L"Control Cmds",        // 0x0040
            L"MemAcc (Move)",       // 0x0080
            L"Get Ctx/Kernel Addr", // 0x0100
            L"KdPacket",            // 0x0200
            L"StackWalk",           // 0x0400
            L"Intel WMMX copro",    // 0x0800
            L"Virtual Mem",         // 0x1000
            L"Handle Explorer",     // 0x2000
            L"KD DbgMsg -> KDAPI",  // 0x4000
            L"Alert",               // 0x8000
        },
        KDZONE_DEFAULT
};


DEBUGGER_DATA *g_pDebuggerData;
    
static ULONG s_ulOldHDEventFilter = 0;
static BOOL s_fEnableHDNotifs = FALSE;

BOOL g_fKdbgRegistered = FALSE;
DWORD *g_pdwModLoadNotifDbg = NULL;
HANDLE g_hInstance;

void KdPostInit();


BOOL
KdInit
(
    DEBUGGER_DATA *pDebuggerData,
    void* pvKernData
)
/*++

Routine Description:

    This routine initializes the portable kernel debugger.

Arguments:

    None.

Return Value:

    None.

--*/
{
    BOOL  fRet = FALSE;

#ifdef ARM
    InitPCBFunctions ();
#endif

    if (pDebuggerData &&
        VALID_DEBUGGER_DATA(pDebuggerData))
    {
        g_pDebuggerData = pDebuggerData;

        REGISTERDBGZONES(g_hInstance);

        g_pdwModLoadNotifDbg = &((*(HDSTUB_SHARED_GLOBALS **) g_pKData->pOsAxsDataBlock->dwPtrHdSharedGlobals)->dwModLoadNotifDbg);

        DEBUGMSG(KDZONE_INIT, (TEXT("++KdInit (pDebuggerData=%08X)\r\n"), pDebuggerData));

        RETAILMSG (1, (L"KD: Starting kernel debugger software probe (KdStub) - KD API version %i\r\n", KDAPI_PROTOCOL_VERSION));

        // Initialize the breakpoints tables.
        KdpResetBps (); // This will initialize all BP table
        KeSweepCurrentIcache();
#ifdef SHx
        // Initialize the registers belonging to the User Break Controller
        WRITE_REGISTER_ULONG(UBCBarA, 0);
        WRITE_REGISTER_UCHAR(UBCBasrA, 0);
        WRITE_REGISTER_UCHAR(UBCBamrA, 0);
        WRITE_REGISTER_USHORT(UBCBbrA, 0);
        WRITE_REGISTER_ULONG(UBCBarB, 0);
        WRITE_REGISTER_UCHAR(UBCBasrB, 0);
        WRITE_REGISTER_UCHAR(UBCBamrB, 0);
        WRITE_REGISTER_USHORT(UBCBbrB, 0);
        WRITE_REGISTER_ULONG(UBCBdrB, 0);
        WRITE_REGISTER_ULONG(UBCBdmrB, 0);
        WRITE_REGISTER_USHORT(UBCBrcr, 0);
#endif
        KdpEnable(TRUE);
        g_fForceReload = TRUE;
       
        //Add basic debugger capabilities even if the filter is enabled
        AddSimpleFilterRule((DWORD) STATUS_BREAKPOINT, KDFEX_RULE_DECISION_BREAK);
        AddSimpleFilterRule((DWORD) STATUS_SYSTEM_BREAK, KDFEX_RULE_DECISION_BREAK);
        AddSimpleFilterRule((DWORD) STATUS_SINGLE_STEP, KDFEX_RULE_DECISION_BREAK);
        fRet = TRUE;
    }
    else
    {
        RETAILMSG (1, (L"KD: Failed to initialize kernel debugger software probe (KdStub): incompatible kernel\r\n"));
    }

    DEBUGMSG(KDZONE_INIT, (TEXT("--KdInit\r\n")));

    return fRet;
}


void KdpEnable(BOOL enable)
{
    if (enable)
    {
        DD_eventHandlers[KDBG_EH_DEBUGGER].pfnException = KdpTrap;
        DD_eventHandlers[KDBG_EH_DEBUGGER].dwFilter = HDSTUB_FILTER_EXCEPTION;
        DD_eventHandlers[KDBG_EH_FILTER].pfnException = FilterTrap;
        DD_eventHandlers[KDBG_EH_FILTER].dwFilter = HDSTUB_FILTER_EXCEPTION;
        DD_kdbgBuffer = g_abMessageBuffer;
        DD_kdbgBufferSize = KDP_MESSAGE_BUFFER_SIZE;
        DD_HandleKdApi = KdpHandleKdApi;
        DD_KDBGSend = KdpSendPacket;
        DD_KDBGRecv = KdpReceivePacket;
        DD_KDBGDebugMessage = KdpSendDbgMessage;
        if (KITLIoctl(IOCTL_EDBG_IS_STARTED, NULL, 0, NULL, 0, NULL))
        { 
            PUCHAR pucDummy;
            EnableHDNotifs (TRUE);
            g_fKdbgRegistered = KITLIoctl(IOCTL_EDBG_REGISTER_DFLT_CLIENT, &pucDummy, KITL_SVC_KDBG, &pucDummy, 0, NULL);
            DEBUGGERMSG(KDZONE_INIT, (TEXT("  KdpEnable: KITLRegisterDfltClient returned: %d\r\n"), g_fKdbgRegistered));
        }
        else
        {
            DEBUGGERMSG(KDZONE_INIT, (TEXT("  KdpEnable: KITL is not started\r\n")));
        }
        g_fDbgConnected = TRUE;
    }
    else
    {
        DD_eventHandlers[KDBG_EH_DEBUGGER].pfnException = NULL;
        DD_eventHandlers[KDBG_EH_DEBUGGER].dwFilter = 0;
        DD_eventHandlers[KDBG_EH_FILTER].pfnException = NULL;
        DD_eventHandlers[KDBG_EH_FILTER].dwFilter = 0;
        DD_kdbgBuffer = NULL;
        DD_kdbgBufferSize = 0;
        DD_HandleKdApi = NULL;
        DD_KDBGSend = NULL;
        DD_KDBGRecv = NULL;
        DD_KDBGDebugMessage = NULL;
        if (g_fKdbgRegistered)
        {
            EnableHDNotifs(FALSE);
            KITLIoctl(IOCTL_EDBG_DEREGISTER_CLIENT, NULL, KITL_SVC_KDBG, NULL, 0, NULL);
            g_fKdbgRegistered = FALSE;
        }
        g_fDbgConnected = FALSE;
    }
}


BOOL
WINAPI
KdDLLEntry
(
    HINSTANCE   hInstance,
    ULONG       ulReason,
    FARPROC     pvReserved
)
{
    BOOL fRet = TRUE;
    FARPROC pKernelLibIoControl = pvReserved;
    FARPROC pKdInit = KdInit;

    g_hInstance = hInstance;
    switch (ulReason)
    {
        case DLL_PROCESS_ATTACH:
            fRet = (pKernelLibIoControl &&
                    pKernelLibIoControl((HANDLE)KMOD_DBG, (DWORD)IOCTL_DBG_INIT,
                                        (VOID*)&pKdInit, (DWORD)sizeof(pKdInit), NULL, (DWORD)0, NULL));
            break;

        case DLL_PROCESS_DETACH:
            break;
    }

    return fRet;
}


void EnableHDNotifs (BOOL fEnable)
{
    if (fEnable)
    {
        if (pulHDEventFilter && !s_fEnableHDNotifs)
        { // We need to enable HW debug notif of module load and unload for the real debugger notification (the KdpModLoad and KdpModUnload are just for display of info)
            s_ulOldHDEventFilter = (*pulHDEventFilter & (HDSTUB_FILTER_MODLOAD | HDSTUB_FILTER_MODUNLOAD | HDSTUB_FILTER_VMPAGEOUT));
            *pulHDEventFilter |= HDSTUB_FILTER_MODLOAD | HDSTUB_FILTER_MODUNLOAD | HDSTUB_FILTER_VMPAGEOUT;
            s_fEnableHDNotifs = TRUE;
            DD_EnableHDEvents(TRUE);
        }
    }
    else
    {
        if (pulHDEventFilter && s_fEnableHDNotifs)
        {
            *pulHDEventFilter &= ~(HDSTUB_FILTER_MODLOAD | HDSTUB_FILTER_MODUNLOAD | HDSTUB_FILTER_VMPAGEOUT);
            *pulHDEventFilter |= s_ulOldHDEventFilter;
            DD_EnableHDEvents(FALSE);
        }
        s_fEnableHDNotifs = FALSE;
    }
}
