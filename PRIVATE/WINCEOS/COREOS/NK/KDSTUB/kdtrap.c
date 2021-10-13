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

extern BOOL g_fDbgConnected;

CRITICAL_SECTION csDbg;

BOOL g_fForceReload = TRUE;
ACCESSKEY g_ulOldKey;

void FlushDCache(void);

ULONG
KdpTrap (
    IN PEXCEPTION_RECORD ExceptionRecord,
    IN CONTEXT *ContextRecord,
    IN BOOLEAN SecondChance
    )

/*++

Routine Description:

    This routine is called whenever a exception is dispatched and the kernel
    debugger is active.

Arguments:

    ExceptionRecord - Supplies a pointer to an exception record that
        describes the exception.

    ContextRecord - Supplies the context at the time of the exception.

    PreviousMode - Supplies the previous processor mode.

    SecondChance - Supplies a boolean value that determines whether this is
        the second chance (TRUE) that the exception has been raised.

Return Value:

    A value of TRUE is returned if the exception is handled. Otherwise a
    value of FALSE is returned.

--*/

{
    BOOLEAN bRet=TRUE;
    KD_SYMBOLS_INFO SymbolInfo;
    STRING ImageName;
    ULONG ImageSize, OldFir;
    NAMEANDBASE NameandBase;
    DWORD BreakCode;
    KD_EXCEPTION_INFO kei;
    BOOL fHostDbgConnected = FALSE;
    BOOL fLeftPpfsCs = FALSE;
    PUCHAR pucDummy;
    ACCESSKEY ulOldKey;

    if (!g_fDbgConnected)
        return FALSE;

    DEBUGGERMSG (KDZONE_TRAP, (L"++KdTrap\r\n"));

    SWITCHKEY (ulOldKey, 0xffffffff);

    // initialize stackwalk variables to represent valid state
    if (pCurThread)
    {
        pStk        = pCurThread->pcstkTop;
        pLastProc   = pCurProc;
        pWalkThread = pCurThread;
    }
    g_pFocusProcOverride = NULL;

    if (!InSysCall())
    {
        DEBUGGERMSG(KDZONE_TRAP, (TEXT("  KdTrap: ++ EnterCriticalSection\r\n")));
        EnterCriticalSection(&csDbg);
        DEBUGGERMSG(KDZONE_TRAP, (TEXT("  KdTrap: -- EnterCriticalSection\r\n")));
    }

    if (InterlockedIncrement(&kdpKData->dwInDebugger) > 1)
    {
        // temporarily suspend a breakpoint if it was known and hit during exception processing
        if ((STATUS_BREAKPOINT == ExceptionRecord->ExceptionCode) &&
            KdpSuspendBreakpointIfHitByKd((VOID*)CONTEXT_TO_PROGRAM_COUNTER(ContextRecord)))
        {
            DEBUGGERMSG(KDZONE_TRAP, (TEXT("  KdTrap: Suspended breakpoints\r\n")));
            bRet = TRUE;
        }
        else
        {
            DEBUGGERMSG(KDZONE_ALERT, (TEXT("  KdTrap: Exception in debugger - ")));

            if (SecondChance)
            {
                DEBUGGERMSG(KDZONE_ALERT, (TEXT("unable to recover\r\n")));
            }
            else
            {
                DEBUGGERMSG(KDZONE_ALERT, (TEXT("attempting to recover\r\n")));
            }
            bRet = FALSE;
        }

        goto exit;
    }

    g_ulOldKey = ulOldKey; // This must be after the Critical Section & InterlockedIncrement to prevent changing by re-entrancy //

    if (!g_fKdbgRegistered)
    { // We never registered KDBG service
        // Check if KITL has started
        if (g_kdKernData.pKITLIoCtl(IOCTL_EDBG_IS_STARTED, NULL, 0, NULL, 0, NULL) || SecondChance)
        { // Yes, register KDBG
            // JIT Debugging - Make sure no other threads run while we wait for host to connect
            DWORD cSavePrio = GET_CPRIO (pCurThread);
            // Set the quantum to zero to prevent any other real-time thread from running
            DWORD dwSaveQuantum = pCurThread->dwQuantum;
            pCurThread->dwQuantum = 0;
            SET_CPRIO (pCurThread, 0);
            g_fKdbgRegistered = g_kdKernData.pKITLIoCtl(IOCTL_EDBG_REGISTER_DFLT_CLIENT, &pucDummy, KITL_SVC_KDBG, &pucDummy, 0, NULL);
            pCurThread->dwQuantum = dwSaveQuantum;
            SET_CPRIO (pCurThread, cSavePrio);
            DEBUGGERMSG(KDZONE_TRAP, (TEXT("  KdTrap - KITLRegisterDfltClient returned: %d\r\n"), g_fKdbgRegistered));
        }
        else
        {
            DEBUGGERMSG(KDZONE_TRAP, (TEXT("  KdTrap - KITL is not started\r\n")));
        }
    }
    
    if (hCurThread == pppfscs->OwnerThread)
    {
        DEBUGGERMSG(KDZONE_TRAP, (TEXT("  KdTrap: Releasing ppfscs to prevent ordering violation\r\n")));
        LeaveCriticalSection(pppfscs);
        
        fLeftPpfsCs = TRUE;
    }

    OldFir=CONTEXT_TO_PROGRAM_COUNTER(ContextRecord);
    if ((ZeroPtr(CONTEXT_TO_PROGRAM_COUNTER(ContextRecord)) > (1 << VA_SECTION)) ||
        (ZeroPtr(CONTEXT_TO_PROGRAM_COUNTER(ContextRecord)) < (DWORD)DllLoadBase))
    {
         CONTEXT_TO_PROGRAM_COUNTER(ContextRecord)=(UINT)MapPtrProc(CONTEXT_TO_PROGRAM_COUNTER(ContextRecord), pCurProc);
    }

    if (g_kdKernData.pKDIoControl)
    {
        kei.nVersion = 1;
        kei.ulAddress = (ULONG)ExceptionRecord->ExceptionAddress;
        kei.ulExceptionCode = ExceptionRecord->ExceptionCode;
        kei.ulFlags = 0;
        if (KCall(g_kdKernData.pKDIoControl, KD_IOCTL_MAP_EXCEPTION, &kei, sizeof(KD_EXCEPTION_INFO)))
            ExceptionRecord->ExceptionCode = kei.ulExceptionCode;
    }


#ifdef MIPS
    if ((ExceptionRecord->ExceptionCode == STATUS_BREAKPOINT) &&
        (ExceptionRecord->ExceptionInformation[0] > DEBUGBREAK_UNLOAD_SYMBOLS_BREAKPOINT))
    {
        if (Is16BitSupported) {

            if (CONTEXT_TO_PROGRAM_COUNTER(ContextRecord) & 1)
            {
                BreakCode = (ExceptionRecord->ExceptionInformation[0] >> 5) & 0x3f;
            }
            else
            {
                BreakCode = (ExceptionRecord->ExceptionInformation[0] >> 16) & 0x3FF;
            }
        } else 
            BreakCode = (ExceptionRecord->ExceptionInformation[0] >> 16) & 0x3FF;

        // If the breakcode happens to resolve to Load/unload symbols,
        // then something bad happened when calculating the breakcode.
        // Override to default full stop.
        if (BreakCode == DEBUGBREAK_LOAD_SYMBOLS_BREAKPOINT ||
            BreakCode == DEBUGBREAK_UNLOAD_SYMBOLS_BREAKPOINT)
        {
            BreakCode = DEBUGBREAK_STOP_BREAKPOINT;
        }
    }
    else
    {
        BreakCode = ExceptionRecord->ExceptionInformation[0];
    }
    DEBUGGERMSG(KDZONE_TRAP, (TEXT("  KdTrap: Exception at %08X Breakcode = %08X\r\n"),
                              CONTEXT_TO_PROGRAM_COUNTER(ContextRecord), BreakCode));

#else
    BreakCode = ExceptionRecord->ExceptionInformation[0];
    DEBUGGERMSG(KDZONE_TRAP, (TEXT("  KdTrap: Exception at %08X Breakcode = %08X ExceptionCode = %08X\r\n"),
        CONTEXT_TO_PROGRAM_COUNTER(ContextRecord),
        ExceptionRecord->ExceptionInformation[0],
        ExceptionRecord->ExceptionCode));
#endif

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

   //
   // If this is a breakpoint instruction, determine what kind
   //
   
   if ((ExceptionRecord->ExceptionCode == STATUS_BREAKPOINT) ||
       (ExceptionRecord->ExceptionCode == STATUS_SYSTEM_BREAK))
   {
        //
        // Switch on the breakpoint code.
        //

        switch (BreakCode) {
        case DEBUGBREAK_LOAD_SYMBOLS_BREAKPOINT:
        case DEBUGBREAK_UNLOAD_SYMBOLS_BREAKPOINT:
            if (fHostDbgConnected)
            {
                if (!GetNameandImageBase(pCurProc,
                                         CONTEXT_TO_PROGRAM_COUNTER(ContextRecord),
                                         &NameandBase,
                                         FALSE,
                                         BreakCode))
                {
#if   defined(MIPS)
                    if (Is16BitSupported && (CONTEXT_TO_PROGRAM_COUNTER(ContextRecord) & 1))
                    {
                        CONTEXT_TO_PROGRAM_COUNTER(ContextRecord) += 2;
                    }
                    else
                    {
                        CONTEXT_TO_PROGRAM_COUNTER(ContextRecord) += 4;
                    }
#elif defined(SHx)
                    CONTEXT_TO_PROGRAM_COUNTER(ContextRecord) += 2;
#elif defined(x86)
                    CONTEXT_TO_PROGRAM_COUNTER(ContextRecord) += 2;
#elif defined(ARM)
#if defined(THUMBSUPPORT)
                    if (CONTEXT_TO_PROGRAM_COUNTER(ContextRecord) & 1)
                    {
                        CONTEXT_TO_PROGRAM_COUNTER(ContextRecord) += 2;
                    }
                    else
#endif
                    CONTEXT_TO_PROGRAM_COUNTER(ContextRecord) += 4;
#endif
                    goto exit;
               }

               ImageName.Buffer = (PCHAR)NameandBase.szName;
               ImageName.Length = strlen(ImageName.Buffer);
               ImageSize = NameandBase.ImageSize;

               SymbolInfo.BaseOfDll = (void *) NameandBase.ImageBase;
               SymbolInfo.SizeOfImage = ImageSize;
               SymbolInfo.dwDllRwStart = NameandBase.dwDllRwStart;
               SymbolInfo.dwDllRwEnd = NameandBase.dwDllRwEnd;
               SymbolInfo.ProcessId = (ULONG)-1;
               SymbolInfo.CheckSum = 0;
               if (!KdpReportLoadSymbolsStateChange(&ImageName,
                                                    &SymbolInfo,
                                                    (BOOLEAN) BreakCode,
                                                    ContextRecord))
               {
                    DEBUGGERMSG(KDZONE_TRAP, (L"  KdTrap: Load symbols failed\r\n"));
                    CONTEXT_TO_PROGRAM_COUNTER(ContextRecord)=OldFir;
                    bRet=FALSE;
               }
            }
            else
            {
                g_fForceReload = TRUE; // We are disconnected, we may miss load notifications: force reload
                bRet = TRUE;
            }
            goto exit;

#if 0
        case DEBUGBREAK_STOP_BREAKPOINT:
            break;

        case DEBUG_PROCESS_SWITCH_BREAKPOINT:
            DEBUGGERMSG(KDZONE_TRAP,(L"Process switch\r\n"));
            break;

        case DEBUG_THREAD_SWITCH_BREAKPOINT:
            DEBUGGERMSG(KDZONE_TRAP,(L"Thread switch\r\n"));
            ClearThreadSwitch(ContextRecord);
            break;

        case DEBUG_BREAK_IN:
            DEBUGGERMSG(KDZONE_TRAP,(L"Hit black button\r\n"));
            break;
#endif            
        default:
            break;
        }
    }

    // save off the context to the owner of the DSP or FPU so thread-specific stackwalk will be accurate
    KdpFlushExtendedContext(ContextRecord);

    DEBUGGERMSG(KDZONE_TRAP,(TEXT("  KdTrap: Exception StateChange at %08X (%a chance)\r\n"),CONTEXT_TO_PROGRAM_COUNTER(ContextRecord),SecondChance?"2nd":"1st"));

    if (fHostDbgConnected || SecondChance)
    {
        if (!(bRet = KdpReportExceptionStateChange(ExceptionRecord,
                                       ContextRecord,
                                       SecondChance)))
        {
            CONTEXT_TO_PROGRAM_COUNTER(ContextRecord)=OldFir;
        }
    }
    else
    {
        g_fForceReload = TRUE; // We are disconnected, we may miss load notifications: force reload
        bRet = FALSE;          // tell the kernel to continue normally
    }

    CONTEXT_TO_PROGRAM_COUNTER(ContextRecord)=ZeroPtr(CONTEXT_TO_PROGRAM_COUNTER(ContextRecord));

    // reinstate breakpoints that were suspended because we hit them during exception processing
    KdpReinstateSuspendedBreakpoints();

exit:
    if (fLeftPpfsCs)
    {
        DEBUGGERMSG(KDZONE_TRAP, (TEXT("  KdTrap: Reacquiring ppfscs\r\n")));
    
        EnterCriticalSection(pppfscs);
    }
#if   defined(MIPS)
    FlushICache();
#elif defined(SHx)
    FlushCache();
#elif defined(ARM)
    FlushDCache();
    FlushICache();
#endif

    SETCURKEY (ulOldKey);

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

    DEBUGGERMSG (KDZONE_TRAP, (L"--KdTrap\r\n"));
    return bRet;
}


