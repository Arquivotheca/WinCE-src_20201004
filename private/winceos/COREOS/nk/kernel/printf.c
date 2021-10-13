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
 *              NK Kernel printf code
 *
 *
 * Module Name:
 *
 *              printf.c
 *
 * Abstract:
 *
 *              This file implements debug and string routines
 *
 */
 
/*
@doc    EXTERNAL KERNEL
@topic  Debug Support for Applications | 
    The kernel provides several kinds of supports for debugging
    applications. These are:
    
    <b Debug Messages>: The kernel provides API's for printing out
    of debug messages which can be turned on and off dynamically 
    using zones. Any module (DLL/Process) can register itself
    with the debug subsystem using <f DEBUGREGISTER>. This API
    registers the address of a Zonemask (which is a DWORD) with the
    kernel. Using the debug shell, a user can now dynamically turn
    bits of this zonemask on and off from the shell window.        
    The most typical way to use this is to filter debug messages
    based on these bits. A structured way to do this is to use
    the <f DEBUGZONE> macro to associate zones with bits, and then
    use the <f DEBUGMSG> function to associate each debug message
    with a zone. Type ? in the shell to see how to change zones 
    dynamically from the shell which uses the <f SetDbgZone> function
    to implement the change. See the example below for general zone
    usage.
    
    <b Asserts>: The kernel also provides for simple asserts. See
    <f DEBUGCHK> for details.
    
@ex     An example of using debug zones |
    // Declare a DBGPARAM structure & give names to all used zones
    DBGPARAM dpCurSettings = { L"foodll", {
        L"Info",       L"Validate",  L"bar",       L"random",
        L"Undefined",  L"Undefined", L"Undefined", L"Undefined",
        L"Undefined",  L"Undefined", L"Undefined", L"Undefined",
        L"Undefined",  L"Undefined", L"Undefined", L"Undefined" },
        0x00000000 };
        
    // Associate bits with a zone 
    // these should match up with the text strings above!
    #define ZONE_INFO       DEBUGZONE(0)
    #define ZONE_VALIDATE   DEBUGZONE(1)

    // Register : Assume this is a DLL 
    // A Process would do the same in their libmain
    BOOL DllEntry (HANDLE hinstDLL, DWORD fdwReason, LPVOID lpv) {
        if ( fdwReason == DLL_PROCESS_ATTACH ) {
        DEBUGREGISTER(hinstDLL);
        }
        ...
    }

    // Use the defined zone in debug messages
    DEBUGMSG (ZONE_INFO, (L"This is an illustrative messages only!"));

    // Or use a zone to turn execution of some code on & off
    if (ZONE_VALIDATE) {
        // validate some stuff ...
    }            
            
@xref   
    <f DEBUGMSG> <tab> <f RETAILMSG> <tab> <f ERRORMSG> <tab> <f DEBUGCHK> <tab>
    <f DEBUGREGISTER> <tab> <f DEBUGZONE> <tab> <t DBGPARAM> <tab>
    <f RegisterDebugZones> <tab> <f SetDbgZone>
*/ 

#include "kernel.h"
#include "spinlock.h"

static TCHAR strPrefix[] = L"PID:00000000 TID:00000000 ";

static const LPCWSTR hexMap = L"0123456789ABCDEF";

static void DwToHex (DWORD dw, LPWSTR pstr)
{
    pstr[7] = hexMap[dw & 0xf];
    pstr[6] = hexMap[(dw>>4) & 0xf];
    pstr[5] = hexMap[(dw>>8) & 0xf];
    pstr[4] = hexMap[(dw>>12) & 0xf];
    pstr[3] = hexMap[(dw>>16) & 0xf];
    pstr[2] = hexMap[(dw>>20) & 0xf];
    pstr[1] = hexMap[(dw>>24) & 0xf];
    pstr[0] = hexMap[(dw>>28) & 0xf];
}

