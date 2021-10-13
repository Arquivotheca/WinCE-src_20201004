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
#include <pehdr.h>
#include <kerncmn.h>    // for DLIST
#include <loader.h>
#include <loader_core.h>
#include <coredll.h>
#include <AppVerifier.h>
#include <Corecrtstorage.h>
#include "dllheap.h"

extern "C" BOOL ShowErr (DWORD dwExcpCode, DWORD dwExcpAddr);
extern "C" BOOL Imm_DllEntry(HANDLE hinstDll, DWORD dwReason, LPVOID lpvReserved);
extern "C" BOOL WINAPI CoreDllInit (HANDLE  hinstDLL, DWORD fdwReason, DWORD dwModCnt);
extern "C" void RegisterDlgClass (void);

ERRFALSE(offsetof(MODULE, lpSelf)  == offsetof(USERMODULELIST, hModule));
ERRFALSE(offsetof(MODULE, wRefCnt) == offsetof(USERMODULELIST, wRefCnt));
ERRFALSE(offsetof(MODULE, wFlags)  == offsetof(USERMODULELIST, wFlags));
ERRFALSE(offsetof(MODULE, startip) == offsetof(USERMODULELIST, pfnEntry));
ERRFALSE(offsetof(MODULE, BasePtr) == offsetof(USERMODULELIST, dwBaseAddr));
ERRFALSE(offsetof(MODULE, pmodResource) == offsetof(USERMODULELIST, pmodResource));


static BOOL CallDllMain (PUSERMODULELIST pMod, DWORD dwReason);
static PUSERMODULELIST DoImportAndCallDllMain (PUSERMODULELIST pMod);
static void UnDoDepends (PUSERMODULELIST pMod);
static FARPROC DoGetProcAddressA (PUSERMODULELIST pMod, LPCSTR pszFuncName);
BOOL
AppVerifierIoControl (
    DWORD   dwIoControlCode,
    LPVOID  lpInBuf,
    DWORD   nInBufSize,
    LPVOID  lpOutBuf,
    DWORD   nOutBufSize,
    LPDWORD lpBytesReturned
    );

BOOL    IsExiting;
extern "C" {
    // used by th32heap.c
    BOOL    g_fInitDone;
}

// rw lock tls slot and tls cleanup function
extern DWORD g_dwLockSlot;
extern void CeFreeRwTls (DWORD dwThreadId, LPVOID lpvSlotValue);

#define NKExitThread        WIN32_CALL(void, NKExitThread, (DWORD dwExitCode))

#ifdef KCOREDLL

LPCWSTR g_pszProgName = L"NK.EXE";
LPCWSTR g_pszCmdLine = L"";

PUSERMODULELIST (* FindModule) (HMODULE hMod);
extern "C" {
    int (*NKwvsprintfW) (LPWSTR lpOut, LPCWSTR lpFmt, va_list lpParms, int maxchars);
}

#define FindModuleByName(pszName, dwFlags)               ((PUSERMODULELIST) GetModHandle (hActiveProc, pszName, dwFlags))
#define FindModuleByNameInProcess(hProcess, pszName, dwFlags)               ((PUSERMODULELIST) GetModHandle (hProcess, pszName, dwFlags))

// kernel DLL uses kernel's loader CS directly
#define LockLoader()
#define UnlockLoader()    AppVerifierUnlockLoader ()
#define OwnLoaderLock()

BOOL LoaderInit (HANDLE hCoreDll)
{
    LOADERINITINFO lii = {
        (FARPROC) DoGetProcAddressA,
        (FARPROC) DoImportAndCallDllMain,
        (FARPROC) UnDoDepends,
        (FARPROC) AppVerifierIoControl,
        (FARPROC) NULL,
        (FARPROC) NULL
    };
    (*g_pfnKLibIoctl) ((HANDLE) KMOD_CORE, IOCTL_KLIB_LOADERINIT, &lii, sizeof (lii), NULL, 0, NULL);
    FindModule = (PUSERMODULELIST (*) (HMODULE)) lii.pfnFindModule;
    NKwvsprintfW = (int (*) (LPWSTR lpOut, LPCWSTR lpFmt, va_list lpParms, int maxchars)) lii.pfnNKwvsprintfW;

    return TRUE;
}

#else // KCOREDLL

#define NKLoadExistingModule WIN32_CALL(HMODULE, LoadExistingModule, (HMODULE hMod, PUSERMODULELIST pMod))

typedef DWORD (*comthread_t) (HANDLE, ulong, LPCWSTR, ulong, ulong, ulong, ulong);
typedef DWORD (*mainthread_t) (HANDLE, ulong, LPCWSTR, ulong);

DWORD   g_dwMainThId;

LPCWSTR g_pszProgName;
LPCWSTR g_pszCmdLine;

USERMODULELIST   g_mlCoreDll, *g_pExeMUI;
static DLIST   g_dllList;
DLIST   g_loadOrderList;

DWORD       g_dwExeBase;
PCInfo g_impExe;
struct info g_resExe;   // save the resource information of the exe

#if defined(x86)
extern "C" BOOL    g_fDepEnabled = TRUE; // Data Execution Prevention enabled
#endif

#define LockLoader()      LockProcCS()
#define UnlockLoader()    AppVerifierUnlockLoader () // UnlockProcCS()
#define OwnLoaderLock()   OwnProcCS()

BOOL g_fAbnormalExit;

void SetAbnormalTermination (void)
{
    g_fAbnormalExit = TRUE;
}

// EnumerateModules - enumerate through the moudle list, call pfnEnum on every item.
//      Stop when pfnEnum returns TRUE, or the list is exhausted.
//          Return the item when pfnEnum returns TRUE,
//          or NULL if list is exhausted.
PUSERMODULELIST EnumerateModules (PFN_DLISTENUM pfnEnum, LPVOID pEnumData)
{
    PDLIST ptrav;
    DEBUGCHK (OwnLoaderLock ());
    for (ptrav = g_dllList.pFwd; ptrav != &g_dllList; ptrav = ptrav->pFwd) {
        if (pfnEnum (ptrav, pEnumData)) {
            // maintain MRU
            RemoveDList (ptrav);
            AddToDListHead (&g_dllList, ptrav);
            return (PUSERMODULELIST) ptrav;
        }
    }
    return NULL;
}

static BOOL MatchMod (PDLIST pItem, LPVOID pEnumData)
{
    return ((PUSERMODULELIST)pItem)->hModule == (HMODULE) pEnumData;
}

PUSERMODULELIST FindModule (HMODULE hMod)
{
    return EnumerateModules (MatchMod, hMod);
}

