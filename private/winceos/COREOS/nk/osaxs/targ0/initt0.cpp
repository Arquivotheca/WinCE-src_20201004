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

initt0.cpp

Module Description:

Initialize OsAxsT0.

--*/

#include "osaxs_p.h"
#define  WATSON_BINARY_DUMP TRUE
#include "DwPublic.h"
#include "DwDmpGen.h"
#include "Diagnose.h"


DEBUGGER_DATA *g_pDebuggerData;

extern HANDLE g_hEventDumpFileReady;  // DwDmpGen.cpp
extern LPBYTE g_pDDxHeapBegin;        // diagnose.cpp
extern LPBYTE g_pDDxHeapFree;         // diagnose.cpp

HANDLE   g_hProbeDll = NULL;

HRESULT OsaxsT0Ioctl (DWORD dwFunction, void *);

#define STRING_UNDEFINED L"Undefined"

VOID DwDmpGenInit()
{
    DWORD dwSPI;
    BOOL fOk;
    const DWORD c_dwInvalidMajorVersion = 0;

    // NOTE: If anything fails here we continue anyway, we just have reduced info in the dump file.

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"++Initt0!DwDmpGenInit: Enter\r\n"));

    // THIS HANDLE IS CREATED IN NK.EXE.
    // Create event to send notification to Watson Transfer Driver (DwXfer.dll) that a dump file is ready
    KD_ASSERT (pActvProc == g_pprcNK);
    g_hEventDumpFileReady = CreateEventW(NULL, FALSE, FALSE, WATSON_EVENT_DUMP_FILE_READY);
    if (NULL == g_hEventDumpFileReady)
    {
        // This means the dumps will not be transferred immediately, but will be transferred eventually.
        DEBUGGERMSG (OXZONE_ALERT, (L"  Initt0!DwDmpGenInit: CreateEventW failed for dump file ready event, error=0x%08X\r\n", GetLastError()));
    }

    // We need to do this at init time, since these APIs may block on Critical Sections on some BSPs
    // Get OEM string
    dwSPI = SPI_GETPLATFORMMANUFACTURER;
    g_dwOEMStringSize = 0;
    if ((!NKKernelLibIoControl((HANDLE) KMOD_OAL, IOCTL_HAL_GET_DEVICE_INFO, &dwSPI, sizeof(dwSPI), g_wzOEMString, sizeof(g_wzOEMString), &g_dwOEMStringSize)))
    {
        g_dwOEMStringSize = (wcslen(STRING_UNDEFINED) + 1) * sizeof(WCHAR);
        memcpy(g_wzOEMString, STRING_UNDEFINED, g_dwOEMStringSize);
    }
    else
    {
        if (g_dwOEMStringSize <= 1)
        {
            // Only set this if IOCTL_HAL_GET_DEVICE_INFO did not set it
            // since the string may have embedded NULL characters
            g_dwOEMStringSize = (wcslen(g_wzOEMString) + 1) * sizeof(WCHAR);
        }
    }

    // Get PlatformType String
    dwSPI = SPI_GETPLATFORMNAME;
    g_dwPlatformTypeSize = 0;
    if ((!NKKernelLibIoControl((HANDLE) KMOD_OAL, IOCTL_HAL_GET_DEVICE_INFO, &dwSPI, sizeof(dwSPI), g_wzPlatformType, sizeof(g_wzPlatformType), &g_dwPlatformTypeSize)))
    {
        g_dwPlatformTypeSize = (wcslen(STRING_UNDEFINED) + 1) * sizeof(WCHAR);
        memcpy(g_wzPlatformType, STRING_UNDEFINED, g_dwPlatformTypeSize);
    }
    else
    {
        if (g_dwPlatformTypeSize <= 1)
        {
            // Only set this if IOCTL_HAL_GET_DEVICE_INFO did not set it
            // since the string may have embedded NULL characters
            g_dwPlatformTypeSize = (wcslen(g_wzPlatformType) + 1) * sizeof(WCHAR);
        }
    }
    RETAILMSG (1, (L"OSAXST0: Platform Name = %s\r\n", g_wzPlatformType));

    // Get PlatformVersion
    dwSPI = SPI_GETPLATFORMVERSION;

    // Preset to 0.  This does two things.  First, we rely on the array being
    // initialized to 0 below.  Second, since it's preset to the faliure condition
    // we don't need to do anything if SPI fails.
    memset(g_platformVersion, 0, sizeof(g_platformVersion));
    g_dwPlatformVersionSize = 0;

    fOk = SystemParametersInfo(dwSPI, sizeof(g_platformVersion), &g_platformVersion, 0); 
    if (fOk)
    {
        // g_platformVersion is an array of PLATFORMVERSIONs.  Presumably, SPI could
        // fill out multiple versions, but it doesn't tell us how much it wrote.
        // We'll assume that only entries with dwMajor version > 0 are valid and 
        // that only valid entries were written.  So, when we find the first with
        // version == 0, we know we can stop.
        int numEntries = sizeof(g_platformVersion) / sizeof(PLATFORMVERSION);
        int index;

        for (index = 0; index < numEntries; index++)
        {
            if (g_platformVersion[index].dwMajor == c_dwInvalidMajorVersion)
            {
                break;
            }
        }

        // Index now points to the entry AFTER the last correct one
        // but that's what we happen to want, since index is 0 based
        // and we need a 1 based value to multiply.
        g_dwPlatformVersionSize = index * sizeof(PLATFORMVERSION);
    }

    DEBUGGERMSG(OXZONE_DWDMPGEN,(L"--Initt0!DwDmpGenInit: Leave\r\n"));
}


