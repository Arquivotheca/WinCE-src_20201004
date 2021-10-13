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

    initt1.cpp

Module Description:

    Initialize OsAxsT1.
    Handle OsAxsT1 Ioctl Commands

--*/

#include "osaxs_p.h"

HDSTUB_DATA Hdstub = {0};
OSAXS_DATA g_OsaxsData = {0};
void (*g_pfnOutputDebugString) (const char *sz, ...) = 0;

// Pointer to kdata
struct KDataStruct *g_pKData;

HRESULT OsaxsT1Ioctl (DWORD, void *);

BOOL OsaxsT1ModLoad (DWORD);
BOOL OsaxsT1ModUnload (DWORD);

HDSTUB_CLIENT g_OsaxsT1Client =
{
    OSAXST1_NAME,
    NULL,
    NULL,
    OsaxsT1ModLoad,
    OsaxsT1ModUnload,

    OsaxsT1Ioctl,       
    HDSTUB_FILTER_MODLOAD | HDSTUB_FILTER_MODUNLOAD,
    
    NULL
};

extern "C" BOOL OsaxsT1Init (HDSTUB_DATA *pHdstub, void *pvOsaxsData)
{
    BOOL fRet;
    OSAXS_DATA *pOsaxsData;

    fRet = FALSE;
    pOsaxsData = (OSAXS_DATA *)pvOsaxsData;
    DEBUGGERMSG(OXZONE_INIT, (L"++OsaxsT1Init\r\n"));

    if (pHdstub && pOsaxsData && pOsaxsData->cbSize == sizeof (OSAXS_DATA))
    {
        Hdstub = *pHdstub;
        g_OsaxsData = *pOsaxsData;
        g_pKData = pOsaxsData->pKData;
        if (g_OsaxsData.pKITLIoCtl)
        {
            g_OsaxsData.pKITLIoCtl (IOCTL_EDBG_GET_OUTPUT_DEBUG_FN, NULL, 0,
                (VOID*)&g_pfnOutputDebugString, sizeof (g_pfnOutputDebugString),
                NULL);
        }

        if (Hdstub.pfnRegisterClient (&g_OsaxsT1Client, HdstubClientLast))
        {
            DEBUGGERMSG(OXZONE_INIT, (L"  OsaxsT1Init: Registered OsaxsT0 with hdstub.\r\n"));
        }
        else
        {
            DEBUGGERMSG(OXZONE_INIT, (L"  OsaxsT1Init: Failed to register OsaxsT0 with hdstub.\r\n"));
        }
        fRet = TRUE;
    }
    DEBUGGERMSG(OXZONE_INIT, (L"--OsaxsT1Init: %d.\r\n", fRet));
    return fRet;
}

