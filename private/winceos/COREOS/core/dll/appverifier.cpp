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
#include <coredll.h>
#include <loader_core.h>

extern "C" BOOL xxx_KernelLibIoControl (HANDLE hLib, DWORD dwIoControlCode, LPVOID lpInBuf, DWORD nInBufSize, LPVOID lpOutBuf, DWORD nOutBufSize, LPDWORD lpBytesReturned);

PUSERMODULELIST EnumerateModules (PFN_DLISTENUM pfnEnum, LPVOID pEnumData);

extern LPCWSTR g_pszProgName;
extern LPCWSTR g_pszCmdLine;

extern DWORD       g_dwExeBase;
extern PCInfo g_impExe;

#ifndef KCOREDLL
extern USERMODULELIST   g_mlCoreDll;
#endif

// In each process, keep a list of modules that are shimmed, along with the
// shims that are applied to that module. When a module is freed, we'll need to
// also free the shim dll's applied to that module.

typedef struct _SHIM_INFO {
    PUSERMODULELIST pMod;
    LPTSTR szName;
    LPTSTR pFunctionTable;
    struct _SHIM_INFO *pNext;
} SHIM_INFO, *LPSHIM_INFO;

typedef struct _MODULE_INFO {
    LPTSTR szName;
    BOOL bIsAppVerifier;
    BOOL bReceivesNotifications;
    PUSERMODULELIST pMod;
    info impInfo;
    LPSHIM_INFO pShimInfo;
    struct _MODULE_INFO *pNext;
    struct _MODULE_INFO *pPrev;
} MODULE_INFO, *LPMODULE_INFO;

LPMODULE_INFO process_modules;
BOOL loading_shim;
BOOL g_bIsAppVerifierActive;

typedef LPTSTR (*PFN_GETFUNCTIONTABLE)(VOID);
typedef void (*PFN_SHIMNOTIFY)(HMODULE hModule, DWORD dwReason);

// --------------------------------------------------------------------------------
//  Print formatted output to a buffer (varargs)

extern "C"
int
#ifdef KCOREDLL
(* NKwvsprintfW)
#else
NKwvsprintfW
#endif
    (
    LPWSTR lpOut,
    LPCWSTR lpFmt,
    va_list lpParms,
    int maxchars
    );

static
void
AvPrintfW(
    LPWSTR szDest,
    DWORD cchStringSize,
    LPCWSTR szFormat,
    ...
    )
{
    va_list va;
    va_start(va, szFormat);
    NKwvsprintfW(szDest, szFormat, va, cchStringSize);
    va_end(va);
}

#if defined (UNICODE)
#define AvPrintf AvPrintfW
#endif

// --------------------------------------------------------------------------------
//

BOOL
IsAppVerifierEnabled (
    void
    )
{
    BOOL fRet;
    BOOL fEnabled;
    HKEY hKey;
    DWORD dwLastErr;

    // Call into the kernel to see if the global flag is set.
    fRet = xxx_KernelLibIoControl (
        (HANDLE) KMOD_APPVERIF,
        IOCTL_APPVERIF_ENABLE,
        NULL,
        0,
        & fEnabled,
        sizeof (BOOL),
        NULL
        );

    if (fRet)
    {
        return fEnabled;
    }

    // The ioctl failed, so fall back to checking the registry for a global key.

    // Preserve last error
    dwLastErr = GetLastError ();

    // Default.
    fEnabled = FALSE;

    if (WAIT_OBJECT_0 == WaitForAPIReady (SH_FILESYS_APIS, 0))
    {
        if (ERROR_SUCCESS == RegOpenKeyEx (HKEY_LOCAL_MACHINE, _T("ShimEngine"), 0, KEY_READ, & hKey))
        {
            fEnabled = TRUE;

            RegCloseKey (hKey);
        }
    }
    else
    {
        fEnabled = TRUE;
    }

    SetLastError (dwLastErr);

    return fEnabled;
}

BOOL
IsAppVerifierBinary (
    PUSERMODULELIST pMod
    )
{
    TCHAR szBuffer [MAX_PATH];

    if (LoadString (pMod->hModule, 1, szBuffer, MAX_PATH))
    {
        return (0 == _tcscmp (_T("AppVerifier"), szBuffer));
    }
    else if (_tcsstr (g_pszProgName, _T("appverif.exe")))
    {
        return TRUE;
    }

    return FALSE;
}

LPCTSTR
PlainNamePart2 (
    LPCTSTR pszFileName,
    BOOL fAllowKPrefix
    )
{
    LPCTSTR p = _tcsrchr (pszFileName, _T('\\'));
    p = p ? (p + 1) : pszFileName;
    return fAllowKPrefix ? p : ((p[0] == _T('k')) && (p[1] == _T('.'))) ? p + 2 : p;
}

LPCTSTR
PlainName (
    LPCTSTR pszFileName
    )
{
    return PlainNamePart2 (pszFileName, FALSE);
}

