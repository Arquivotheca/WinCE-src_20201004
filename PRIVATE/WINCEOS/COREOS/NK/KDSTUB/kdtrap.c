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
CONTEXT *g_pctxException;
DWORD g_dwExceptionCode = 0;
SAVED_THREAD_STATE g_svdThread = {0};

void FlushDCache(void);

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
    HRESULT hrOsAxsT0 = E_FAIL;
    HRESULT hrOsAxsT1 = E_FAIL;
    PCONTEXT pContextSaveOsAxsT0=NULL;
    PCONTEXT pContextSaveOsAxsT1=NULL;
    SAVED_THREAD_STATE *psvdThreadSaveOsAxsT0=NULL;
    SAVED_THREAD_STATE *psvdThreadSaveOsAxsT1=NULL;

    // WARNING: Global variable initialization must be done after the Critical Section & InterlockedIncrement below
    
    if (!g_fDbgConnected)
        return FALSE; // KDInit not called yet - Ignore exception

    DEBUGGERMSG (KDZONE_TRAP, (L"++KdTrap v%i.1\r\n", CUR_KD_VER));

    SWITCHKEY (svdThread.aky, 0xffffffff);

    KDEnableInt (FALSE, &svdThread); // Disable interupts, set thread prio / quantum to real-time, save current

    if (!InSysCall())
    {
        DEBUGGERMSG(KDZONE_TRAP, (TEXT("  KdTrap: ++ EnterCriticalSection\r\n")));
        EnterCriticalSection(&csDbg);
        DEBUGGERMSG(KDZONE_TRAP, (TEXT("  KdTrap: -- EnterCriticalSection\r\n")));
    }

    if (InterlockedIncrement(&kdpKData->dwInDebugger) > 1)
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

    if (CAPTUREDUMPFILEONDEVICE_CALLED(ExceptionRecord, *g_kdKernData.ppCaptureDumpFileOnDevice) && (!SecondChance))
    {
        // We ignore these in Kernel Debugger for 1st chance, used for Watson support
        DEBUGGERMSG(KDZONE_TRAP, (TEXT("  KdTrap: CaptureDumpFileOnDevice 1st chance exception, passing on to OsAxs ... \r\n")));
        fExceptionHandledByKD = FALSE;
        goto exit;
    }

    // Global variable initialization must be done after the Critical Section & InterlockedIncrement to prevent changing by re-entrancy
    g_svdThread = svdThread;
    g_pctxException = pContextRecord;
    
    // initialize stackwalk variables to represent valid state
    if (pCurThread)
    {
        pStk        = pCurThread->pcstkTop;
        pLastProc   = pCurProc;
        pWalkThread = pCurThread;
    }
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
    if ((ZeroPtr(CONTEXT_TO_PROGRAM_COUNTER (pContextRecord)) > (1 << VA_SECTION)) ||
        (ZeroPtr(CONTEXT_TO_PROGRAM_COUNTER (pContextRecord)) < (DWORD)DllLoadBase))
    {
         CONTEXT_TO_PROGRAM_COUNTER (pContextRecord) = (UINT) MapPtrProc (CONTEXT_TO_PROGRAM_COUNTER (pContextRecord), pCurProc);
    }

    // Allow OAL to remap exceptions
    kei.nVersion = 1;
    kei.ulAddress = (ULONG)ExceptionRecord->ExceptionAddress;
    kei.ulExceptionCode = ExceptionRecord->ExceptionCode;
    kei.ulFlags = 0;
    if (KDIoControl (KD_IOCTL_MAP_EXCEPTION, &kei, sizeof(KD_EXCEPTION_INFO)))
    {
        ExceptionRecord->ExceptionCode = kei.ulExceptionCode;
    }

    g_dwExceptionCode = ExceptionRecord->ExceptionCode;

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

    // Set exception context & saved thread info in OsAxsT0 & OsAxsT1
    if (Hdstub.pfnCallClientIoctl)
    {
        // OsAxsT0 will also flush the FPU registers
        hrOsAxsT0 = Hdstub.pfnCallClientIoctl (OSAXST0_NAME, OSAXST0_IOCTL_SAVE_EXCEPTION_CONTEXT, 
                                               (DWORD) pContextRecord,
                                               (DWORD) &svdThread,
                                               (DWORD) &pContextSaveOsAxsT0,
                                               (DWORD) &psvdThreadSaveOsAxsT0);
        if (FAILED(hrOsAxsT0))
        {
            DEBUGGERMSG(KDZONE_ALERT, (TEXT("  KdTrap: Failed to set exception context for OsAxsT0, hr = 0x%08X\r\n"),hrOsAxsT0));
        }
        
        // Re-Disable interrupts (FPUFlushContext in OsAxsT0 may restore them with KCall)
        KDEnableInt (FALSE, NULL);
        
        hrOsAxsT1 = Hdstub.pfnCallClientIoctl (OSAXST1_NAME, OSAXST1_IOCTL_SAVE_EXCEPTION_CONTEXT, 
                                               (DWORD) pContextRecord,
                                               (DWORD) &svdThread,
                                               (DWORD) &pContextSaveOsAxsT1,
                                               (DWORD) &psvdThreadSaveOsAxsT1);
        if (FAILED(hrOsAxsT1))
        {
            DEBUGGERMSG(KDZONE_ALERT, (TEXT("  KdTrap: Failed to set exception context for OsAxsT1, hr = 0x%08X\r\n"),hrOsAxsT1));
        }
    }
    else
    {
        DEBUGGERMSG(KDZONE_ALERT, (TEXT("  KdTrap: Hdstub.pfnCallClientIoctl not set\r\n")));
    }

    DEBUGGERMSG(KDZONE_TRAP,(TEXT("  KdTrap: Exception at %08X (%a chance)\r\n"), CONTEXT_TO_PROGRAM_COUNTER (pContextRecord), SecondChance ? "2nd" : "1st"));

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

    CONTEXT_TO_PROGRAM_COUNTER (pContextRecord) = ZeroPtr (CONTEXT_TO_PROGRAM_COUNTER (pContextRecord));

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

    InterlockedDecrement(&kdpKData->dwInDebugger);
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

    // Restore exception context & saved thread info in OsAxsT0 & OsAxsT1
    if (Hdstub.pfnCallClientIoctl)
    {
        if (SUCCEEDED(hrOsAxsT0))
        {
            // Only restore if the earlier set operation was successful
            hrOsAxsT0 = Hdstub.pfnCallClientIoctl (OSAXST0_NAME, OSAXST0_IOCTL_SAVE_EXCEPTION_CONTEXT, 
                                                   (DWORD) pContextSaveOsAxsT0,
                                                   (DWORD) psvdThreadSaveOsAxsT0,
                                                   (DWORD) NULL,
                                                   (DWORD) NULL);
            if (FAILED(hrOsAxsT0))
            {
                DEBUGGERMSG(KDZONE_ALERT, (TEXT("  KdTrap: Failed to restore exception context for OsAxsT0, hr = 0x%08X\r\n"),hrOsAxsT0));
            }
        }
        
        if (SUCCEEDED(hrOsAxsT1))
        {
            // Only restore if the earlier set operation was successful
            hrOsAxsT1 = Hdstub.pfnCallClientIoctl (OSAXST1_NAME, OSAXST1_IOCTL_SAVE_EXCEPTION_CONTEXT, 
                                                   (DWORD) pContextSaveOsAxsT1,
                                                   (DWORD) psvdThreadSaveOsAxsT1,
                                                   (DWORD) NULL,
                                                   (DWORD) NULL);
            if (FAILED(hrOsAxsT1))
            {
                DEBUGGERMSG(KDZONE_ALERT, (TEXT("  KdTrap: Failed to restore exception context for OsAxsT1, hr = 0x%08X\r\n"),hrOsAxsT1));
            }
        }
    }

    KDEnableInt (TRUE, &svdThread); // Re-enable interupts and restore thread prio / quantum

    SETCURKEY (svdThread.aky);

    DEBUGGERMSG (KDZONE_TRAP, (L"--KdTrap\r\n"));
    return fExceptionHandledByKD;
}

