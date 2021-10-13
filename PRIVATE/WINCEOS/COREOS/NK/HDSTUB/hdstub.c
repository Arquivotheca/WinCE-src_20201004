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
    
    hdstub.c : HDStub layer

Abstract:

    HDStub is a small kernel dll that accepts and filters notifications from
    the kernel.  These notifications include (and are not limited to)
    exceptions, virtual memory paging, module load, and module unload.

Environment:

    CE kernel

History:

--*/

#include "hdstub_p.h"

// BEGIN OsAxsHCe50.dll parameters
// Global stopping event structure.
HDSTUB_EVENT g_Event;

// Hardware event filter.
static ULONG s_ulFakeHDEventFilter = 0;
static ULONG* s_pulHDEventFilter = &s_ulFakeHDEventFilter;

// Taint flag for modules.  Indicates the number of module events that OsAccess
// has missed.
static BOOL s_dwTaintedModuleCount = 0;
// END parameters


// Pointer to kdata
struct KDataStruct *g_pKData;


// Current Hdstub event handlers
HDSTUB_CLIENT *pClientListHead;

// KITL Ioctl
DWORD (*g_pfnKitlIoctl) (DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD) = 0;

// Sprintf
void (WINAPIV* g_pfnNKvsprintfW)(LPWSTR, LPCWSTR, va_list, int) = 0;
void (*g_pfnOutputDebugString)(const char *sz, ...) = 0;


static void PageInModule (DWORD dwVmBaseAddr);

#ifdef MIPS
/* Interlocked functions for mips */
LONG (*g_pfnInterlockedDecrement)(LPLONG);
LONG (*g_pfnInterlockedIncrement)(LPLONG);
#endif

CRITICAL_SECTION csModLoad;
CRITICAL_SECTION csHdStubEventRecord;

HDSTUB_INIT g_HdStubData = {0};

static ULONG s_ulOldHDEventFilter = 0;

// During boot, allow 2 load events to go through without altering the event flags.
// This is for performance reasons, because 2 load events would guarantee the hardware debugger starting up.
DWORD g_dwModInitCount = 2;


/*++

Routine Name:

    HdstubCallClientIoctl

Routine Description:

    Call any of hdstub's clients.  This is used for inter Kdstub - Osaccess communication.

Arguments:

    szClientName - name of the client ("kdstub", "osaxst0", "osaxst1", etc.)
    dwFunction   - Which function to call.
    dwArg1,2,3,4 - Arguments 1 - 4.  Size of a machine word and should be castable to pointers
                                     if necessary.

Return Value:

    E_FAIL - Unable to find client (Must Change.)

--*/
HRESULT HdstubCallClientIoctl (const char *szClientName, DWORD dwFunction, DWORD dwArg1, DWORD dwArg2, DWORD dwArg3, DWORD dwArg4)
{
    HDSTUB_CLIENT *pClient;
    HRESULT hr = HDSTUB_E_NOCLIENT;

    DEBUGGERMSG (HDZONE_CLIENT, (L"++HdstubCallClientIoctl, %S, %d, %d, %d, %d, %d\r\n", szClientName,
        dwFunction, dwArg1, dwArg2, dwArg3, dwArg4));

    pClient = pClientListHead;
    while (pClient)
    {
        if (!strcmp (pClient->szClientName, szClientName))
        {
            DEBUGGERMSG (HDZONE_CLIENT, (L"  HdstubCallClientIoctl: Found and calling.\r\n"));
            if (pClient->pfnIoctl)
            {
                hr = pClient->pfnIoctl (dwFunction, dwArg1, dwArg2, dwArg3, dwArg4);
                DEBUGGERMSG (HDZONE_CLIENT, (L"  HdstubCallClientIoctl: returned 0x%.08x\r\n", hr));
            }
            break;
        }
        pClient = pClient->pCliNext;
    }

    DEBUGGERMSG (HDZONE_CLIENT, (L"--HdstubCallClientIoctl: 0x%.08x\r\n", hr));    
    return hr;          
}


