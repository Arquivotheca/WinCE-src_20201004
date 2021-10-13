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

    initt1.cpp

Module Description:

    Initialize OsAxsT1.
    Handle OsAxsT1 Ioctl Commands

--*/

#include "osaxs_p.h"
#include "step.h"
#include "ThreadAffinity.h"

DEBUGGER_DATA *g_pDebuggerData;

HRESULT OsaxsT1Ioctl (DWORD, void *);

BOOL OsaxsT1ModLoad (DWORD);
BOOL OsaxsT1ModUnload (DWORD);

extern "C" BOOL OsaxsT1Init (DEBUGGER_DATA *pDebuggerData, void *pvOsaxsData)
{
    BOOL fRet;

#ifdef ARM
    InitPCBFunctions ();
#endif
    
    fRet = FALSE;
    DEBUGGERMSG(OXZONE_INIT, (L"++OsaxsT1Init\r\n"));

    if (pDebuggerData && VALID_DEBUGGER_DATA(pDebuggerData))
    {
        g_pDebuggerData = pDebuggerData;
        fRet = TRUE;

        DD_eventHandlers[KDBG_EH_SSTEP].pfnException = SingleStep::Trap;
        DD_eventHandlers[KDBG_EH_SSTEP].pfnModUnload = SingleStep::ModuleUnload;
        DD_eventHandlers[KDBG_EH_SSTEP].dwFilter = HDSTUB_FILTER_EXCEPTION | HDSTUB_FILTER_MODUNLOAD;

        DD_eventHandlers[KDBG_EH_ACTION].pfnModLoad = OsaxsT1ModLoad;
        DD_eventHandlers[KDBG_EH_ACTION].pfnModUnload = OsaxsT1ModUnload;
        DD_eventHandlers[KDBG_EH_ACTION].dwFilter = HDSTUB_FILTER_MODLOAD | HDSTUB_FILTER_MODUNLOAD;

        DD_eventHandlers[KDBG_EH_THREADAFFINITY].pfnException = ThreadAffinity::Trap;
        DD_eventHandlers[KDBG_EH_THREADAFFINITY].dwFilter = HDSTUB_FILTER_EXCEPTION;

        DD_SingleStepThread = SingleStep::StepThread;
        DD_SingleStepCBP = SingleStep::StepCBP;
        DD_SingleStepDBP = SingleStep::StepDBP;
        DD_GetNextPC = SingleStep::GetNextPC;
        DD_SimulateDebugBreakExecution = SingleStep::SimulateDebugBreakExecution;
    }
    DEBUGGERMSG(OXZONE_INIT, (L"--OsaxsT1Init: %d.\r\n", fRet));
    return fRet;
}

extern "C" BOOL WINAPI OsaxsT1DLLEntry (HINSTANCE hInstance, ULONG ulReason,
    LPVOID pvReserved)
{
    BOOL fResult;
    BOOL (*pfnKernelLibIoctl) (HANDLE, DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD);
    BOOL (*pfnInit) (DEBUGGER_DATA *, void *);

    pfnKernelLibIoctl = reinterpret_cast<BOOL (*)(HANDLE,DWORD,LPVOID,DWORD,LPVOID,DWORD,LPDWORD)> (pvReserved);
    pfnInit = OsaxsT1Init;
    fResult = TRUE;

    DEBUGREGISTER (hInstance);
    switch (ulReason)
    {
        case DLL_PROCESS_ATTACH:
            fResult = pfnKernelLibIoctl &&
                pfnKernelLibIoctl ((HANDLE)KMOD_DBG,
                    IOCTL_DBG_OSAXST1_INIT,
                    (void *)&pfnInit,
                    sizeof (pfnInit),
                    NULL,
                    0,
                    NULL);
            break;
        case DLL_PROCESS_DETACH:
            break;
    }

    return fResult;
}


void kdbgWtoA (LPCWSTR pWstr, LPCHAR pAstr) 
{
    while((*pAstr++ = (char)*pWstr++) != '\0')
        ;
}


static const CHAR lpszUnk[]="UNKNOWN";
CHAR lpszModuleName[MAX_PATH];

LPCHAR GetWin32ExeName(PPROCESS pProc)
{
    if (pProc->lpszProcName) {
        kdbgWtoA(pProc->lpszProcName,lpszModuleName);
        return lpszModuleName;
    }
    return (LPCHAR)lpszUnk;
}


