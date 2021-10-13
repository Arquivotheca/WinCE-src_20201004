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

    kdtrap.c

Abstract:

    This module contains code to implement the target side of the portable
    kernel debugger.

--*/

#include "kdp.h"
#include "kdfilter.h"

BOOL  g_fForceReload = TRUE;
DWORD g_dwLastKDTrapCurMSec = 0;

extern BREAKPOINT_ENTRY g_aBreakpointTable[BREAKPOINT_TABLE_SIZE];

BOOL
KdpRegisterKITL(CONTEXT *pContextRecord, BOOL SecondChance)
{
    PUCHAR pucDummy;
    BOOL fKitlStarted;
    BOOL letKDHandle;

    if (g_fKdbgRegistered)
    {
        letKDHandle = TRUE;
        goto exit;
    }

    if (CONTEXT_TO_PROGRAM_COUNTER(pContextRecord) >= (DWORD)HwTrap &&
        CONTEXT_TO_PROGRAM_COUNTER(pContextRecord) <= ((DWORD)HwTrap + HD_NOTIFY_MARGIN))
    {
        letKDHandle = FALSE;
        goto exit;
    }

    // Check if KITL has started or JIT
    DD_ThawSystem();
    {
        // register KDBG while allowing the system to continue running.
        fKitlStarted = KITLIoctl(IOCTL_EDBG_IS_STARTED, NULL, 0, NULL, 0, NULL);
        if (fKitlStarted)
        {
            letKDHandle = TRUE;
        }
        else if (SecondChance)
        {
            DEBUGGERMSG(KDZONE_ALERT, (TEXT("  KdTrap: JIT debugging requested, waiting for OEM selection\r\n")));
            letKDHandle = !DD_IoControl(KD_IOCTL_JIT_NOTIF, NULL, 0);
            if (letKDHandle)
            {
                DEBUGGERMSG(KDZONE_ALERT, (TEXT("  KdTrap: JIT debugging accepted\r\n")));
            }
            else
            {
                DEBUGGERMSG(KDZONE_ALERT, (TEXT("  KdTrap: JIT debugging bypassed\r\n")));
            }
        }
        else
        {
            letKDHandle = FALSE;
        }

        if (letKDHandle)
        {
            EnableHDNotifs (TRUE);
            g_fKdbgRegistered = KITLIoctl(IOCTL_EDBG_REGISTER_DFLT_CLIENT, &pucDummy, KITL_SVC_KDBG, &pucDummy, 0, NULL);
            DEBUGGERMSG(KDZONE_TRAP, (TEXT("  KdTrap: KITLRegisterDfltClient returned: %d\r\n"), g_fKdbgRegistered));

            // if register kitl failed, kd can't handle it
            letKDHandle = g_fKdbgRegistered;
        }
        // stop the system once kitl registration is done.
    }
    DD_FreezeSystem();

exit:
    return letKDHandle;
}

BOOL KdpSuspendBreakpointHitByKD(DWORD_PTR pc)
{
    MEMADDR curaddr;
    BOOL fIsDebuggerBP = FALSE;
    Breakpoint *bp;
    HRESULT hr;
    enum BreakpointState bps;

    if (!DD_DisableAllBPOnHalt())
    {
        DEBUGGERMSG(KDZONE_TRAP,(L"+KdpSuspendBreakpointHitByKD(%08x)\r\n",pc));
        
        SetVirtAddr(&curaddr, PcbGetVMProc(), (void *)pc);
        // For each breakpoint on pc
        hr = DD_FindVMBreakpoint(&curaddr, NULL, &bp);
        while (SUCCEEDED(hr) && bp != NULL)
        {
            hr = DD_GetBPState(bp, &bps);
            if (SUCCEEDED(hr) && bps == BPS_ACTIVE)
            {
                DEBUGGERMSG(KDZONE_TRAP,(L" KdpSuspendBreakpointHitByKD, found bp @%08X::%08X\r\n",bp->Address.vm,bp->Address.addr));
                // Disable if it's active.
                hr = DD_SetBPState(bp, BPS_INACTIVE);
                if (FAILED(hr))
                {
                    DEBUGGERMSG(KDZONE_TRAP,(L" KdpSuspendBreakpointHitByKD, failed to disable %08X\r\n",hr));
                }
                fIsDebuggerBP = TRUE;
            }
            hr = DD_FindVMBreakpoint(&curaddr, bp, &bp);
        }
        DEBUGGERMSG(KDZONE_TRAP,(L"-KdpSuspendBreakpointHitByKD(%08x) %d\r\n",pc,fIsDebuggerBP));
    }
    return fIsDebuggerBP;
}

