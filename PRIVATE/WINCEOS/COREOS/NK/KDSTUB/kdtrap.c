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
/*++


Module Name:

    kdtrap.c

Abstract:

    This module contains code to implement the target side of the portable
    kernel debugger.

--*/

#include "kdp.h"

//#define NON_BLOCKING_KDSTUB // Uncomment this to allow KDSTUB to be non blocking until the KDBG stream is established
// Note: The drawback of non-blocking kdstub is that we may miss instanciation of defered BP on first modules (until KDBG connects)

CRITICAL_SECTION csDbg;

BOOL g_fForceReload = TRUE;
EXCEPTION_INFO g_exceptionInfo = {0};
SAVED_THREAD_STATE g_svdThread = {0};
BOOL g_fWorkaroundPBForVMBreakpoints = FALSE;
BOOL g_fHandlingLoadNotification = FALSE;

void FlushDCache(void);

// Allow device side exception filtering with a number of specified processes and/or exception codes.
BOOL g_fFilterExceptions = FALSE;
CHAR g_szFilterExceptionProc[MAX_FILTER_EXCEPTION_PROC][CCH_PROCNAME] = {0};
DWORD g_dwFilterExceptionCode[MAX_FILTER_EXCEPTION_CODE] = {STATUS_BREAKPOINT, STATUS_INVALID_SYSTEM_SERVICE, STATUS_SYSTEM_BREAK, 0, 0, 0, 0, 0, 0, 0};

