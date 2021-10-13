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
/*
 *              NK Kernel loader code
 *
 *
 * Module Name:
 *
 *              nkstub.c
 *
 * Abstract:
 *
 *              This file implements the NK kernel stub routines to be used in OAL and KITL
 *
 */

#include "kernel.h"

#undef INTERRUPTS_OFF
#undef INTERRUPTS_ON

PNKGLOBAL g_pNKGlobal;

void NKOutputDebugString (LPCWSTR str)
{
    g_pNKGlobal->pfnWriteDebugString (str);
}

void NKvDbgPrintfW  (LPCWSTR lpszFmt, va_list lpParms)
{
    g_pNKGlobal->pfnNKvDbgPrintfW (lpszFmt, lpParms);
}

void WINAPIV NKDbgPrintfW (LPCWSTR lpszFmt, ...)
{
    va_list arglist;
    va_start(arglist, lpszFmt);
    g_pNKGlobal->pfnNKvDbgPrintfW (lpszFmt, arglist);
    va_end(arglist);
}

int NKwvsprintfW  (LPWSTR lpOut, LPCWSTR lpFmt, va_list lpParms, int maxchars)
{
    return g_pNKGlobal->pfnNKwvsprintfW (lpOut, lpFmt, lpParms, maxchars);
}

void NKSetLastError (DWORD dwErr)
{
    g_pNKGlobal->pfnSetLastError (dwErr);
}

DWORD NKGetLastError (void)
{
    return g_pNKGlobal->pfnGetLastError ();
}

void WINAPI EnterCriticalSection (LPCRITICAL_SECTION lpcs)
{
    g_pNKGlobal->pfnEnterCS (lpcs);
}

void WINAPI LeaveCriticalSection (LPCRITICAL_SECTION lpcs)
{
    g_pNKGlobal->pfnLeaveCS (lpcs);
}

void WINAPI InitializeCriticalSection (LPCRITICAL_SECTION lpcs)
{
    g_pNKGlobal->pfnInitializeCS (lpcs);
}

void WINAPI DeleteCriticalSection (LPCRITICAL_SECTION lpcs)
{
    g_pNKGlobal->pfnDeleteCS (lpcs);
}

void INTERRUPTS_OFF (void)
{
    g_pNKGlobal->pfnINT_OFF ();
}

void INTERRUPTS_ON (void)
{
    g_pNKGlobal->pfnINT_ON ();
}

BOOL INTERRUPTS_ENABLE (BOOL fEnable)
{
    return g_pNKGlobal->pfnINT_ENABLE (fEnable);
}

BOOL HookInterrupt (int irq, FARPROC pfnHandler)
{
    return g_pNKGlobal->pfnHookInterrupt (irq, pfnHandler);
}

BOOL UnhookInterrupt (int irq, FARPROC pfnHandler)
{
    return g_pNKGlobal->pfnUnhookInterrupt (irq, pfnHandler);
}

DWORD NKCallIntChain (BYTE irq)
{
    return g_pNKGlobal->pfnNKCallIntChain (irq);
}

BOOL NKIsSysIntrValid (DWORD idInt)
{
    return g_pNKGlobal->pfnNKIsSysIntrValid (idInt);
}

BOOL NKSetInterruptEvent (DWORD idInt)
{
    return g_pNKGlobal->pfnNKSetInterruptEvent (idInt);
}

void NKSleep (DWORD cmsec)
{
    g_pNKGlobal->pfnSleep (cmsec);
}

BOOL NKVirtualSetAttributes (LPVOID lpvAddress, DWORD cbSize, DWORD dwNewFlags, DWORD dwMask, LPDWORD lpdwOldFlags)
{
    return g_pNKGlobal->pfnVMSetAttrib (lpvAddress, cbSize, dwNewFlags, dwMask, lpdwOldFlags);
}

LPVOID NKCreateStaticMapping (DWORD dwPhysBase, DWORD cbSize)
{
    return g_pNKGlobal->pfnCreateStaticMapping (dwPhysBase, cbSize);
}

BOOL NKDeleteStaticMapping (LPVOID pVirtAddr, DWORD cbSize)
{
    return g_pNKGlobal->pfnDeleteStaticMapping (pVirtAddr, cbSize);
}

LONG NKRegCloseKey (HKEY hKey)
{
    return g_pNKGlobal->pfnRegCloseKey (hKey);
}