LPTSTR
GetFunctionTable (
    HMODULE hMod
    )
{
    PFN_GETFUNCTIONTABLE pfnGetFunctionTable;
    LPTSTR pRet = NULL;

    // Call into the shim, and let it report at run time the functions it will replace.
    pfnGetFunctionTable = (PFN_GETFUNCTIONTABLE) GetProcAddressW (hMod, _T("GetFunctionTable"));
    if (pfnGetFunctionTable)
    {
        pRet = pfnGetFunctionTable ();
    }

    // If the shim was not able to report its function table programmatically,
    // try the 'old' way (parsing its resources).
    return pRet ?
        pRet :
        (LPTSTR) LoadResource (hMod, FindResource (hMod, MAKEINTRESOURCE(65024), RT_RCDATA));
}

LPSHIM_INFO
LoadShim (
    LPCTSTR pszShimName
    )
{
    DEBUGMSG (DBGLOADER, (_T("LoadShim: '%s'\r\n"), pszShimName));

    g_bIsAppVerifierActive = TRUE;

    LPSHIM_INFO pShimInfo = (LPSHIM_INFO) LocalAlloc (LMEM_ZEROINIT, sizeof (SHIM_INFO));

    if (pShimInfo)
    {
        loading_shim = TRUE;
        pShimInfo->pMod = DoLoadLibrary (pszShimName, 0);
        loading_shim = FALSE;

        if (pShimInfo->pMod)
        {
            pShimInfo->pFunctionTable = GetFunctionTable (pShimInfo->pMod->hModule);

            if (pShimInfo->pFunctionTable)
            {
                pShimInfo->szName = _tcsdup (PlainName (pszShimName));

                return pShimInfo;
            }

            // This isn't a valid shim; unload it.
            RETAILMSG (1, (_T("ERROR: '%s' is not a valid shim.\r\n"), pszShimName));
            FreeLibrary (pShimInfo->pMod->hModule);
        }
        else
        {
            RETAILMSG (1, (_T("ERROR: could not load '%s' %u\r\n"), pszShimName, GetLastError ()));
        }

        // Don't need this anymore...
        LocalFree (pShimInfo);
    }

    // Error.
    return NULL;
}

void
FreeShim (
    LPSHIM_INFO pShimInfo
    )
{
    DEBUGMSG (DBGLOADER, (_T("Freeing '%s'\r\n"), pShimInfo->szName));

    loading_shim = TRUE;
    FreeLibrary (pShimInfo->pMod->hModule);
    loading_shim = FALSE;
    LocalFree (pShimInfo->szName);
    LocalFree (pShimInfo);
}

BOOL
DoesShimProvideThisFunction (
    LPSHIM_INFO pShimInfo,
    LPCWSTR szExportingDll,
    DWORD ord,
    DWORD BaseAddr // of importing dll
    )
{
    // Each shim is required to contain an RCDATA block (65024) with a series of
    // strings representing the functions it provides. The strings are formatted as
    // REPLACING.DLL-ORDINAL or REPLACING.DLL-NAME. Walk through the terminated
    // strings to see if this shim dll provides the requested function.

    if (!pShimInfo || !pShimInfo->pFunctionTable)
    {
        return FALSE;
    }

    WCHAR szMatchString [MAX_PATH];

    if (ord & 0x80000000)
    {
        AvPrintfW (szMatchString, MAX_PATH, L"%s-%d",
            szExportingDll,
            ord & 0x7fffffff
            );
    }
    else
    {
        AvPrintfW (szMatchString, MAX_PATH, L"%s-%S",
            szExportingDll,
            (LPCHAR)(((struct ImpProc *)(ord+BaseAddr))->ip_name)
            );
    }

    LPTSTR p = pShimInfo->pFunctionTable;

    while (p[0])
    {
        if (0 == _tcsicmp (p, szMatchString))
        {
            DEBUGMSG (DBGLOADER, (_T("Redirecting function pointer %s\r\n"), szMatchString));
            return TRUE;
        }
        p += (_tcslen (p) + 1);
    }

    // Return the export table of the shim dll.
    return FALSE;
}

LPSHIM_INFO
ShouldShimThisFunction (
    LPMODULE_INFO pModuleInfo, // The module being shimmed
    LPCWSTR ucptr, // The module exporting functions
    DWORD ord, // The ordinal (or string) of the exported function
    DWORD dwBaseAddr
    )
{
    // Walk through the shims applied to this module.
    for (LPSHIM_INFO pShimInfo = pModuleInfo->pShimInfo; pShimInfo; pShimInfo = pShimInfo->pNext)
    {
        if (DoesShimProvideThisFunction (pShimInfo, ucptr, ord, dwBaseAddr))
        {
            // Return the export table for the shim dll providing the new functions.
            return pShimInfo;
        }
    }

    return NULL;
}

HKEY
OpenShimKey (
    LPCTSTR pszModuleName,
    BOOL fKernelPrefix
    )
{
    HKEY hKey;
    TCHAR szSubKey [MAX_PATH];
    LONG lRet;

    if (!pszModuleName)
    {
        return NULL;
    }

    AvPrintf (szSubKey, MAX_PATH,
        _T("ShimEngine\\%s%s%s"),
        fKernelPrefix ? _T("k.") : _T(""),
        (pszModuleName && pszModuleName[0]) ? pszModuleName : _T("nk.exe"),
        (_tcschr (pszModuleName, _T('.')) || _tcschr (pszModuleName, _T('{'))) ? _T("") : _T(".dll")
        );

    lRet = RegOpenKeyEx (HKEY_LOCAL_MACHINE, szSubKey, 0, KEY_READ, & hKey);

    return (ERROR_SUCCESS == lRet) ? hKey : NULL;
}