static PUSERMODULELIST FindModuleByName (LPCWSTR pszName, DWORD dwFlags)
{
    HMODULE hMod = GetModHandle (hActiveProc, pszName, dwFlags);
    return hMod? FindModule (hMod) : NULL;
}

static PUSERMODULELIST FindModuleByNameInProcess (HANDLE hProcess, LPCWSTR pszName, DWORD dwFlags)
{
    return NULL; // this is supported only in kcoredll
}

static PUSERMODULELIST AllocateUserModule (void)
{
    PUSERMODULELIST pMod = (PUSERMODULELIST) LocalAlloc (LMEM_FIXED, sizeof (USERMODULELIST));

    if (pMod) {
        InitDList (&pMod->loadOrderLink);
    }

    return pMod;
}

static
PUSERMODULELIST InitMUI (HMODULE hMUI)
{
    PUSERMODULELIST pMod = NULL;
    if (hMUI) {
        if (NULL != (pMod = FindModule (hMUI))) {

            if (pMod->wRefCnt < 0xffff) {
                pMod->wRefCnt ++;
            } else {
                DEBUGCHK (0);
            }

        } else if (NULL != (pMod = AllocateUserModule ())) {

            pMod->hModule = NKLoadExistingModule (hMUI, pMod);
            if (pMod->hModule) {
                pMod->wRefCnt = 1;
                pMod->wFlags  = DONT_RESOLVE_DLL_REFERENCES | LOAD_LIBRARY_AS_DATAFILE;

                DEBUGCHK (pMod->hModule == hMUI);

                AddToDListHead (&g_dllList, &pMod->link);
            } else {
                LocalFree (pMod);
                pMod = NULL;
            }
        }
    }
    return pMod;
}

static
void ThreadNotifyDLLs (
    DWORD dwReason
    )
{
    PUSERMODULELIST pMod;
    PDLIST ptrav;
    LockLoader ();

    for (ptrav = g_loadOrderList.pFwd; ptrav != &g_loadOrderList; ptrav = ptrav->pFwd) {
        pMod = GetModFromLoadOrderList (ptrav);

        if (!((MF_LOADING|MF_NO_THREAD_CALLS) & pMod->wFlags)
            && (MF_IMPORTED & pMod->wFlags)) {
            CallDllMain (pMod, dwReason);
        }
    }

    // if thread is exiting, remove any rw lock support from this thread
    if ((dwReason == DLL_THREAD_DETACH) && (UTlsPtr()[g_dwLockSlot])) {
        CeTlsFree (g_dwLockSlot);
    }        

    UnlockLoader ();
}

extern "C" BOOL IsCurrentProcessTerminated (void)
{
    return g_fAbnormalExit || CeGetModuleInfo (GetCurrentProcess (), NULL, MINFO_PROC_TERMINATED, NULL, 0);
}

static void ProcessDetach (void)
{
    AppVerifierProcessDetach ();

    // no need to take loader lock, for we're the only thread running at this point.
    if (!IsCurrentProcessTerminated ()) {

        //
        // in order to ensure correct unload order, we need to iterate through all the DLL loaded
        // mulitple times, decrementing refcnt along the way. Only call DllMain when refcnt gets
        // to zero. This way we ensure that when the DllMain of a DLL is called, there are no other
        // DLLs depending on the particular DLL.
        //

        DWORD dwMinCnt = 1;
        DWORD dwNewMinCnt;
        PDLIST ptrav;
        PUSERMODULELIST pMod;

        __try {
            do {
                dwNewMinCnt = 0x10000;
                for (ptrav = g_loadOrderList.pFwd; ptrav != &g_loadOrderList; ptrav = ptrav->pFwd) {
                    pMod = GetModFromLoadOrderList (ptrav);

                    DEBUGCHK (&g_mlCoreDll != pMod);

                    if (   pMod->wRefCnt                    // dllmain (PROCESS_DETACH,...) not called
                        && !(MF_LOADING & pMod->wFlags)     // not in the middle of loading
                        && (MF_IMPORTED & pMod->wFlags)) {  // dllmain (PROCESS_ATTACH,...) called

                        pMod->wRefCnt -= (WORD) dwMinCnt;
                        if (!pMod->wRefCnt) {
                            // no other DLL depends on us, call DllMain
                            CallDllMain (pMod, DLL_PROCESS_DETACH);

                        // refcnt not zero, update min refcnt
                        } else if (dwNewMinCnt > pMod->wRefCnt) {
                            dwNewMinCnt = pMod->wRefCnt;
                        }
                    }
                }
                dwMinCnt = dwNewMinCnt;
            }while (0x10000 > dwMinCnt);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            RETAILMSG (1, (L"Exception occurs during process detach, DLLs might not be notified of process exit!\r\n"));
        }
    }
}

static BOOL MatchResource (PDLIST pItem, LPVOID hrsrc)
{
    return IsResValid (GetResInfo ((PUSERMODULELIST)pItem), (HRSRC) hrsrc);
}

PUSERMODULELIST FindModByResource (HRSRC hRsrc)
{
    PUSERMODULELIST pMod;
    LockLoader ();
    pMod = EnumerateModules (MatchResource, hRsrc);
    UnlockLoader ();
    return pMod;
}