/*++

Routine Name:

    HdstubConnectKdstub

Routine Description:

    Hook KDStub to HDStub.  Assume that the Kernel debugger dll is already
    loaded.

Arguments:

    pfnCliInit    - in, Pointer to Client init function
    pvExtra       - in, Pointer to Extra Data to pass through to Client

Return Value:

    TRUE    : Load successful
    FALSE   : Failure

--*/
BOOL HdstubConnectClient (HDSTUB_CLINIT_FUNC pfnCliInit, void *pvExtra)
{
    BOOL fReturn;
    HDSTUB_DATA HdstubData =
    {
        HdstubRegisterClient,
        HdstubUnregisterClient,
        HdstubCallClientIoctl
    };
    
    DEBUGGERMSG (HDZONE_INIT, (L"++HdstubConnectKdstub: CliInit=0x%.8x, pvExtra=0x%.8x\r\n", pfnCliInit, pvExtra));
    fReturn = pfnCliInit (&HdstubData, pvExtra);
    DEBUGGERMSG (HDZONE_INIT, (L"--HdstubConnectKdstub: return %d\r\n", fReturn));
    return fReturn;
}


/*++

Routine Name:

    HdstubDLLEntry

Routine Description:

    Attach to nk.exe.  Provide nk.exe with a pointer to HdstubInit

Arguments:

    hInstance -
    ulReason -
    pvReserved -

Return Value:

    TRUE  - Loaded
    FALSE - Error during entry.

--*/
BOOL WINAPI HdstubDLLEntry (HINSTANCE hInstance, ULONG ulReason,
        LPVOID pvReserved)
{
    BOOL bResult;
    FARPROC pfnKernelLibIoctl;
    FARPROC pfnInit;
    
    pfnKernelLibIoctl = (FARPROC)pvReserved;
    pfnInit = (FARPROC)HdstubInit;
    bResult = TRUE;

    DEBUGREGISTER(hInstance);
    
    switch (ulReason)
    {
        case DLL_PROCESS_ATTACH:
            bResult = pfnKernelLibIoctl
                && pfnKernelLibIoctl ((HANDLE)KMOD_DBG,
                        IOCTL_DBG_HDSTUB_INIT,
                        (void *) &pfnInit,
                        sizeof (pfnInit),
                        NULL,
                        0,
                        NULL);
            break;

        case DLL_PROCESS_DETACH:
            break;
    }

    return bResult;
}


/*++

Routine Name:

    HdstubInit

Routine Description:

    Initialize Hdstub.

Arguments:

    pHdInit     - out, export event notification functions.

Return Value:

    TRUE : success.

--*/
BOOL HdstubInit(HDSTUB_INIT *pHdInit)
{
    BOOL fResult = FALSE;

    if (pHdInit && (pHdInit->cbSize == sizeof(*pHdInit)))
    {
        g_HdStubData = *pHdInit;

        g_HdStubData.pfnInitializeCriticalSection(&csModLoad);

#ifdef MIPS
        g_pfnInterlockedDecrement = pHdInit->pfnInterlockedDecrement;
        g_pfnInterlockedIncrement = pHdInit->pfnInterlockedIncrement;
#endif
        // Get existing imported functions and data
        g_pfnNKvsprintfW = pHdInit->pNKSprintfW;
        g_pfnKitlIoctl = pHdInit->pKITLIoCtl;
        g_pKData = pHdInit->pKData;
        s_pulHDEventFilter = pHdInit->pulHDEventFilter;

        // Obtain KITLOutputDebugString
        if (g_pfnKitlIoctl)
        {
            g_pfnKitlIoctl(IOCTL_EDBG_GET_OUTPUT_DEBUG_FN, NULL, 0, (VOID*)&g_pfnOutputDebugString,
                sizeof(g_pfnOutputDebugString), NULL);
        }

        // Export trap functions
        pHdInit->pfnException = HdstubTrapException;
        pHdInit->pfnVmPageIn = HdstubTrapVmPageIn;
        pHdInit->pfnModLoad = HdstubTrapModuleLoad;
        pHdInit->pfnModUnload = HdstubTrapModuleUnload;
        pHdInit->pfnConnectClient = HdstubConnectClient;

        // Export hw debug notification function and structure.
        pHdInit->pEvent = &g_Event;
        pHdInit->pdwTaintedModuleCount = &s_dwTaintedModuleCount;

        fResult = TRUE;
    }
    
    return fResult;
}