LPSHIM_INFO
LoadShimsFromRegistry (
    HKEY hKey,
    LPMODULE_INFO pModuleInfo
    )
{
    LPSHIM_INFO pShimTrav = NULL;
    LPSHIM_INFO pShimRet = NULL;
    LPSHIM_INFO pShimTemp;
    DWORD dwIndex;
    TCHAR szName [MAX_PATH];
    DWORD cchName;
    BOOL fAlreadyLoaded;

    dwIndex = 0;
    cchName = MAX_PATH;

    // Each key under this hKey names a shim that is to be applied to this module.
    // Enumerate the keys, loading each shim. Return the list of shims that have
    // been loaded.

    while (ERROR_SUCCESS == RegEnumKeyEx (hKey,
        dwIndex,
        szName,
        & cchName,
        NULL,
        NULL,
        NULL,
        NULL))
    {
        fAlreadyLoaded = FALSE;

        // Make sure we haven't already loaded this shim for this module.
        for (pShimTemp = pModuleInfo->pShimInfo; pShimTemp; pShimTemp = pShimTemp->pNext)
        {
            if (pShimTemp->szName && !_tcsicmp (pShimTemp->szName, PlainName (szName)))
            {
                fAlreadyLoaded = TRUE;
                break;
            }
        }

        if (!fAlreadyLoaded)
        {
            pShimTemp = LoadShim (szName);
            if (pShimTemp)
            {
                if (pShimTrav)
                {
                    // We've already started the list, add this to the end.
                    pShimTrav->pNext = pShimTemp;
                    pShimTrav = pShimTemp;
                }
                else
                {
                    // Start the list.
                    pShimTrav = pShimTemp;
                    pShimRet = pShimTrav;
                }
            }
        }

        dwIndex ++;
        cchName = MAX_PATH;
    }

    return pShimRet;
}

LPSHIM_INFO
MergeShimInfoLists (
    LPSHIM_INFO pShimInfo,
    LPSHIM_INFO pShimInfoAll
    )
{
    LPSHIM_INFO pTrav;

    // Is there only one list?
    if (NULL == pShimInfo) return pShimInfoAll;
    if (NULL == pShimInfoAll) return pShimInfo;

    // Walk the pShimInfo list.
    for (pTrav = pShimInfo; pTrav; pTrav = pTrav->pNext)
    {
        // Look for the end of the list.
        if (NULL == pTrav->pNext)
        {
            // Tack on the pShimInfoAll list.
            pTrav->pNext = pShimInfoAll;
            return pShimInfo;
        }
    }

    return NULL;
}

LPSHIM_INFO
LoadAllShims (
    LPMODULE_INFO pModuleInfo
    )
{
    // HKLM\ShimEngine\module.ext\shim_name.dll
    // module.ext search order:
    // 0. Kernel-prefixed pModuleInfo->szName
    // 1. pModuleInfo->szName
    // 2. g_szProgName.exe (append .exe if needed)
    // 3. if pModuleInfo->szName does not have an extension:
    // 3a. .exe, .dll, .cpl
    // 3b. others? does the kernel allow any extension?
    // 4. {all}

    // Do we even want to bother with extensions anymore? Do we strip them off and
    // treat foo.exe, foo.dll and foo.cpl as the same?

    if (WAIT_OBJECT_0 == WaitForAPIReady (SH_FILESYS_APIS, 0))
    {
        // The file system is up and running. Look in the registry.
        LPSHIM_INFO pShimInfoMod = NULL;
        LPSHIM_INFO pShimInfoAll = NULL;
        HKEY hKey;

#ifdef KCOREDLL
        hKey = OpenShimKey (pModuleInfo->szName, TRUE);
        if (!hKey)
        {
#endif
            hKey = OpenShimKey (pModuleInfo->szName, FALSE);
            if (!hKey)
            {
                hKey = OpenShimKey (g_pszProgName, FALSE);
                if (hKey)
                {
                    // What about wanting to shim only foo.exe, and not the dll's?
                    DWORD dwType;
                    BOOL fShimProcessDlls = TRUE;
                    DWORD cbData = sizeof (DWORD);

                    // Query hKey ShimProcessDlls - default will be 1
                    RegQueryValueEx (hKey, _T("ShimProcessDlls"), NULL, & dwType, (LPBYTE) & fShimProcessDlls, & cbData);
                    if (!fShimProcessDlls)
                    {
                        RegCloseKey (hKey);
                        hKey = NULL;

                        // Don't return here - we still might need to shim this because
                        // of an {all} rule.
                    }
                }
            }
#ifdef KCOREDLL
        }
#endif

        if (hKey)
        {
            pShimInfoMod = LoadShimsFromRegistry (hKey, pModuleInfo);
            RegCloseKey (hKey);
        }

        // Always load shims specified in {all}
        hKey = OpenShimKey (_T("{all}"), FALSE);
        if (hKey)
        {
            pShimInfoAll = LoadShimsFromRegistry (hKey, pModuleInfo);
            RegCloseKey (hKey);
        }

        pShimInfoMod = MergeShimInfoLists (pShimInfoMod, pShimInfoAll);

        // Merge in the list of previously-loaded shims.
        pShimInfoMod = MergeShimInfoLists (pShimInfoMod, pModuleInfo->pShimInfo);

        // Return a new, complete list.
        return pShimInfoMod;
    }
    else
    {
        // TODO - We need to be able to shim filesys.dll.
        DEBUGMSG (DBGLOADER, (_T("Filesys/registry not ready yet\r\n")));
        return NULL;
    }
}