ULONG
KdpTrap (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN CONTEXT *pContextRecord,
    IN BOOLEAN SecondChance
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
    BOOLEAN fExceptionHandledByKD = TRUE;
    ULONG OldFir;
    KD_EXCEPTION_INFO kei;
    BOOL fHostDbgConnected = FALSE;
    PUCHAR pucDummy;
    SAVED_THREAD_STATE svdThread;
    EXCEPTION_INFO exceptionInfo;
    HRESULT hrOsAxsT0 = E_FAIL;
    HRESULT hrOsAxsT1 = E_FAIL;
    EXCEPTION_INFO *pexceptionInfoSaveOsAxsT0=NULL;
    EXCEPTION_INFO *pexceptionInfoSaveOsAxsT1=NULL;
    SAVED_THREAD_STATE *psvdThreadSaveOsAxsT0=NULL;
    SAVED_THREAD_STATE *psvdThreadSaveOsAxsT1=NULL;

    // WARNING: Global variable initialization must be done after the Critical Section & InterlockedIncrement below
    
    if (!g_fDbgConnected)
        return FALSE; // KDInit not called yet - Ignore exception

    if (g_fFilterExceptions)
    {
        BOOL fAllowException = FALSE;
        DWORD dwCodeNo;
        DWORD dwProcNo;
        
        for (dwCodeNo = 0; (dwCodeNo < MAX_FILTER_EXCEPTION_CODE) && (!fAllowException); ++ dwCodeNo)
        {
            if (g_dwFilterExceptionCode[dwCodeNo] == ExceptionRecord->ExceptionCode)
            {
                fAllowException = TRUE;
            }
        }

        for (dwProcNo = 0; (dwProcNo < MAX_FILTER_EXCEPTION_PROC) && (!fAllowException); ++ dwProcNo)
        {
            CHAR* szProcName = GetWin32ExeName (g_pKData->pCurPrc);
            if (strcmp(szProcName, g_szFilterExceptionProc[dwProcNo]) == 0)
            {
                fAllowException = TRUE;
            }
        }

        if (!fAllowException)
        {
            DEBUGGERMSG (KDZONE_ALERT, (L"  KdTrap: IGNORING EXCEPTION (0x%08x) in process '%s'--\r\n", ExceptionRecord->ExceptionCode, GetWin32ExeName (g_pKData->pCurPrc)));
            return FALSE;
        }
    }

    DBGRETAILMSG (KDZONE_TRAP, (L"++KdTrap v%i.0\r\n", CUR_KD_VER));

    KDEnableInt (FALSE, &svdThread); // Disable interupts, set thread prio / quantum to real-time, save current

    if (!InSysCall())
    {
        DEBUGGERMSG(KDZONE_TRAP, (TEXT("  KdTrap: ++ EnterCriticalSection\r\n")));
        EnterCriticalSection(&csDbg);
        DEBUGGERMSG(KDZONE_TRAP, (TEXT("  KdTrap: -- EnterCriticalSection\r\n")));
    }

    if (InterlockedIncrement(&g_pKData->dwInDebugger) > 1)
    { // Recursion in KdStub: attempt to recover
        // temporarily suspend a breakpoint if it was known and hit during exception processing
        if ((STATUS_BREAKPOINT == ExceptionRecord->ExceptionCode) &&
            KdpSuspendBreakpointIfHitByKd ((VOID*) CONTEXT_TO_PROGRAM_COUNTER (pContextRecord)))
        { // Hit BP while in KDStub and succeeded to remove it (likely to be in KITL or Kernel code)
            fExceptionHandledByKD = TRUE;
            DEBUGGERMSG(KDZONE_TRAP, (TEXT("  KdTrap: Suspended breakpoints\r\n")));
        }
        else
        {
            DEBUGGERMSG(KDZONE_ALERT, (TEXT("  KdTrap: Exception in debugger, Addr=0x%08X - "),ExceptionRecord->ExceptionAddress));

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

        goto exit;
    }

    if (CAPTUREDUMPFILEONDEVICE_CALLED(ExceptionRecord, *g_kdKernData.ppCaptureDumpFileOnDevice, *g_kdKernData.ppKCaptureDumpFileOnDevice)
                                       && (!SecondChance))
    {
        // We ignore these in Kernel Debugger for 1st chance, used for Watson support
        DEBUGGERMSG(KDZONE_TRAP, (TEXT("  KdTrap: CaptureDumpFileOnDevice 1st chance exception, passing on to OsAxs ... \r\n")));
        fExceptionHandledByKD = FALSE;
        goto exit;
    }

    // Global variable initialization must be done after the Critical Section & InterlockedIncrement to prevent changing by re-entrancy
    exceptionInfo.ExceptionPointers.ContextRecord = pContextRecord;
    exceptionInfo.ExceptionPointers.ExceptionRecord = ExceptionRecord;
    exceptionInfo.SecondChance = SecondChance;
    g_svdThread = svdThread;
    g_exceptionInfo = exceptionInfo;
    
    g_pFocusProcOverride = NULL;

    if (!g_fKdbgRegistered)
    { // We never registered KDBG service
        // Check if KITL has started or JIT
        BOOL fKitlStarted = g_kdKernData.pKITLIoCtl(IOCTL_EDBG_IS_STARTED, NULL, 0, NULL, 0, NULL);
        if (fKitlStarted || SecondChance)
        {
            // Don't activate KITL because of HDStub's breakpoint
            if ((CONTEXT_TO_PROGRAM_COUNTER (pContextRecord) <  (DWORD)g_kdKernData.pfnHwTrap) ||
                (CONTEXT_TO_PROGRAM_COUNTER (pContextRecord) > ((DWORD)g_kdKernData.pfnHwTrap + HD_NOTIFY_MARGIN)))
            {
                fExceptionHandledByKD = TRUE; // Default: KD handles exception
                if (!fKitlStarted)
                { // JIT Debugging case
                    // Notify OAL that a JIT debug is pending, give a chance to notify user and accept or bypass
                    // KD_IOCTL_JIT_NOTIF can be used by OEM to warn the user that the device is currently
                    // blocked at an exception waiting to be handled by the debugger. The OAL code could
                    // simply blink some LEDs and wait for a key to be pressed. Depending on the key pressed,
                    // the user could indicate if the exception should be handled by the debugger or simply
                    // ignored and handled by the OS.
                    DEBUGGERMSG(KDZONE_ALERT, (TEXT("  KdTrap: JIT debugging requested, waiting for OEM selection\r\n")));
                    fExceptionHandledByKD = !KDIoControl (KD_IOCTL_JIT_NOTIF, NULL, 0);
                    if (fExceptionHandledByKD)
                    {
                        DEBUGGERMSG(KDZONE_ALERT, (TEXT("  KdTrap: JIT debugging accepted\r\n")));
                    }
                    else
                    {
                        DEBUGGERMSG(KDZONE_ALERT, (TEXT("  KdTrap: JIT debugging bypassed\r\n")));
                    }
                }

                if (fExceptionHandledByKD)
                {
                    EnableHDNotifs (TRUE);
                    // register KDBG
                    g_fKdbgRegistered = g_kdKernData.pKITLIoCtl(IOCTL_EDBG_REGISTER_DFLT_CLIENT, &pucDummy, KITL_SVC_KDBG, &pucDummy, 0, NULL);
                    DEBUGGERMSG(KDZONE_TRAP, (TEXT("  KdTrap: KITLRegisterDfltClient returned: %d\r\n"), g_fKdbgRegistered));
                }
            }
            else
            {
                fExceptionHandledByKD = FALSE;
            }

            if (!fExceptionHandledByKD) goto exit;
        }
        else
        {
            DEBUGGERMSG(KDZONE_TRAP, (TEXT("  KdTrap: KITL is not started\r\n")));
        }
    }

    OldFir = CONTEXT_TO_PROGRAM_COUNTER (pContextRecord);

    // Allow OAL to remap exceptions
    kei.nVersion = 1;
    kei.ulAddress = (ULONG)ExceptionRecord->ExceptionAddress;
    kei.ulExceptionCode = ExceptionRecord->ExceptionCode;
    kei.ulFlags = 0;
    if (KDIoControl (KD_IOCTL_MAP_EXCEPTION, &kei, sizeof(KD_EXCEPTION_INFO)))
    {
        ExceptionRecord->ExceptionCode = kei.ulExceptionCode;
    }

    DEBUGGERMSG(KDZONE_TRAP, (TEXT("  KdTrap: Exception at %08X Breakcode = %08X ExceptionCode = %08X\r\n"),
        CONTEXT_TO_PROGRAM_COUNTER (pContextRecord),
        ExceptionRecord->ExceptionInformation[0],
        ExceptionRecord->ExceptionCode));

    if (g_fKdbgRegistered
#ifdef NON_BLOCKING_KDSTUB
        && pfnIsDesktopDbgrExist && pfnIsDesktopDbgrExist()
#endif
        )
    {
        DEBUGGERMSG(KDZONE_TRAP, (L"  KdTrap: Desktop debugger connected\r\n"));
        fHostDbgConnected = TRUE;
    }
    else
    {
        DEBUGGERMSG(KDZONE_TRAP, (L"  KdTrap: Desktop debugger NOT connected\r\n"));
    }

    // Set exception info & saved thread info in OsAxsT0 & OsAxsT1
    if (Hdstub.pfnCallClientIoctl)
    {
        OSAXSFN_SAVEEXCEPTIONSTATE osaxsArgs = {0};

        osaxsArgs.pNewExceptionInfo    = &exceptionInfo;
        osaxsArgs.pNewSavedThreadState = &svdThread;
        osaxsArgs.ppSavedExceptionInfo = &pexceptionInfoSaveOsAxsT0;
        osaxsArgs.ppSavedThreadState   = &psvdThreadSaveOsAxsT0;
        
        // OsAxsT0 will also flush the FPU registers
        DBGRETAILMSG(KDZONE_TRAP, (TEXT("  KdTrap: calling OsAxsT0!SaveExceptionState\r\n")));
        hrOsAxsT0 = Hdstub.pfnCallClientIoctl (OSAXST0_NAME, OSAXST0_IOCTL_SAVE_EXCEPTION_STATE, &osaxsArgs);
        if (FAILED(hrOsAxsT0))
        {
            DBGRETAILMSG(KDZONE_ALERT, (TEXT("  KdTrap: Failed to set exception context for OsAxsT0, hr = 0x%08X\r\n"),hrOsAxsT0));
        }
        else
        {
            DBGRETAILMSG(KDZONE_TRAP, (TEXT("  KdTrap: OsAxsT0!SaveExceptionState prev %08X, now %08X\r\n"), pexceptionInfoSaveOsAxsT0, &exceptionInfo));
        }
        
        // Re-Disable interrupts (FPUFlushContext in OsAxsT0 may restore them with KCall)
        KDEnableInt (FALSE, NULL);

        osaxsArgs.pNewExceptionInfo    = &exceptionInfo;
        osaxsArgs.pNewSavedThreadState = &svdThread;
        osaxsArgs.ppSavedExceptionInfo = &pexceptionInfoSaveOsAxsT1;
        osaxsArgs.ppSavedThreadState   = &psvdThreadSaveOsAxsT1;

        hrOsAxsT1 = Hdstub.pfnCallClientIoctl (OSAXST1_NAME, OSAXST1_IOCTL_SAVE_EXCEPTION_STATE, &osaxsArgs);
        if (FAILED(hrOsAxsT1))
        {
            DBGRETAILMSG(KDZONE_ALERT, (TEXT("  KdTrap: Failed to set exception context for OsAxsT1, hr = 0x%08X\r\n"),hrOsAxsT1));
        }
        else
        {
            DBGRETAILMSG(KDZONE_TRAP, (TEXT("  KdTrap: OsAxsT1!SaveExceptionState prev %08X, now %08X\r\n"), pexceptionInfoSaveOsAxsT1, &exceptionInfo));
        }
    }
    else
    {
        DBGRETAILMSG(KDZONE_ALERT, (TEXT("  KdTrap: Hdstub.pfnCallClientIoctl not set\r\n")));
    }

    DEBUGGERMSG(KDZONE_TRAP,(TEXT("  KdTrap: Exception at %08X (%a chance)\r\n"), CONTEXT_TO_PROGRAM_COUNTER (pContextRecord), SecondChance ? "2nd" : "1st"));

    KdpFlushBreakpointKva();
     
    if (fHostDbgConnected || SecondChance)
    {
        if (!(fExceptionHandledByKD = KdpReportExceptionNotif (ExceptionRecord, SecondChance)))
        {
            CONTEXT_TO_PROGRAM_COUNTER (pContextRecord) = OldFir;
        }
    }
    else
    {
        g_fForceReload = TRUE; // We are disconnected, we may miss load notifications: force reload
        fExceptionHandledByKD = FALSE; // tell the kernel to continue normally
    }

    // Once a notification is processed, this flag is cleared.
    g_fHandlingLoadNotification = FALSE;

    // reinstate breakpoints that were suspended because we hit them during exception processing
    KdpReinstateSuspendedBreakpoints();

exit:
    g_pFocusProcOverride = NULL; // Remove debugger focus

#if   defined(MIPS)
    FlushICache();
#elif defined(SHx)
    FlushCache();
#elif defined(ARM)
    FlushDCache();
    FlushICache();
#endif

    InterlockedDecrement(&g_pKData->dwInDebugger);
    if (!InSysCall())
    {
        DEBUGGERMSG(KDZONE_TRAP, (TEXT("  KdTrap: ++ LeaveCriticalSection\r\n")));
        LeaveCriticalSection(&csDbg);
        DEBUGGERMSG(KDZONE_TRAP, (TEXT("  KdTrap: -- LeaveCriticalSection\r\n")));
    }

    // deallocate csDbg if TerminateApi was sent
    if (!g_fDbgConnected)
    {
        DEBUGGERMSG(KDZONE_TRAP, (TEXT("  KdTrap: ++ DeleteCriticalSection\r\n")));
        DeleteCriticalSection(&csDbg);
        DEBUGGERMSG(KDZONE_TRAP, (TEXT("  KdTrap: -- DeleteCriticalSection\r\n")));
    }

    // Restore exception info & saved thread info in OsAxsT0 & OsAxsT1
    if (Hdstub.pfnCallClientIoctl)
    {
        OSAXSFN_SAVEEXCEPTIONSTATE osaxsArgs = {0};
        EXCEPTION_INFO *pPrevExceptionInfoT0;
        EXCEPTION_INFO *pPrevExceptionInfoT1;
        
        // Only restore if the earlier set operation was successful
        if (SUCCEEDED(hrOsAxsT0))
        {
            osaxsArgs.pNewExceptionInfo    = pexceptionInfoSaveOsAxsT0;
            osaxsArgs.pNewSavedThreadState = psvdThreadSaveOsAxsT0;
            osaxsArgs.ppSavedExceptionInfo = &pPrevExceptionInfoT0;
            osaxsArgs.ppSavedThreadState   = NULL;

            hrOsAxsT0 = Hdstub.pfnCallClientIoctl (OSAXST0_NAME, OSAXST0_IOCTL_SAVE_EXCEPTION_STATE, &osaxsArgs);
            if (FAILED(hrOsAxsT0))
            {
                DBGRETAILMSG(KDZONE_ALERT, (TEXT("  KdTrap: Failed to restore exception context for OsAxsT0, hr = 0x%08X\r\n"),hrOsAxsT0));
            }
            else
            {
                DBGRETAILMSG(KDZONE_TRAP, (TEXT("  KdTrap: Restoring OsAxsT0 Exception Context to %08X, Prev = %08X\r\n"),
                                            pexceptionInfoSaveOsAxsT0,
                                            pPrevExceptionInfoT0));
            }
        }
        

        // Only restore if the earlier set operation was successful
        if (SUCCEEDED(hrOsAxsT1))
        {
            osaxsArgs.pNewExceptionInfo    = pexceptionInfoSaveOsAxsT1;
            osaxsArgs.pNewSavedThreadState = psvdThreadSaveOsAxsT1;
            osaxsArgs.ppSavedExceptionInfo = &pPrevExceptionInfoT1;
            osaxsArgs.ppSavedThreadState   = NULL;

            hrOsAxsT1 = Hdstub.pfnCallClientIoctl (OSAXST1_NAME, OSAXST1_IOCTL_SAVE_EXCEPTION_STATE, &osaxsArgs);
            if (FAILED(hrOsAxsT1))
            {
                DBGRETAILMSG(KDZONE_ALERT, (TEXT("  KdTrap: Failed to restore exception context for OsAxsT1, hr = 0x%08X\r\n"),hrOsAxsT1));
            }
            else
            {
                DBGRETAILMSG(KDZONE_TRAP, (TEXT("  KdTrap: Restoring OsAxsT1 Exception Context to %08X, Prev = %08X\r\n"),
                                            pexceptionInfoSaveOsAxsT1,
                                            pPrevExceptionInfoT1));
            }

        }
    }

    KDEnableInt (TRUE, &svdThread); // Re-enable interupts and restore thread prio / quantum

    DBGRETAILMSG (KDZONE_TRAP, (L"--KdTrap\r\n"));
    return fExceptionHandledByKD;
}


BOOL KdpModLoad (DWORD dwStructAddr)
{
    g_fHandlingLoadNotification = TRUE;
    return FALSE;
}


BOOL KdpModUnload (DWORD dwStructAddr)
{
    g_fHandlingLoadNotification = TRUE;
    return FALSE;
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
    if (fReboot)
    {
        // We are about to reboot, so suspend all the breakpoints
        // This is required for warm reboot since the OS will still be the same.
        // However kd.dll will be reloaded and as such will not know about the breakpoints.
        KdpSuspendAllBreakpoints();
    }
    else
    {
        // Reboot failed so we reinstate all the suspended breakpoints
        KdpReinstateSuspendedBreakpoints();
    }

    g_fHandlingLoadNotification = FALSE;
    g_fWorkaroundPBForVMBreakpoints = FALSE;
    DEBUGGERMSG(KDZONE_TRAP,(L"--KdpReboot\r\n"));
}