typedef struct _KD_MODULE_INFO
{
    PCHAR   szName;
    ULONG   ImageBase;
    ULONG   ImageSize;
    ULONG   dwTimeStamp;
    HMODULE hDll;
    WORD    wFlags;
} KD_MODULE_INFO;


BOOL GetModuleInfo (PROCESS *pProc, DWORD dwStructAddr, KD_MODULE_INFO *pkmodi, BOOL fRedundant, BOOL fUnloadSymbols)
{
    LPMODULE lpMod;
    static BOOL PhysUpd;
    BOOL fRet = FALSE;

    DEBUGGERMSG(OXZONE_INIT, (TEXT("\r\nGetting name and base for module %8.8lx \r\n"), dwStructAddr));

    pkmodi->dwTimeStamp = 0; // set default to no TimeStamp data
    pkmodi->hDll = NULL;
    pkmodi->wFlags = 0;

    if ((PROCESS *) dwStructAddr == pProc)
    { // Process
        pkmodi->szName       = GetWin32ExeName (pProc);
        pkmodi->ImageBase    = (UINT) pProc->BasePtr;
        pkmodi->ImageSize    = pProc->e32.e32_vsize;
        pkmodi->dwTimeStamp  = pProc->e32.e32_timestamp;
        pkmodi->hDll         = NULL;
        pkmodi->wFlags       = 0;
        DEBUGGERMSG(OXZONE_INIT, (TEXT("Returning name: %a, Base: %8.8lx, Size: %8.8lx, TimeStamp: %8.8lx of Executable\r\n"),
                    pkmodi->szName, pkmodi->ImageBase, pkmodi->ImageSize, pkmodi->dwTimeStamp));
        fRet = TRUE;
    }
    else if ((void *) dwStructAddr == ((MODULE *) dwStructAddr)->lpSelf)
    { // DLL
        lpMod = (MODULE *) dwStructAddr;
        if ((lpMod->DbgFlags & DBG_SYMBOLS_LOADED) &&
            fRedundant && !fUnloadSymbols)
        {
            DEBUGGERMSG(OXZONE_INIT, (TEXT("\r\nReturing redundant\r\n")));
        }

        kdbgWtoA(lpMod->lpszModName,lpszModuleName);
        pkmodi->szName       = lpszModuleName;
        pkmodi->ImageBase    = (DWORD) lpMod->BasePtr;
        pkmodi->ImageSize    = lpMod->e32.e32_vsize;
        pkmodi->dwTimeStamp  = lpMod->e32.e32_timestamp;
        pkmodi->hDll         = (HMODULE) lpMod;
        pkmodi->wFlags       = lpMod->wFlags;
        fRet = TRUE;
    }

    if (fRet)
    {
        DEBUGGERMSG(OXZONE_INIT, (TEXT("Returning name: %a, Base: %8.8lx, Size: %8.8lx, TimeStamp: %8.8lx, Handle: %8.8lX of DLL\r\n"),
                    pkmodi->szName, pkmodi->ImageBase, pkmodi->ImageSize, pkmodi->dwTimeStamp, pkmodi->hDll));
    }
    else
    {
        DEBUGGERMSG(OXZONE_INIT, (TEXT("No module associated with address %8.8lx\r\n"), dwStructAddr));
    }

    return fRet;
}


// This is kept just for displaying module load info
void DisplayModuleChange(DWORD dwStructAddr, BOOL fUnloadSymbols)
{
    KD_MODULE_INFO kmodi;

    DEBUGGERMSG(OXZONE_INIT, (L"++DisplayModuleChange\r\n"));

    if (GetModuleInfo (GetPCB()->pCurPrc, dwStructAddr, &kmodi, TRUE, fUnloadSymbols))
    {
         RETAILMSG (1, (L"OSAXST1: %soading Module '%S' (0x%08X) at address 0x%08X-0x%08X in Process '%s' (0x%08X)\r\n", 
            fUnloadSymbols ? L"<<< Unl" : L">>> L",
            kmodi.szName, dwStructAddr, kmodi.ImageBase, kmodi.ImageBase + kmodi.ImageSize, 
            GetPCB()->pCurPrc->lpszProcName, GetPCB()->pCurPrc));
    }
    DEBUGGERMSG(OXZONE_INIT, (L"--DisplayModuleChange\r\n"));
}


BOOL OsaxsT1ModLoad (DWORD dwStructAddr)
{
    DisplayModuleChange (dwStructAddr, FALSE);
    return FALSE;
}


BOOL OsaxsT1ModUnload (DWORD dwStructAddr)
{
    DisplayModuleChange (dwStructAddr, TRUE);
    return FALSE;
}