BOOL DDxInit()
{
    BOOL   fRet = TRUE;
    LPBYTE pVa = NULL;
    LPBYTE pPa = NULL;

    // Alloc Log buffer

    fRet = NKKernelLibIoControl((HANDLE) KMOD_CORE, IOCTL_KLIB_ALLOCSHAREMEM, (LPVOID) &pVa, NUM_LOG_PAGES, (LPVOID) &pPa, TRUE, NULL);

    if (!fRet)
    {
        DBGRETAILMSG(OXZONE_ALERT, (L"  DDxInt:  FAILED AllocShareMem. pVa = 0x%08x,  pPa = 0x%08x", pVa, pPa));
        return FALSE;
    }

    InitDDxLog((WCHAR*) pPa);

    DEBUGGERMSG(OXZONE_DIAGNOSE, (L"  DDxInit:  Log Buffer = 0x%08x\r\n", pPa));


    // Alloc DDx Heap buffer

    fRet = NKKernelLibIoControl((HANDLE) KMOD_CORE, IOCTL_KLIB_ALLOCSHAREMEM, (LPVOID) &pVa, NUM_HEAP_PAGES, (LPVOID) &pPa, TRUE, NULL);

    if (!fRet)
    {
        DBGRETAILMSG(OXZONE_ALERT, (L"  DDxInt:  FAILED AllocShareMem. pVa = 0x%08x,  pPa = 0x%08x", pVa, pPa));
        return FALSE;
    }

    g_pDDxHeapBegin = (LPBYTE) pPa;
    g_pDDxHeapFree  = g_pDDxHeapBegin;

    DEBUGGERMSG(OXZONE_DIAGNOSE, (L"  DDxInit:  g_pDDxHeapBegin = 0x%08x\r\n", g_pDDxHeapBegin));


    // Load the probe

    g_hProbeDll = NKLoadKernelLibrary(DDX_PROBE_DLL);

    if (g_hProbeDll)
    {
        if (!NKKernelLibIoControl(g_hProbeDll, IOCTL_DDX_PROBE_INIT, 
            (LPVOID) &g_pDebuggerData,
            sizeof(g_pDebuggerData), NULL, 0, NULL))
        {
            DEBUGGERMSG(OXZONE_DIAGNOSE, (L"  DDxInit:  FAILED to initialize probe!\r\n"));
            return FALSE;
        }
    }
    else
    {
        DEBUGGERMSG(OXZONE_DIAGNOSE, (L"  DDxInit:  FAILED to load library!\r\n"));
        return FALSE;
    }

    // Initialize the DDx internal state

    ResetDDx();

    return fRet;
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


extern "C" BOOL OsaxsT0Init (DEBUGGER_DATA *pDebuggerData, void *pvOsaxsData)
{
    BOOL fRet;

#ifdef ARM
    InitPCBFunctions ();
#endif

    fRet = FALSE;

    if (pDebuggerData && 
        VALID_DEBUGGER_DATA(pDebuggerData))
    {
        g_pDebuggerData = pDebuggerData;

        DD_CaptureExtraContext = OsAxsCaptureExtraContext;
        DD_GetCpuContext = OsAxsGetCpuContext;
        DD_SetCpuContext = OsAxsSetCpuContext;
        DD_WasCaptureDumpFileOnDeviceCalled = WasCaptureDumpFileOnDeviceCalled;
        DD_HandleToProcess = OsAxsHandleToProcess;
        DD_HandleToThread = OsAxsHandleToThread;
        DD_HandleOsAxsApi = OsAxsHandleOsAxsApi;
        DD_PushExceptionState = OsAxsPushExceptionState;
        DD_PopExceptionState = OsAxsPopExceptionState;
        DD_TranslateOffset = OsAxsTranslateOffset;

        DD_eventHandlers[KDBG_EH_WATSON].pfnException = CaptureDumpFile;
        DD_eventHandlers[KDBG_EH_WATSON].dwFilter = HDSTUB_FILTER_EXCEPTION;

        DwDmpGenInit();

        fRet = TRUE;
    }

    return fRet;
}

extern "C" BOOL WINAPI OsaxsT0DLLEntry (HINSTANCE hInstance, ULONG ulReason,
                                        LPVOID pvReserved)
{
    BOOL fResult;

    BOOL (*pfnInit) (DEBUGGER_DATA *, void *);

    //g_pfnKernelLibIoctl = reinterpret_cast<BOOL (*)(HANDLE,DWORD,LPVOID,DWORD,LPVOID,DWORD,LPDWORD)> (pvReserved);
    PFN_KENRELLIBIOCTL pfnKernelLibIoctl = (PFN_KENRELLIBIOCTL) pvReserved;
    pfnInit = OsaxsT0Init;
    fResult = TRUE;

    RETAILREGISTERZONES (hInstance);
    switch (ulReason)
    {
    case DLL_PROCESS_ATTACH:

        // Register Dbg Zones

        // NOTE: This will enable debug zones on RETAIL / SHIP builds.  
        //       For dev purposes only.  Switch to DEBUGREGISTER before deployment.

        RegisterDbgZones(hInstance, &dpCurSettings);
        //DEBUGREGISTER(hInstance);

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



//------------------------------------------------------------------------------
// Invoked when an application calls KernelLibIoControl with the handle of this
// DLL.  The kernel has already validated that the caller is trusted.
//------------------------------------------------------------------------------
BOOL
IOControl( 
          DWORD   dwInstData,
          DWORD   dwIoControlCode,
          LPVOID  lpInBuf,
          DWORD   nInBufSize,
          LPVOID  lpOutBuf,
          DWORD   nOutBufSize,
          LPDWORD lpBytesReturned
          )
{
    switch (dwIoControlCode)
    {
        case IOCTL_DDX_LOAD_PROBE:
            {  
                RETAILMSG(1, (L"OsAxsT0!Got IOCTL_DDX_LOAD_PROBE\r\n"));
                return DDxInit(); 
            }

        default:
            break;
    }

    return FALSE;
}


#if 0

// TODO: Turn this into OsAxsCommandHandler
HRESULT OsaxsT0Ioctl (DWORD dwFunction, void *pvArg)
{
    HRESULT hr = E_FAIL;

    DEBUGGERMSG(OXZONE_IOCTL, (L"++OsaxsT0Ioctl: Calling GetFPTMI, dwFunction=%u, pvArg=0x%08X\r\n", dwFunction, pvArg));

    switch (dwFunction)
    {
    case OSAXS_IOCTL_GET_FLEXIPTMINFO:
        {
            POSAXSFN_GETFLEXIPTMINFO a = reinterpret_cast<POSAXSFN_GETFLEXIPTMINFO>(pvArg);
            hr = GetFPTMI (a->pRequest, &a->dwBufferSize, a->pbBuffer);
            if (FAILED(hr))
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  OsaxsT0Ioctl: GetFPTMI failed, hr=0x%08x\r\n",hr));
            }
            break;
        }

    case OSAXS_IOCTL_GET_WATSON_DUMP:
        {
            POSAXSFN_GETWATSONDUMP a = reinterpret_cast<POSAXSFN_GETWATSONDUMP>(pvArg);
            hr = CaptureDumpFileOnTheFly (a->pWatsonOtfRequest,
                a->pbOutBuffer,
                &a->dwBufferSize,
                a->pfnResponse);
            if (FAILED(hr))
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  OsaxsT0Ioctl: CaptureDumpFileOnTheFly failed, hr=0x%08x\r\n",hr));
            }
            break;
        }

    case OSAXS_IOCTL_SAVE_EXCEPTION_STATE:
        {
            POSAXSFN_SAVEEXCEPTIONSTATE a = reinterpret_cast<POSAXSFN_SAVEEXCEPTIONSTATE>(pvArg);
            hr = SaveExceptionState(a->pNewExceptionInfo, a->ppSavedExceptionInfo);
            if (FAILED(hr))
            {
                DEBUGGERMSG(OXZONE_ALERT, (L"  OsaxsT0Ioctl: SaveExceptionState failed, hr=0x%08x\r\n",hr));
            }
            break;
        }

    case OSAXS_IOCTL_GET_THREAD_CONTEXT:
        {
            OSAXSFN_THD_CONTEXT *a = (OSAXSFN_THD_CONTEXT *) pvArg;
            PTHREAD pth;

            pth = OSAXSHandleToThread(a->hThread);
            if (pth)
            {
                hr = OsAxsGetThreadContext(pth, a->pctx, a->cbctx);
            }
            else
            {
                hr = E_HANDLE;
            }
            break;
        }

    case OSAXS_IOCTL_SET_THREAD_CONTEXT:
        {
            OSAXSFN_THD_CONTEXT *a = (OSAXSFN_THD_CONTEXT *) pvArg;
            PTHREAD pth;

            pth = OSAXSHandleToThread(a->hThread);
            if (pth)
            {
                hr = OsAxsSetThreadContext(pth, a->pctx, a->cbctx);
            }
            else
            {
                hr = E_HANDLE;
            }
            break;
        }
    case OSAXS_IOCTL_TRANSLATE_ADDRESS:
        {
            POSAXSFN_TRANSLATEADDRESS a = reinterpret_cast<POSAXSFN_TRANSLATEADDRESS>(pvArg);
            hr = OSAXSTranslateAddress(a->hProcess, a->pvVirtual, a->fReturnKVA, &a->pvTranslated);
            break;
        }
    case OSAXS_IOCTL_CPU_PCUR:
        {
            OSAXS_CPU_PCUR *a = (OSAXS_CPU_PCUR *) pvArg;
            PPROCESS pprcVM;
            PPROCESS pprcActv;
            PTHREAD  pth;

            hr = OsAxsGetCpuPCur (a->dwCpuNum,
                &pprcVM,
                &pprcActv,
                &pth);

            if (SUCCEEDED (hr))
            {
                a->ullCurrentVM = (ULONG32) pprcVM;
                a->ullCurrentProcess = (ULONG32) pprcActv;
                a->ullCurrentThread = (ULONG32) pth;

                if (pprcVM)
                    a->hCurrentVM = pprcVM->dwId;
                if (pprcActv)
                    a->hCurrentProcess = pprcActv->dwId;
                if (pth)
                    a->hCurrentThread = pth->dwId;
            }
            break;
        }
    default:
        hr = OSAXS_E_APINUMBER;
        break;
    }

    DEBUGGERMSG(OXZONE_IOCTL, (L"--OsaxsT0Ioctl: hr=0x%08X\r\n", hr));

    return hr;
}
#endif

