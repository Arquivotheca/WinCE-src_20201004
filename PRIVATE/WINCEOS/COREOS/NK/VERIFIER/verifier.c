//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
        TEXT("Entry"), TEXT("Entry2"), TEXT("Search"), TEXT("<unused>"),
        TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"),
        TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"),
        TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>"), TEXT("<unused>") },
    0x00000000};

#undef ZONE_ENTRY
#define ZONE_ENTRY DEBUGZONE(0)
#define ZONE_ENTRY2 DEBUGZONE(1)
#define ZONE_SEARCH DEBUGZONE(2)

//------------------------------------------------------------------------------
// EXPORTED GLOBAL VARIABLES

//------------------------------------------------------------------------------
// MISC

// Inputs from kernel
VerifierImportTable g_Imports;

// We don't need to protect our globals, since we're already protected by LLCs.
BOOL g_fInit = FALSE;
BOOL g_fLoadingShim = FALSE;
BOOL g_fUnLoadingShim = FALSE;
TCHAR g_szLoadingModule [MAX_PATH];
HINSTANCE gDllInstance;

//------------------------------------------------------------------------------
// Somewhat more abstracted input interface (yuck)

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

//
// Local version of some CRT functions...
//

static size_t vrf_wcslen (const wchar_t * wcs) {
	const wchar_t *eos = wcs;
	while( *eos++ )
		;
	return( (size_t)(eos - wcs - 1) );
}

static wchar_t *vrf_wcscpy (wchar_t *strDest, const wchar_t *strSource)
{
    wchar_t * cp = strDest;
    while( *cp++ = *strSource++ )
        ;		/* Copy src over dst */
    return( strDest );
}

static wchar_t *vrf_wcsncpy (wchar_t *strDest, const wchar_t *strSource, size_t count)
{
    wchar_t *start = strDest;

    while (count && (*strDest++ = *strSource++))
        count--;

    if (count)
        while (--count)
            *strDest++ = '\0';

    return (start);
}

static wchar_t vrf_tolower(wchar_t c) {
    return ((c>=L'A') && (c<=L'Z')) ? (c-L'A'+L'a') : c;
}

static int vrf_wcsnicmp(const wchar_t *psz1, const wchar_t *psz2, size_t cb) {
    wchar_t ch1 = 0, ch2 = 0; // Zero for case cb = 0.
    while (cb--) {
        ch1 = vrf_tolower(*(psz1++));
        ch2 = vrf_tolower(*(psz2++));
        if (!ch1 || (ch1 != ch2))
        	break;
    }
    return (ch1 - ch2);
}

static int vrf_wcsicmp(const wchar_t *str1, const wchar_t*str2) {
    wchar_t ch1, ch2;
    for (;*str1 && *str2; str1++, str2++) {
        ch1 = vrf_tolower(*str1);
        ch2 = vrf_tolower(*str2);
        if (ch1 != ch2)
            return ch1 - ch2;
    }
    // Check last character.
    return vrf_tolower(*str1) - vrf_tolower(*str2);
}

BOOL IsFilesysReady (void)
{
    // Can only check the local registry if the filesys API is registered.
    return (UserKInfo[KINX_API_MASK] & (1 << SH_FILESYS_APIS)) != 0;
}

//------------------------------------------------------------------------------
//