/*++

Routine Name:

    HdstubRegisterClient

Routine Description:

    Add a client structure into the linked list.  Does not copy.

Arguments:

    pClient     - Client Structure, must be either allocated or static data from client..
    Disposition - Position to insert the element, either at head or at tail.

Return Value:

    TRUE - successfully added client.
    FALSE - unable to add the client.

--*/
BOOL HdstubRegisterClient (HDSTUB_CLIENT *pClient, int Disposition)
{
    HDSTUB_CLIENT *pClientCur = NULL;
    HDSTUB_CLIENT *pClientLast = NULL;
    BOOL fResult = FALSE;

    DEBUGGERMSG (HDZONE_CLIENT, (L"++HdstubRegisterClient: 0x%.08x, %d\r\n", pClient, Disposition));

    pClientCur = pClientListHead;
    while (pClientCur)
    {
        if (pClientCur == pClient)
        {
            DEBUGGERMSG (HDZONE_CLIENT, (L"  HdstubRegisterClient: Already registered.\r\n"));
            fResult = TRUE;
            goto Done;
        }
        pClientLast = pClientCur;
        pClientCur = pClientCur->pCliNext;
    }

    pClient->pCliNext = NULL;
    switch (Disposition)
    {
        case HdstubClientFirst:
            DEBUGGERMSG (HDZONE_CLIENT, (L"  HdstubRegisterClient: Insert at head.\r\n"));
            pClient->pCliNext = pClientListHead;
            pClientListHead = pClient;
            fResult = TRUE;
            break;
            
        case HdstubClientLast:
            if (pClientLast)
            {
                DEBUGGERMSG (HDZONE_CLIENT, (L"  HdstubRegisterClient: Insert at tail after 0x%.08x\r\n", pClientLast));
                pClientLast->pCliNext = pClient;
            }
            else
            {
                DEBUGGERMSG (HDZONE_CLIENT, (L"  HdstubRegisterClient: Tail, Insert at head because no other clients.\r\n"));
                pClientListHead = pClient;
            }
            fResult = TRUE;
            break;

        default:
            break;
    }
Done:
    DEBUGGERMSG (HDZONE_CLIENT, (L"--HdstubRegisterClient: %d\r\n", fResult));
    return fResult;
}


//
// Trap functions for hdstub.  These functions are called by kernel
// to notify of a specific event.
//
BOOL HdstubTrapException(PEXCEPTION_RECORD pex, CONTEXT *pContext,
        BOOLEAN b2ndChance)
{
    BOOL fHandled = FALSE;
    HDSTUB_CLIENT* pClientCur;
    
    DEBUGGERMSG(HDZONE_ENTRY, 
        (TEXT("++HdstubTrapException: pex = 0x%.8x, pContext = 0x%.8x, b2ndChance = %d, ExcAddr=0x%08X\r\n"),
        pex, pContext, b2ndChance, pex ? pex->ExceptionAddress : NULL));
   
    pClientCur = pClientListHead;
    while (pClientCur && !fHandled)
    {
        if ((pClientCur->dwFilter & HDSTUB_FILTER_EXCEPTION) && pClientCur->pfnException)
        {
            fHandled = pClientCur->pfnException(pex, pContext, b2ndChance);
        }
        pClientCur = pClientCur->pCliNext;
    }

    // Notify hardware support
    if (!fHandled && (*s_pulHDEventFilter & HDSTUB_FILTER_EXCEPTION))
    {
        fHandled = HwExceptionHandler (pex, pContext, b2ndChance);
        DEBUGGERMSG (HDZONE_HW, (L"  HdstubTrapException: hardware fHandled=%d\r\n", fHandled));
    }

    DEBUGGERMSG (HDZONE_ENTRY, (TEXT ("--HdstubTrapException: pex = 0x%.8x, fHandled = %d\r\n"), pex, fHandled));
    return fHandled;
}


static void HdstubTrapVmPageInRange (DWORD dwPageAddr, DWORD dwNumPages, BOOL bWriteable)
{
    BOOL fHandled = FALSE;
    HDSTUB_CLIENT *pClientCur;
    
    DEBUGGERMSG(HDZONE_ENTRY, (TEXT ("++HdstubTrapVmPageInRange: dwPageAddr=0x%.8x, dwNumPages=%d bWriteable=%d\r\n"),
        dwPageAddr, dwNumPages, bWriteable));

    pClientCur = pClientListHead;
    while (pClientCur && !fHandled)
    {
        if ((pClientCur->dwFilter & HDSTUB_FILTER_VMPAGEIN) && pClientCur->pfnVmPageIn)
            fHandled = pClientCur->pfnVmPageIn (dwPageAddr, dwNumPages, bWriteable);
        pClientCur = pClientCur->pCliNext;        
    }

    if (!fHandled && (*s_pulHDEventFilter & HDSTUB_FILTER_VMPAGEIN))
        HwPageInHandler (dwPageAddr, dwNumPages, bWriteable);

    DEBUGGERMSG(HDZONE_ENTRY, (TEXT ("--HdstubTrapVmPageInRange\r\n")));
}