PUSERMODULELIST
DoLoadLibrary (
    LPCWSTR pszFileName,
    DWORD dwFlags
    )
{
    PUSERMODULELIST pMod;
    DEBUGCHK (OwnLoaderLock ());

    pMod = FindModuleByName (pszFileName, dwFlags);

    if (pMod) {
        // already loaded, or loading
        if (pMod->wRefCnt < 0xffff) {
            pMod->wRefCnt ++;

            if ((DONT_RESOLVE_DLL_REFERENCES == (pMod->wFlags & NO_IMPORT_FLAGS))
                && !(dwFlags & NO_IMPORT_FLAGS)) {
                // if a module is loaded previously with DONT_RESOLVE_DLL_REFERENCES flag
                // and the new request is to load as a normal dll, upgrade the module to a regular dll
                pMod->wFlags &= ~DONT_RESOLVE_DLL_REFERENCES;   // this will enable resolving imports
                pMod->wFlags |= MF_LOADING;                     // this will enable calling dllmain

                // add to load order list, such that we'll call DllMain when needed
                AddToDListTail (&g_loadOrderList, &pMod->loadOrderLink);
            }

        } else {
            DEBUGCHK (0);
            pMod = NULL;
        }

    } else if ((NULL != (pMod = AllocateUserModule ()))
            && (NULL != (pMod->hModule = NKLoadLibraryEx (pszFileName, dwFlags, pMod)))) {

        pMod->wRefCnt = 1;
        // kernel ignore DONT_RESOLVE_DLL_REFERENCES flag for user-mode DLLs, unless loaded as data file.
        // in either case, we need to add the flag to the per-process module list if requested.
        pMod->wFlags |= (NO_IMPORT_FLAGS & dwFlags)? DONT_RESOLVE_DLL_REFERENCES : MF_LOADING;

        AddToDListHead (&g_dllList, &pMod->link);

        if (!(DONT_RESOLVE_DLL_REFERENCES & pMod->wFlags)) {
            AddToDListTail (&g_loadOrderList, &pMod->loadOrderLink);

        }

#if defined(x86)
        // if any module that doesn't support DEP is loaded, DEP must be disabled
        if (!(pMod->wFlags & (MF_DEP_COMPACT|LOAD_LIBRARY_AS_DATAFILE))) {
            g_fDepEnabled = FALSE;
        }
#endif

        pMod->pmodResource = InitMUI ((HMODULE) pMod->pmodResource);

    } else if (pMod) {
        // failed, last error already set
        LocalFree (pMod);
        pMod = NULL;
    }
    DEBUGMSG (DBGLOADER&&pMod, (L"Resolving Import for Library '%s'\r\n", pszFileName));

    AppVerifierDoLoadLibrary (pMod, pszFileName);

    //
    // If you see the following message, it means that there are 2 DLLs that are
    // 'mutual dependent'(DLLA depends on DLLB, and DLLB depends on DLLA). It's
    // STRONGLY NOT RECOMMENDED to create such dependency for it can cause memory
    // leak. For example:
    //      h = LoadLibary (szMutualDependentDll);
    //      FreeLibrary (h);
    // will NOT release memory for this DLL until process exit. The reason is
    // that due to mutual dependency, the ref-count of the DLL will be 2 with a single
    // LoadLibrary call. Therefore FreeLibrary won't release the memory because the
    // library is still 'in-use'.
    //

    DEBUGMSG(pMod && (pMod->wFlags & MF_IMPORTING),
        (L"!!!WARNING: Mutually dependent DLL detected: %s (pMod = 0x%08x)\n", pszFileName, pMod));

    return DoImportAndCallDllMain (pMod);
}

BOOL
DoFreeLibrary (
    PUSERMODULELIST pMod
    )
{
    BOOL fRet = TRUE;

    DEBUGCHK (OwnLoaderLock ());

    if (! pMod->wRefCnt) {
        DEBUGCHK (0);
        fRet = FALSE;

    } else if (! -- pMod->wRefCnt) {

        if (&g_mlCoreDll == pMod) {
            DEBUGMSG (1, (L"Freeing coredll before process exit, ignored\r\n"));
            pMod->wRefCnt ++;
            fRet = FALSE;

        } else {
            RemoveDList (&pMod->link);
            RemoveDList (&pMod->loadOrderLink);

            UnDoDepends (pMod);

            if (pMod->pmodResource) {
                DoFreeLibrary (pMod->pmodResource);
            }

            // free up memory
            NKFreeLibrary (pMod->hModule);
            LocalFree (pMod);
        }
    }
    return fRet;
}

void InjectDlls (void)
{
    HKEY  hKey;

    // get Inject Dlls from registry
    if ((WAIT_OBJECT_0 == WaitForAPIReady (SH_FILESYS_APIS, 0))
        && (ERROR_SUCCESS == RegOpenKeyExW (HKEY_LOCAL_MACHINE, L"SYSTEM\\KERNEL", 0, KEY_READ, &hKey))) {
        DWORD dwType, cbSize;
        LPWSTR pInjectDLLs = NULL;

        if ((ERROR_SUCCESS == RegQueryValueExW (hKey, L"InjectDLL", NULL, &dwType, NULL, &cbSize))
            && cbSize
            && (REG_MULTI_SZ == dwType)
            && (NULL != (pInjectDLLs = (LPWSTR) LocalAlloc (LMEM_FIXED, cbSize)))
            && (ERROR_SUCCESS == RegQueryValueExW (hKey, L"InjectDLL", NULL, &dwType, (LPBYTE)pInjectDLLs, &cbSize))) {

            LPWSTR pszDllName;
            for (pszDllName = pInjectDLLs; *pszDllName; pszDllName += wcslen (pszDllName)+1) {
                DEBUGMSG (DBGLOADER, (TEXT("Inject %s into Process %s\r\n"), pszDllName, g_pszProgName));
                LoadLibraryExW (pszDllName, NULL, 0);
            }

        }

        RegCloseKey (hKey);
        LocalFree (pInjectDLLs);
    }
}


BOOL LoaderInit (HANDLE hCoreDll)
{
    e32_lite e32;

    if (!ReadPEHeader (hActiveProc, RP_READ_PE_BY_MODULE, (DWORD) hCoreDll, &e32, sizeof (e32))) {
        // initialization failed - memory critically low
        return FALSE;
    }
    g_mlCoreDll.hModule  = (HMODULE) hCoreDll;
    g_mlCoreDll.pfnEntry = (dllntry_t) CoreDllInit;
    g_mlCoreDll.wRefCnt  = 1;
    g_mlCoreDll.wFlags   = MF_IMPORTED | MF_NO_THREAD_CALLS | MF_DEP_COMPACT | MF_COREDLL;
    g_mlCoreDll.dwBaseAddr = e32.e32_vbase;
    g_mlCoreDll.udllInfo[IMP] = e32.e32_unit[IMP];
    g_mlCoreDll.udllInfo[EXP] = e32.e32_unit[EXP];
    g_mlCoreDll.udllInfo[RES] = e32.e32_unit[RES];
    // sect14 for coredll is always 0, don't bother copying it.

    InitDList (&g_mlCoreDll.loadOrderLink);
    InitDList (&g_loadOrderList);
    InitDList (&g_dllList);
    AddToDListHead (&g_dllList, &g_mlCoreDll.link);

    return TRUE;
}

#endif // KCOREDLL