BOOL HookImportsFromThisModule (DWORD BaseAddr, LPCTSTR szModuleName, LPTSTR szRegKeyRoot, DWORD cchData)
{
    WCHAR szModuleNameBare [MAX_PATH];
    size_t cchModuleNameBare;
    WCHAR szProcessName [MAX_PATH];
    const WCHAR *p;
    const WCHAR *p2;
    WCHAR szKeyName [MAX_PATH] = { TEXT("ShimEngine\\") };
    LONG lRet;
    HKEY hKey;
    DWORD dwType;
    DWORD cbData;
    BOOL fProcess;
    BOOL fAllowInheritance;
    BOOL fRet = FALSE;

    DEBUGMSG(ZONE_ENTRY2, (TEXT("++HookImportsFromThisModule (0x%08x, %s)\r\n"), BaseAddr, szModuleName));

    if (!IsFilesysReady ())
        return FALSE;

    // Don't try to alter a shim dll's imports.
    if (g_fLoadingShim) {
        DEBUGMSG(1, (TEXT("Never alter a shim's imports\r\n")));
        return FALSE;
    }

    // Find the name (no path).
    for (p = p2 = szModuleName; *p; p++) {
        if ((*p == L'\\') || (*p == L'/'))
            p2 = p+1;
    }

    // p points past the end of szModuleName
    // p2 points to the name portion of szModuleName

    if (!vrf_wcsnicmp (p2, TEXT("lmemdebug"), 9)) {
        DEBUGMSG (1, (TEXT("Never alter lmemdebug's imports\r\n")));
        return FALSE;
    }

    // Copy the name of the module (no path) into szModuleNameBare.
    vrf_wcscpy (szModuleNameBare, p2);

    // Is this an exe or dll? Make sure it has an extension
    p -= 4;

    if ((p > szModuleName) && !vrf_wcsnicmp (p, TEXT(".exe"), 4)) {
        fProcess = TRUE;
    }
    else if ((p > szModuleName) && !vrf_wcsnicmp (p, TEXT(".dll"), 4)) {
        fProcess = FALSE;
    }
    else if ((p > szModuleName) && !vrf_wcsnicmp (p, TEXT(".cpl"), 4)) {
        fProcess = FALSE;
    }
    else {
        cchModuleNameBare = vrf_wcslen (szModuleNameBare);

        if (BaseAddr == 0x00010000) {
            fProcess = TRUE;
            vrf_wcscpy (szModuleNameBare + cchModuleNameBare, TEXT(".exe"));
        }
        else {
            fProcess = FALSE;
            vrf_wcscpy (szModuleNameBare + cchModuleNameBare, TEXT(".dll"));
        }
    }

    // Look for the registry key HKLM\ShimEngine\<module name>

    vrf_wcsncpy (& szKeyName [11], szModuleNameBare, MAX_PATH - 11);

    lRet = RegOpenKeyEx (HKEY_LOCAL_MACHINE, szKeyName, 0, 0, & hKey);

    // This module (exe or dll) is tagged for shimming.
    if (ERROR_SUCCESS == lRet) {
        DEBUGMSG(ZONE_SEARCH, (TEXT("This module (%s) is tagged for shimming.\r\n"), szModuleName));
        fRet = TRUE;
        goto done;
    }

    // We are doing imports for a process, and it is not tagged for shimming.
    if (fProcess) {
        DEBUGMSG(ZONE_SEARCH, (TEXT("This module (%s) is not tagged for shimming.\r\n"), szModuleName));
        fRet = FALSE;
        goto done;
    }

    // If we've reached this point, this must be a dll.
    // See if the process is tagged for shimming...
    WIN32CALL (GetModuleFileNameW, (NULL, szProcessName, MAX_PATH));
    for (p = p2 = szProcessName; *p; p++)
        if ((*p == L'\\') || (*p == L'/'))
            p2 = p + 1;

    // p points past the end of szProcessName
    // p2 points to the name portion of szProcessName

    vrf_wcsncpy (& szKeyName [11], p2, MAX_PATH - 11);

    lRet = RegOpenKeyEx (HKEY_LOCAL_MACHINE, szKeyName, 0, 0, & hKey);

    // The process is not tagged for shimming, either.
    if (ERROR_SUCCESS != lRet) {
        DEBUGMSG(ZONE_SEARCH, (TEXT("Neither this module (%s) nor the process (%s) are tagged for shimming.\r\n"), szModuleName, p2));
        fRet = FALSE;
        goto done;
    }

    // The process is tagged for shimming. Does it allow inheritance?
    cbData = sizeof (BOOL);
    lRet = RegQueryValueEx (HKEY_LOCAL_MACHINE, TEXT("ShimModules"), szKeyName, & dwType, & fAllowInheritance, & cbData);

    // Does the registry specify whether to allow inheritance?
    if (ERROR_SUCCESS == lRet) {
        DEBUGMSG(ZONE_SEARCH, (TEXT("This process (%s) does %sallow inheritance.\r\n"), p2, fAllowInheritance ? _T("") : _T("not ")));
        fRet = fAllowInheritance;
        goto done;
    }

    // Default inheritance
    fRet = FALSE;
    DEBUGMSG(ZONE_SEARCH, (TEXT("This process (%s) does allow inheritance (default).\r\n"), p2, fRet ? _T("") : _T("not ")));

done:

    // If the caller provided a buffer, copy the registry path to the root of
    // this module's shim info.
    if (fRet && szRegKeyRoot && cchData) {
        vrf_wcsncpy (szRegKeyRoot, szKeyName, cchData);
    }

    if (hKey)
        RegCloseKey (hKey);

    return fRet;
}