// ----------
// Keep a list of modules in this process that are being monitored by app verifier.

LPMODULE_INFO
GetModuleInfo (
    DWORD dwBaseAddr
    )
{
    LPMODULE_INFO pModuleInfo;

    // Is this module already shimmed?
    // Look through process_modules, is this module shimmed?
    for (pModuleInfo = process_modules; pModuleInfo; pModuleInfo = pModuleInfo->pNext)
    {
        if (pModuleInfo->pMod && (pModuleInfo->pMod->dwBaseAddr == dwBaseAddr))
        {
            return pModuleInfo;
        }
    }

    return NULL;
}

LPMODULE_INFO
AddModuleInfo (
    LPCWSTR pszFileName,
    PUSERMODULELIST pMod
    )
{
    LPMODULE_INFO pModuleInfo;

    // Do we already have info on this module?
    pModuleInfo = GetModuleInfo (pMod->dwBaseAddr);
    if (pModuleInfo)
    {
        return pModuleInfo;
    }

    if (NULL == process_modules)
    {
        // Initialize the list of modules with an empty node.
        process_modules = (LPMODULE_INFO) LocalAlloc (LMEM_ZEROINIT, sizeof (MODULE_INFO));
    }

    if (NULL == process_modules)
    {
        return NULL;
    }

    pModuleInfo = (LPMODULE_INFO) LocalAlloc (LMEM_ZEROINIT, sizeof (MODULE_INFO));
    if (pModuleInfo)
    {
        pModuleInfo->szName = _tcsdup (PlainNamePart2 (pszFileName, TRUE));
        pModuleInfo->pMod = pMod;

        // Link in this node.
        pModuleInfo->pPrev = process_modules;
        pModuleInfo->pNext = process_modules->pNext;

        if (process_modules->pNext) process_modules->pNext->pPrev = pModuleInfo;
        process_modules->pNext = pModuleInfo;

        return pModuleInfo;
    }

    return NULL;
}

void
FreeModuleInfo (
    LPMODULE_INFO pModuleInfo
    )
{
    // Unlink this node.
    if (pModuleInfo->pPrev) pModuleInfo->pPrev->pNext = pModuleInfo->pNext;
    if (pModuleInfo->pNext) pModuleInfo->pNext->pPrev = pModuleInfo->pPrev;

    // Don't do anything with the list of shims; this funclet is just for cleanup.

    LocalFree (pModuleInfo->szName);
    LocalFree (pModuleInfo);
}

// ----------
// Keep a list of all modules loaded in this process, so we can associate module name and pMod.

BOOL
GetNameFromBaseAddr (
    __in_ecount (cchModuleName) LPTSTR szModuleName,
    DWORD cchModuleName,
    DWORD dwBaseAddr,
    PUSERMODULELIST *ppModList
    )
{
    szModuleName[0] = _T('\0');

    for (LPMODULE_INFO pModuleInfo = process_modules; pModuleInfo; pModuleInfo = pModuleInfo->pNext)
    {
        if (pModuleInfo->pMod && pModuleInfo->szName && (pModuleInfo->pMod->dwBaseAddr == dwBaseAddr))
        {
            // Found it.
            StringCchCopy (szModuleName, cchModuleName, pModuleInfo->szName);

            if (ppModList)
            {
                *ppModList = pModuleInfo->pMod;
            }

            return TRUE;
        }
    }

    return FALSE;
}

BOOL
IsDependent (
    PUSERMODULELIST pMod,
    PUSERMODULELIST pMod2
    )
{
    // Does pMod call into pMod2?
    LPMODULE_INFO pModInfo1;
    LPMODULE_INFO pModInfo2;

    pModInfo1 = GetModuleInfo (pMod->dwBaseAddr);
    pModInfo2 = GetModuleInfo (pMod2->dwBaseAddr);

    if ((NULL == pModInfo1) || (NULL == pModInfo2))
    {
        return FALSE;
    }

    // Make sure both are shims.
    if (!pModInfo1->bIsAppVerifier || !pModInfo2->bIsAppVerifier)
    {
        DEBUGMSG (DBGLOADER, (_T("ERROR: Shim list contains non-shim dll's\r\n")));
        return FALSE;
    }

    TCHAR szModName [MAX_PATH];
    GetNameFromBaseAddr (szModName, MAX_PATH, pMod->dwBaseAddr, NULL);

    TCHAR szMod2Name [MAX_PATH];
    if (!GetNameFromBaseAddr (szMod2Name, MAX_PATH, pMod2->dwBaseAddr, NULL))
    {
        DEBUGMSG (DBGLOADER, (_T("Unable to get pMod2 name\r\n")));
        return FALSE;
    }

    PCImpHdr    blockptr;
    WCHAR       ucptr[MAX_DLLNAME_LEN];
    DWORD dwBaseAddr = pMod->dwBaseAddr;
    PCInfo pImpInfo = GetImportInfo (pMod);

    for (
        blockptr = (PCImpHdr) (dwBaseAddr + pImpInfo->rva);
        blockptr->imp_lookup;
        blockptr++
        )
    {
        AsciiToUnicode (ucptr, (LPCSTR)dwBaseAddr+blockptr->imp_dllname, MAX_DLLNAME_LEN);
        DEBUGMSG (DBGLOADER, (_T("'%s' uses '%s', checking against '%s'\r\n"), szModName, ucptr, szMod2Name));

        if (!_tcsicmp (ucptr, szMod2Name))
        {
            return TRUE;
        }
    }

    return FALSE;
}

