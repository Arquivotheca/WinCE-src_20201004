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

    initt0.cpp

Module Description:

    Initialize OsAxsT0.

--*/

// WARNING: WINCEMACRO allows the use of PSL calls. This source file
// should only contain code that is used outside the scope of
// exception handling. Such as initialization code.
#define WINCEMACRO      

#include "osaxs_p.h"
#define  WATSON_BINARY_DUMP TRUE
#include "DwPublic.h"
#include "DwDmpGen.h"

HDSTUB_DATA Hdstub = {0};
OSAXS_DATA g_OsaxsData = {0};
void (*g_pfnOutputDebugString) (const char *sz, ...) = 0;

extern HANDLE g_hEventDumpFileReady;

HRESULT OsaxsT0Ioctl (DWORD dwFunction, DWORD dwArg1, DWORD dwArg2, DWORD dwArg3, DWORD dwArg4);

HDSTUB_CLIENT g_OsaxsT0Client =
{
    OSAXST0_NAME,
    CaptureDumpFile,       
    NULL,
    NULL,
    NULL,

    OsaxsT0Ioctl,       
    HDSTUB_FILTER_EXCEPTION,          
    
    NULL
};

#define STRING_UNDEFINED L"Undefined"

VOID DwDmpGenInit()
{
    DWORD dwSPI;

    // NOTE: If anything fails here we continue anyway, we just have reduced info in the dump file.

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++Initt0!DwDmpGenInit: Enter\r\n"));
    
    // Create event to send notification to Watson Transfer Driver (DwXfer.dll) that a dump file is ready
    g_hEventDumpFileReady = CreateEventW(NULL, FALSE, FALSE, WATSON_EVENT_DUMP_FILE_READY);
    if (NULL == g_hEventDumpFileReady)
    {
        // This means the dumps will not be transferred immediately, but will be transferred eventually.
        DEBUGGERMSG (OXZONE_ALERT, (L"  Initt0!DwDmpGenInit: CreateEventW failed for dump file ready event, error=0x%08X\r\n", GetLastError()));
    }

    // We need to do this at init time, since these APIs may block on Critical Sections on some BSPs
    // Get OEMInfo String
    dwSPI = SPI_GETOEMINFO;
    g_dwOEMStringSize = 0;
    if ((!pfnNKKernelLibIoControl((HANDLE) KMOD_OAL, IOCTL_HAL_GET_DEVICE_INFO, &dwSPI, sizeof(dwSPI), g_wzOEMString, sizeof(g_wzOEMString), &g_dwOEMStringSize)))
    {
        g_dwOEMStringSize = (kdbgwcslen(STRING_UNDEFINED) + 1) * sizeof(WCHAR);
        memcpy(g_wzOEMString, STRING_UNDEFINED, g_dwOEMStringSize);
    }
    else
    {
        if (g_dwOEMStringSize <= 1)
        {
            // Only set this if IOCTL_HAL_GET_DEVICE_INFO did not set it
            // since the string may have embedded NULL characters
            g_dwOEMStringSize = (kdbgwcslen(g_wzOEMString) + 1) * sizeof(WCHAR);
        }
    }

    // Get PlatformType String
    dwSPI = SPI_GETPLATFORMTYPE;
    g_dwPlatformTypeSize = 0;
    if ((!pfnNKKernelLibIoControl((HANDLE) KMOD_OAL, IOCTL_HAL_GET_DEVICE_INFO, &dwSPI, sizeof(dwSPI), g_wzPlatformType, sizeof(g_wzPlatformType), &g_dwPlatformTypeSize)))
    {
        g_dwPlatformTypeSize = (kdbgwcslen(STRING_UNDEFINED) + 1) * sizeof(WCHAR);
        memcpy(g_wzPlatformType, STRING_UNDEFINED, g_dwPlatformTypeSize);
    }
    else
    {
        if (g_dwPlatformTypeSize <= 1)
        {
            // Only set this if IOCTL_HAL_GET_DEVICE_INFO did not set it
            // since the string may have embedded NULL characters
            g_dwPlatformTypeSize = (kdbgwcslen(g_wzPlatformType) + 1) * sizeof(WCHAR);
        }
    }

    // Get PlatformVersion
    dwSPI = SPI_GETPLATFORMVERSION;
    g_dwPlatformVersionSize = 0;
    if ((!pfnNKKernelLibIoControl((HANDLE) KMOD_OAL, IOCTL_HAL_GET_DEVICE_INFO, &dwSPI, sizeof(dwSPI), g_platformVersion, sizeof(g_platformVersion), &g_dwPlatformVersionSize)))
    {
        memset(g_platformVersion, 0, sizeof(g_platformVersion));
        g_dwPlatformVersionSize = 0;
    }
    else
    {
        // Make sure the size is aligned to size of PLATFORMVERSION
        g_dwPlatformVersionSize = (g_dwPlatformVersionSize / sizeof(PLATFORMVERSION)) * sizeof(PLATFORMVERSION);
    }

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--Initt0!DwDmpGenInit: Leave\r\n"));
}