void UpdateSymbols(DWORD dwAddr, BOOL UnloadSymbols)
{
    EXCEPTION_RECORD er;
    CONTEXT cr;
    NAMEANDBASE NameandBase;

    DEBUGGERMSG(KDZONE_TRAP, (L"++UpdateSymbols\r\n"));
    if (GetNameandImageBase (pCurProc,
                             dwAddr,
                             &NameandBase,
                             TRUE,
                             UnloadSymbols ? DEBUGBREAK_UNLOAD_SYMBOLS_BREAKPOINT : DEBUGBREAK_LOAD_SYMBOLS_BREAKPOINT))
    {
    	BOOL fRomDll = (0xFFFFFFFF != NameandBase.dwDllRwStart) || (0x00000000 != NameandBase.dwDllRwEnd);
		if (UnloadSymbols)
		{
			if (fRomDll)
			{
				RETAILMSG (1, (L"<<< Unloading module %a at address 0x%08X-0x%08X (RW data at 0x%08X-0x%08X)\r\n", NameandBase.szName, NameandBase.ImageBase, NameandBase.ImageBase + NameandBase.ImageSize, NameandBase.dwDllRwStart, NameandBase.dwDllRwEnd));
			}
			else
			{ // RAM DLL or EXE
				RETAILMSG (1, (L"<<< Unloading module %a at address 0x%08X-0x%08X\r\n", NameandBase.szName, NameandBase.ImageBase, NameandBase.ImageBase + NameandBase.ImageSize));
			}
		}
		else
		{
			if (fRomDll)
			{
				RETAILMSG (1, (L">>> Loading module %a at address 0x%08X-0x%08X (RW data at 0x%08X-0x%08X)\r\n", NameandBase.szName, NameandBase.ImageBase, NameandBase.ImageBase + NameandBase.ImageSize, NameandBase.dwDllRwStart, NameandBase.dwDllRwEnd));
			}
			else
			{ // RAM DLL or EXE
				RETAILMSG (1, (L">>> Loading module %a at address 0x%08X-0x%08X\r\n", NameandBase.szName, NameandBase.ImageBase, NameandBase.ImageBase + NameandBase.ImageSize));
			}
		}
		memset(&er, 0, sizeof(er));
		memset(&cr, 0, sizeof(cr));
		er.ExceptionAddress = (PVOID)dwAddr;
		er.ExceptionInformation[0] = UnloadSymbols ? DEBUGBREAK_UNLOAD_SYMBOLS_BREAKPOINT : DEBUGBREAK_LOAD_SYMBOLS_BREAKPOINT;
		DEBUGGERMSG(KDZONE_TRAP,(TEXT("  UpdateSymbols, er.ExceptionInformation[0] = %08X\r\n"), er.ExceptionInformation[0]));
		er.ExceptionCode = STATUS_BREAKPOINT;
		CONTEXT_TO_PROGRAM_COUNTER(&cr) = dwAddr;
		KdpTrap(&er, &cr, FALSE);
    }
    DEBUGGERMSG(KDZONE_TRAP, (L"--UpdateSymbols\r\n"));
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

