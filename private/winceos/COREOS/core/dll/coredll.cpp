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
#include <windows.h>
#include <coredll.h>
#include <kernel.h>
#include <GweApiSet1.hpp>

extern "C" VOID WINAPI WinRegInit(HANDLE  hinstDLL, DWORD fdwReason, LPVOID lpvReserved);
extern "C" void LMemDeInit (void);
extern "C" void InitLocale(void);
extern "C" void CrtEntry(BOOL);
extern "C" void InitializeTimeZoneList(void);
extern "C" void DeinitializeTimeZoneList(void);

CRITICAL_SECTION g_csProc;
CRITICAL_SECTION g_csCFFM;    // Critical section to protect CFFM list

GweApiSet1_t *pGweApiSet1Entrypoints;
GweApiSet2_t *pGweApiSet2Entrypoints;


#ifdef KCOREDLL

static DWORD (* g_pfnNKHandleCall) (DWORD, va_list);
FARPROC *g_ppfnWin32Calls;              // kernel WIN32 APIs
FARPROC *g_ppfnCritCalls;               // Critical Seciton calls
FARPROC *g_ppfnFysCalls;                // FILESYS APIs
FARPROC *g_ppfnDevMgrCalls;             // DEVICE manager APIs

static int KMessageBoxW_I(HWND hwnd, __in const WCHAR *szText, __in_opt const WCHAR *szCaption, UINT uType, __out_bcount(cbMsg) MSG *pMsg, size_t cbMsg)
{
    RETAILMSG(1, (__TEXT("Warning: MessageBox not supported from kernel mode\r\n")));
    ASSERT(0);
    return 0;
}

static FARPROC *GetFuncTable (const CINFO *pci)
{
    return  (FARPROC *) (pci? pci->ppfnIntMethods : NULL);
}

extern "C" void UpdateAPISetInfo (void)
{
    static BOOL fInitialized = FALSE;
    static const CINFO **_SystemAPISets = (const CINFO **)(UserKInfo[KINX_APISETS]);

    if (!fInitialized) {

        if (!g_ppfnWin32Calls) {
            g_ppfnWin32Calls = GetFuncTable (_SystemAPISets[SH_WIN32]);
            g_ppfnCritCalls = GetFuncTable (_SystemAPISets[HT_CRITSEC]);

            DEBUGCHK (g_ppfnWin32Calls && g_ppfnCritCalls);
            g_pfnNKHandleCall = (DWORD (*) (DWORD, va_list)) g_ppfnWin32Calls[W32_HandleCall];
            DEBUGCHK (g_pfnNKHandleCall);
        }
        
        pGweApiSet1Entrypoints  = (GweApiSet1_t*)GetFuncTable (_SystemAPISets[SH_WMGR]);
        pGweApiSet2Entrypoints  = (GweApiSet2_t*)GetFuncTable (_SystemAPISets[SH_GDI]);
        if (pGweApiSet1Entrypoints) {
            pGweApiSet1Entrypoints->m_pMessageBoxW = KMessageBoxW_I;
        }
        
        g_ppfnFysCalls    = GetFuncTable (_SystemAPISets[SH_FILESYS_APIS]);
        g_ppfnDevMgrCalls = GetFuncTable (_SystemAPISets[SH_DEVMGR_APIS]);

        if (pGweApiSet1Entrypoints && pGweApiSet2Entrypoints && g_ppfnFysCalls && g_ppfnDevMgrCalls)
            fInitialized = TRUE;
    }
}


//
// DirectHandleCall - direct call from kcoredll to make a handle-based API call
//
extern "C" DWORD DirectHandleCall (
        REG_TYPE dwHtype,          // the expected handle type
        ...                     // variable # of arguments
        )
{
    va_list arglist;
    DWORD   dwRet;
    va_start(arglist, dwHtype);
    dwRet = (*g_pfnNKHandleCall) ((DWORD)dwHtype, arglist);
    va_end(arglist);
    return dwRet;
}

#else // KCOREDLL

// NOTENOTE SetDirectCall/DirectHandleCall exist only in kernel mode.
// It is better to hit a link error if there is confusion, than it is to
// fail at run-time.  So they are not defined in user mode.
GweApiSet1_t GweApiSet1Traps;
GweApiSet2_t GweApiSet2Traps;

void InitializeGweApiSetTraps()
{
    int i;
    pGweApiSet1Entrypoints = (GweApiSet1_t*) &GweApiSet1Traps;
    pGweApiSet2Entrypoints = (GweApiSet2_t*) &GweApiSet2Traps;
    for (i = 1; i < sizeof(GweApiSet1_t)/sizeof(DWORD); i ++) {
        ((LPDWORD) pGweApiSet1Entrypoints)[i] = IMPLICIT_CALL(SH_WMGR, i);
    }
    for (i = 1; i < sizeof(GweApiSet2_t)/sizeof(DWORD); i ++) {
        ((LPDWORD) pGweApiSet2Entrypoints)[i] = IMPLICIT_CALL(SH_GDI, i);
    }
}

// DirectHandleCall is not supported from user-mode but needs to be defined
// so that it is available in the coredll export library.
extern "C" DWORD DirectHandleCall (
        REG_TYPE /* dwHtype */, // the expected handle type
        ...                     // variable # of arguments
        )
{
    RETAILMSG(1, (L"!ERROR: Calling DirectHandleCall from user-mode not allowed\r\n"));
    DebugBreak (); // DirectHandleCall is not available in user-mode.
    return 0;
}