static BOOL CallDllMain (PUSERMODULELIST pMod, DWORD dwReason)
{
    BOOL fRet = TRUE;

    DEBUGMSG (DBGLOADER, (L"Calling DllMain %8.8lx, pMod = %8.8lx, reason = %8.8lx\r\n",
                        pMod->pfnEntry, pMod, dwReason));

    // Call into appverifier so it can notify shim modules
    // about pMod. (BEFORE calling DllMain() for ATTACH)
    if (DLL_PROCESS_ATTACH == dwReason)
    {
        AppVerifierDoNotify (pMod, DLL_PROCESS_ATTACH);
    }

    __try {
        if (pMod->pfnEntry) {
            LPVOID pReserved = (DLL_PROCESS_ATTACH == dwReason)? (LPVOID) g_pfnKLibIoctl : (LPVOID)IsExiting;
            fRet = (* pMod->pfnEntry) (pMod->hModule, dwReason, pReserved);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        fRet = FALSE;
    }

    // Call into appverifier so it can notify shim modules
    // about pMod (AFTER calling DllMain for DETACH / if ATTACH failed, 
    // in which case CallDllMain is not called by the loader.)
    if ((DLL_PROCESS_DETACH == dwReason) || (!fRet && DLL_PROCESS_ATTACH == dwReason))
    {
        AppVerifierDoNotify (pMod, DLL_PROCESS_DETACH);
    }

    if (!fRet) {
        SetLastError (ERROR_DLL_INIT_FAILED);
    }

    return fRet;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static void UnicodeToAscii(LPCHAR chptr, LPCWSTR wchptr, int maxlen) {
    while ((maxlen-- > 1) && *wchptr) {
        *chptr++ = (CHAR)*wchptr++;
    }
    *chptr = 0;
}


static
DWORD
AddrFromEat (
    PUSERMODULELIST pMod,
    DWORD  eat
    );


static void FreeLibraryByName (LPCWSTR pszName)
{
    PUSERMODULELIST pMod = FindModuleByName (pszName, 0);
    if (pMod) {
        DoFreeLibrary (pMod);
    }
}


//------------------------------------------------------------------------------
// Get address from ordinal
//------------------------------------------------------------------------------
DWORD
ResolveImpOrd (
    PUSERMODULELIST pMod,
    PCExpHdr    expptr,
    DWORD       ord
    )
{
    LPDWORD eatptr = (LPDWORD) (pMod->dwBaseAddr + expptr->exp_eat);
    DWORD   hint   = ord - expptr->exp_ordbase;
    DWORD   retval = (hint >= expptr->exp_eatcnt)? 0 : AddrFromEat (pMod, eatptr[hint]);

    return retval;
}


//------------------------------------------------------------------------------
// Get address from string
//------------------------------------------------------------------------------
static
DWORD
ResolveImpStr(
    PUSERMODULELIST pMod,
    PCExpHdr    expptr,
    LPCSTR      str
    )
{
    DWORD  retval = 0;
    DWORD  dwBaseAddr = pMod->dwBaseAddr;
    LPCHAR *nameptr = (LPCHAR *) (dwBaseAddr + expptr->exp_name);

    DEBUGCHK (expptr->exp_namecnt <= expptr->exp_eatcnt);

    for (DWORD loop = 0; loop < expptr->exp_namecnt; loop++) {
        if (!strcmp (str, nameptr[loop] + dwBaseAddr)) {
            LPDWORD eatptr = (LPDWORD)(dwBaseAddr + expptr->exp_eat);
            LPWORD  ordptr = (LPWORD) (dwBaseAddr + expptr->exp_ordinal);

            retval = AddrFromEat (pMod, eatptr[ordptr[loop]]);
            break;
        }
    }

    return retval;
}

static PUSERMODULELIST FindForwardDll (LPCWSTR pszDllName)
{
    static WCHAR szFwdDllName[MAX_AFE_FILESIZE];
    static PUSERMODULELIST pFwdMod;

    // Note: we're holding the loader lock at this point, so it's safe to access/update the global data.
    PUSERMODULELIST pMod = pFwdMod;

    if (wcsicmp (pszDllName, szFwdDllName)) {

        //
        // not the last forwarding DLL loaded, try t load it.
        //

        pMod = FindModuleByName (pszDllName, 0);
        if (pMod) {
            DEBUGMSG (DBGLOADER, (L"AddrFromEat: Library '%s' already loaded\r\n", pszDllName));
            // module already loaded, do import if necessary
            //
            // If you see the following message, it means that there are 2 DLLs that are
            // 'mutual dependent'(DLLA depends on DLLB, and DLLB depends on DLLA). It's
            // STRONGLY NOT RECOMMENDED to create such dependency for it can cause memory
            // leak. For example:
            //      h = LoadLibary (szMutualDependentDll);
            //      FreeLibrary (h);
            // will NOT release memory for this DLL until process exit. The reason is
            // that due to mutual dependency, the ref-count of the DLL will be 2 with a single
            // LoadLibrary call. Therefore FreeLibrary won't release the memory because the
            // library is still 'in-use'.
            //
            DEBUGMSG(pMod && (pMod->wFlags & MF_IMPORTING),
                (L"!!!WARNING: Mutually dependent DLL detected: %s (pMod = 0x%08x)\n", pszDllName, pMod));
            pMod = DoImportAndCallDllMain (pMod);
        } else {
            DEBUGMSG (DBGLOADER, (L"AddrFromEat: Loading Library '%s'\r\n", pszDllName));
            pMod = DoLoadLibrary (pszDllName, 0);
        }

        if (pMod) {
            // remember the last forwarding DLL.
            VERIFY (SUCCEEDED (StringCchCopyW (szFwdDllName, MAX_AFE_FILESIZE, pszDllName)));
            pFwdMod = pMod;
        }
    }

    return pMod;
}


//------------------------------------------------------------------------------
// Get address from export entry
//------------------------------------------------------------------------------
static
DWORD
AddrFromEat (
    PUSERMODULELIST pMod,
    DWORD  eat
    )
{
    DWORD result = 0;
    PCInfo pExpInfo = GetExportInfo (pMod);

    if (eat) {
        if (   (eat <  pExpInfo->rva)
            || (eat >= pExpInfo->rva + pExpInfo->size)) {
            result = eat + pMod->dwBaseAddr;
            DEBUGMSG (DBGLOADER, (L"eat = %8.8lx, result = %8.8lx\n", eat, result));

        } else {

            // Dll redirection

            WCHAR   filename[MAX_AFE_FILESIZE];
            LPCHAR  str;
            int     loop;

            str = (LPCHAR) (eat + pMod->dwBaseAddr);
            for (loop = 0; loop < MAX_AFE_FILESIZE-1; loop++) {
                if ((filename[loop] = (WCHAR)*str++) == (WCHAR)'.')
                    break;
            }
            filename[loop] = 0;

            pMod = FindForwardDll (filename);

            if (pMod) {

                pExpInfo = GetExportInfo (pMod);

                if (pExpInfo->rva) {

                    PCExpHdr expptr = (PCExpHdr) (pMod->dwBaseAddr + pExpInfo->rva);

                    result =  (*str == '#')
                                ? ResolveImpOrd (pMod, expptr, atoi(str+1))
                                : ResolveImpStr (pMod, expptr, str);
                }
            }
        }
    }

    return result;
}


//------------------------------------------------------------------------------
// Get address from hint and string
//------------------------------------------------------------------------------
DWORD
ResolveImpHintStr(
    PUSERMODULELIST pMod,
    PCExpHdr    expptr,
    DWORD       hint,
    LPCSTR      str
    )
{
    DWORD  retval;
    DWORD  dwBaseAddr = pMod->dwBaseAddr;
    LPCHAR *nameptr = (LPCHAR *) (dwBaseAddr + expptr->exp_name);

    if ((hint >= expptr->exp_namecnt) || (strcmp(str, nameptr[hint] + dwBaseAddr)))
        retval = ResolveImpStr (pMod, expptr, str);
    else {
        LPDWORD eatptr = (LPDWORD) (dwBaseAddr + expptr->exp_eat);
        LPWORD  ordptr = (LPWORD)  (dwBaseAddr + expptr->exp_ordinal);
        retval = AddrFromEat (pMod, eatptr[ordptr[hint]]);
    }

    return retval;
}


//------------------------------------------------------------------------------
// ImportOneBlock: helper function to do import for a DLL
//------------------------------------------------------------------------------
static
DWORD
ImportOneBlock (
    PUSERMODULELIST pMod,           // DLL to be imported from
    DWORD           dwImpBase,      // base address of the DLL importing
    LPDWORD         ltptr,          // starting ltptr
    LPDWORD         atptr           // starting atptr
    )
{
    DWORD  retval;
    DWORD  dwErr = 0;
    PCInfo pExpInfo = GetExportInfo (pMod);
    PCExpHdr expptr;

    if (!pExpInfo->rva) {
        return ERROR_BAD_EXE_FORMAT;
    }

    expptr = (PCExpHdr) (pMod->dwBaseAddr + pExpInfo->rva);

    DEBUGMSG(DBGLOADER,(TEXT("ltptr = %8.8lx atptr = %8.8lx\n"),ltptr, atptr));

    for (dwErr = 0; !dwErr && *ltptr; ltptr ++, atptr ++) {

        if (*ltptr & 0x80000000) {
            retval = ResolveImpOrd (pMod, expptr, *ltptr & 0x7fffffff);
            if (!retval) {
                NKDbgPrintfW (L"ERROR: function @ Ordinal %d missing\r\n", *ltptr & 0x7fffffff);
            }

        } else {
            PCImpProc impptr = (PCImpProc) (*ltptr + dwImpBase);
            retval = ResolveImpHintStr (pMod, expptr, impptr->ip_hint, (LPCSTR)impptr->ip_name);
            if (!retval) {
                NKDbgPrintfW (L"ERROR: could not resolve import %S\r\n", (LPCSTR)impptr->ip_name);
            }
        }

        if (!retval) {
            // intentionally using NKDbgPrintfW here to let the message visible even in SHIP BUILD
            NKDbgPrintfW (L"!!! Please Check your SYSGEN variable !!!\r\n");
            DEBUGCHK (0);
            dwErr = ERROR_BAD_EXE_FORMAT;

        } else if (*atptr!= retval) {
            PREFAST_SUPPRESS (394, "bad access only occur when EXE/DLL is bad, and since we're in-proc, no security issue here");
            *atptr = retval;
        }
    }

    return dwErr;
}

static LPWSTR g_szImportFailureDllName;

const o32_lite *FindRWOptrByRva (const o32_lite *optr, int nCnt, DWORD dwRva)
{
    for ( ; nCnt; nCnt --, optr ++) {
        if ((optr->o32_flags & IMAGE_SCN_MEM_WRITE)
            && ((DWORD) (dwRva - optr->o32_rva) < optr->o32_vsize)) {
            return optr;
        }
    }

    return NULL;
}

static
BOOL
DoImports (
    PUSERMODULELIST pModCalling,
    DWORD  dwBaseAddr,
    PCInfo pImpInfo
    )
{
    DWORD   dwErr = 0;

    DEBUGMSG (DBGLOADER, (TEXT("DoImports: BaseAddr = %8.8lx\r\n"), dwBaseAddr));

    if (pImpInfo->size) {

        WCHAR       ucptr[MAX_DLLNAME_LEN];
        PCImpHdr    blockstart = (PCImpHdr) (dwBaseAddr+pImpInfo->rva);
        PCImpHdr    blockptr;
        DWORD       dwOfstImportTable = 0;

        DEBUGMSG(DBGLOADER,(TEXT("DoImports: blockstart = %8.8lx, blocksize = %8.8lx, BaseAddr = %8.8lx\r\n"),
            blockstart, pImpInfo->size, dwBaseAddr));

#ifdef KCOREDLL
        // 'Z' flag modules has code/data split. Calculate the offset to get to import table
        PMODULE pKMod = (PMODULE) pModCalling;
        const o32_lite *optr = FindRWOptrByRva (pKMod->o32_ptr, pKMod->e32.e32_objcnt, blockstart->imp_address);

        DEBUGCHK (optr);
        dwOfstImportTable = optr->o32_realaddr - (dwBaseAddr + optr->o32_rva);
#endif

        for (blockptr = blockstart; !dwErr && blockptr->imp_lookup; blockptr++) {

            PUSERMODULELIST pMod = NULL;

            AsciiToUnicode (ucptr, (LPCSTR)dwBaseAddr+blockptr->imp_dllname, MAX_DLLNAME_LEN);
            pMod = DoLoadLibrary (ucptr, 0);

            if (!pMod) {
                dwErr = GetLastError();
                RETAILMSG (1, (L"DoImport Failed! Unable to import from Library '%s'. Last error: %d\r\n", ucptr, dwErr));
                break;
            }

            LPDWORD ltptr = (LPDWORD) (dwBaseAddr + blockptr->imp_lookup); // starting ltptr
            LPDWORD atptr = (LPDWORD) (dwBaseAddr + blockptr->imp_address + dwOfstImportTable); // starting atptr

            DEBUGMSG(DBGLOADER, (TEXT("Doing imports from module %a (pMod %8.8lx, ucptr = '%s')\r\n"),
                            dwBaseAddr+blockptr->imp_dllname,pMod, ucptr));

            dwErr = ImportOneBlock (pMod, dwBaseAddr, ltptr, atptr);
            if (dwErr) {
                RETAILMSG (1, (TEXT("!!!Import from DLL '%s' failed. Last error: %d\r\n"), ucptr, dwErr));
            }

            DEBUGMSG(!dwErr && DBGLOADER, (TEXT("Done imports from module %a\r\n"), dwBaseAddr+blockptr->imp_dllname));
        }


        if (dwErr) {
            // record the DLL name that caused the failure.

            // allocation on demand as 520 bytes on every EXE is kind of too big.
            // NOTE: we have loader CS held here so we don't need to worry about race.
            if (!g_szImportFailureDllName) {
                g_szImportFailureDllName = (LPWSTR) LocalAlloc (LMEM_FIXED, sizeof (ucptr));
            }
            if (g_szImportFailureDllName) {
                // NOTE: This will be the top level DLL name that caused the failure (The recursive
                //       import will end up overwriting the name with the top level name).
                VERIFY (SUCCEEDED (StringCchCopyW (g_szImportFailureDllName, MAX_DLLNAME_LEN, ucptr)));
            }

            // free all the library we loaded.
            while (blockstart != blockptr) {
                AsciiToUnicode (ucptr, (LPCSTR)dwBaseAddr+blockstart->imp_dllname, MAX_DLLNAME_LEN);
                FreeLibraryByName (ucptr);
                blockstart++;
            }
            SetLastError (dwErr);
        }
        else {
            if ((NULL == pModCalling) || (pModCalling->wFlags & MF_APP_VERIFIER))
            {
                AppVerifierDoImports (pModCalling, dwBaseAddr, pImpInfo, FALSE);
            }
        }
    }

    return !dwErr;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
static
void
UnDoDepends (
    PUSERMODULELIST pMod
    )
{
    ulong       blocksize;
    WCHAR       ucptr[MAX_DLLNAME_LEN];
    PCImpHdr    blockptr;
    DWORD       dwBaseAddr = pMod->dwBaseAddr;
    PCInfo      pImpInfo = GetImportInfo (pMod);

    if (MF_IMPORTED & pMod->wFlags) {

        // DllMain (PROCESS_ATTACH) return FALSE if MF_LOADING and MF_IMPORTED are both set.
        // don't call DllMain in this case
        if (!(MF_LOADING & pMod->wFlags)) {
            CallDllMain (pMod, DLL_PROCESS_DETACH);
        }

        blocksize = pImpInfo->size;
        if (blocksize) { // Has imports?
            blockptr = (PCImpHdr) (dwBaseAddr+pImpInfo->rva);
            while (blockptr->imp_lookup) {
                AsciiToUnicode (ucptr, (LPCSTR) dwBaseAddr+blockptr->imp_dllname, MAX_DLLNAME_LEN);
                FreeLibraryByName (ucptr);
                blockptr++;
            }
        }
        pMod->wFlags &= ~MF_IMPORTED;

        AppVerifierUnDoDepends (pMod);
    }
}

extern BOOL DllHeapDoImports (PMODULE pKMod, DWORD  dwBaseAddr, PCInfo pImpInfo);

static PUSERMODULELIST DoImportAndCallDllMain (PUSERMODULELIST pMod)
{
    if (pMod
        && !((MF_IMPORTED|DONT_RESOLVE_DLL_REFERENCES|MF_IMPORTING) & pMod->wFlags)) {

#ifdef KCOREDLL
        PMODULE pKMod = (PMODULE) pMod;
#else
        PMODULE pKMod = NULL;
#endif
        pMod->wFlags |= MF_IMPORTING;

#ifdef KCOREDLL
        // The kernel calls this function directly (does not go through DoLoadLibrary),
        // so we need to call into the App Verifier engine here instead.
        AppVerifierDoLoadLibrary (pMod, NULL);
#endif

        if (DoImports (pMod, pMod->dwBaseAddr, GetImportInfo (pMod))) {
            pMod->wFlags |= MF_IMPORTED;
        }

        pMod->wFlags &= ~MF_IMPORTING;
        DEBUGMSG (DBGLOADER, (L"Import Done for pMod = %8.8lx, wFlags = %8.8lx\r\n", pMod, pMod->wFlags));

        // NOTE: DoImports will recursively call DoLoadLibrary. It's possible
        //       that we imports the pMod becomes fully loaded during the import.
        //       Therefore we need to check the flag again here.


        if ((((MF_IMPORTED|MF_LOADING) & pMod->wFlags) == (MF_IMPORTED|MF_LOADING))
            && CallDllMain (pMod, DLL_PROCESS_ATTACH)) {
                pMod->wFlags &= ~MF_LOADING;

                // dllheap or heaptag override for imports
                DllHeapDoImports(pKMod, pMod->dwBaseAddr, GetImportInfo (pMod));
        }

        if (MF_LOADING & pMod->wFlags) {
            // failed to load due to Import failure, or DllMain return FALSE. Last error already set
            DWORD dwErr = GetLastError ();
            DoFreeLibrary (pMod);
            pMod = NULL;
            SetLastError (dwErr);
        }
    }
    // not importing, so remove from the AppVerifier module list now
    if (pMod && (DONT_RESOLVE_DLL_REFERENCES & pMod->wFlags)) {
        AppVerifierUnDoDepends (pMod);
    }
    return pMod;
}

static FARPROC DoGetProcAddressA (PUSERMODULELIST pMod, LPCSTR pszFuncName)
{
    DWORD    retval = 0;
    PCInfo   pExpInfo = GetExportInfo (pMod);

    if (pExpInfo->rva // module has export section
        && ((pMod->wFlags & MF_IMPORTED) // module is loaded without any flags
        ||((pMod->wFlags & DONT_RESOLVE_DLL_REFERENCES)  // module loaded with not resolving imports
        && !(pMod->wFlags & LOAD_LIBRARY_AS_DATAFILE)))) { // module is not loaded as a datafile
        PCExpHdr expptr = (PCExpHdr) (pMod->dwBaseAddr + pExpInfo->rva);

        retval = ((DWORD) pszFuncName >> 16)
            ? ResolveImpStr (pMod, expptr, pszFuncName)
            : ResolveImpOrd (pMod, expptr, (DWORD) pszFuncName);
    }
    if (!retval) {
        SetLastError (ERROR_INVALID_PARAMETER);
    }

    return (FARPROC) retval;
}

//------------------------------------------------------------------------------
//
// Exported functions
//
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
// GetProcAddressA
//------------------------------------------------------------------------------
extern "C"
FARPROC
WINAPI
GetProcAddressA (
    HMODULE hInst,
    LPCSTR  pszFuncName
    )
{
    FARPROC pfn = NULL;
    LockLoader ();
    PUSERMODULELIST pMod = FindModule (hInst);

    if (!pMod) {
        SetLastError (ERROR_INVALID_PARAMETER);

    } else {

        pfn = DoGetProcAddressA (pMod, pszFuncName);
    }
    UnlockLoader ();
    return pfn;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
FARPROC
WINAPI
GetProcAddressW (
    HMODULE hInst,
    LPCWSTR pszFuncName
    )
{
    CHAR   szImpName[MAX_IMPNAME_LEN];
    LPCSTR pAsciiName = (LPCSTR) pszFuncName;

    if ((DWORD)pszFuncName >> 16) {
        UnicodeToAscii (szImpName, pszFuncName, MAX_IMPNAME_LEN);
        pAsciiName = szImpName;
    }

    return GetProcAddressA (hInst, pAsciiName);
}



//------------------------------------------------------------------------------
// Return the address of a function entry within a given module and a process
// (can be called only from kernel mode); when called from user mode process,
// this will return NULL.
//------------------------------------------------------------------------------
extern "C"
FARPROC
GetProcAddressInProcess (
    HANDLE hProcess,
    LPCWSTR  pszModuleName,
    LPCWSTR  pszFuncName
    )
{
    FARPROC pfn = NULL;
    CHAR   szImpName[MAX_IMPNAME_LEN];
    LPCSTR pAsciiName = (LPCSTR) pszFuncName;
    PUSERMODULELIST pMod = NULL;

    if ((DWORD)pszFuncName >> 16) {
        UnicodeToAscii (szImpName, pszFuncName, MAX_IMPNAME_LEN);
        pAsciiName = szImpName;
    }

    LockLoader ();
    pMod = FindModuleByNameInProcess(hProcess, pszModuleName, 0);

    if (!pMod) {
        SetLastError (ERROR_INVALID_PARAMETER);

    } else {

        pfn = DoGetProcAddressA (pMod, pAsciiName);
    }
    UnlockLoader ();
    return pfn;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
extern "C"
BOOL
DisableThreadLibraryCalls (
    HMODULE hInst
    )
{
    PUSERMODULELIST pMod = NULL;
    LockLoader ();
    pMod = FindModule (hInst);
    if (pMod) {
        pMod->wFlags |= MF_NO_THREAD_CALLS;
    }
    UnlockLoader ();
    return NULL != pMod;
}


#if defined(x86)
// Turn off FPO optimization for base functions so that debugger can correctly unwind retail x86 call stacks
#pragma optimize("y",off)
#endif

extern "C" void ShowImportFailure (LPCWSTR pszExe, LPCWSTR pszDll);

extern "C"
void
MainThreadBaseFunc(
    LPVOID      pfn,
    LPCWSTR     pszProgName,
    LPCWSTR     pszCmdLine,
    HINSTANCE   hCoreDll,
    HINSTANCE   hMUIExe,
    HINSTANCE   hMUICoreDll
    )
{

#ifndef KCOREDLL
    DWORD dwExitCode = 0;
    e32_lite e32;

    g_dwMainThId = GetCurrentThreadId();

    g_pszCmdLine = pszCmdLine;
    g_pszProgName = pszProgName;

    if (!CoreDllInit (hCoreDll, DLL_PROCESS_ATTACH, 0)                              // coredll init failed
        || !ReadPEHeader (hActiveProc, RP_READ_PE_BY_MODULE, 0, &e32, sizeof (e32))) { // failed to read PE header
        RETAILMSG (1, (TEXT("!! Process Initialization failed - Process '%s' not started!!"), g_pszProgName));

    } else {

#ifdef x86
        // disable DEP if this EXE doesn't indicate support
        if (!(e32.e32_unit[EXC].rva & E32_EXC_DEP_COMPAT)) {
            g_fDepEnabled = FALSE;
        }
#endif

        LockLoader ();

        g_pExeMUI   = InitMUI (hMUIExe);
        g_mlCoreDll.pmodResource = InitMUI (hMUICoreDll);
        g_resExe    = e32.e32_unit[RES];
        g_impExe = & e32.e32_unit[IMP];
        g_dwExeBase = e32.e32_vbase;

        dwExitCode = e32.e32_sect14rva;
        if (dwExitCode) {
            // managed code, load mscoree, don't resolve imports
            PUSERMODULELIST pMod = DoLoadLibrary (L"mscoree.dll", 0);
            if (!pMod || (NULL == (pfn = DoGetProcAddressA (pMod, "_CorExeMain")))) {
                RETAILMSG (1, (TEXT("!! Loading mscoree failed for managed code - Process '%s' not started!!"), g_pszProgName));
                dwExitCode = 0;
            }

        } else {
            dwExitCode = DoImports (NULL, e32.e32_vbase, &e32.e32_unit[IMP]);
            if (!dwExitCode) {
                RETAILMSG (1, (TEXT("!! Process Import failed - Process '%s' not started!!"), g_pszProgName));
            } else {
                // dllheap or heaptag override for imports
                DllHeapDoImports(NULL, e32.e32_vbase, &e32.e32_unit[IMP]);
            }            
        }
        UnlockLoader ();

        if (dwExitCode) {

            // notify debugger process created
            DebugNotify (DLL_PROCESS_ATTACH, (DWORD)pfn);

            // inject DLLs if needed.
            InjectDlls ();

            Imm_DllEntry(hInstCoreDll, DLL_PROCESS_ATTACH, 0);

            RegisterDlgClass();

            dwExitCode = e32.e32_sect14rva
                ? ((comthread_t) pfn) ((HINSTANCE) GetCurrentProcessId (), 0, pszCmdLine, SW_SHOW, e32.e32_vbase, e32.e32_sect14rva, e32.e32_sect14size)
                : ((mainthread_t)pfn) ((HINSTANCE) GetCurrentProcessId (), 0, pszCmdLine, SW_SHOW);

        } else if (g_szImportFailureDllName) {
            ShowImportFailure (pszProgName, g_szImportFailureDllName);
        }
    }

    /* ExitThread stops execution of the current thread */
    ExitThread (dwExitCode);

#endif
}

extern "C"
void
ThreadBaseFunc (
    LPTHREAD_START_ROUTINE  pfn,
    LPVOID  param
    )
{
    DWORD retval = 0;

    InitCRTStorage ();

#ifndef KCOREDLL
    Imm_DllEntry (hInstCoreDll, DLL_THREAD_ATTACH, 0);
    DebugNotify (DLL_THREAD_ATTACH, (DWORD)pfn);
    ThreadNotifyDLLs (DLL_THREAD_ATTACH);
#endif

    retval = (* pfn) (param);
    ExitThread(retval);
    /* ExitThread stops execution of the current thread */
}

#if defined(x86)
// Re-Enable optimization
#pragma optimize("",on)
#endif

/* Terminate thread routine */

/*
    @doc BOTH EXTERNAL

    @func VOID | ExitThread | Ends a thread.
    @parm DWORD | dwExitCode | exit code for this thread

    @comm Follows the Win32 reference description with the following restriction:
            If the primary thread calls ExitThread, the application will exit

*/
extern "C"
VOID
WINAPI
ExitThread (
    DWORD dwExitCode
    )
{
    if (g_csProc.hCrit) {
        ReleaseProcCS ();
    }


#ifdef KCOREDLL
    ClearCRTStorage ();
    CeFreeRwTls (0, (LPVOID)UTlsPtr()[g_dwLockSlot]);

#else
    if (g_fInitDone) {

        BOOL fIsMainThread = (GetCurrentThreadId () == g_dwMainThId);

        if (fIsMainThread) {
            IsExiting = 1;
        }

        PrepareThreadExit (dwExitCode);

        //
        // NOTE: We no longer notify DLLs of therad exiting when process is exiting for 2 reasons:
        //      (1) It can cause deadlock - e.g. if heap corrupted, we except while holding heap CS.
        //          Then we'll trying to acquire loader CS for DLL notification. If there is another
        //          thread in the middle of DLL notification doing heap allocation, we deadlock.
        //      (2) As process is going away, thread storage are all going away anyway. Notifying
        //          DLLs of thread exiting is redundent.
        //
        if (!IsExiting) {
            ThreadNotifyDLLs (DLL_THREAD_DETACH);
            Imm_DllEntry (hInstCoreDll, DLL_THREAD_DETACH, 0);
            CeTlsThreadExit ();
            ClearCRTStorage ();
        } else if (fIsMainThread) {
            ProcessDetach ();
            CoreDllInit (hInstCoreDll, DLL_PROCESS_DETACH, 1);
        }
    }
#endif


    // set last erro to exit code
    SetLastError(dwExitCode);

    // call to NK for final cleanup
    NKExitThread (dwExitCode);
}


extern "C"
VOID
WINAPI
ThreadExceptionExit (
    DWORD dwExcpCode,
    DWORD dwExcpAddr
    )
{

    DEBUGCHK (GetCurrentProcessId () == (DWORD) GetOwnerProcess ());

    RETAILMSG (1, (L"%s thread in proc %8.8lx faulted, Exception code = %8.8lx, Exception Address = %8.8x!\r\n",
        IsPrimaryThread ()? L"Main" : L"Secondary", GetCurrentProcessId(), dwExcpCode, dwExcpAddr));

#ifndef KCOREDLL

    static BOOL fErrShown = FALSE;
    if (IsExiting) {
        // faulted in process exit, just exit thread directly
        NKExitThread (dwExcpCode);
        DEBUGCHK (0);
    }

    // don't bother trying to show dialog if coredll isn't even initialized.
    if (g_fInitDone && !fErrShown) {
        fErrShown = TRUE;
        ShowErr (dwExcpCode, dwExcpAddr);
    }
    if (GetCurrentProcessId () == (DWORD) GetOwnerProcess ()) {
        SetAbnormalTermination ();
        if (!IsPrimaryThread ()) {
            RETAILMSG(1,(L"Terminating process %8.8lx (%s)!\r\n", GetCurrentProcessId(), GetProcName ()));
            ExitProcess (dwExcpCode);
        }
    }
#else
    RETAILMSG (1, (L"Terminating Thread %8.8lx\r\n", GetCurrentThreadId ()));
#endif

    ExitThread (dwExcpCode);

}

extern "C"
BOOL
IsProcessDying (
    void
    )
{
    return IsExiting;
}

extern "C"
BOOL
IsPrimaryThread(
    void
    )
{
#ifdef KCOREDLL
    return FALSE;
#else
    return GetCurrentThreadId () == g_dwMainThId;
#endif
}


extern "C" HMODULE WINAPI stub_LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);

extern "C"
HMODULE
WINAPI
LoadLibraryExW (
    LPCWSTR lpLibFileName,
    HANDLE hFile,
    DWORD dwFlags
    )
{
    return stub_LoadLibraryExW (lpLibFileName, hFile, dwFlags);
}


extern "C"
HMODULE
WINAPI
int_LoadLibraryExW(
    LPCWSTR pszFileName,
    HANDLE  hFile,
    DWORD   dwFlags
    )
{

    PUSERMODULELIST pMod = NULL;

    // calling LoadLibrary during DLL_PROCESS_DETACH is a no-no
    DEBUGCHK (!IsExiting);

    if (hFile || IsExiting) {
        SetLastError (ERROR_INVALID_PARAMETER);

    } else {

        LockLoader ();
        pMod = DoLoadLibrary (pszFileName, dwFlags);
        UnlockLoader ();
    }

    DEBUGMSG (DBGLOADER, (L"IntLoadLibraryExW ('%s', %8.8lx) returns %8.8lx\r\n", pszFileName, dwFlags, pMod));
    return pMod? pMod->hModule : NULL;

}

extern "C"
HINSTANCE
WINAPI
LoadLibraryW (
    LPCWSTR lpLibFileName
    )
{
    return LoadLibraryExW (lpLibFileName, 0, 0);
}


extern "C"
HINSTANCE
LoadDriver(
    LPCTSTR lpszFile
    )
{
    return LoadLibraryExW (lpszFile, NULL, MAKELONG (0, LLIB_NO_PAGING));
}

extern "C"
BOOL
WINAPI
FreeLibrary (
    HMODULE hMod
    )
{
#ifdef KCOREDLL
    return DoFreeLibrary (hMod);
#else
    BOOL  fRet = FALSE;

    // when we're exiting, don't bother freeing the library
    // since they're going to be freed automatically
    if (!IsExiting) {
        PUSERMODULELIST pMod;
        LockLoader ();
        pMod = FindModule (hMod);
        if (pMod) {
            fRet = DoFreeLibrary (pMod);
        }
        UnlockLoader ();
    }
    return fRet;
#endif
}

extern "C"
LPWSTR
WINAPI
GetCommandLineW (
    VOID
    )
{
    return (LPWSTR) g_pszCmdLine;
}

extern "C"
LPCWSTR
GetProcName(
    void
    )
{
    return g_pszProgName;
}


extern "C"
VOID
FreeLibraryAndExitThread (
    HMODULE hLibModule,
    DWORD dwExitCode
    )
{
    FreeLibrary (hLibModule);
    ExitThread (dwExitCode);
}


