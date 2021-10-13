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
#include <windows.h>
#include <coredll.h>
#include <kernel.h>

extern "C" VOID WINAPI WinRegInit(HANDLE  hinstDLL, DWORD fdwReason, LPVOID lpvReserved);
extern "C" void LMemDeInit (void);
extern "C" void InitLocale(void);
extern "C" BOOL WINAPI _CRTDLL_INIT(HANDLE hDllHandle, DWORD dwReason, LPVOID lpreserved);

CRITICAL_SECTION g_csProc;
CRITICAL_SECTION g_csCFFM;    // Critical section to protect CFFM list

#ifdef KCOREDLL

static DWORD (* g_pfnNKHandleCall) (DWORD, va_list);
FARPROC *g_ppfnWin32Calls;               // kernel WIN32 APIs
FARPROC *g_ppfnGWEApiSet1Calls;     // GWE API Set 1
FARPROC *g_ppfnGWEApiSet2Calls;     // GWE API Set 2
FARPROC *g_ppfnFysCalls;                   // FILESYS APIs
FARPROC *g_ppfnDevMgrCalls;            // DEVICE manager APIs

static FARPROC *GetFuncTable (const CINFO *pci)
{
    return  (FARPROC *) (pci? pci->ppfnIntMethods : NULL);
}

extern void InitializeGweApiSetTraps();

extern "C" void UpdateAPISetInfo (void)
{
    static const CINFO **_SystemAPISets = (const CINFO **)(UserKInfo[KINX_APISETS]);

    g_ppfnWin32Calls  = GetFuncTable (_SystemAPISets[SH_WIN32]);
    
    g_ppfnGWEApiSet1Calls  = GetFuncTable (_SystemAPISets[SH_WMGR]);
    g_ppfnGWEApiSet2Calls   = GetFuncTable (_SystemAPISets[SH_GDI]);
    if( g_ppfnGWEApiSet1Calls && g_ppfnGWEApiSet2Calls )
        {
        InitializeGweApiSetTraps();
        }
    
    g_ppfnFysCalls    = GetFuncTable (_SystemAPISets[SH_FILESYS_APIS]);
    g_ppfnDevMgrCalls = GetFuncTable (_SystemAPISets[SH_DEVMGR_APIS]);

    DEBUGCHK (g_ppfnWin32Calls);
    VERIFY (g_pfnNKHandleCall = (DWORD (*) (DWORD, va_list)) g_ppfnWin32Calls[W32_HandleCall]);
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

#endif // KCOREDLL

extern "C" BOOL g_fInitDone;

extern "C" LPVOID *Win32Methods;

LPVOID *Win32Methods;
HANDLE hActiveProc;
HANDLE hInstCoreDll;
PFN_KLIBIOCTL g_pfnKLibIoctl;

#ifdef DEBUG
DBGPARAM dpCurSettings = { TEXT("Coredll"), {
    TEXT("FixHeap"),    TEXT("LocalMem"),  TEXT("Mov"),       TEXT("SmallBlock"),
        TEXT("VirtMem"),    TEXT("Devices"),   TEXT("Deprecated APIs"), TEXT("Loader"),
        TEXT("Stdio"),   TEXT("Stdio HiFreq"), TEXT("Shell APIs"), TEXT("Imm/SEH"),
        TEXT("Heap Validate"),  TEXT("RemoteMem"), TEXT("VerboseHeap"), TEXT("Undefined") },
        0x00000000 };
#endif

void ReleaseProcCS ()
{
    while (OwnProcCS ()) {
        LeaveCriticalSection (&g_csProc);
    }
}


extern "C"
DWORD
GetCRTFlags(
    void
    )
{
    crtGlob_t *pcrtg = GetCRTStorage();
    if (pcrtg)
        {
        return pcrtg->dwFlags;
        }

    return 0;
}


extern "C"
void
SetCRTFlag(
    DWORD dwFlag
    )
{
    crtGlob_t *pcrtg = GetCRTStorage();
    if (pcrtg)
        {
        pcrtg->dwFlags |= dwFlag;
        }
}


extern "C"
void
ClearCRTFlag(
    DWORD dwFlag
    )
{
    crtGlob_t *pcrtg = GetCRTStorage();
    if (pcrtg)
        {
        pcrtg->dwFlags &= ~dwFlag;
        }
}


extern "C"
void
InitCRTStorage(
    void
    )
    {
    crtGlob_t* pcrtg = GetCRTStorage();
    if (pcrtg)
        {
        memset(pcrtg, 0, sizeof(*pcrtg));
        pcrtg->rand = 1;
        }
    }

extern "C"
void
ClearCRTStorage()
    {
    crtGlob_t* pcrtg = GetCRTStorage();
    if (pcrtg)
        {
        // Crt storage can be trashed due to user error. Try-except here such that
        // thread/process can exit normally
        __try
            {
            if (pcrtg->pchfpcvtbuf)
                {
                LocalFree ((HLOCAL) pcrtg->pchfpcvtbuf);
                }
            memset(pcrtg, 0, sizeof(*pcrtg));
            }
        __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
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
		WNDCLASS	 wc;
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

extern "C"
BOOL WINAPI CoreDllInit (HANDLE  hinstDLL, DWORD fdwReason, DWORD reserved)
{
    // Note: This function calls __security_init_cookie, so it must not allocate
    //       any "vulnerable" buffers on the stack or use exception handling.

    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:

        hInstCoreDll = hinstDLL;

#ifdef KCOREDLL
        // NOTE: Cannot call any API before calling UpdateAPISetInfo.
        UpdateAPISetInfo ();

        // kernel mode (kcoredll.dll)
        g_pfnKLibIoctl = (PFN_KLIBIOCTL) reserved;
#else
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

        if (!LoaderInit (hInstCoreDll)) {
            return FALSE;
        }


        DEBUGREGISTER((HINSTANCE)hInstCoreDll);

        if(!LMemInit()) {
            RETAILMSG (1, (L"Memory Too Low To Start Process, Process exiting (pid = %8.8lx)\r\n", GetCurrentProcessId ()));
            return FALSE;
        }
        InitLocale();
        DisableThreadLibraryCalls ((HMODULE) hinstDLL);

        InitCRTStorage ();

        g_fInitDone = TRUE;

        // Initialize the compiler /GS security cookie
        // This must happen before any additional threads are created
        __security_init_cookie();

        _CRTDLL_INIT(hinstDLL, fdwReason, 0);

        break;
    case DLL_PROCESS_DETACH:

        LMemDeInit ();
        _CRTDLL_INIT(hinstDLL, fdwReason, 0);

        break;
    default:
        break;
    }
    WinRegInit(hinstDLL, fdwReason, 0);

    return TRUE;
}

#endif // _CRTBLD