void HdstubTrapVmPageIn (DWORD dwPageAddr, BOOL bWriteable)
{
    HdstubTrapVmPageInRange (dwPageAddr, 1, bWriteable);
}

void HdstubTrapModuleLoad(DWORD dwStructAddr)
{
    BOOL fHandled = FALSE;
    HDSTUB_CLIENT *pClientCur;
    
    DEBUGGERMSG(HDZONE_ENTRY, (TEXT("++HdstubTrapModuleLoad, dwStructAddr=0x%08X\r\n"),dwStructAddr));

    if (!InSysCall())
    {
        DEBUGGERMSG(HDZONE_ENTRY, (TEXT("  HdstubTrapModuleLoad: ++ EnterCriticalSection, hCurThread=0x%08X, OwnerThread=0x%08X\r\n"), 
                                  hCurThread, 
                                  csModLoad.OwnerThread));
        g_HdStubData.pfnEnterCriticalSection(&csModLoad);
        DEBUGGERMSG(HDZONE_ENTRY, (TEXT("  HdstubTrapModuleLoad: -- EnterCriticalSection, hCurThread=0x%08X, OwnerThread=0x%08X\r\n"), 
                                  hCurThread, 
                                  csModLoad.OwnerThread));
    }

    PageInModule (dwStructAddr);

    InterlockedIncrement(&(LONG)s_dwTaintedModuleCount);

    pClientCur = pClientListHead;
    while (pClientCur && !fHandled)
    {
        DEBUGGERMSG (HDZONE_CLIENT, (L"  HdstubTrapModuleLoad: Trying client 0x%.08x\r\n", pClientCur));
        if ((pClientCur->dwFilter & HDSTUB_FILTER_MODLOAD) && pClientCur->pfnModLoad)
        {
            DEBUGGERMSG (HDZONE_CLIENT, (TEXT("  HdstubTrapModuleLoad: Found client, calling 0x%.08x\r\n"), pClientCur->pfnModLoad));
            fHandled = pClientCur->pfnModLoad (dwStructAddr);
        }
        pClientCur = pClientCur->pCliNext;
    }

    if (!fHandled && ((*s_pulHDEventFilter & HDSTUB_FILTER_MODLOAD) || g_dwModInitCount))
    {
        if (g_dwModInitCount)
        {
            -- g_dwModInitCount;
        }
        HwModLoadHandler (dwStructAddr);
    }

    if (!InSysCall())
    {
        DEBUGGERMSG(HDZONE_ENTRY, (TEXT("  HdstubTrapModuleLoad: ++ LeaveCriticalSection, hCurThread=0x%08X, OwnerThread=0x%08X\r\n"), 
                                  hCurThread, 
                                  csModLoad.OwnerThread));
        g_HdStubData.pfnLeaveCriticalSection(&csModLoad);
        DEBUGGERMSG(HDZONE_ENTRY, (TEXT("  HdstubTrapModuleLoad: -- LeaveCriticalSection, hCurThread=0x%08X, OwnerThread=0x%08X\r\n"), 
                                  hCurThread, 
                                  csModLoad.OwnerThread));
    }


    DEBUGGERMSG(HDZONE_ENTRY, (TEXT("--HdstubTrapModuleLoad\r\n")));
}


