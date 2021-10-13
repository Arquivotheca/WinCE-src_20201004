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
//------------------------------------------------------------------------------
//
//  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
//  ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
//  PARTICULAR PURPOSE.
//
//------------------------------------------------------------------------------
//
//  Module Name:
//
//      verifier.c
//
//  Abstract:
//
//      Implements the shim engine.
//
//------------------------------------------------------------------------------

#include <kernel.h>
#include "altimports.h"

DBGPARAM dpCurSettings = { TEXT("ShimEngine"), {
        TEXT("Entry"), TEXT("Entry2"), TEXT("Search"), TEXT("NotifyList"),
        TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"),
        TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"),
        TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>") },
    0x00000000};

#undef ZONE_ENTRY
#define ZONE_ENTRY DEBUGZONE(0)
#define ZONE_ENTRY2 DEBUGZONE(1)
#define ZONE_SEARCH DEBUGZONE(2)
#define ZONE_NOTIFYLIST DEBUGZONE(3)

//------------------------------------------------------------------------------
// Globals

// Inputs from kernel
VerifierImportTable g_Imports;

CRITICAL_SECTION llcs;
BOOL g_fInit;
BOOL g_fLoadingShim;
BOOL g_fUnLoadingShim;
HINSTANCE gDllInstance;
HANDLE hCoreDll;
LPDWORD pIsExiting;

//------------------------------------------------------------------------------
//

BOOL
IsShimDll(
    PMODULE pMod
    );

BOOL
GetNameFromE32(
    e32_lite *eptr,
    LPWSTR lpszModuleName,
    DWORD cchModuleName,
    PMODULE *ppModule
    );

typedef struct _SHIMINFO {
    WORD wPool;
    WORD wPad;
    PMODULE pMod;
    LPTSTR pszName;
    struct _SHIMINFO *pNext;
} SHIMINFO, *LPSHIMINFO;

LPSHIMINFO
FindShimInfo(
    LPSHIMINFO pShimInfo,
    LPTSTR szShimName
    );

typedef struct _SHIMREF {
    LPSHIMINFO pShimInfo [MAX_PROCESSES];
} SHIMREF, *LPSHIMREF;

ERRFALSE(sizeof(SHIMREF) <= sizeof(MUTEX));
#define HEAP_SHIMREF    HEAP_MUTEX
#define BASE_SHIM _T("shim_verifier.dll")
#define BASE_SHIM_LEN 17

//------------------------------------------------------------------------------
// Somewhat more abstracted input interface (yuck)

// Be careful using WIN32CALL - pWin32Methods could be providing pointers to functions that
// enforce security checks that we don't want to abide by.
#define WIN32CALL(Api, Args)     ((g_Imports.pWin32Methods[W32_ ## Api]) Args)

#undef RegOpenKeyEx
#define RegOpenKeyEx (g_Imports.pExtraMethods[13])
#undef RegQueryValueEx
#define RegQueryValueEx (g_Imports.pExtraMethods[14])
#undef RegCloseKey
#define RegCloseKey (g_Imports.pExtraMethods[23])

#undef KData
#define KData                    (*(g_Imports.pKData))

#ifndef SHIP_BUILD
#undef RETAILMSG
#define RETAILMSG(cond, printf_exp) ((cond)?(g_Imports.NKDbgPrintfW printf_exp),1:0)
#endif // SHIP_BUILD

#ifdef DEBUG
#undef DEBUGMSG
#define DEBUGMSG                 RETAILMSG
#endif // DEBUG

#ifdef DEBUG
#undef DBGCHK
#undef DEBUGCHK
#define DBGCHK(module,exp)                                                     \
   ((void)((exp)?1                                                             \
                :(g_Imports.NKDbgPrintfW(TEXT("%s: DEBUGCHK failed in file %s at line %d\r\n"), \
                                              (LPWSTR)module, TEXT(__FILE__), __LINE__), \
                  DebugBreak(),                                                \
	              0                                                            \
                 )))
#define DEBUGCHK(exp) DBGCHK(dpCurSettings.lpszName, exp)
#endif // DEBUG

#define LOG(x) RETAILMSG(1, (_T("%s = 0x%x\n"), _T(#x), x))

#define VLOG_DLL_NAME _T("vlog.dll")
#define VLOG_DLL_NAME_LEN 8
#define MAX_DLLNAME_LEN 32
#define EXE_BASE_ADDR 0x00010000

// CRT string manipulation function pointers
size_t (*vrf_wcslen)(const wchar_t *wcs);
wchar_t *(*vrf_wcscpy)(wchar_t *strDest, const wchar_t *strSource);
wchar_t *(*vrf_wcsncpy)(wchar_t *strDest, const wchar_t *strSource, size_t count);
wchar_t (*vrf_tolower)(wchar_t c);
int (*vrf_wcsnicmp)(const wchar_t *psz1, const wchar_t *psz2, size_t cb);
int (*vrf_wcsicmp)(const wchar_t *str1, const wchar_t*str2);

BOOL IsFilesysReady (void)
{
    // Can only check the local registry if the filesys API is registered.
    return (UserKInfo[KINX_API_MASK] & (1 << SH_FILESYS_APIS)) != 0;
}

PMODULE LoadShim (LPCTSTR pszShim)
{
    PMODULE pModRet;

    g_fLoadingShim = TRUE;
    pModRet = (PMODULE) g_Imports.LoadOneLibraryW (pszShim, 0, 0);
    g_fLoadingShim = FALSE;

    return pModRet;
}

BOOL AddShimToList (LPCTSTR pszShim, PMODULE pModShim, PMODULE pModOwner)
{
    LPSHIMINFO pShimInfo;
    LPName lpName;

    lpName = (LPName) g_Imports.AllocName (sizeof (SHIMINFO) + (vrf_wcslen (pszShim) + 1) * sizeof (TCHAR));
    if (!lpName) {
        DEBUGCHK (0);
        return FALSE;
    }

    // Fill in the structure (use the name field)
    pShimInfo = (LPSHIMINFO) lpName;
    pShimInfo->wPool = lpName->wPool;
    pShimInfo->wPad = 0;
    pShimInfo->pMod = pModShim;
    pShimInfo->pszName = (LPTSTR)(pShimInfo + 1);
    vrf_wcscpy (pShimInfo->pszName, pszShim);

    // Link in this new SHIMINFO structure
    if (pModOwner) {
        // This is a dll
        pShimInfo->pNext = ((LPSHIMREF)pModOwner->pShimInfo)->pShimInfo [pCurProc->procnum];
        ((LPSHIMREF)pModOwner->pShimInfo)->pShimInfo [pCurProc->procnum] = pShimInfo;
    }
    else {
        // This is an exe
        pShimInfo->pNext = pCurProc->pShimInfo;
        pCurProc->pShimInfo = pShimInfo;
    }

    return TRUE;
}

LPSHIMINFO FindShimInfo (LPSHIMINFO pShimInfo, LPTSTR szShimName)
{
    // Walk the list of shims loaded by this image (dll or exe), and see if the
    // desired shim is already loaded.
    for (; pShimInfo; pShimInfo = pShimInfo->pNext) {
        if (!vrf_wcsicmp (pShimInfo->pszName, szShimName)) {
            DEBUGCHK (pShimInfo->pMod && IsShimDll (pShimInfo->pMod));
            return pShimInfo;
        }
    }

    return NULL;
}

HKEY OpenShimKey (LPTSTR pszSettingName, BOOL fIsProcess)
{
    PTCHAR p;
    PTCHAR pszSettingNameBare;
    TCHAR szKeyName [MAX_PATH] = { TEXT("ShimEngine\\") };
    size_t cchKeyName;
    LONG lRet;
    HKEY hKey = NULL;

    // Fix up the setting name, if needed. It must have an extension.
    if (vrf_wcsicmp (pszSettingName, _T("{all}"))) {
        // Strip off path
        for (p = pszSettingNameBare = pszSettingName; *p; p++) {
            if ((*p == _T('\\')) || (*p == _T('/')))
                pszSettingNameBare = p + 1;
        }

        vrf_wcscpy (szKeyName + 11, pszSettingNameBare);
        cchKeyName = vrf_wcslen (szKeyName);

        if (szKeyName [cchKeyName - 4] != _T('.')) {
            // There's no extension - append one
            vrf_wcscpy (szKeyName + cchKeyName, fIsProcess ? _T(".exe") : _T(".dll"));
        }
    }
    else {
        vrf_wcscpy (szKeyName + 11, pszSettingName);
    }

    lRet = RegOpenKeyEx (HKEY_LOCAL_MACHINE, szKeyName, 0, 0, & hKey);

    DEBUGMSG (ZONE_SEARCH, (_T("Opening reg key '%s' --> 0x%08x\r\n"), szKeyName, hKey));
    return (ERROR_SUCCESS == lRet) ? hKey : NULL;
}

BOOL ShouldShimLoadedModules (HKEY hKey)
{
    DWORD cbData;
    LONG lRet;
    DWORD dwType;
    BOOL fShimModules = FALSE;

    cbData = sizeof (BOOL);
    lRet = RegQueryValueEx (hKey, TEXT("ShimModules"), 0, & dwType, & fShimModules, & cbData);

    // Does the registry specify whether to shim modules?
    if (ERROR_SUCCESS == lRet) {
        DEBUGMSG(ZONE_SEARCH, (TEXT("This process (%s) does %sallow shimming loaded modules.\r\n"),
            pCurProc->lpszProcName, fShimModules ? _T("") : _T("not ")));
    }

    return fShimModules;
}

BOOL
IsShimBinary(
    LPCTSTR szModuleName
    )
{
    TCHAR Buffer [MAX_PATH];

    if (g_Imports.LoadStringW (hCurProc, 1, Buffer, MAX_PATH)) {
        if (0 == vrf_wcsicmp (Buffer, _T("AppVerifier"))) {
            return TRUE;
        }
    }

    return FALSE;
}

BOOL ShouldShimThisModuleROM (LPMODULE pMod, LPCTSTR szModuleName)
{
    DEBUGMSG(1, (TEXT("++ShouldHookFromThisModuleROM (0x%08x, %s)\r\n"), pMod, szModuleName));

    // TODO
    return FALSE;
}

BOOL ShouldShimThisModule (LPMODULE pMod, LPTSTR szModuleName)
{
    HKEY hKey = 0;
    BOOL fShouldHook = FALSE;

    DEBUGMSG(ZONE_ENTRY, (TEXT("++ShouldHookFromThisModule (0x%08x, %s)\r\n"), pMod, szModuleName));

    if (g_fLoadingShim)
        return FALSE;

    // Don't alter a shim's imports
    if (pMod && IsShimDll (pMod)) {
        DEBUGMSG(1, (TEXT("Dynamically loading shim dll '%s'\r\n"), szModuleName));
        return FALSE;
    }
    else if (!pMod && IsShimBinary (szModuleName)) {
        DEBUGMSG (1, (TEXT("Not shimming app verifier binaries\r\n")));
        return FALSE;
    }

    if (!IsFilesysReady ()) {
        // We can't use the registry, look for a .ini file in ROM
        return ShouldShimThisModuleROM (pMod, szModuleName);
    }

    // First, check {all}
    if (hKey = OpenShimKey (_T("{all}"), TRUE)) {
        fShouldHook = TRUE;
    }

    // Then, check szModuleName
    else if (hKey = OpenShimKey (szModuleName, pMod ? FALSE : TRUE)) {
        fShouldHook = TRUE;
    }

    // Finally, check the current process and ShimModules, if this is a dll
    else if (pMod && (hKey = OpenShimKey (pCurProc->lpszProcName, TRUE))) {
        fShouldHook = ShouldShimLoadedModules (hKey);
    }

    if (hKey) {
        RegCloseKey (hKey);
    }

    return fShouldHook;
}

BOOL VerifyNullLists (void)
{
    LPMODULE pMod;
    LPSHIMINFO pShimInfo;
    LPSHIMINFO pShimInfoTemp;
    BOOL fRet = TRUE;

    g_Imports.EnterCriticalSection (g_Imports.pModListcs);

    for (pMod = pModList; pMod; pMod = pMod->pMod) {
        // Walk the list of shim modules injected into this image, and free the
        // list. Everything is invalid.
        pShimInfo = pMod->pShimInfo ? ((LPSHIMREF)pMod->pShimInfo)->pShimInfo [pCurProc->procnum] : NULL;

        if (pShimInfo) {
            fRet = FALSE;
            RETAILMSG (1, (_T("ShimEngine ERROR: stale info for '%s' (pMod 0x%08x)\r\n"),
                pMod->lpszModName, pMod));
            DEBUGCHK(0);

            while (pShimInfo) {
                pShimInfoTemp = pShimInfo;
                pShimInfo = pShimInfo->pNext;

                g_Imports.FreeMem (pShimInfoTemp, pShimInfoTemp->wPool);
            }

            ((LPSHIMREF)pMod->pShimInfo)->pShimInfo [pCurProc->procnum] = NULL;
        }
    }

    g_Imports.LeaveCriticalSection (g_Imports.pModListcs);

    return fRet;
}

BOOL ShimInitModule (e32_lite *eptr, o32_lite *oarry, DWORD BaseAddr, LPCTSTR szModuleName)
{
    PMODULE pMod;
    PMODULE pModShim;
    TCHAR _szModuleName [MAX_PATH];

    // Don't alter a shim's imports
    if (!GetNameFromE32 (eptr, _szModuleName, MAX_PATH, & pMod)) {
        RETAILMSG(1, (TEXT("ShimInitModule: Couldn't find module '%s'\r\n"), szModuleName));
        return FALSE;
    }

    if (!ShouldShimThisModule (pMod, _szModuleName))
        return FALSE;

    // Make sure we acknowledge the fact that we're using imports that weren't
    // originally intended.

    RETAILMSG(1, (TEXT("-----> Using alternate imports for module %s\r\n"), szModuleName));

    // Make sure shim_verifier is injected into all shimmed modules.
    // Only inject into a process here - we'll inject into a dll later (when
    // the shimref structure is allocated).
    if (!pMod && (pModShim = LoadShim (BASE_SHIM))) {
        // This is a new process - make sure there are no dll's out there that
        // have shim references in this process. If they do, they're old, and
        // the need to be cleared.
        VerifyNullLists ();

        // Add the base shim to the process's shim list.
        AddShimToList (BASE_SHIM, pModShim, NULL);
    }

    return TRUE;
}

static PTCHAR _ustoa (unsigned short int n, PTCHAR pszOut)
{
    DWORD d4, d3, d2, d1, d0, q;
    PTCHAR p;
    INT i;

    if (!n) {
        pszOut [0] = _T('0');
        pszOut [1] = 0;
        return pszOut;
    }

    d1 = (n >> 4) & 0xF;
    d2 = (n >> 8) & 0xF;
    d3 = (n >> 12) & 0xF;

    // We can't use FP operations here, so a divide by 10 is implemented as
    // a multiply, and a shift right.

    d0 = 6 * (d3 + d2 + d1) + (n & 0xF);
    q = (d0 * 0x19A) >> 12; // * 410 / 4096, or / .10009
    d0 = d0 - 10 * q;

    d1 = q + 9 * d3 + 5 * d2 + d1;
    q = (d1 * 0x19A) >> 12; // * 410 / 4096, or / .10009
    d1 = d1 - 10 * q;

    d2 = q + 2 * d2;
    q = (d2 * 0x1A) >> 8; // * 26 / 256, or / .1015
    d2 = d2 - 10 * q;

    d3 = q + 4 * d3;
    d4 = (d3 * 0x1A) >> 8; // * 26 / 256, or / .1015
    d3 = d3 - 10 * d4;

    if (pszOut) {
        pszOut [0] = (unsigned short) d4 + _T('0');
        pszOut [1] = (unsigned short) d3 + _T('0');
        pszOut [2] = (unsigned short) d2 + _T('0');
        pszOut [3] = (unsigned short) d1 + _T('0');
        pszOut [4] = (unsigned short) d0 + _T('0');

        // Trim the leading zero's
        for (p = pszOut; *p == TEXT('0'); p++);
        if (p == pszOut)
            return pszOut;
        for (i = 0; i < 5 - (p - pszOut); i++)
            pszOut [i] = p [i];
        pszOut [i] = 0;
    }

    return pszOut;
}

BOOL MakeValueName (LPCTSTR impmodname, DWORD ord, DWORD BaseAddr, LPTSTR szValueName, DWORD cchValueName)
{
    struct ImpProc *impptr;
    DWORD cch;

    // Get the name of the dependent module (module which is being imported from).
    vrf_wcscpy (szValueName, impmodname);

    // Append '-'
    cch = vrf_wcslen (szValueName);
    szValueName [cch++] = _T('-');

    // Append the imported ordinal (name or number).
    if (ord & 0x80000000) {
        // Importing by ordinal
        _ustoa ((unsigned short)(ord & 0x7fffffff), szValueName + cch);
    }
    else {
        // Importing by name
        impptr = (struct ImpProc *)((ord&0x7fffffff)+BaseAddr);
        g_Imports.KAsciiToUnicode (szValueName + cch, (LPCHAR)impptr->ip_name, 38);
    }

    // szValueName now contains a string representing the function being imported,
    // in the form 'module-ordinal'

    DEBUGMSG (ZONE_SEARCH, (_T("MakeValueName: '%s'\r\n"), szValueName));
    return TRUE;
}

PMODULE
ShimWhichMod (
    PMODULE pmod, // module structure of DLL from which the image imports functions
    LPCTSTR modname, // name of the image
    LPCTSTR impmodname, // names of the DLL from which the image imports functions
    DWORD BaseAddr, // base address of image
    DWORD ord, // ordinal (or name) of imported function
    e32_lite *eptr
    )
{
    PMODULE pModImage;
    PMODULE pModRet;
    TCHAR szModuleName [MAX_PATH];
    WCHAR szValueName [MAX_PATH];
    HKEY hKey;
    WCHAR szShim [MAX_PATH];
    LPSHIMINFO pShimInfo;
    DWORD cbData;
    DWORD dwType;
    LONG lRet;

    DEBUGMSG(ZONE_ENTRY2, (TEXT("++WhichMod (0x%08x, %s, %s, 0x%08x, 0x%08x)\r\n"),
        pmod, modname, impmodname, BaseAddr, ord));

    // Is this an exe or dll? We need to know where to put the shim info.
    if (!GetNameFromE32 (eptr, szModuleName, MAX_PATH, & pModImage)) {
        RETAILMSG (1, (_T("ShimWhichMod: Couldn't find module '%s'\r\n"), szModuleName));
        return pmod;
    }

    if (!MakeValueName (impmodname, ord, BaseAddr, szValueName, MAX_PATH)) {
        RETAILMSG (1, (_T("ShimWhichMod: Couldn't generate value key\r\n")));
        return pmod;
    }

    szShim [0] = _T('\0');

    // Look for an alternate import under this module's reg key root
    if (hKey = OpenShimKey (szModuleName, pModImage ? FALSE : TRUE)) {
        cbData = MAX_PATH;
        lRet = RegQueryValueEx (hKey, szValueName, 0, & dwType, szShim, & cbData);
        RegCloseKey (hKey);
    }

    // If this is a dll, and the loading process's settings are to shim loaded
    // modules, look for an alternate import under that key.
    if (!szShim[0] && pModImage && (hKey = OpenShimKey (pCurProc->lpszProcName, TRUE)) && ShouldShimLoadedModules (hKey)) {
        cbData = MAX_PATH;
        lRet = RegQueryValueEx (hKey, szValueName, 0, & dwType, szShim, & cbData);
    }

    if (hKey) {
        RegCloseKey (hKey);
    }

    // Finally, see if there's a global setting for this import
    if (!szShim[0] && (hKey = OpenShimKey (_T("{all}"), FALSE))) {
        cbData = MAX_PATH;
        lRet = RegQueryValueEx (hKey, szValueName, 0, & dwType, szShim, & cbData);
        RegCloseKey (hKey);
    }

    if (szShim[0]) {
        // This import is to be shimmed. szShim contains the name of the shim.
        if (pModImage && !pModImage->pShimInfo) {
            // Allocate a SHIMREF structure for this module
            pModImage->pShimInfo = (LPVOID) g_Imports.AllocMem (HEAP_SHIMREF);
            memset (pModImage->pShimInfo, 0, sizeof (SHIMREF));

            // Make sure shim_verifier is injected into all shimmed modules, too
            pModRet = LoadShim (BASE_SHIM);

            if (pModRet) {
                // Successfully loaded the shim. Add it to this module's list of shims.
                AddShimToList (BASE_SHIM, pModRet, pModImage);
            }
        }

        if (!(pShimInfo = FindShimInfo (pModImage ? ((LPSHIMREF)pModImage->pShimInfo)->pShimInfo [pCurProc->procnum] : pCurProc->pShimInfo, szShim))) {
            // The shim has not been loaded by this module yet. Load it now.
            pModRet = LoadShim (szShim);

            if (pModRet) {
                // Successfully loaded the shim. Add it to this module's list of shims.
                AddShimToList (szShim, pModRet, pModImage);
            }
            else {
                // Loading the shim failed. Import from the original module.
                pModRet = pmod;
            }
        }
        else {
            // The shim has already been loaded by this image. Don't load it again.
            pModRet = pShimInfo->pMod;
        }
    }
    else {
        // This import is not shimmed.
        pModRet = pmod;
    }

    RETAILMSG((pModRet != pmod) && !g_fUnLoadingShim,
        (TEXT("*** DoImports (%s): importing %s from %s\r\n"), modname, szValueName, pModRet->lpszModName));

    return pModRet;
}

BOOL
GetNameFromE32(
    e32_lite *eptr,
    LPWSTR lpszModuleName,
    DWORD cchModuleName,
    PMODULE *ppModule
    )
{
    PMODULE pMod;

    *ppModule = NULL;

    if (eptr == & pCurProc->e32) {
        vrf_wcsncpy (lpszModuleName, pCurProc->lpszProcName, cchModuleName);
        return TRUE;
    }

    g_Imports.EnterCriticalSection (g_Imports.pModListcs);

    for (pMod = pModList; pMod; pMod = pMod->pMod) {
        if (eptr == & pMod->e32) {
            vrf_wcsncpy (lpszModuleName, pMod->lpszModName, cchModuleName);
            *ppModule = pMod;
            break;
        }
    }

    g_Imports.LeaveCriticalSection (g_Imports.pModListcs);

    RETAILMSG (!*ppModule, (TEXT("Couldn't find module with eptr=0x%08x\r\n"), eptr));
    DEBUGCHK (*ppModule);
    return *ppModule ? TRUE : FALSE;
}

BOOL
IsShimDll(
    PMODULE pMod
    )
{
    return (g_Imports.GetProcAddressA (pMod, "QueryShimInfo")) ? TRUE : FALSE;
}

BOOL
ShimUnDoDepends(
    e32_lite *eptr,
    DWORD BaseAddr,
    BOOL fAddToList
    )
{
    TCHAR szModuleName [MAX_PATH];
    PMODULE pMod;
    PMODULE pModVLog = NULL;
    PMODULE pModCoredll = NULL;
    LPSHIMINFO pShimInfo;
    LPSHIMINFO pShimInfoTemp;

    //
    // We can't use the info in the registry, since that could have changed
    // since this module was loaded.
    //

    if ((BaseAddr == (DWORD) pCurProc->BasePtr) && !pCurProc->pShimInfo) {
        // This is a process, and is not shimmed
        return TRUE;
    }

    // If we're here, this is either a shimmed process, or a dll. Either way,
    // we need to get some more info.

    if (!GetNameFromE32 (eptr, szModuleName, MAX_PATH, & pMod)) {
        // Couldn't find the module??
        return FALSE;
    }

    if (pMod && !pMod->pShimInfo) {
        // This is a dll, and is not shimmed
        return FALSE;
    }

    DEBUGMSG (ZONE_ENTRY, (TEXT("++UnDoDepends 0x%08x 0x%08x %s\r\n"), eptr, BaseAddr, szModuleName));

    // Walk the list of shim modules injected into this image
    pShimInfo = pMod ? ((LPSHIMREF)pMod->pShimInfo)->pShimInfo [pCurProc->procnum] : pCurProc->pShimInfo;

    while (pShimInfo) {
        // Free the shims
        g_fUnLoadingShim = TRUE;
        g_Imports.FreeOneLibrary (pShimInfo->pMod, 1);
        g_fUnLoadingShim = FALSE;

        pShimInfoTemp = pShimInfo;
        pShimInfo = pShimInfo->pNext;

        g_Imports.FreeMem (pShimInfoTemp, pShimInfoTemp->wPool);
    }

    if (pMod) {
        // Remove the list for this process
        ((LPSHIMREF)pMod->pShimInfo)->pShimInfo [pCurProc->procnum] = NULL;
    }

    // Note: The kernel does not notify dll's anymore. It now keeps a list of
    // dll's that need to be notified, and coredll walks that list. Re-order
    // the list in GetProcModList.

    return TRUE;
}

BOOL ShimIoControl(
    DWORD   dwIoControlCode,
    LPVOID  lpInBuf,
    DWORD   nInBufSize,
    LPVOID  lpOutBuf,
    DWORD   nOutBufSize,
    LPDWORD lpBytesReturned
    )
{
    switch (dwIoControlCode) {
    }

    return FALSE;
}

BOOL ShimGetProcModList (PDLLMAININFO pList, DWORD nMods)
{
    DLLMAININFO DllMainInfoTemp;
    DWORD idx;
    DWORD idx2;
    PDLLMAININFO pCurr;
    PDLLMAININFO pWalk;
    struct ImpHdr *blockptr;
    TCHAR imp_lookupw [MAX_DLLNAME_LEN];
    BOOL fSwapped = FALSE;

    if (nMods == 1) {
        // A list of 1 is already sorted
        return TRUE;
    }

    // Reorder this list, making sure that the app's dll's are notified
    // before the shims and their helpers. This will prevent some false
    // leak information. Also reorder so the shim's dependent dll's are called
    // after the shim.

    // We only need to reorder the list of dll's if the process is exiting.
    if (pIsExiting && *(LPDWORD)MapPtrProc(pIsExiting,pCurProc)) {

        // Print out the current list
        if (ZONE_NOTIFYLIST) {
            RETAILMSG (1, (_T("Old list:\r\n")));
            for (idx = 0; idx < nMods; idx ++) {
                RETAILMSG (1, (_T("%02u: %s (%s)\r\n"), idx, ((PMODULE)pList[idx].hLib)->lpszModName,
                    IsShimDll ((PMODULE) pList[idx].hLib) ? _T("shim") : _T("normal")));
            }
        }

        // Order by shim vs. non-shim. pCurr will point to the next position
        // that will contain a shim. pWalk will be used to walk through the list.
        for (pWalk = pList, pCurr = & pList [nMods - 1]; pWalk < pCurr; ) {
            if (IsShimDll ((LPMODULE)(pWalk->hLib))) {
                // Move the shim to the end of the list, shifting the rest of
                // the non-moved modules down one slot.
                memcpy (& DllMainInfoTemp, pWalk, sizeof (DLLMAININFO));
                memcpy (pWalk, pWalk + 1, (pCurr - pWalk) * sizeof (DLLMAININFO));
                memcpy (pCurr, & DllMainInfoTemp, sizeof (DLLMAININFO));
                pCurr--;
            }
            else {
                // Move on to the next position.
                pWalk++;
            }
        }

        // Print out the current list
        if (ZONE_NOTIFYLIST) {
            RETAILMSG (1, (_T("Intermediate list:\r\n")));
            for (idx = 0; idx < nMods; idx ++) {
                RETAILMSG (1, (_T("%02u: %s (%s)\r\n"), idx, ((PMODULE)pList[idx].hLib)->lpszModName,
                    IsShimDll ((PMODULE) pList[idx].hLib) ? _T("shim") : _T("normal")));
            }
        }

        // Now resolve dependency orders for shims, leaving non-shim dll's in
        // the kernel's order.
        for (idx = 0, pCurr = & pList [nMods - 1]; (idx < nMods) && IsShimDll ((LPMODULE)(pCurr->hLib)); idx++, pCurr--) {
            DEBUGMSG (ZONE_NOTIFYLIST, (_T("pCurr: %s\r\n"), ((LPMODULE)(pCurr->hLib))->lpszModName));
            for (idx2 = idx + 1, pWalk = pCurr - 1; (idx2 < nMods) && IsShimDll ((LPMODULE)(pWalk->hLib)); idx2++, pWalk--) {
                DEBUGMSG (ZONE_NOTIFYLIST, (_T("   pWalk: %s\r\n"), ((LPMODULE)(pWalk->hLib))->lpszModName));

                blockptr = (struct ImpHdr *)(ZeroPtr (((BYTE *)((LPMODULE)(pCurr->hLib))->BasePtr)+((LPMODULE)(pCurr->hLib))->e32.e32_unit[IMP].rva));
                while (blockptr->imp_lookup) {
                    g_Imports.KAsciiToUnicode (imp_lookupw, (LPCHAR)((LPMODULE)(pCurr->hLib))->BasePtr+blockptr->imp_dllname, MAX_DLLNAME_LEN);
                    DEBUGMSG (ZONE_NOTIFYLIST, (_T("      imp_lookup: %s\r\n"), imp_lookupw));

                    fSwapped = FALSE;
                    if (!vrf_wcsicmp (imp_lookupw, ((LPMODULE)(pWalk->hLib))->lpszModName)) {
                        DEBUGMSG (ZONE_NOTIFYLIST, (_T("---> Swapping '%s' and '%s'\r\n"),
                            ((LPMODULE)(pCurr->hLib))->lpszModName,
                            ((LPMODULE)(pWalk->hLib))->lpszModName));

                        // Swap the two modules
                        memcpy (& DllMainInfoTemp, pCurr, sizeof (DLLMAININFO));
                        memcpy (pCurr, pWalk, sizeof (DLLMAININFO));
                        memcpy (pWalk, & DllMainInfoTemp, sizeof (DLLMAININFO));

                        fSwapped = TRUE;

                        break;
                    }

                    blockptr ++;
                }

                if (fSwapped)
                    break;
            }
        }

        if (ZONE_NOTIFYLIST) {
            // Print out the new list
            RETAILMSG (1, (_T("New list:\r\n")));
            for (idx = 0; idx < nMods; idx ++) {
                RETAILMSG (1, (_T("%02u: %s (%s)\r\n"), idx, ((PMODULE)pList[idx].hLib)->lpszModName,
                    IsShimDll ((PMODULE) pList[idx].hLib) ? _T("shim") : _T("normal")));
            }
        }
    }

    return TRUE;
}

void
ShimCloseModule(
    PMODULE pMod
    )
{
    // Note: Most of pMod is invalid already (including lpszName)
    DEBUGMSG (ZONE_ENTRY, (TEXT("++ShimCloseModule 0x%08x\r\n"), pMod));

    if (pMod && pMod->pShimInfo) {
        g_Imports.FreeMem (pMod->pShimInfo, HEAP_SHIMREF);
        pMod->pShimInfo = NULL;
    }
}

void
ShimCloseProcess(
    HANDLE hProc
    )
{
    PMODULELIST pList;
    LPSHIMINFO pShimInfo;
    LPSHIMINFO pShimInfoTemp;

    DEBUGMSG (ZONE_ENTRY, (TEXT("++ShimCloseProcess 0x%08x\r\n"), hProc));

    // Walk the list of dll's, freeing up the list of applied shims, if needed.
    for (pList = pCurProc->pLastModList; pList; pList = pList->pNext) {
        if (pList->pMod->pShimInfo) {
            pShimInfo = ((LPSHIMREF)pList->pMod->pShimInfo)->pShimInfo [pCurProc->procnum];
            while (pShimInfo) {
                pShimInfoTemp = pShimInfo;
                pShimInfo = pShimInfo->pNext;
                g_Imports.FreeMem (pShimInfoTemp, pShimInfoTemp->wPool);
            }
            ((LPSHIMREF)pList->pMod->pShimInfo)->pShimInfo [pCurProc->procnum] = NULL;
        }
    }
}

BOOL static
DeInitLibrary(
    FARPROC pfnKernelLibIoControl
    )
{
    VerifierExportTable exports;

    // Zero out all the pointers to exported functions
    memset (& exports, 0, sizeof (VerifierExportTable));
    pfnKernelLibIoControl ((HANDLE) KMOD_VERIFIER, IOCTL_VERIFIER_REGISTER, & exports, sizeof (VerifierExportTable), NULL, 0, NULL);

    return TRUE;
}

//------------------------------------------------------------------------------
// Hook up function pointers and initialize logging
//------------------------------------------------------------------------------
BOOL static
InitLibrary(
    FARPROC pfnKernelLibIoControl
    )
{
    VerifierExportTable exports;

    //
    // KernelLibIoControl provides the back doors we need to obtain kernel
    // function pointers and register logging functions.
    //

    // Get imports from kernel
    g_Imports.dwVersion = VERIFIER_IMPORT_VERSION;
    if (!pfnKernelLibIoControl((HANDLE)KMOD_VERIFIER, IOCTL_VERIFIER_IMPORT, &g_Imports,
        sizeof(VerifierImportTable), NULL, 0, NULL)) {
        // Can't DEBUGMSG or anything here b/c we need the import table to do that
        return FALSE;
    }

    // Register logging functions with the kernel
    exports.dwVersion = VERIFIER_EXPORT_VERSION;
    exports.pfnShimInitModule = (FARPROC) ShimInitModule;
    exports.pfnShimWhichMod = (FARPROC) ShimWhichMod;
    exports.pfnShimUndoDepends = (FARPROC) ShimUnDoDepends;
    exports.pfnShimGetProcModList = (FARPROC) ShimGetProcModList;
    exports.pfnShimCloseModule = (FARPROC) ShimCloseModule;
    exports.pfnShimCloseProcess = (FARPROC) ShimCloseProcess;
    exports.pfnShimIoControl = (FARPROC) ShimIoControl;

    if (!pfnKernelLibIoControl((HANDLE)KMOD_VERIFIER, IOCTL_VERIFIER_REGISTER, &exports,
        sizeof(VerifierExportTable), NULL, 0, NULL)) {
        RETAILMSG(1, (TEXT("Verifier: Unable to register logging functions with kernel\r\n")));
        WIN32CALL(SetLastError, (ERROR_ALREADY_EXISTS));
        return FALSE;
    }

    // Get coredll's flag indicating whether or not the process is exiting
    hCoreDll = (HANDLE) g_Imports.LoadOneLibraryW (L"coredll.dll",LLIB_NO_PAGING,0);
    pIsExiting = (LPDWORD) g_Imports.GetProcAddressA (hCoreDll, (LPCSTR)159); // IsExiting

    // Get some string manipulation functions
    #pragma warning (push)
    #pragma warning (disable : 4047)
    vrf_wcslen = g_Imports.GetProcAddressA (hCoreDll, "wcslen");
    vrf_wcscpy = g_Imports.GetProcAddressA (hCoreDll, "wcscpy");
    vrf_wcsncpy = g_Imports.GetProcAddressA (hCoreDll, "wcsncpy");
    vrf_tolower = g_Imports.GetProcAddressA (hCoreDll, "tolower");
    vrf_wcsnicmp = g_Imports.GetProcAddressA (hCoreDll, "_wcsnicmp");
    vrf_wcsicmp = g_Imports.GetProcAddressA (hCoreDll, "_wcsicmp");

    // Initialize the linked list cs
    g_Imports.InitializeCriticalSection (& llcs);

    if (!(vrf_wcslen && vrf_wcscpy && vrf_wcsncpy && vrf_tolower && vrf_wcsnicmp && vrf_wcsicmp)) {
        RETAILMSG(1, (TEXT("--------------------------------------------------------------------------------\r\n")));
        RETAILMSG(1, (TEXT("Shim engine not activated - error getting functions\r\n")));
        RETAILMSG(1, (TEXT("--------------------------------------------------------------------------------\r\n")));
        return FALSE;
    }
    #pragma warning (pop)

    g_Imports.RegisterDbgZones (gDllInstance, & dpCurSettings);

    RETAILMSG(1, (TEXT("--------------------------------------------------------------------------------\r\n")));
    RETAILMSG(1, (TEXT("Shim engine activated.\r\n")));
    RETAILMSG(1, (TEXT("--------------------------------------------------------------------------------\r\n")));

    g_fInit = TRUE;
    return TRUE;
}

//------------------------------------------------------------------------------
// DLL entry

BOOL WINAPI
VerifierDLLEntry(HINSTANCE DllInstance, INT Reason, LPVOID Reserved)
{
    switch (Reason) {
    case DLL_PROCESS_ATTACH:
        if (!g_fInit) {
            // Reserved parameter is a pointer to KernelLibIoControl function
            gDllInstance = DllInstance;
            if (!InitLibrary((FARPROC)Reserved)) {
                DeInitLibrary ((FARPROC)Reserved);
                return FALSE;
            }
        }
        break;
    }

    return TRUE;
}

