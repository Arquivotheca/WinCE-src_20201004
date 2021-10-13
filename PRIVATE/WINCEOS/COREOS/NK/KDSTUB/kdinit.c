//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

    kdinit.c

Abstract:

    This module implements the initialization for the portable kernel debgger.

Revision History:

--*/

// WARNING: WINCEMACRO allows the use of PSL calls. This source file
// should only contain code that is used outside the scope of
// exception handling. Such as initialization code.
#define WINCEMACRO      

#include "kdp.h"

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
            L"ConCan copro",        // 0x0800
            L"Virtual Mem",         // 0x1000
            L"Handle Explorer",     // 0x2000
            L"",                    // 0x4000
            L"Alert",               // 0x8000
        },
        KDZONE_DEFAULT
};


KERNDATA g_kdKernData = {0};
HDSTUB_DATA Hdstub = {0};

static ULONG s_ulOldHDEventFilter = 0;
static BOOL s_fEnableHDNotifs = FALSE;

BOOL g_fKdbgRegistered = FALSE;

HDSTUB_CLIENT g_KdstubClient =
{
    "kdstub",
    KdpTrap,
    KdpHandlePageIn,
    KdpModLoad,
    KdpModUnload,
    NULL,
    HDSTUB_FILTER_EXCEPTION | HDSTUB_FILTER_VMPAGEIN | HDSTUB_FILTER_MODLOAD | HDSTUB_FILTER_MODUNLOAD,
    NULL
};

BOOL
KdInit
(
    HDSTUB_DATA *pHdData,
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
    PUCHAR pucDummy;
    int Index;
    KERNDATA *pKernData = (KERNDATA *)pvKernData;

    if (pHdData &&
        pKernData &&
        (sizeof(*pKernData) == pKernData->nSize) &&
        pKernData->pKITLIoCtl)
    {
        Hdstub = *pHdData;
        g_kdKernData = *pKernData;

        pKernData->pKITLIoCtl(IOCTL_EDBG_GET_OUTPUT_DEBUG_FN, NULL, 0,
                               (VOID*)&g_pfnOutputDebugString, sizeof(g_pfnOutputDebugString), NULL);
        // NOTE: DEBUGGERMSG IS A NOP BEFORE THIS POINT

        DEBUGGERMSG(KDZONE_INIT, (TEXT("++KdInit\r\n")));

        RETAILMSG (1, (L"Starting kernel debugger software probe (KdStub) - KD API version %i\r\n", KDAPI_PROTOCOL_VERSION));

        // Initialize the breakpoints tables.
        g_nTotalNumDistinctSwCodeBps = 0;
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

        InitializeCriticalSection(&csDbg);

        pKernData->pKdSanitize      = KdpSanitize;
        pKernData->pKdReboot        = KdpReboot;

        if (Hdstub.pfnRegisterClient (&g_KdstubClient, HdstubClientFirst))
        {
            DEBUGGERMSG(KDZONE_INIT, (L"  KdInit: Successfully registered with Hdstub.\r\n"));
        }
        else
        {
            DEBUGGERMSG(KDZONE_INIT, (L"  KdInit: Unable to register with Hdstub.\r\n"));
        }

        memset (g_aRom2RamPageTable, 0, sizeof (g_aRom2RamPageTable));
        DEBUGGERMSG(KDZONE_INIT, (TEXT("  KdInit: Rom2Ram Data Pool address is 0x%08X\r\n"), g_abRom2RamDataPool));
        for (Index = 0; Index < NB_ROM2RAM_PAGES; Index++)
        {// Inittialize each RAM page pointer from the pool, aligned on page beginning
            g_aRom2RamPageTable [Index].pbRAMAddr =
                (BYTE *) ((((DWORD) (&(g_abRom2RamDataPool [PAGE_SIZE * Index])) - 1) & ~(PAGE_SIZE - 1)) + PAGE_SIZE);
            DEBUGGERMSG(KDZONE_INIT, (TEXT("  KdInit: Rom2Ram page %i address is 0x%08X\r\n"), Index, g_aRom2RamPageTable [Index].pbRAMAddr));
        }

        if (pKernData->pKITLIoCtl(IOCTL_EDBG_IS_STARTED, NULL, 0, NULL, 0, NULL))
        { // Kitl is already started
            // JIT Debugging - Make sure no other threads run while we wait for host to connect
            DWORD cSavePrio = GET_CPRIO (pCurThread);
            // Set the quantum to zero to prevent any other real-time thread from running
            DWORD dwSaveQuantum = pCurThread->dwQuantum;
            pCurThread->dwQuantum = 0;
            SET_CPRIO (pCurThread, 0);
            EnableHDNotifs (TRUE);
            g_fKdbgRegistered = pKernData->pKITLIoCtl(IOCTL_EDBG_REGISTER_DFLT_CLIENT, &pucDummy, KITL_SVC_KDBG, &pucDummy, 0, NULL);
            pCurThread->dwQuantum = dwSaveQuantum;
            SET_CPRIO (pCurThread, cSavePrio);
            DEBUGGERMSG(KDZONE_INIT, (TEXT("  KdInit: KITLRegisterDfltClient returned: %d\r\n"), g_fKdbgRegistered));
        }
        else
        {
            DEBUGGERMSG(KDZONE_INIT, (TEXT("  KdInit: KITL is not started\r\n")));
        }

        g_fDbgConnected = TRUE;
        g_fForceReload  = TRUE;

        fRet = TRUE;
    }

    DEBUGGERMSG(KDZONE_INIT, (TEXT("--KdInit\r\n")));

    return fRet;
}


BOOL
WINAPI
KdDLLEntry
(
    HINSTANCE   hInstance,
    ULONG       ulReason,
    LPVOID      pvReserved
)
{
    BOOL fRet = TRUE;
    FARPROC pKernelLibIoControl = (FARPROC)pvReserved;
    FARPROC pKdInit = (FARPROC)KdInit;

    DEBUGREGISTER(hInstance);
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
            s_ulOldHDEventFilter = (*pulHDEventFilter & KDSTUB_HDEVENTFILT_MASK);
            *pulHDEventFilter |= KDSTUB_HDEVENTFILT_MASK;
            s_fEnableHDNotifs = TRUE;
        }
    }
    else
    {
        if (pulHDEventFilter && s_fEnableHDNotifs)
        {
            *pulHDEventFilter &= ~KDSTUB_HDEVENTFILT_MASK;
            *pulHDEventFilter |= s_ulOldHDEventFilter;
        }
        s_fEnableHDNotifs = FALSE;
    }
}