void
DumpDllList (
    DLIST *pDlist
    )
{

#ifndef KCOREDLL
    PDLIST ptrav;
    PUSERMODULELIST pMod;

    DEBUGMSG (1, (_T("Dll list:\r\n")));

    for (ptrav = pDlist->pFwd; ptrav != pDlist; ptrav = ptrav->pFwd)
    {
        pMod = GetModFromLoadOrderList (ptrav);
        TCHAR szModuleName [MAX_PATH];
        GetNameFromBaseAddr (szModuleName, MAX_PATH, pMod->dwBaseAddr, NULL);
        if (szModuleName[0]) {
            DEBUGMSG (1, (_T("   '%s'\r\n"), szModuleName));
        }
    }
#endif // KCOREDLL

}

// --------------------------------------------------------------------------------
// Following functions are called from the loader code.

void
AppVerifierInit (
    LPVOID      pfn,
    LPCWSTR     pszProgName,
    LPCWSTR     pszCmdLine,
    HINSTANCE   hCoreDll
    )
{
    // This is called once per process, from the main thread's base function.
    DEBUGMSG (DBGLOADER, (_T("AppVerifierInit: 0x%08x '%s' '%s' 0x%08x\r\n"),
        pfn,
        pszProgName,
        pszCmdLine,
        hCoreDll
        ));

    // Note that there is no corresponding de-init function. If there were, it
    // would be called immediately before the process terminates. Instead of
    // taking the time to clean up, we'll just let everything go away when the
    // underlying VM disappears.
}

void
AppVerifierDoLoadLibrary (
    PUSERMODULELIST pMod,
    LPCWSTR pszFileName
    )
{
    // There must have been an error loading the module.
    if (!pMod)
    {
        return;
    }

#ifdef KCOREDLL
    if (!IsAppVerifierEnabled ())
    {
        return;
    }
#else
    if (!(pMod->wFlags & MF_APP_VERIFIER))
    {
        return;
    }
#endif

    // Do we know the module name? Or do we need to ask the kernel?
    if (!pszFileName)
    {
        TCHAR szFileName [MAX_PATH];
        GetModuleFileName (pMod->hModule, szFileName, MAX_PATH);

        pszFileName = PlainName (szFileName);
    }
    else
    {
        pszFileName = PlainNamePart2 (pszFileName, TRUE);
    }

    DEBUGMSG (DBGLOADER, (_T("AppVerifierDoLoadLibrary '%s' 0x%08x\r\n"), pszFileName, pMod->dwBaseAddr));

    // We'll keep an association of name & pMod - saves us the trouble of passing
    // that info around all the time.
    LPMODULE_INFO pModuleInfo;
    pModuleInfo = AddModuleInfo (pszFileName, pMod);

    // Tag all the app verifier modules.
    if (pModuleInfo)
    {
        if ((loading_shim || IsAppVerifierBinary (pMod) || (0 == _tcsicmp (pszFileName, _T("toolhelp.dll"))) || (0 == _tcsicmp (pszFileName, _T("k.toolhelp.dll"))))
#ifndef KCOREDLL
            && (& g_mlCoreDll != pMod)
#endif
            )
        {
            DEBUGMSG (DBGLOADER, (_T("Tagging '%s' (0x%08x) as app verifier module\r\n"), pszFileName, pMod->hModule));
            pModuleInfo->bIsAppVerifier = TRUE;
        }
    }
}

int my_charcmp (TCHAR c1, TCHAR c2)
{
    TCHAR lc1;
    TCHAR lc2;

    lc1 = (c1 >= _T('A')) && (c1 <= _T('Z')) ? (c1 + _T('a') - _T('A')) : c1;
    lc2 = (c2 >= _T('A')) && (c2 <= _T('Z')) ? (c2 + _T('a') - _T('A')) : c2;

    return (lc2 - lc1);
}

int my_tcsicmp (LPCTSTR pszString1, LPCTSTR pszString2)
{
    const TCHAR *p1 = pszString1;
    const TCHAR *p2 = pszString2;
    int nRet;

    for (
        p1 = pszString1, p2 = pszString2;
        *p1 && *p2;
        p1++, p2++
        )
    {
        if (my_charcmp (*p1, *p2))
        {
            break;
        }
    }

    nRet = *p2 - *p1;
    return nRet;
}

extern const o32_lite *FindRWOptrByRva (const o32_lite * optr, int nCnt, DWORD dwRva);