// TODO: Add a call to DwDmpGenDeInit when OsaxsT0 unloaded ...

VOID DwDmpGenDeInit()
{
    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++Initt0!DwDmpGenDeInit: Enter\r\n"));

    if (g_hEventDumpFileReady)
    {
        CloseHandle(g_hEventDumpFileReady);
        g_hEventDumpFileReady = NULL;
    }

    g_dwOEMStringSize = 0;
    g_dwPlatformTypeSize = 0;
    g_dwPlatformVersionSize = 0;

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--Initt0!DwDmpGenDeInit: Leave\r\n"));
}

BOOL OsaxsT0Init (HDSTUB_DATA *pHdstub, void *pvOsaxsData)
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
            DEBUGGERMSG(OXZONE_INIT, (L"  OsaxsT0Init: g_OsaxsData.pProcArray = 0x%.08x\r\n", g_OsaxsData.pProcArray));
        }

        if (Hdstub.pfnRegisterClient (&g_OsaxsT0Client, HdstubClientLast))
        {
            DEBUGGERMSG(OXZONE_INIT, (L"  OsaxsT0Init: Registered OsaxsT0 with hdstub.\r\n"));
        }
        else
        {
            DEBUGGERMSG(OXZONE_INIT, (L"  OsaxsT0Init: Failed to register OsaxsT0 with hdstub.\r\n"));
        }
        
        DwDmpGenInit();

        fRet = TRUE;
    }

    return fRet;
}

BOOL WINAPI OsaxsT0DLLEntry (HINSTANCE hInstance, ULONG ulReason,
    LPVOID pvReserved)
{
    BOOL fResult;
    BOOL (*pfnKernelLibIoctl) (HANDLE, DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD);
    BOOL (*pfnInit) (HDSTUB_DATA *, void *);

    pfnKernelLibIoctl = reinterpret_cast<BOOL (*)(HANDLE,DWORD,LPVOID,DWORD,LPVOID,DWORD,LPDWORD)> (pvReserved);
    pfnInit = OsaxsT0Init;
    fResult = TRUE;

    DEBUGREGISTER (hInstance);
    switch (ulReason)
    {
        case DLL_PROCESS_ATTACH:
            fResult = (pfnKernelLibIoctl &&
                       pfnKernelLibIoctl ((HANDLE)KMOD_DBG,
                                          IOCTL_DBG_OSAXST0_INIT,
                                          (void *)&pfnInit,
                                          sizeof (pfnInit),
                                          NULL,
                                          0,
                                          NULL));
            break;
        case DLL_PROCESS_DETACH:
            break;
    }

    return fResult;
}


HRESULT OsaxsT0Ioctl (DWORD dwFunction, DWORD dwArg1, DWORD dwArg2, DWORD dwArg3, DWORD dwArg4)
{
    HRESULT hr;
    
    DEBUGGERMSG(OXZONE_IOCTL, (L"++OsaxsT0Ioctl: Calling GetFPTMI, dwFunction=%u, dwArg1=0x%08X, dwArg2=0x%08X, dwArg3=0x%08X, dwArg4=0x%08X\r\n",
                            dwFunction,
                            dwArg1,
                            dwArg2,
                            dwArg3,
                            dwArg4));

    switch (dwFunction)
    {
        case OSAXST0_IOCTL_GET_FLEXIPTMINFO:
            hr = GetFPTMI ((FLEXI_REQUEST *) dwArg1, (DWORD *) dwArg2, (BYTE *) dwArg3);
            if (FAILED(hr))
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  OsaxsT0Ioctl: GetFPTMI failed, hr=0x%08x\r\n",hr));
            }
            break;

        case OSAXST0_IOCTL_SAVE_EXCEPTION_CONTEXT:
            // Check if we are doing the initial save (Both dwArg1 & dwArg3 will be set)
            if (dwArg1 && dwArg3)
            {
                // Flush FPU or DSP registers so that context information is correct
                KdpFlushExtendedContext ((CONTEXT *) dwArg1);
            }

            hr = SaveExceptionContext((CONTEXT *) dwArg1, 
                                      (SAVED_THREAD_STATE *) dwArg2,
                                      (CONTEXT **) dwArg3,
                                      (SAVED_THREAD_STATE **) dwArg4);
            if (FAILED(hr))
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  OsaxsT0Ioctl: SaveExceptionContext failed, hr=0x%08x\r\n",hr));
            }
            break;

        default:
            hr = OSAXS_E_APINUMBER;
            break;
    }

    DEBUGGERMSG(OXZONE_IOCTL, (L"--OsaxsT0Ioctl: hr=0x%08X\r\n", hr));

    return hr;
}