extern "C" BOOL WINAPI OsaxsT1DLLEntry (HINSTANCE hInstance, ULONG ulReason,
    LPVOID pvReserved)
{
    BOOL fResult;
    BOOL (*pfnKernelLibIoctl) (HANDLE, DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD);
    BOOL (*pfnInit) (HDSTUB_DATA *, void *);

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


HRESULT OsaxsT1Ioctl (DWORD dwFunction, void *pvArg)
{
    HRESULT hr = E_FAIL;
    DEBUGGERMSG(OXZONE_INIT, (L"++OsaxsT1Ioctl\r\n"));

    switch (dwFunction)
    {
        DEBUGGERMSG(OXZONE_INIT, (L"  OsaxsT1Ioctl:  dwFunction = %d\r\n", dwFunction));
        case OSAXST1_IOCTL_SAVE_EXCEPTION_STATE:
        {
            POSAXSFN_SAVEEXCEPTIONSTATE a = reinterpret_cast<POSAXSFN_SAVEEXCEPTIONSTATE>(pvArg);
            hr = SaveExceptionState(a->pNewExceptionInfo,
                                    a->pNewSavedThreadState,
                                    a->ppSavedExceptionInfo,
                                    a->ppSavedThreadState);
            break;
        }

#if defined(x86)
        case OSAXST1_IOCTL_GET_EXCEPTION_REGISTRATION:
            hr = GetExceptionRegistration((DWORD*)pvArg);
            break;
#endif
        case OSAXST1_IOCTL_TRANSLATE_ADDRESS:
        {
            POSAXSFN_TRANSLATEADDRESS a = reinterpret_cast<POSAXSFN_TRANSLATEADDRESS>(pvArg);
            hr = OSAXSTranslateAddress(a->hProcess, a->pvVirtual, a->fReturnKVA, &a->pvTranslated);
            break;
        }

        case OSAXST1_IOCTL_TRANSLATE_HPCI: 
        {
            POSAXSFN_TRANSLATEHPCI a = reinterpret_cast<POSAXSFN_TRANSLATEHPCI>(pvArg);
            DEBUGGERMSG(OXZONE_HANDLE, (L"  OsaxsT1Ioctl: GET_PCINFO h=%08X, proc=%08X\r\n", a->handle, a->dwProcessHandle));
            a->pcinfo = OSAXSHandleToPCInfo(a->handle, a->dwProcessHandle);
            if (a->pcinfo)
            {
                hr = S_OK;
            }
            else
            {
                DEBUGGERMSG(OXZONE_HANDLE, (L"  OsaxsT1Ioctl: GET_PCINFO failed to translate handle to cinfo, h=%08X, proc=%08X\r\n", a->handle,
                    a->dwProcessHandle));
                hr = E_FAIL;
            }
            DEBUGGERMSG(OXZONE_HANDLE, (L"  OsaxsT1Ioctl: GET_PCINFO returned %08x, hr=%08x\r\n", a->pcinfo, hr));
            break;
        }

        case OSAXST1_IOCTL_GET_HDATA:
        {
            POSAXSFN_GETHDATA a = reinterpret_cast<POSAXSFN_GETHDATA>(pvArg);
            DEBUGGERMSG(OXZONE_HANDLE, (L"  OsaxsT1Ioctl: GET_HDATA h=%08X, proc=%08X\r\n", a->handle, a->dwProcessHandle));
            a->phdata = OSAXSHandleToPHData(a->handle, a->dwProcessHandle);
            if (a->phdata)
            {
                hr = S_OK;
            }
            else
            {
                hr = E_FAIL;
            }
            DEBUGGERMSG(OXZONE_HANDLE, (L"  OsaxsT1Ioctl: GET_HDATA returned %08x, hr=%08x\r\n", a->phdata, hr));
            break;
        }

        case OSAXST1_IOCTL_PROCESS_FROM_HANDLE:
        {
            POSAXSFN_PROCESSFROMHANDLE a = reinterpret_cast<POSAXSFN_PROCESSFROMHANDLE>(pvArg);
            a->pProc = OSAXSHandleToProcess(a->hProc);
            hr = S_OK;
            break;
        }

        case OSAXST1_IOCTL_CALLSTACK:
        {
            OSAXSFN_CALLSTACK *a = reinterpret_cast<OSAXSFN_CALLSTACK *>(pvArg);
            hr = OSAXSGetCallstack(a->hThread, a->FrameStart, a->FramesToRead, &a->FramesReturned, a->FrameBuffer, &a->FrameBufferLength);
            break;
        }

        default:
            hr = OSAXS_E_APINUMBER;
            break;
    }
    
    DEBUGGERMSG(OXZONE_INIT, (L"--OsaxsT1Ioctl.  hr = 0x%x\r\n", hr));
    return hr;
}


void kdbgWtoA (LPCWSTR pWstr, LPCHAR pAstr) 
{
    while (*pAstr++ = (CHAR)*pWstr++);
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

    if (GetModuleInfo (g_pKData->pCurPrc, dwStructAddr, &kmodi, TRUE, fUnloadSymbols))
    {
         RETAILMSG (1, (L"OSAXST1: %soading Module '%S' (0x%08X) at address 0x%08X-0x%08X in Process '%s' (0x%08X)\r\n", 
            fUnloadSymbols ? L"<<< Unl" : L">>> L",
            kmodi.szName, dwStructAddr, kmodi.ImageBase, kmodi.ImageBase + kmodi.ImageSize, 
            g_pKData->pCurPrc->lpszProcName, g_pKData->pCurPrc));
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



