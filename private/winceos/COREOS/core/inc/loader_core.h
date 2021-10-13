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
#ifndef _LOADER_COREDLL_H_
#define _LOADER_COREDLL_H_

#ifdef KCOREDLL
#define WIN32_CALL(type, api, args) (*(type (*) args) g_ppfnWin32Calls[W32_ ## api])
#define GetHeaderInfo(pMod, idx)    (&((((PMODULE)(pMod))->e32.e32_unit[idx])))
#else
#define WIN32_CALL(type, api, args) IMPLICIT_DECL(type, SH_WIN32, W32_ ## api, args)
#define GetHeaderInfo(pMod, idx)    (&((((PUSERMODULELIST)(pMod))->udllInfo[idx])))
#endif

#define GetExportInfo(pMod)         GetHeaderInfo(pMod, EXP)
#define GetImportInfo(pMod)         GetHeaderInfo(pMod, IMP)
#define GetResInfo(pMod)            ((pMod)? GetHeaderInfo(pMod, RES) : NULL)

#define NKFreeLibrary       WIN32_CALL(BOOL, FreeLibrary, (HMODULE hInst))
#define NKLoadLibraryEx     WIN32_CALL(HMODULE, LoadLibraryEx, (LPCWSTR lpLibFileName, DWORD dwFlags, PUSERMODULELIST pMod))

#define MAX_DLLNAME_LEN     MAX_PATH
#define MAX_IMPNAME_LEN     128
#define MAX_AFE_FILESIZE    MAX_DLLNAME_LEN      // max name length of DLL redirection

typedef const struct ExpHdr *PCExpHdr;
typedef const struct ImpHdr *PCImpHdr;
typedef const struct ImpProc *PCImpProc;
typedef const e32_lite *PCE32;

#ifdef KCOREDLL
#define DoLoadLibrary(pszFileName, dwFlags)     ((PUSERMODULELIST) NKLoadLibraryEx (pszFileName, dwFlags, NULL))
#define DoFreeLibrary(pMod)                     NKFreeLibrary ((HMODULE) pMod)
#else
PUSERMODULELIST
DoLoadLibrary (
    LPCWSTR pszFileName,
    DWORD dwFlags
    );

extern DLIST g_loadOrderList;

#define GetModFromLoadOrderList(pdl)    ((PUSERMODULELIST) ((DWORD)(pdl) - offsetof(USERMODULELIST, loadOrderLink)))

#endif

DWORD
ResolveImpOrd (
    PUSERMODULELIST pMod,
    PCExpHdr    expptr,
    DWORD       ord
    );

DWORD
ResolveImpHintStr(
    PUSERMODULELIST pMod,
    PCExpHdr    expptr,
    DWORD       hint,
    LPCSTR      str
    );

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
inline void AsciiToUnicode (LPWSTR wchptr, LPCSTR chptr, int maxlen)
{
    while ((maxlen-- > 1) && *chptr) {
        *wchptr++ = (WCHAR)*chptr++;
    }
    *wchptr = 0;
}

inline BOOL IsResValid (PCInfo pResInfo, HRSRC hrsrc)
{
    return pResInfo && ((DWORD) hrsrc - pResInfo->rva < pResInfo->size);
}



#endif // _LOADER_COREDLL_H