BOOL
KdpTrap (
    PEXCEPTION_RECORD ExceptionRecord,
    CONTEXT *pContextRecord,
    BOOL SecondChance,
    BOOL *pfIgnore
    )

/*++

Routine Description:

    This routine is called whenever a exception is dispatched and the kernel
    debugger is active.

Arguments:

    ExceptionRecord - Supplies a pointer to an exception record that
        describes the exception.

    pContextRecord - Supplies the context at the time of the exception.

    PreviousMode - Supplies the previous processor mode.

    SecondChance - Supplies a boolean value that determines whether this is
        the second chance (TRUE) that the exception has been raised.

Return Value:

    A value of TRUE is returned if the exception is handled. Otherwise a
    value of FALSE is returned.

--*/

{
    BOOL fExceptionHandledByKD;
    ULONG OldFir;
    BOOL fHostDbgConnected = FALSE;
    DWORD dwInDebugger;
   
    // WARNING: Global variable initialization must be done after the Critical Section & InterlockedIncrement below

    fExceptionHandledByKD =  FALSE;

    // KDInit not called yet.  Ignore the exception.
    if (!g_fDbgConnected)
        goto exit;

    if (DD_WasCaptureDumpFileOnDeviceCalled(ExceptionRecord) && (!SecondChance))
    {
        // We ignore these in Kernel Debugger for 1st chance, used for Watson support
        DEBUGGERMSG(KDZONE_TRAP, (TEXT("  KdTrap: CaptureDumpFileOnDevice 1st chance exception, passing on to OsAxs ... \r\n")));
        goto exit;
    }

    //We need to store the CurMSec in case we need it to debug (and for some reason interrupts were not disabled)
    //having a local copy of the CurMSec at this point allows us to know "when" the last break occurred.
    if (g_pKData->pNk)
    {
        PNKGLOBAL pNKGlobal = (PNKGLOBAL)g_pKData->pNk;
        g_dwLastKDTrapCurMSec = pNKGlobal->dwCurMSec;
    }
    
    DBGRETAILMSG (KDZONE_TRAP, (L"++KdTrap v%i.0 CPU#%d\r\n", CUR_KD_VER, PcbGetCurCpu()));
    
    // If this is a recursion into the debugger, attempt to handle.
    dwInDebugger = InterlockedIncrement(&(LONG)g_pKData->dwInDebugger);
    if (dwInDebugger > 1)
    {
        DEBUGGERMSG(KDZONE_ALERT,(L"  KdTrap[%d]: Exception of %08X in debugger, Addr=0x%08X\r\n",
                                  dwInDebugger,
                                  ExceptionRecord->ExceptionCode,
                                  ExceptionRecord->ExceptionAddress));
        if (ExceptionRecord->ExceptionCode == STATUS_BREAKPOINT &&
              KdpSuspendBreakpointHitByKD(CONTEXT_TO_PROGRAM_COUNTER(pContextRecord)))
        {
            fExceptionHandledByKD = TRUE;
            *pfIgnore = TRUE;
            DEBUGGERMSG(KDZONE_TRAP,(TEXT("  KdTrap[%d]: Suspended BP @%08X\r\n"),
                dwInDebugger,CONTEXT_TO_PROGRAM_COUNTER(pContextRecord)));
        }
        else
        {   
            if (SecondChance)
            {
                DEBUGGERMSG(KDZONE_ALERT, (TEXT(" unable to recover\r\n")));
            }
            else
            {
                DEBUGGERMSG(KDZONE_ALERT, (TEXT(" attempting to recover\r\n")));
            }
            fExceptionHandledByKD = FALSE;
        }
        goto decrement_and_exit;
    }

    // Global variable initialization must be done after the Critical Section & InterlockedIncrement to prevent changing by re-entrancy
    DD_PushExceptionState(ExceptionRecord, pContextRecord, SecondChance);
    
    if (!KdpRegisterKITL(pContextRecord, SecondChance))
    {
        DBGRETAILMSG (KDZONE_TRAP, (L"  KdTrap: KdpRegisterKITL failed\r\n"));
        fExceptionHandledByKD = FALSE;
        goto popstate_and_exit;
    }

    OldFir = CONTEXT_TO_PROGRAM_COUNTER (pContextRecord);

    DEBUGGERMSG(KDZONE_TRAP, (TEXT("  KdTrap: Exception at %08X Breakcode = %08X ExceptionCode = %08X\r\n"),
        CONTEXT_TO_PROGRAM_COUNTER (pContextRecord),
        ExceptionRecord->ExceptionInformation[0],
        ExceptionRecord->ExceptionCode));

    if (g_fKdbgRegistered)
    {
        DEBUGGERMSG(KDZONE_TRAP, (L"  KdTrap: Desktop debugger connected\r\n"));
        fHostDbgConnected = TRUE;
    }
    else
    {
        DEBUGGERMSG(KDZONE_TRAP, (L"  KdTrap: Desktop debugger NOT connected\r\n"));
    }

    DEBUGGERMSG(KDZONE_TRAP,(TEXT("  KdTrap: Exception at %08X (%a chance)\r\n"), CONTEXT_TO_PROGRAM_COUNTER (pContextRecord), SecondChance ? "2nd" : "1st"));

    if (fHostDbgConnected || SecondChance)
    {
        // BEGIN Interacting with the desktop.
        DD_UpdateModLoadNotif ();
        DD_CaptureExtraContext();
        fExceptionHandledByKD = KdpReportExceptionNotif(ExceptionRecord, SecondChance);
        
        // END
    }
    else
    {
        g_fForceReload = TRUE;         // We are disconnected, we may miss load notifications: force reload
        fExceptionHandledByKD = FALSE; // tell the kernel to continue normally
    }

    *pfIgnore = fExceptionHandledByKD;

    if (!DD_DisableAllBPOnHalt())
    {
        DEBUGGERMSG(KDZONE_TRAP,(L" KdpTrap: Enable all BP before leaving debugger\r\n"));
        // Breakpoints are not getting disabled on halt, so we need to turn breakpoints
        // back on just in case the user is debugging osaxst0/osaxst1/hdstub.
        DD_EnableAllBP();
    }
    
popstate_and_exit:
    DD_PopExceptionState();
decrement_and_exit:
    InterlockedDecrement(&(LONG) g_pKData->dwInDebugger);
    DEBUGGERMSG(KDZONE_TRAP,(L"  KdTrap: Decrement debugger entrance count.\r\n"));
exit:
    DBGRETAILMSG (KDZONE_TRAP, (L"--KdTrap\r\n"));
    DEBUGGERMSG(KDZONE_SWBP, (TEXT("  ### KDTrap return %d Second=%d\r\n"), 
        fExceptionHandledByKD, SecondChance));
    return fExceptionHandledByKD;
}


VOID KdpReboot(
    IN BOOL fReboot
    )
/*++

Routine Description:

    This routine is called with fReboot = TRUE when we are about to reboot the hardware.
    If the reboot fails this routine is called again with fReboot = FALSE.

Arguments:

    fReboot - TRUE - We are about to reboot, FALSE - reboot failed

--*/
{
    DEBUGGERMSG(KDZONE_TRAP,(L"++KdpReboot\r\n"));
    DEBUGGERMSG(KDZONE_TRAP,(L"--KdpReboot\r\n"));
}

