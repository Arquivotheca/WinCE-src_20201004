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

#include "kdp.h"

KERNDATA g_kdKernData = {0};

BOOL g_fKdbgRegistered = FALSE;


BOOL
KdInit
(
    KERNDATA* pKernData
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
    ULONG Index;
    PUCHAR pucDummy;

    if (pKernData &&
        (sizeof(*pKernData) == pKernData->nSize) &&
        pKernData->pKITLIoCtl)
    {
        g_kdKernData = *pKernData;  // may need memcpy here

        pKernData->pKITLIoCtl(IOCTL_EDBG_GET_OUTPUT_DEBUG_FN, NULL, 0,
                               (VOID*)&g_pfnOutputDebugString, sizeof(g_pfnOutputDebugString), NULL);
        // NOTE: DEBUGGERMSG IS A NOP BEFORE THIS POINT

        DEBUGGERMSG(KDZONE_INIT, (TEXT("++KdInit\r\n")));

        // Initialize the breakpoint table.
        for (Index = 0; Index < BREAKPOINT_TABLE_SIZE; Index += 1)
        {
            KdpBreakpointTable[Index].Flags = 0L;
            KdpBreakpointTable[Index].Address = NULL;
            KdpBreakpointTable[Index].KAddress = NULL;
        }

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
        
        pKernData->pUpdateSymbols = UpdateSymbols;
        pKernData->pKdTrap        = KdpTrap;
        pKernData->pKdSanitize    = KdpSanitize;
        pKernData->pKdReboot      = KdpReboot;

        if (pKernData->pKITLIoCtl(IOCTL_EDBG_IS_STARTED, NULL, 0, NULL, 0, NULL))
        { // Kitl is already started
            // JIT Debugging - Make sure no other threads run while we wait for host to connect
            DWORD cSavePrio = GET_CPRIO (pCurThread);
            // Set the quantum to zero to prevent any other real-time thread from running
            DWORD dwSaveQuantum = pCurThread->dwQuantum;
            pCurThread->dwQuantum = 0;
            SET_CPRIO (pCurThread, 0);
            g_fKdbgRegistered = pKernData->pKITLIoCtl(IOCTL_EDBG_REGISTER_DFLT_CLIENT, &pucDummy, KITL_SVC_KDBG, &pucDummy, 0, NULL);
            pCurThread->dwQuantum = dwSaveQuantum;
            SET_CPRIO (pCurThread, cSavePrio);
            DEBUGGERMSG(KDZONE_INIT, (TEXT("  KdInit - KITLRegisterDfltClient returned: %d\r\n"), g_fKdbgRegistered));
        }
        else
        {
            DEBUGGERMSG(KDZONE_INIT, (TEXT("  KdInit - KITL is not started\r\n")));
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

