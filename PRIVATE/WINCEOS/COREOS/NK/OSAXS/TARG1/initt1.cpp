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

    initt1.cpp

Module Description:

    Initialize OsAxsT1.
    Handle OsAxsT1 Ioctl Commands

--*/

// WARNING: WINCEMACRO allows the use of PSL calls. This source file
// should only contain code that is used outside the scope of
// exception handling. Such as initialization code.
#define WINCEMACRO      

#include "osaxs_p.h"

HDSTUB_DATA Hdstub = {0};
OSAXS_DATA g_OsaxsData = {0};
void (*g_pfnOutputDebugString) (const char *sz, ...) = 0;

HRESULT OsaxsT1Ioctl (DWORD, DWORD, DWORD, DWORD, DWORD);

HDSTUB_CLIENT g_OsaxsT1Client =
{
    OSAXST1_NAME,
    NULL,
    NULL,
    NULL,
    NULL,

    OsaxsT1Ioctl,       
    0,
    
    NULL
};

BOOL OsaxsT1Init (HDSTUB_DATA *pHdstub, void *pvOsaxsData)
{
    BOOL fRet;
    OSAXS_DATA *pOsaxsData;

    fRet = FALSE;
    pOsaxsData = (OSAXS_DATA *)pvOsaxsData;

    if (pHdstub && pOsaxsData && pOsaxsData->cbSize == sizeof (OSAXS_DATA))
    {
        Hdstub = *pHdstub;
        g_OsaxsData = *pOsaxsData;
        if (g_OsaxsData.pKITLIoCtl)
        {
            g_OsaxsData.pKITLIoCtl (IOCTL_EDBG_GET_OUTPUT_DEBUG_FN, NULL, 0,
                (VOID*)&g_pfnOutputDebugString, sizeof (g_pfnOutputDebugString),
                NULL);
            /* Debug ok from here on */
            DEBUGGERMSG(OXZONE_INIT, (L"  OsaxsT1Init: Hello.\r\n"));
            DEBUGGERMSG(OXZONE_INIT, (L"  OsaxsT1Init: g_OsaxsData.pProcArray = 0x%.08x\r\n", g_OsaxsData.pProcArray));
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
    DEBUGGERMSG(OXZONE_INIT, (L"--OsaxsT0Init: %d.\r\n", fRet));
    return fRet;
}

BOOL WINAPI OsaxsT1DLLEntry (HINSTANCE hInstance, ULONG ulReason,
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


HRESULT OsaxsT1Ioctl (DWORD dwFunction, DWORD dwArg1, DWORD dwArg2, DWORD dwArg3, DWORD dwArg4)
{
    HRESULT hr = E_FAIL;
    DEBUGGERMSG(OXZONE_INIT, (L"++OsaxsT1Ioctl\r\n"));

    switch (dwFunction)
    {
        DEBUGGERMSG(OXZONE_INIT, (L"  OsaxsT1Ioctl:  dwFunction = %d\r\n", dwFunction));
        case OSAXST1_IOCTL_GET_MODULE_O32_DATA:
            hr = GetModuleO32Data (dwArg1, 
                    reinterpret_cast <DWORD *> (dwArg2),
                    reinterpret_cast <void *> (dwArg3),
                    reinterpret_cast <DWORD *> (dwArg4));
            break;
            
        case OSAXST1_IOCTL_MANIPULATE_VM:
            hr = ManipulateVm (dwArg1, dwArg2, dwArg3, (DWORD *)dwArg4);
            break;
            
        case OSAXST1_IOCTL_GET_OSSTRUCT:
            hr = GetStruct ((OSAXS_GETOSSTRUCT *) dwArg1, (void *) dwArg2, (DWORD *) dwArg3);
            break;

        case OSAXST1_IOCTL_GET_THREADCTX:
            hr = PackThreadContext ((PTHREAD) dwArg1, (void *) dwArg2, (DWORD *) dwArg3);
            break;

        case OSAXST1_IOCTL_SET_THREADCTX:
            hr = UnpackThreadContext ((PTHREAD) dwArg1, (void *)dwArg2, (DWORD) dwArg3);
            break;

        case OSAXST1_IOCTL_SAVE_EXCEPTION_CONTEXT:
            hr = SaveExceptionContext((CONTEXT *) dwArg1, 
                                      (SAVED_THREAD_STATE *) dwArg2,
                                      (CONTEXT **) dwArg3,
                                      (SAVED_THREAD_STATE **) dwArg4);
            break;
#if defined(x86)
        case OSAXST1_IOCTL_GET_EXCEPTION_REGISTRATION:
            hr = GetExceptionRegistration((DWORD*)dwArg1);
            break;
#endif
            
        default:
            hr = OSAXS_E_APINUMBER;
            break;
    }
    
    DEBUGGERMSG(OXZONE_INIT, (L"++OsaxsT1Ioctl.  hr = 0x%x\r\n", hr));
    return hr;
}