LONG NKRegFlushKey (HKEY hKey)
{
    return g_pNKGlobal->pfnRegFlushKey (hKey);
}

LONG NKRegCreateKeyExW (HKEY hKey, LPCWSTR lpSubKey, DWORD Reserved, LPWSTR lpClass, DWORD dwOptions, REGSAM samDesired, LPSECURITY_ATTRIBUTES lpsa, PHKEY phkResult, LPDWORD lpdwDisp)
{
    return g_pNKGlobal->pfnRegCreateKeyExW (hKey, lpSubKey, Reserved, lpClass, dwOptions, samDesired, lpsa, phkResult, lpdwDisp);
}

LONG NKRegOpenKeyExW (HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult)
{
    return g_pNKGlobal->pfnRegOpenKeyExW (hKey, lpSubKey, ulOptions, samDesired, phkResult);
}

LONG NKRegQueryValueExW (HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData)
{
    return g_pNKGlobal->pfnRegQueryValueExW (hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
}

LONG NKRegSetValueExW (HKEY hKey, LPCWSTR lpValueName, DWORD dwReserved, DWORD dwType, LPBYTE lpData, DWORD cbData)
{
    return g_pNKGlobal->pfnRegSetValueExW (hKey, lpValueName, dwReserved, dwType, lpData, cbData);
}

void NKForceCleanBoot (void)
{
    g_pNKGlobal->pfnNKForceCleanBoot ();
}

BOOL NKReboot (BOOL fClean)
{
    return g_pNKGlobal->pfnNKReboot (fClean);
}

DWORD NKwcslen (LPCWSTR wcs)
{
    return g_pNKGlobal->pfnNKwcslen (wcs);
}

int NKwcscmp (LPCWSTR str1, LPCWSTR str2)
{
    return g_pNKGlobal->pfnNKwcscmp (str1, str2);
}

int NKwcsicmp (LPCWSTR str1, LPCWSTR str2)
{
    return g_pNKGlobal->pfnNKwcsicmp (str1, str2);
}

int NKwcsnicmp (LPCWSTR str1, LPCWSTR str2, int cchLen)
{
    return g_pNKGlobal->pfnNKwcsnicmp (str1, str2, cchLen);
}

void NKwcscpy (LPWSTR dst, LPCWSTR src)
{
    g_pNKGlobal->pfnNKwcscpy (dst, src);
}

DWORD NKwcsncpy (LPWSTR dst, LPCWSTR src, DWORD cchLen)
{
    return g_pNKGlobal->pfnNKwcsncpy (dst, src, cchLen);
}

int NKstrcmpiAandW (LPCSTR pascii, LPCWSTR pUnicode)
{
    return g_pNKGlobal->pfnNKstrcmpiAandW (pascii, pUnicode);
}

void NKUnicodeToAscii (LPSTR chptr, LPCWSTR wchptr, int maxlen)
{
    g_pNKGlobal->pfnNKUnicodeToAscii (chptr, wchptr, maxlen);
}

void NKAsciiToUnicode (LPWSTR wchptr, LPCSTR chptr, int maxlen)
{
    g_pNKGlobal->pfnNKAsciiToUnicode (wchptr, chptr, maxlen);
}

BOOL NKSystemTimeToFileTime (const SYSTEMTIME *pst, LPFILETIME pft)
{
    return g_pNKGlobal->pfnSystemTimeToFileTime (pst, pft);
}

BOOL NKFileTimeToSystemTime (const FILETIME *pft, LPSYSTEMTIME pst)
{
    return g_pNKGlobal->pfnFileTimeToSystemTime (pft, pst);
}

BOOL NKCompareFileTime (const FILETIME *pft1, const FILETIME *pft2)
{
    return g_pNKGlobal->pfnCompareFileTime (pft1, pft2);
}

void ProfilerHit (UINT addr)
{
    OEMProfilerData data;
    data.ra = addr;
    data.dwBufSize = 0;
    g_pNKGlobal->pfnProfilerHitEx (&data);
}

void ProfilerHitEx (OEMProfilerData *pData)
{
    g_pNKGlobal->pfnProfilerHitEx (pData);
}


DWORD GetEPC (void)
{
    return g_pNKGlobal->pfnGetEPC ();
}

LPVOID NKPhysToVirt (DWORD dwShiftedPhysAddr, BOOL fCached)
{
    return g_pNKGlobal->pfnPhysToVirt (dwShiftedPhysAddr, fCached);
}

DWORD NKVirtToPhys (LPCVOID pVirtAddr)
{
    return g_pNKGlobal->pfnVirtToPhys (pVirtAddr);
}

void NKSendInterProcessorInterrupt (DWORD dwType, DWORD dwTarget, DWORD dwCommand, DWORD dwData)
{
    g_pNKGlobal->pfnNKSendIPI (dwType, dwTarget, dwCommand, dwData);
}

void NKAcquireOalSpinLock (void)
{
    g_pNKGlobal->pfnAcquireOalSpinLock ();
}

void NKReleaseOalSpinLock (void)
{
    g_pNKGlobal->pfnReleaseOalSpinLock ();
}


/*
 * CRT storage access, currently used by the VFP support code
 */

#ifdef ARM

DWORD* __crt_get_storage_fsr()
{
    return &((crtGlob_t*)UTlsPtr()[TLSSLOT_RUNTIME])->fsr;
}

unsigned int _get_fsr()
{
    return *__crt_get_storage_fsr();
}

void _set_fsr(unsigned int fsr)
{
    *__crt_get_storage_fsr() = fsr;
}

#endif // ARM


DWORD* __crt_get_storage_flags()
{
    return &((crtGlob_t*)UTlsPtr()[TLSSLOT_RUNTIME])->dwFlags;
}


/*
 * Kernel flags access
 */
DWORD* __crt_get_kernel_flags()
{
    return &UTlsPtr()[TLSSLOT_KERNEL];
}


/*
 * compiler /GS runtime
 */
void __report_gsfailure(void)
{
    g_pNKGlobal->pfn__report_gsfailure();
}

/*
 * The global security cookie.  This name is known to the compiler.
 * Initialize to a garbage non-zero value just in case we have a buffer overrun
 * in any code that gets run before __security_init_cookie() has a chance to
 * initialize the cookie to the final value.
 */
__declspec(selectany) DWORD_PTR __security_cookie = DEFAULT_SECURITY_COOKIE;
__declspec(selectany) DWORD_PTR __security_cookie_complement = ~DEFAULT_SECURITY_COOKIE;


#ifdef x86

BOOL NKwrmsr (
    DWORD dwAddr,       // Address of MSR being written
    DWORD dwValHigh,    // Upper 32 bits of value being written
    DWORD dwValLow      // Lower 32 bits of value being written
    )
{
    return g_pNKGlobal->pfnX86wrmsr (dwAddr, dwValHigh, dwValLow);
}

BOOL NKrdmsr (
    DWORD dwAddr,       // Address of MSR being read
    DWORD *lpdwValHigh, // Receives upper 32 bits of value, can be NULL
    DWORD *lpdwValLow   // Receives lower 32 bits of value, can be NULL
    )
{
    return g_pNKGlobal->pfnX86rdmsr (dwAddr, lpdwValHigh, lpdwValLow);
}

BOOL HookIpi (int irq, FARPROC pfnHandler)
{
    return g_pNKGlobal->pfnHookIpi (irq, pfnHandler);
}
#endif


#ifdef ARM

#define IDX_DABORT      4

DWORD GetOriginalDAbortHandlerAddress (void)
{
    return g_pNKGlobal->pdwOrigVectors[IDX_DABORT];
}

PFNVOID NKSetDataAbortHandler(PFNVOID pfnDataAbortHandler)
{
    PFNVOID pfnCurr = (PFNVOID) g_pNKGlobal->pdwCurrVectors[IDX_DABORT];
    g_pNKGlobal->pdwCurrVectors[IDX_DABORT] = (DWORD) pfnDataAbortHandler;
    g_pOemGlobal->pfnCacheRangeFlush (NULL, 0, CACHE_SYNC_INSTRUCTIONS|CACHE_SYNC_DISCARD);
    return pfnCurr;
}

#endif


/* access to KData */
DWORD __GetUserKData (DWORD dwOfst)
{
    DWORD dwRet;
#if defined (ARM) || defined (x86)
    dwRet = (dwOfst < MAX_PCB_OFST)? PCBGetDwordAtOffset (dwOfst) : *(LPDWORD) (PUserKData+dwOfst);
#else
    dwRet = *(LPDWORD) (PUserKData+dwOfst);
#endif
    return dwRet;
}


void NKInitInterlockedFuncs (void)
{
    InitInterlockedFunctions ();
#ifdef ARM
    InitPCBFunctions ();
#endif
}