static void DoODS (LPCWSTR str)
{
    DwToHex (dwActvProcId + PcbGetCurCpu () - 1, &strPrefix[4]);
    DwToHex (dwCurThId, &strPrefix[17]);
    OEMWriteDebugString (strPrefix);
    OEMWriteDebugString ((unsigned short *)str);
}

int 
NKwvsprintfW(
    LPWSTR lpOut,
    LPCWSTR lpFmt,
    va_list lpParms,
    int maxchars
    );

extern CRITICAL_SECTION ODScs;
SPINLOCK g_odsLock;      // used only when KITL is not present

extern volatile LONG g_nCpuReady;

static void ODSInKCall (LPCWSTR str)
{
    DEBUGCHK (str);
    if (!InPrivilegeCall ()) {
        KCall ((PKFN) ODSInKCall, str);
    } else {
        DWORD dwId = dwCurThId;
        volatile CRITICAL_SECTION *vpcs = (volatile CRITICAL_SECTION *) &ODScs;
        AcquireSpinLock (&g_odsLock);
        KCALLPROFON(56);

        if (g_nCpuReady > 1) {
            // spin till ODScs is released
            while (vpcs->OwnerThread && (dwId != ((DWORD)vpcs->OwnerThread & ~1))) {
                DataMemoryBarrier();
            }
        }
        DoODS (str);

        KCALLPROFOFF(56);
        ReleaseSpinLock (&g_odsLock);
    }
}