BOOL InitLoaderHook (DWORD BaseAddr, LPCTSTR szModuleName)
{
    BOOL fRet;

    fRet = HookImportsFromThisModule (BaseAddr, szModuleName, NULL, 0);

    // Make sure we acknowledge the fact that we're using imports that weren't
    // originally intended.

    RETAILMSG(fRet, (TEXT("-----> Using alternate imports for module %s\r\n"), szModuleName));

    return fRet;
}

static PCHAR _ustoa (unsigned short int n, PCHAR pszOut)
{
    DWORD d4, d3, d2, d1, d0, q;
    PCHAR p;
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
        pszOut [0] = (unsigned short) d4 + '0';
        pszOut [1] = (unsigned short) d3 + '0';
        pszOut [2] = (unsigned short) d2 + '0';
        pszOut [3] = (unsigned short) d1 + '0';
        pszOut [4] = (unsigned short) d0 + '0';

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

PMODULE
WhichMod (
    PMODULE pmod,
    LPCTSTR modname,
    LPCTSTR impmodname,
    DWORD BaseAddr,
    DWORD ord
    )
{
    PMODULE pModRet;
    struct ImpProc *impptr;
    WCHAR szRegKeyRoot [MAX_PATH];
    CHAR aszValueName [MAX_PATH];
    WCHAR szValueName [MAX_PATH];
    WCHAR szShim [MAX_PATH];
    LONG lRet;
    DWORD dwType;
    DWORD cbData;
    const WCHAR *p;
    WCHAR *p2;

    DEBUGMSG(ZONE_ENTRY2, (TEXT("++WhichMod (0x%08x, %s, %s, 0x%08x, 0x%08x)\r\n"),
        pmod, modname, impmodname, BaseAddr, ord));

    // Is this module (modname) tagged for shimming?
    if (!HookImportsFromThisModule (BaseAddr, modname, szRegKeyRoot, MAX_PATH)) {
        return pmod;
    }

    // Get the name of the dependent module (module which is being imported from).
    // Copy into a wchar buffer.
    for (p = impmodname, p2 = szValueName; *p; p++, p2++)
        *p2 = *p;

    // Append '-'
    *p2++ = TEXT('-');

    // Append the imported ordinal (name or number).
    if (ord & 0x80000000) {
        // Importing by ordinal
        _ustoa ((unsigned short)(ord & 0x7fffffff), aszValueName);
        g_Imports.KAsciiToUnicode (p2, aszValueName, MAX_PATH);
    }
    else {
        // Importing by name
        impptr = (struct ImpProc *)((ord&0x7fffffff)+BaseAddr);
        g_Imports.KAsciiToUnicode (p2, (LPCHAR)impptr->ip_name, 38);
    }

    // szValueName now contains a string representing the function being imported,
    // in the form 'module-ordinal'

    cbData = MAX_PATH;
    szShim [0] = 0;
    lRet = RegQueryValueEx (HKEY_LOCAL_MACHINE, szValueName, szRegKeyRoot, & dwType, szShim, & cbData);
    if (ERROR_SUCCESS != lRet) {
        // This import is not shimmed.
        pModRet = pmod;
    }
    else {
        // This import is to be shimmed. szShim contains the name of the shim.

        g_fLoadingShim = TRUE;
        if (g_szLoadingModule [0]) {
            DEBUGMSG (1, (_T("WhichMod: Overwriting g_szLoadingModule (%s)\r\n"), g_szLoadingModule));
        }
        vrf_wcsncpy (g_szLoadingModule, modname, MAX_PATH);
        pModRet = (PMODULE) g_Imports.LoadOneLibraryW (szShim, 1, 0);
        g_szLoadingModule [0] = _T('\0');
        g_fLoadingShim = FALSE;

        if (!pModRet) {
            // Loading the shim failed. Import from the original module.
            pModRet = pmod;
        }
        else {
            // Notify the shim of the module that's causing the shim to be loaded.
            // Will be called once for each import.
//            g_Imports.CallDLLEntry (pModRet, SHIM_DLL_ATTACH, modname);
        }
    }

    RETAILMSG((pModRet != pmod) && !g_fUnLoadingShim, (TEXT("*** DoImports: importing %s from %s\r\n"), szValueName, szShim));
    return pModRet;
}

BOOL
GetNameFromE32(
    e32_lite *eptr,
    LPWSTR lpszModuleName,
    DWORD cchModuleName
    )
{
    PMODULE pMod;

    if (eptr == & pCurProc->e32) {
        vrf_wcsncpy (lpszModuleName, pCurProc->lpszProcName, cchModuleName);
        return TRUE;
    }

    // We're already holding the loader CS.
    for (pMod = pModList; pMod; pMod = pMod->pMod) {
        if (eptr == & pMod->e32) {
            vrf_wcsncpy (lpszModuleName, pMod->lpszModName, cchModuleName);
            return TRUE;
        }
    }

    RETAILMSG (1, (TEXT("Couldn't find module with eptr=0x%08x\r\n"), eptr));
    return FALSE;
}

BOOL
IsShimDll(
    PMODULE pMod
    )
{
    return WIN32CALL(GetProcAddressA, (pMod, "QueryShimInfo")) ? TRUE : FALSE;
}

#define MAX_DLLNAME_LEN 32

BOOL
UndoDepends(
    e32_lite *eptr,
    DWORD BaseAddr
    )
{
    TCHAR szModuleName [MAX_PATH];
    WCHAR ucptr[MAX_DLLNAME_LEN];
    struct ImpHdr *blockptr, *blockstart;
    LPDWORD ltptr;
    PMODULE pMod;
    PMODULE pModVLog = NULL;
    PMODULE pModCoredll = NULL;

    if (!GetNameFromE32 (eptr, szModuleName, MAX_PATH))
        return FALSE;

    if (!HookImportsFromThisModule (BaseAddr, szModuleName, NULL, 0)) {
        return FALSE;
    }

    DEBUGMSG (ZONE_ENTRY, (TEXT("++UndoDepends 0x%08x 0x%08x %s\r\n"), eptr, BaseAddr, szModuleName));

    g_fUnLoadingShim = TRUE;

    blockstart = blockptr = (struct ImpHdr *)(BaseAddr+eptr->e32_unit[IMP].rva);

    while (blockptr->imp_lookup) {
        g_Imports.KAsciiToUnicode(ucptr,(LPCHAR)BaseAddr+blockptr->imp_dllname,MAX_DLLNAME_LEN);
        ltptr = (LPDWORD)(BaseAddr+blockptr->imp_lookup);
        while (*ltptr) {
            pMod = WhichMod (NULL, szModuleName, ucptr, BaseAddr, *ltptr);
            if (pMod) {
                // A LoadLibrary was done for this import - free the library now.
                g_fLoadingShim = TRUE;
                g_Imports.FreeOneLibrary (pMod, 1);

                // Actually need to do it twice - it was loaded once by the
                // loading module, and once for the above WhichMod check...
                g_Imports.FreeOneLibrary (pMod, 1);
                g_fLoadingShim = FALSE;
            }
            ltptr++;
        }
        blockptr++;
    }

    if (BaseAddr == (DWORD) pCurProc->BasePtr) {
        //
        // The process is dying. Notify all the 'hanging' dll's (dll's that were
        // loaded via LoadLibrary, and not freed). The order is important -
        // first, notify the non-shim dll's (to give them a chance to clean up
        // before a shim reports a leak), then the shims, and finally vlog.dll.
        //
        pModCoredll = (PMODULE) WIN32CALL(GetModuleHandleW, (_T("coredll.dll")));
        pModVLog = (PMODULE) WIN32CALL (GetModuleHandleW, (_T("vlog.dll")));

        for (pMod = pModList; pMod; pMod = pMod->pMod) {
            if (HasModRefProcPtr (pMod,pCurProc) && !IsShimDll (pMod) && (pMod != pModCoredll) && (pMod != pModVLog))
                g_Imports.CallDLLEntry (pMod, DLL_PROCESS_DETACH, (LPVOID) 1);
        }
        for (pMod = pModList; pMod; pMod = pMod->pMod) {
            if (HasModRefProcPtr (pMod, pCurProc) && IsShimDll (pMod))
                g_Imports.CallDLLEntry (pMod, DLL_PROCESS_DETACH, (LPVOID) 1);
        }
        if (pModVLog)
            g_Imports.CallDLLEntry (pModVLog, DLL_PROCESS_DETACH, (LPVOID) 1);
    }

    g_fUnLoadingShim = FALSE;

    return TRUE;
}

BOOL VerifierIoControl(
    DWORD   dwIoControlCode,
    LPVOID  lpInBuf,
    DWORD   nInBufSize,
    LPVOID  lpOutBuf,
    DWORD   nOutBufSize,
    LPDWORD lpBytesReturned
    )
{
    switch (dwIoControlCode) {
        case 0x10000: // IOCTL_VERIFIER_GETLOADINGMODULE
            if (g_szLoadingModule[0] && lpOutBuf) {
                vrf_wcsncpy ((wchar_t *) lpOutBuf, g_szLoadingModule, nOutBufSize / sizeof (TCHAR));
                return TRUE;
            }
            break;
        case 0x10001: //IOCTLS_VERIFIER_SETLOADINGMODULE
            if (lpInBuf && (nInBufSize <= MAX_PATH * sizeof (TCHAR))) {
                vrf_wcsncpy (g_szLoadingModule, (wchar_t *) lpInBuf, nInBufSize / sizeof (TCHAR));
                return TRUE;
            }
    }

    return FALSE;
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
    exports.pfnInitLoaderHook = (FARPROC) InitLoaderHook;
    exports.pfnWhichMod = (FARPROC) WhichMod;
    exports.pfnUndoDepends = (FARPROC) UndoDepends;
    exports.pfnVerifierIoControl = (FARPROC) VerifierIoControl;
    if (!pfnKernelLibIoControl((HANDLE)KMOD_VERIFIER, IOCTL_VERIFIER_REGISTER, &exports,
        sizeof(VerifierExportTable), NULL, 0, NULL)) {
        RETAILMSG(1, (TEXT("Verifier: Unable to register logging functions with kernel\r\n")));
        WIN32CALL(SetLastError, (ERROR_ALREADY_EXISTS));
        return FALSE;
    }

    WIN32CALL(RegisterDbgZones, (gDllInstance, & dpCurSettings));

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
            return InitLibrary((FARPROC)Reserved);
        }
        break;
    }

    return TRUE;
}