BOOL
AppVerifierDoImports (
    PUSERMODULELIST pModCalling,    // used only for kernel
    DWORD  dwBaseAddr, // of importing dll
    PCInfo pImpInfo, // of importing dll
    BOOL fRecursive
    )
{
    if (loading_shim || (pImpInfo->size == 0))
    {
        // No reason to go any farther.
        return FALSE;
    }

#ifndef KCOREDLL
    if (g_dwExeBase == dwBaseAddr)
    {
        if (!IsAppVerifierEnabled ())
        {
            // No reason to go any farther.
            return FALSE;
        }

        // This is the exe.
        TCHAR szModuleName [MAX_PATH];
        StringCchCopy (szModuleName, MAX_PATH, PlainName (g_pszProgName));

        // We haven't gotten a notification for the exe; create a struct so we
        // can treat it the same as dll's.
        PUSERMODULELIST pProcMod;
        pProcMod = (PUSERMODULELIST) LocalAlloc (LMEM_ZEROINIT, sizeof (USERMODULELIST));

        if (pProcMod)
        {
            pProcMod->dwBaseAddr = dwBaseAddr;

            AddModuleInfo (szModuleName, pProcMod);
        }
    }
#endif

    // We keep our own cache of info about this module.
    LPMODULE_INFO pModuleInfo = GetModuleInfo (dwBaseAddr);
    if (!pModuleInfo)
    {
        DEBUGMSG (DBGLOADER, (_T("Module info not found.\r\n")));
        return FALSE;

        // Do we need to free the name map structure? Do we need that anymore?
    }

    if (pModuleInfo->bIsAppVerifier)
    {
        // Don't do anything with app verifier modules.
        return FALSE;
    }

    DEBUGMSG (DBGLOADER, (_T("AppVerifierDoImports: '%s' 0x%08x\r\n"), pModuleInfo->szName, dwBaseAddr));

    pModuleInfo->pShimInfo = LoadAllShims (pModuleInfo);
    if (!pModuleInfo->pShimInfo)
    {
        DEBUGMSG (DBGLOADER, (_T("No shims to apply.\r\n")));

        // We're not shimming this module; no need to keep its info around.
        FreeModuleInfo (pModuleInfo);

        return FALSE;
    }

    memcpy (& pModuleInfo->impInfo, pImpInfo, sizeof (info));

    DWORD dwErr = 0;
    LPDWORD     ltptr, atptr;
    WCHAR       ucptr[MAX_DLLNAME_LEN];
    PCImpHdr    blockptr = (PCImpHdr) (dwBaseAddr + pImpInfo->rva);
    PCExpHdr    expptr;
    DWORD       dwOfstImportTable = 0;

#ifdef KCOREDLL
    // 'Z' flag modules has code/data split. Calculate the offset to get to import table
    PMODULE pKMod = (PMODULE) pModCalling;
    const o32_lite *optr = FindRWOptrByRva (pKMod->o32_ptr, pKMod->e32.e32_objcnt, blockptr->imp_address);

    DEBUGCHK (optr);
    dwOfstImportTable = optr->o32_realaddr - (dwBaseAddr + optr->o32_rva);
#endif

    // Walk the import table, searching for imports that need to be redirected
    // to functions in a shim dll.
    for ( ; !dwErr && blockptr->imp_lookup; blockptr++)
    {
        AsciiToUnicode (ucptr, (LPCSTR)dwBaseAddr+blockptr->imp_dllname, MAX_DLLNAME_LEN);
        DEBUGMSG (DBGLOADER, (_T("Searching imports from module '%s'\r\n"), ucptr));

        ltptr = (LPDWORD) (dwBaseAddr + blockptr->imp_lookup);
        atptr = (LPDWORD) (dwBaseAddr + blockptr->imp_address + dwOfstImportTable);

        DEBUGMSG (DBGLOADER, (_T("ltptr: 0x%08x, atptr: 0x%08x\r\n"), ltptr, atptr));

        for (; !dwErr && *ltptr; ltptr ++, atptr ++)
        {
            LPSHIM_INFO pShimInfo = ShouldShimThisFunction (pModuleInfo, ucptr, *ltptr, dwBaseAddr);
            if (pShimInfo)
            {
                DWORD retval;

                PCInfo pExpInfo = GetExportInfo (pShimInfo->pMod);
                expptr = (PCExpHdr) (pShimInfo->pMod->dwBaseAddr + pExpInfo->rva);

                if (*ltptr & 0x80000000)
                {
                    // Importing by ordinal
                    retval = ResolveImpOrd (pShimInfo->pMod, expptr, *ltptr & 0x7fffffff);

                    // intentionally using NKDbgPrintfW here to let the message visible even in SHIP BUILD
                    NKDbgPrintfW (_T("%s: Importing ordinal %u from Application Verifier module '%s'\r\n"), pModuleInfo->szName, *ltptr & 0x7fffffff, pShimInfo->szName);
                }
                else
                {
                    // Importing by string
                    PCImpProc impptr = (PCImpProc) (*ltptr + dwBaseAddr);
                    retval = ResolveImpHintStr (pShimInfo->pMod, expptr, impptr->ip_hint, (LPCSTR)impptr->ip_name);

                    // intentionally using NKDbgPrintfW here to let the message visible even in SHIP BUILD
                    NKDbgPrintfW (_T("%s: Importing '%S' from Application Verifier module '%s'\r\n"), pModuleInfo->szName, (LPCSTR)impptr->ip_name, pShimInfo->szName);
                }

                if (!retval) {
                    // intentionally using NKDbgPrintfW here to let the message visible even in SHIP BUILD
                    NKDbgPrintfW (L"ERROR: function @ Ordinal %d missing from Application Verifier module '%s'\r\n", *ltptr & 0x7fffffff, pShimInfo->szName);
                } else if (*atptr!= retval) {
                    *atptr = retval;
                }
            }
        }

        if (fRecursive)
        {
            PUSERMODULELIST pMod = ((PUSERMODULELIST) GetModHandle (hActiveProc, ucptr, 0));
            AppVerifierDoImports (pMod, pMod->dwBaseAddr, GetImportInfo (pMod), TRUE);
        }
    }

    return TRUE;
}