//------------------------------------------------------------------------------
//
//
//------------------------------------------------------------------------------
VOID
NKOutputDebugString (
    LPCWSTR str
    ) 
{
    PTHREAD     pth  = pCurThread;
    PPROCESS    pprc = pActvProc;
    

    DEBUGCHK (str);
    
    if (!pth) {
        // system startup
        g_pNKGlobal->pfnWriteDebugString (str);
    
    } else {
    
        DWORD dwLastErr = KGetLastError (pth);
        
        pth->bDbgCnt ++;
        
        if (g_pOemGlobal->pfnWriteDebugString != g_pNKGlobal->pfnWriteDebugString) {
            // KITL in place, it'll take care of serialization itself
            g_pNKGlobal->pfnWriteDebugString (str);
            
        } else if (InSysCall ()) {
            // Run with kernel stack such that we'll not acquire memory spinlock.
            ODSInKCall (str);
            
        } else {
            // regular call without KITL
            volatile SPINLOCK *podsLock = &g_odsLock;
            
            for ( ; ; ) {
                EnterCriticalSection(&ODScs);

                if (!podsLock->owner_cpu) {
                    break;
                }
                
                LeaveCriticalSection (&ODScs);
                while (podsLock->owner_cpu) {
                    DataMemoryBarrier();
                }
            }

            DoODS (str);
    
            LeaveCriticalSection(&ODScs);
        }
    
        // restore last error
        CELOG_OutputDebugString (pth->dwId, pprc->dwId, str);
        KSetLastError (pth, dwLastErr);

        pth->bDbgCnt --;
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
int 
InputDebugCharW(void) 
{
    int ch;
    DEBUGCHK (!InSysCall ());
    EnterCriticalSection(&ODScs);
    ch = OEMReadDebugByte ();
    LeaveCriticalSection(&ODScs);
    return ch;
}


#define NKD_MAX_CHARS             200     // in wchar


//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL KERNEL
//  @func   VOID | NKDbgPrintf | Prints debug messages
//  @parm   LPWSTR | lpszFmt | Printf style formatting string
//  @parmvar Variable argument list
//  @comm   Should not be used directly - macros like <f DEBUGMSG> should
//      be used to print messages. This function will format the
//      debug string into a buffer and then log it according to the
//      current logging paramters. If terminal logging is on it
//      outputs this to the debug terminal, and if file logging
//      is on it stores it to the file peg.log on the host PC.
//      
//      <b WARNING>: The message being output must be smaller than
//      256 bytes - ie 128 unicode characters.
//  @xref   <f DEBUGMSG>
//
//------------------------------------------------------------------------------
void WINAPIV 
NKDbgPrintfW(
    LPCWSTR lpszFmt,
    ...
    )  
{
    va_list arglist;
    va_start(arglist, lpszFmt);
    NKvDbgPrintfW(lpszFmt, arglist);
    va_end(arglist);
}

VOID NKvDbgPrintfW (LPCWSTR lpszFmt, va_list lpParms) 
{
    WCHAR rgchBuf[NKD_MAX_CHARS];

    NKwvsprintfW (rgchBuf, lpszFmt, lpParms, NKD_MAX_CHARS);
    NKOutputDebugString (rgchBuf);
}

#define MAX_ODS_SIZE                 0xfffe

#define IsOnStack(tlsPtr, str, len)    (((DWORD) (str) > (tlsPtr)[PRETLS_STACKBOUND])   \
                                     && ((DWORD) (str) + (len) < (DWORD) (tlsPtr)))

BOOL SendAndReceiveDebugInfo(PPROCESS pprc, PTHREAD pth, LPCVOID lpBuffer, DWORD dwccbBuffer, LPDWORD pdwContinueStatus);

VOID WINAPI 
Win32OutputDebugStringW(
    LPCWSTR str
    ) 
{
    DWORD  cbStrSize, dwStrLen;
    if (!str) {
        str = __TEXT("(NULL)");
    }

    dwStrLen  = NKwcslen (str);                     // can fault, no resource leak
    cbStrSize = (dwStrLen + 1)*sizeof (WCHAR);      // size, including EOS

    if (cbStrSize <= MAX_ODS_SIZE) {
        
        WCHAR       _localCopy[NKD_MAX_CHARS];
        LPWSTR      strCopy     = (LPWSTR) str;
        PTHREAD     pth         = pCurThread;
        PPROCESS    pprc        = pActvProc;
        DWORD       dwLastErr   = KGetLastError (pth);

        // notify debugger of debugger event
        if (   pprc->pDbgrThrd
            && pth->hDbgEvent
            && !IsInPwrHdlr ()
            && !IsKModeAddr ((DWORD) str)) {
            
            DEBUG_EVENT dbginfo = {0};
            dbginfo.dwDebugEventCode = OUTPUT_DEBUG_STRING_EVENT;
            dbginfo.dwProcessId = pprc->dwId;
            dbginfo.dwThreadId  = pth->dwId;
            dbginfo.u.DebugString.lpDebugStringData = (LPSTR) str;
            dbginfo.u.DebugString.fUnicode = TRUE;
            dbginfo.u.DebugString.nDebugStringLength = (WORD) cbStrSize;
            
            // send the debug info to the debugger and get the response back
            SendAndReceiveDebugInfo (pprc, pth, &dbginfo, sizeof(dbginfo), NULL);    
        }

        // Make a copy of the string if needed.
        // NOTE: we're running on secure stack. so, if the string is on stack, we don't
        //       need to make a copy.
        if (!IsKernelVa (str) && !IsOnStack (pth->tlsPtr, str, cbStrSize)) {
            // make a local copy
            if (cbStrSize > sizeof (_localCopy)) {
                strCopy = NKmalloc (cbStrSize);
                if (!strCopy) {
                    // no memory for the string. not much we can do but truncating the string
                    cbStrSize = sizeof (_localCopy);
                    strCopy   = _localCopy;
                    dwStrLen  = NKD_MAX_CHARS-1;
                }
            } else {
                strCopy = _localCopy;
            }
            CeSafeCopyMemory (strCopy, str, cbStrSize);
            strCopy[dwStrLen] = 0;      // NULL terminate the string
        }
        
        NKOutputDebugString (strCopy);

        if ((strCopy != str) && (strCopy != _localCopy)) {
            NKfree (strCopy);
        }
        // restore last error
        KSetLastError (pth, dwLastErr);
    }
}