void HdstubTrapModuleUnload(DWORD dwStructAddr)
{
    BOOL fHandled = FALSE;
    HDSTUB_CLIENT *pClientCur;
    
    DEBUGGERMSG(HDZONE_ENTRY, (TEXT("++HdstubTrapModuleUnload, dwStructAddr=0x%08X\r\n"),dwStructAddr));

    if (!InSysCall())
    {
        DEBUGGERMSG(HDZONE_ENTRY, (TEXT("  HdstubTrapModuleUnload: ++ EnterCriticalSection, hCurThread=0x%08X, OwnerThread=0x%08X\r\n"), 
                                  hCurThread, 
                                  csModLoad.OwnerThread));
        g_HdStubData.pfnEnterCriticalSection(&csModLoad);
        DEBUGGERMSG(HDZONE_ENTRY, (TEXT("  HdstubTrapModuleUnload: -- EnterCriticalSection, hCurThread=0x%08X, OwnerThread=0x%08X\r\n"), 
                                  hCurThread, 
                                  csModLoad.OwnerThread));
    }

    InterlockedIncrement(&(LONG)s_dwTaintedModuleCount);
    
    pClientCur = pClientListHead;
    while (pClientCur && !fHandled)
    {
        if ((pClientCur->dwFilter & HDSTUB_FILTER_MODUNLOAD) && pClientCur->pfnModUnload)
            fHandled = pClientCur->pfnModUnload (dwStructAddr);
        pClientCur = pClientCur->pCliNext;
    }

    if (!fHandled && (*s_pulHDEventFilter & HDSTUB_FILTER_MODUNLOAD))
        HwModUnloadHandler (dwStructAddr);

    if (!InSysCall())
    {
        DEBUGGERMSG(HDZONE_ENTRY, (TEXT("  HdstubTrapModuleUnload: ++ LeaveCriticalSection, hCurThread=0x%08X, OwnerThread=0x%08X\r\n"), 
                                  hCurThread, 
                                  csModLoad.OwnerThread));
        g_HdStubData.pfnLeaveCriticalSection(&csModLoad);
        DEBUGGERMSG(HDZONE_ENTRY, (TEXT("  HdstubTrapModuleUnload: -- LeaveCriticalSection, hCurThread=0x%08X, OwnerThread=0x%08X\r\n"), 
                                  hCurThread, 
                                  csModLoad.OwnerThread));
    }

    DEBUGGERMSG(HDZONE_ENTRY, (TEXT("--HdstubTrapModuleUnload\r\n")));
}


/*++

Routine Name:

    HdstubUnregisterClient

Routine Description:

    Remove a client from the list in hdstub.
    
Argument:

    pClient - Pointer to the client structure to remove from the list.

--*/
BOOL HdstubUnregisterClient (HDSTUB_CLIENT *pClient)
{
    BOOL fResult = TRUE;
    
    DEBUGGERMSG (HDZONE_CLIENT, (L"++HdstubUnregisterClient: 0x%.08x\r\n", pClient));
    if (pClient == pClientListHead)
    {
        DEBUGGERMSG (HDZONE_CLIENT, (L"  HdstubUnregisterClient: Updating head of list\r\n"));
        if (pClient)
            pClientListHead = pClient->pCliNext;
    }
    else
    {
        HDSTUB_CLIENT *pClientCur;

        pClientCur = pClientListHead;
        while (pClientCur)
        {
            if (pClientCur->pCliNext == pClient)
            {
                DEBUGGERMSG (HDZONE_CLIENT, (L"  HdstubUnregisterClient: Found client, removing.\r\n"));
                if (pClient)
                    pClientCur->pCliNext = pClient->pCliNext;
                goto Exit;
            }
            pClientCur = pClientCur->pCliNext;
        }
        DEBUGGERMSG (HDZONE_CLIENT, (L"  HdstubUnregisterClient: Unable to find client.\r\n"));
        fResult = FALSE;
    }
Exit:
    DEBUGGERMSG (HDZONE_CLIENT, (L"--HdstubUnregisterClient: %d.\r\n", fResult));
    return fResult;;
}