void
AppVerifierUnDoDepends (
    PUSERMODULELIST pMod
    )
{
    if (loading_shim || !g_bIsAppVerifierActive)
    {
        // No reason to go any farther.
        return;
    }

    DEBUGMSG (DBGLOADER, (_T("AppVerifierUnDoDepends: 0x%08x\r\n"),
        pMod
        ));

    // Look through process_modules, is this module shimmed?
    for (LPMODULE_INFO pModuleInfoNext, pModuleInfo = process_modules; pModuleInfo; pModuleInfo = pModuleInfoNext)
    {
        pModuleInfoNext = pModuleInfo->pNext;
        if (pModuleInfo->pMod && (pModuleInfo->pMod->dwBaseAddr == pMod->dwBaseAddr))
        {
            // Found the module.
            DEBUGMSG (DBGLOADER, (_T("Freeing shim dll's loaded for '%s'\r\n"), pModuleInfo->szName));

            // All we need to do is walk the list of shim_dlls, and free each library.
            // The calling order will be correct, as all the dll's normally linked to
            // this module have already been unloaded.
            for (LPSHIM_INFO pShimInfoNext, pShimInfo = pModuleInfo->pShimInfo; pShimInfo; pShimInfo = pShimInfoNext)
            {
                pShimInfoNext = pShimInfo->pNext;
                FreeShim (pShimInfo);
            }

            // Finally, free up the structure for this module, and remove it from the list.
            FreeModuleInfo (pModuleInfo);

            // quit the loop once the module is found
            break;
        }
    }
}

void
AppVerifierDoNotify (
    PUSERMODULELIST pMod,
    DWORD dwReason
    )
{
    if (!g_bIsAppVerifierActive)
    {
        // No reason to go any farther.
        return;
    }

    DEBUGMSG (DBGLOADER, (_T("AppVerifierDoNotify: 0x%08x\r\n"), pMod));

    // Walk the process's module list and notify all initialized shims about the new module.
    for (LPMODULE_INFO pModuleInfo = process_modules; pModuleInfo; pModuleInfo = pModuleInfo->pNext)
    {
        if (pModuleInfo->pMod && pModuleInfo->bIsAppVerifier)
        {
            // We don't notify the shim about its own load/unload. Simply update its 'bReceivesNotifications' flag.
            if (pModuleInfo->pMod->hModule == pMod->hModule)
            {
                // If the shim is being initialized, it should receive notifications from the NEXT load/unload.
                // Else if has been uninitialized, stop sending notifications.
                pModuleInfo->bReceivesNotifications = (DLL_PROCESS_ATTACH == dwReason) ? TRUE : FALSE;
            }
            else if (pModuleInfo->bReceivesNotifications)
            {
                PFN_SHIMNOTIFY pfnNotify = (PFN_SHIMNOTIFY) GetProcAddressW (pModuleInfo->pMod->hModule, _T("ShimNotify"));
                if (pfnNotify)
                {
                    pfnNotify (pMod->hModule, dwReason);
                }
            }
        }
    }
}