// This is kept just for displaying module load info
// We don't send module load / unload info anymore

void DisplayModuleChange(DWORD dwStructAddr, BOOL fUnloadSymbols)
{
    KD_MODULE_INFO kmodi;

    DEBUGGERMSG(KDZONE_TRAP, (L"++DisplayModuleChange\r\n"));
    if (GetModuleInfo (pCurProc, dwStructAddr, &kmodi, TRUE, fUnloadSymbols))
    {
        BOOL fRomDll = (0xFFFFFFFF != kmodi.dwDllRwStart) || (0x00000000 != kmodi.dwDllRwEnd);

        DEBUGGERMSG(KDZONE_TRAP, (L"  DisplayModuleChange: %s module %S, at address 0x%08X-0x%08X\r\n", 
                                 fUnloadSymbols ? L"<<< Unloading" : L">>> Loading",
                                 kmodi.szName,
                                 kmodi.ImageBase, 
                                 kmodi.ImageBase + kmodi.ImageSize));

        if (fUnloadSymbols)
        {
            if (fRomDll)
            {
                RETAILMSG (1, (L"<<< Unloading module %a at address 0x%08X-0x%08X (RW data at 0x%08X-0x%08X)\r\n", kmodi.szName, kmodi.ImageBase, kmodi.ImageBase + kmodi.ImageSize, kmodi.dwDllRwStart, kmodi.dwDllRwEnd));
            }
            else
            { // RAM DLL or EXE
                RETAILMSG (1, (L"<<< Unloading module %a at address 0x%08X-0x%08X\r\n", kmodi.szName, kmodi.ImageBase, kmodi.ImageBase + kmodi.ImageSize));
            }
        }
        else
        {
            if (fRomDll)
            {
                RETAILMSG (1, (L">>> Loading module %a at address 0x%08X-0x%08X (RW data at 0x%08X-0x%08X)\r\n", kmodi.szName, kmodi.ImageBase, kmodi.ImageBase + kmodi.ImageSize, kmodi.dwDllRwStart, kmodi.dwDllRwEnd));
            }
            else
            { // RAM DLL or EXE
                RETAILMSG (1, (L">>> Loading module %a at address 0x%08X-0x%08X\r\n", kmodi.szName, kmodi.ImageBase, kmodi.ImageBase + kmodi.ImageSize));
            }
        }
    }
    DEBUGGERMSG(KDZONE_TRAP, (L"--DisplayModuleChange\r\n"));
}


BOOL KdpModLoad (DWORD dwStructAddr)
{
    DisplayModuleChange (dwStructAddr, FALSE);
    return FALSE;
}


BOOL KdpModUnload (DWORD dwStructAddr)
{
    DisplayModuleChange (dwStructAddr, TRUE);
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
}