/*++

Routine Name:

    NotifyNewPages

Routine Description:

    Loop over the sections in a process / module and determine whether each section is
    unpageable.  If the section is going to end up unpageable, then manufacture a page-in
    event for that section.

--*/
static void NotifyNewPages (openexe_t *popenexe, DWORD dwNbObjects, o32_lite *pobj32)
{
    DWORD i;
    DWORD dwBaseAddress;

    DEBUGGERMSG (HDZONE_ENTRY, (L"++NotifyNewPages: 0x%08x, %d, 0x%08x\r\n", popenexe, dwNbObjects, pobj32));
    if (pobj32)
    {
        for (i = 0; i < dwNbObjects; i++)
        {
            dwBaseAddress = 0;

            if ((popenexe->filetype & FA_XIP)
                && !(pobj32[i].o32_flags & (IMAGE_SCN_COMPRESSED|IMAGE_SCN_MEM_WRITE)))
            {
                // XIP file and the section is not compressed / writeable
                DEBUGGERMSG (HDZONE_ENTRY, (L"  NotifyNewPages: XIP file, o32_ptr[%d] is not compressed / not RW\r\n", i));
                dwBaseAddress = pobj32[i].o32_realaddr;
            }
            else  if (!PageAble (popenexe) || (pobj32[i].o32_flags & IMAGE_SCN_MEM_NOT_PAGED))
            {
                // This section is guaranteed to never page
                DEBUGGERMSG (HDZONE_ENTRY, (L"  NotifyNewPages: Unpaged section o32_ptr[%d]\r\n", i));
                dwBaseAddress = pobj32[i].o32_realaddr;

                // If this section is RW, fixed-up, and not shared - put the pointer into slot 0
                if ((popenexe->filetype & FA_PREFIXUP)
                    && !(pobj32[i].o32_flags & IMAGE_SCN_MEM_SHARED)
                    && (pobj32[i].o32_flags & IMAGE_SCN_MEM_WRITE))
                {
                    DEBUGGERMSG (HDZONE_ENTRY, (L"  NotifyNewPages: prefixup, RW, unshared section o32_ptr[%d]\r\n", i));
                    // Zero pointer.  it's going into slot zero
                    dwBaseAddress = ZeroPtr (dwBaseAddress);
                }
            }

            if (dwBaseAddress)
            {
                // Have an address to report.
                DEBUGGERMSG (HDZONE_ENTRY, (L"  NotifyNewPages: o32_ptr[%d].o32_realaddr = 0x%08x\r\n", i, pobj32[i].o32_realaddr));
                DEBUGGERMSG (HDZONE_ENTRY, (L"  NotifyNewPages: o32_ptr[%d].o32_vsize = %d\r\n", i, pobj32[i].o32_vsize));
                HdstubTrapVmPageInRange (
                    PAGEALIGN_DOWN (dwBaseAddress),
                    (pobj32[i].o32_vsize + (PAGE_SIZE - 1)) >> VA_PAGE,
                    TRUE);
            }
            else
            {
                // No address.  
                DEBUGGERMSG (HDZONE_ENTRY, (L"  NotifyNewPages: o32_ptr[%d].o32_flags = 0x%08x -> 0, Ignoring\r\n", i, pobj32[i].o32_flags));
                DEBUGGERMSG (HDZONE_ENTRY, (L"  NotifyNewPages: o32_ptr[%d].o32_realaddr = 0x%08x -> 0, Ignoring\r\n", i, pobj32[i].o32_realaddr));
            }
        }
    }
    else
    {
        DEBUGGERMSG (HDZONE_ENTRY, (L"  NotifyNewPages: section array is null.\r\n"));
    }
    DEBUGGERMSG (HDZONE_ENTRY, (L"--NotifyNewPages\r\n"));
}


/*++

Routine Name:

    PageInModule

Routine Description:

    If the module that was just paged in is unpageable, then manufacture page in notification
    for the module.  This is essential for catching delayed assembly breakpoints on addresses
    within the loaded module.

--*/
static void PageInModule (DWORD dwVmBaseAddr)
{
    MODULE *pModule;

    DEBUGGERMSG (HDZONE_ENTRY, (L"++PageInModule: 0x%08x\r\n", dwVmBaseAddr));

    if ((MapPtr (dwVmBaseAddr) >= MapPtr ((DWORD) pCurProc->BasePtr))
        && (MapPtr (dwVmBaseAddr) < MapPtr ((DWORD) pCurProc->BasePtr + pCurProc->e32.e32_vsize)))
    {
        // Loaded a process.
        DEBUGGERMSG (HDZONE_ENTRY, (L"  PageInModule: Proc: %s\r\n", pCurProc->lpszProcName));
        NotifyNewPages (&pCurProc->oe, pCurProc->e32.e32_objcnt, pCurProc->o32_ptr);
    }
    else
    {
        // Just loaded a module
        pModule = pModList;
        while (pModule)
        {
            if (dwVmBaseAddr == ((DWORD) pModule->BasePtr) + 1)
            {
                DEBUGGERMSG (HDZONE_ENTRY, (L"  PageInModule: Mod use = 0x%08x\r\n", pModule->inuse));
                DEBUGGERMSG (HDZONE_ENTRY, (L"  PageInModule: refcnt[%d] = %d\r\n", pCurProc->procnum,
                    pModule->refcnt[pCurProc->procnum]));

                // Unlike processes, make sure that this is the first time the dll is loaded.
                if (!(pModule->inuse & (pModule->inuse - 1))    // Only one bit set.  Inuse != 0
                    && pModule->refcnt[pCurProc->procnum] < 2)  // And refcount is 0/1  (representing first instance)
                {
                    DEBUGGERMSG (HDZONE_ENTRY, (L"  PageInModule: Mod: %s\r\n", pModule->lpszModName));
                    NotifyNewPages (&pModule->oe, pModule->e32.e32_objcnt, pModule->o32_ptr);
                }
            }
            pModule = pModule->pMod;
        }
    }
    DEBUGGERMSG (HDZONE_ENTRY, (L"--PageInModule\r\n"));
}