void
AppVerifierProcessDetach (
    void
    )
{
#ifndef KCOREDLL
    DLIST shimlist;
    PDLIST ptrav;
    PDLIST ptrav2;
    PUSERMODULELIST pMod;
    PUSERMODULELIST pMod2;

    DEBUGMSG (DBGLOADER, (_T("AppVerifierProcessDetach\r\n")));
    DumpDllList (& g_loadOrderList);

    // Set up a list to temporarily contain all the app verifier modules we find.
    InitDList (&shimlist);

    // Sort out the app verifier modules from the process's normal modules.
    for (ptrav = g_loadOrderList.pFwd; ptrav != &g_loadOrderList; ptrav = ptrav->pFwd)
    {
        pMod = GetModFromLoadOrderList (ptrav);

        LPMODULE_INFO pModuleInfo;
        pModuleInfo = GetModuleInfo (pMod->dwBaseAddr);

        if (pModuleInfo && pModuleInfo->bIsAppVerifier)
        {
            // Make sure the app verifier dll's have the highest possible ref cnt,
            // to ensure that they're called last (giving all the dll's that are
            // intended to be in the process a chance to clean up in their PROCESS_DETACH).
            pMod->wRefCnt = 0xffff;

            // We're about to remove this node. Back up to the previous node.
            ptrav = ptrav->pBack;

            // Take the App Verifier modules out of the global list, and place
            // them in a local list.
            RemoveDList (& pMod->loadOrderLink);
            AddToDListHead (& shimlist, & pMod->loadOrderLink);
        }
    }

    // Sort the app verifier module list. Resolve dependencies between app verifier modules.
    if (!IsDListEmpty (& shimlist))
    {
        DWORD dwSwappedPrev;
        DWORD dwSwapped = 0;

        do
        {
            dwSwappedPrev = dwSwapped;

            for (ptrav = shimlist.pBack; ptrav != & shimlist; ptrav = ptrav->pBack)
            {
                pMod = GetModFromLoadOrderList (ptrav);
                for (ptrav2 = ptrav->pBack; ptrav2 != & shimlist; ptrav2 = ptrav2->pBack)
                {
                    pMod2 = GetModFromLoadOrderList (ptrav2);

                    if (IsDependent (pMod, pMod2))
                    {
                        // We're about to remove this node. Back up to the previous node.
                        ptrav2 = ptrav2->pFwd;

                        //
                        RemoveDList (& pMod2->loadOrderLink);

                        // Add to the list after the current dll.
                        pMod2->loadOrderLink.pBack = ptrav;
                        pMod2->loadOrderLink.pFwd = ptrav->pFwd;
                        ptrav->pFwd->pBack = & pMod2->loadOrderLink;
                        ptrav->pFwd = & pMod2->loadOrderLink;

                        // We're going to have to run through the list again.
                        dwSwapped++;
                    }
                }
            }

            if (dwSwapped > 100)
            {
                DEBUGMSG (DBGLOADER, (_T("ERROR: Circular dependency in app verifier modules?\r\n")));
                DebugBreak ();
                break;
            }
        } while (dwSwapped != dwSwappedPrev);

        // Link the app verifier module list back into the global list.
        shimlist.pFwd->pBack = g_loadOrderList.pBack;
        shimlist.pBack->pFwd = & g_loadOrderList;
        g_loadOrderList.pBack->pFwd = shimlist.pFwd;
        g_loadOrderList.pBack = shimlist.pBack;

        DumpDllList (& g_loadOrderList);
    }

    // Clean up? Nothing that needs to be freed.
#endif
}

void
AppVerifierUnlockLoader (
    void
    )
{
#ifndef KCOREDLL
    UnlockProcCS ();
#endif
}

BOOL AttachToRunning (PUSERMODULELIST pMod)
{
#ifdef KCOREDLL
    if (pMod)
    {
        AppVerifierDoLoadLibrary (pMod, NULL);
        AppVerifierDoImports (pMod, pMod->dwBaseAddr, GetImportInfo (pMod), FALSE);

        return TRUE;
    }
#endif

    return FALSE;
}

#ifndef KCOREDLL
static BOOL BuildModuleList (PDLIST pItem, LPVOID p)
{
    LPMODULE_INFO pModule = NULL;
    LPMODULE_INFO pHead = (LPMODULE_INFO) p;

    pModule = (LPMODULE_INFO) LocalAlloc (LMEM_ZEROINIT, sizeof (MODULE_INFO));

    if (pModule)
    {
        pModule->pMod = (PUSERMODULELIST) pItem;
        pModule->pNext = pHead->pNext;
        pHead->pNext = pModule;
    }

    // Continue...
    return FALSE;
}
#endif

BOOL RepatchThisProcess (void)
{
#ifndef KCOREDLL

    LPMODULE_INFO pModules = NULL;
    LPMODULE_INFO pModuleInfoWalk = NULL;
    LPMODULE_INFO pTemp;

    pModules = (LPMODULE_INFO) LocalAlloc (LMEM_ZEROINIT, sizeof (MODULE_INFO));

    LockProcCS ();

    // Build up a list of all dll's loaded into this address space. Build up a
    // cache, making sure not to do anything that will effect the ordering of
    // the list (and potentially putting us into an infinite loop while enumerating).
    EnumerateModules (BuildModuleList, pModules);

    // Patch up the exe itself.
    AppVerifierDoImports (NULL, g_dwExeBase, g_impExe, FALSE);

    // Patch up each dll.
    for (pModuleInfoWalk = pModules; pModuleInfoWalk;)
    {
        if (pModuleInfoWalk->pMod)
        {
            pModuleInfoWalk->pMod->wFlags |= MF_APP_VERIFIER;

            // If app verifier wasn't enabled when this module loaded, we'll need
            // to get its info now.
            AppVerifierDoLoadLibrary (pModuleInfoWalk->pMod, NULL);

            AppVerifierDoImports (
                NULL,
                pModuleInfoWalk->pMod->dwBaseAddr,
                GetImportInfo (pModuleInfoWalk->pMod),
                FALSE
                );
        }

        pTemp = pModuleInfoWalk;
        pModuleInfoWalk = pModuleInfoWalk->pNext;

        LocalFree (pTemp);
    }

    AppVerifierUnlockLoader ();

#endif

    return TRUE;
}

BOOL
AppVerifierIoControl (
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
        case IOCTL_APPVERIF_ATTACH:
        {
            return lpInBuf ?
                AttachToRunning ((PUSERMODULELIST) lpInBuf) :
                RepatchThisProcess ();
        }
    }

    return FALSE;
}