#endif // KCOREDLL

extern "C" BOOL g_fInitDone;

extern "C" LPVOID *Win32Methods;

LPVOID *Win32Methods;
HANDLE hActiveProc;
HANDLE hInstCoreDll;
PFN_KLIBIOCTL g_pfnKLibIoctl;

// zones enabled in all non-ship builds
#if !defined(SHIP_BUILD)
DBGPARAM dpCurSettings = 
{ 
    TEXT("Coredll"), 
    {
        TEXT("FixHeap"),    
        TEXT("LocalMem"),  
        TEXT("Mov"),       
        TEXT("HeapChecks"),
        TEXT("VirtMem"),    
        TEXT("Devices"),   
        TEXT("Deprecated API"), 
        TEXT("Loader"),
        TEXT("Stdio"),   
        TEXT("Stdio HiFreq"), 
        TEXT("Shell APIs"), 
        TEXT("Imm/SEH"),
        TEXT("Heap Validate"),  
        TEXT("RemoteMem"), 
        TEXT("VerboseHeap"), 
        TEXT("Undefined") 
    },
    0x00000000 
};
#endif // SHIP_BUILD

void ReleaseProcCS ()
{
    while (OwnProcCS ()) {
        LeaveCriticalSection (&g_csProc);
    }
}


#ifndef _CRTBLD // A testing CRT build cannot build CoreDllInit()
#include <..\..\gwe\inc\dlgmgr.h>

extern "C"
void
RegisterDlgClass(
    void
    )
{
    if ( WAIT_OBJECT_0 == WaitForAPIReady(SH_WMGR, 0) )
        {
        WNDCLASS wc = {0};
        
        wc.style         = 0;
        wc.lpfnWndProc   = DefDlgProcW;
        wc.cbClsExtra    = 0;
        wc.cbWndExtra    = sizeof(DLG);
        wc.hInstance     = (HINSTANCE)hInstCoreDll;
        wc.hIcon         = NULL;
        wc.hCursor       = NULL;
        wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
        wc.lpszMenuName  = NULL;
        wc.lpszClassName = DIALOGCLASSNAME;
        RegisterClassW(&wc);
        }
}

BOOL LoaderInit (HANDLE hCoreDll);

extern "C" CRITICAL_SECTION g_veCS;    // Critical section to protect vector handler list
extern "C" BOOL CeInitializeRwLocks(void); // r-w lock initialization

extern "C"
BOOL WINAPI CoreDllInit (HANDLE  hinstDLL, DWORD fdwReason, DWORD reserved)
{
    // Note: This function calls __security_init_cookie, so it must not allocate
    //       any "vulnerable" buffers on the stack or use exception handling.

    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        InitInterlockedFunctions ();
#ifdef ARM
        InitPCBFunctions ();
#endif
        hInstCoreDll = hinstDLL;

#ifdef KCOREDLL
        // NOTE: Cannot call any API before calling UpdateAPISetInfo.
        UpdateAPISetInfo ();

        // kernel mode (kcoredll.dll)
        g_pfnKLibIoctl = (PFN_KLIBIOCTL) reserved;
#else
        InitializeGweApiSetTraps ();
        // critical section for vector exception list
        InitializeCriticalSection (&g_veCS);
        if (!g_veCS.hCrit) {
            // CS initialization failed - memory critically low
            return FALSE;
        }
#endif

        InitializeCriticalSection (&g_csProc);
        if (!g_csProc.hCrit) {
            // CS initialization failed - memory critically low
            return FALSE;
        }
        
        InitializeCriticalSection (&g_csCFFM);
        if (!g_csCFFM.hCrit) {
            // CS initialization failed - memory critically low
            return FALSE;
        }

        if (!CeTlsCoreInit ()) {
            // memory critically low; fail the call
            return FALSE;
        }

        if (!LoaderInit (hInstCoreDll)) {
            return FALSE;
        }

        DEBUGREGISTER((HINSTANCE)hInstCoreDll);

        if(!LMemInit()) {
            RETAILMSG (1, (L"Memory Too Low To Start Process, Process exiting (pid = %8.8lx)\r\n", GetCurrentProcessId ()));
            return FALSE;
        }

        if (!CeInitializeRwLocks()) {
            RETAILMSG (1, (L"Memory Too Low To Start application, Process exiting (pid = %8.8lx)\r\n", GetCurrentProcessId ()));
            return FALSE;
        }

        InitLocale();
        InitializeTimeZoneList();
        DisableThreadLibraryCalls ((HMODULE) hinstDLL);

        CrtEntry(TRUE);

        g_fInitDone = TRUE;

        break;
    case DLL_PROCESS_DETACH:

#ifndef KCOREDLL
        if (IsCurrentProcessTerminated ()) {
            // current process been terminated. only call LMemDeInit to free remote heap, nothing else.
            LMemDeInit ();
            return TRUE;
        }
        DeinitializeTimeZoneList();
        LMemDeInit ();
        CrtEntry(FALSE);
#endif

        break;
    default:
        break;
    }
    WinRegInit(hinstDLL, fdwReason, 0);

    return TRUE;
}

#endif // _CRTBLD

