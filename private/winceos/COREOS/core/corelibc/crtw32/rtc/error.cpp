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
/***
*error.cpp - RTC support
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*
*Revision History:
*       07-28-98  JWM   Module incorporated into CRTs (from KFrei)
*       11-03-98  KBF   added throw() to eliminate C++ EH code
*       05-11-99  KBF   Error if RTC support define not enabled
*       05-26-99  KBF   Added -RTCu stuff, _RTC_ prefix on all non-statics
*       11-30-99  PML   Compile /Wp64 clean.
*       03-19-01  KBF   Fix buffer overruns (vs7#227306), eliminate all /GS
*                       checks (vs7#224261).
*       03-26-01  PML   Use GetVersionExA, not GetVersionEx (vs7#230286)
*       07-15-01  PML   Remove all ALPHA, MIPS, and PPC code
*       10-17-03  SJ    Added UNICODE RTC Reporting
*       11-18-03  CSP   Added _alloca checking support.
*       03-13-04  MSL   Removed _alloca usage, tidied other prefast defects
*       08-18-04  SJ    Removing memset dependency from runtmchk.lib
*       09-25-04  JL    Replace usage of _alloca() with _alloca_s() / _freea_s()
*       10-01-04  AGH   Print more info upon _alloca memory corruption.  Fix
*                       _RTC_StackFailure bug.
*       10-21-04  AC    Replace #pragma prefast with #pragma warning
*                       VSW#368280
*       11-11-04  MR    Add support for special handles in C# - VSW#272079
*       03-04-05  PAL   Fix wsprintf format string in _RTC_AllocaFailure (VSW 457656).
*       03-08-05  PAL   Correct previous fix for Win64.
*       03-23-05  MSL   packing cleanup
*       04-04-05  JL    Replace _alloca_s and _freea_s with _malloca and _freea
*       04-01-05  MSL   Integer overflow protection
*       05-13-05  MSL   Rely on IsDebuggerPresent
*       12-05-05  MSL   Clarify RTC error
*       01-27-06  AC    Check if a debugger is present before probing for VS debugger.
*                       VSW#570841
*       04-23-07  AC    Make _RTC_ErrorMessages const
*                       DDB#93943
*
****/

#ifndef _RTC
#error  RunTime Check support not enabled!
#endif

#include "rtcpriv.h"

#include <crtdbg.h>
#include <stdlib.h>
#include <windows.h>

// The following is needed only for str* functions, which are used for CORESYS only
// Non-CORESYS doesn't use CRT functions, because the RTC lib can be linked without linking to the CRT itself.
#ifdef _CORESYS
#include <stdio.h>
#endif

#pragma intrinsic(strcpy)
#pragma intrinsic(strcat)
#pragma intrinsic(strlen)

#pragma warning(push)
#pragma warning(disable:4996)

int _RTC_ErrorLevels[_RTC_ILLEGAL] = {
    _CRT_ERROR, // ESP was trashed
    _CRT_ERROR, // Shortening convert 
    _CRT_ERROR, // Stack corruption
    _CRT_ERROR, // Uninitialized use
    _CRT_ERROR  // _alloca corrupted
};
static const char * const _RTC_ErrorMessages[_RTC_ILLEGAL+1] =
{
    "The value of ESP was not properly saved across a function "
        "call.  This is usually a result of calling a function "
        "declared with one calling convention with a function "
        "pointer declared with a different calling convention.\n\r",
    "A cast to a smaller data type has caused a loss of data.  "
        "If this was intentional, you should mask the source of "
        "the cast with the appropriate bitmask.  For example:  \n\r"
        "\tchar c = (i & 0xFF);\n\r"
        "Changing the code in this way will not affect the quality of the resulting optimized code.\n\r",
    "Stack memory was corrupted\n\r",
    "A local variable was used before it was initialized\n\r",
#ifdef _RTC_ADVMEM
    "Referencing invalid memory\n\r",
    "Referencing memory across different blocks\n\r",
#endif
    "Stack memory around _alloca was corrupted\n\r",
    "Unknown Runtime Check Error\n\r"
};

static const BOOL _RTC_NoFalsePositives[_RTC_ILLEGAL+1] =
{
    TRUE,   // ESP was trashed
    FALSE,  // Shortening convert
    TRUE,   // Stack corruption
    TRUE,   // Uninitialized use
#ifdef _RTC_ADVMEM
    TRUE,   // Invalid memory reference
    FALSE,  // Different memory blocks
#endif
    TRUE,   // _alloca corrupted 
    TRUE    // Illegal
};

#if defined(_M_IX86) || defined(_M_X64)
#define MD_CALL_SIZE (5)
#elif defined(_M_ARM)
/*UNDONE: This is used to determine the call instruction from the return address
          I suspect that four would always be valid for a compiler generated
          direct call to the RTC helpers.
*/
#define MD_CALL_SIZE (4) 
#else
#error  MD_CALL_SIZE not defined for the current platfrom
#endif

// returns TRUE if debugger understands, FALSE if not
static BOOL
DebuggerProbe( DWORD dwLevelRequired ) throw()
{
    BYTE bDebuggerListening = FALSE;

    EXCEPTION_VISUALCPP_DEBUG_INFO info;
    info.dwType = EXCEPTION_DEBUGGER_PROBE;
    info.DebuggerProbe.dwLevelRequired = dwLevelRequired;
    info.DebuggerProbe.pbDebuggerPresent = &bDebuggerListening;

    __try
    {
        HelloVC( info );
    }
    __except((GetExceptionCode() == EXCEPTION_VISUALCPP_DEBUGGER) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
        ; // Do nothing in case of the debugger notification exception only
          // Other exceptions will continue to be processed
    }

    return (BOOL)bDebuggerListening;
}

// returns TRUE if debugger reported it (or was ignored), FALSE if runtime needs to report it
static int
DebuggerRuntime( DWORD dwErrorNumber, BOOL bRealBug, PVOID pvReturnAddr, LPCWSTR pwMessage ) throw()
{
    EXCEPTION_VISUALCPP_DEBUG_INFO info;
    BYTE bDebuggerListening = FALSE;

    info.dwType = EXCEPTION_DEBUGGER_RUNTIMECHECK;
    info.RuntimeError.dwRuntimeNumber = dwErrorNumber;
    info.RuntimeError.bRealBug = bRealBug;
    info.RuntimeError.pvReturnAddress = pvReturnAddr;
    info.RuntimeError.pbDebuggerPresent = &bDebuggerListening;
    info.RuntimeError.pwRuntimeMessage = pwMessage;

    __try
    {
        HelloVC( info );
    }
    __except((GetExceptionCode() == EXCEPTION_VISUALCPP_DEBUGGER) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
        ; // Do nothing in case of the debugger notification exception only
          // Other exceptions will continue to be processed
    }

    return (BOOL)bDebuggerListening;
}

static void
failwithmessage(void *retaddr, int crttype, int errnum, const char *msg)
{
    _RTC_error_fn fn = 0;
    wchar_t msgB[512]; // we'll refuse messages larger than this. 
    wchar_t *msgW=NULL;
    // Can't use malloca here because we are in RTC 
    // - means we don't want to depend
    // - may not be safe to call malloc right now
    
    _RTC_error_fnW fnW = _RTC_GetErrorFuncW(retaddr);
        
    if(fnW == NULL) 
    {
        // If a UNICODE Reporting Function is available we call that.
        // Otherwise we call the ANSI one.            

        fn = _RTC_GetErrorFunc(retaddr);
    }        

    int bufsize = MultiByteToWideChar(CP_UTF8, 0, msg, -1, NULL , 0);
    if(bufsize <(sizeof(msgB)/sizeof(msgB[0]))) 
    {
        if(MultiByteToWideChar(CP_UTF8, 0, msg, -1, msgB , bufsize) != 0) 
        {
            msgW = msgB;
        }
    }
    if(msgW == NULL) 
    {
        msgW = L"Runtime Check Error.\n\r Unable to display RTC Message.";
    }
    
    bool dobreak;

    if (DebuggerProbe( EXCEPTION_DEBUGGER_RUNTIMECHECK ))
    {
        if (DebuggerRuntime(errnum, _RTC_NoFalsePositives[errnum], retaddr, msgW))
            return;
        dobreak = false;

    } else
        dobreak = true;
    
    if ((!fn && !fnW) || (dobreak && IsDebuggerPresent()))
        __debugbreak();
    else
    {
        wchar_t srcNameW[_MAX_PATH];
        int lineNum;
        wchar_t moduleNameW[_MAX_PATH];
        _RTC_GetSrcLine(((LPBYTE)retaddr)-MD_CALL_SIZE, srcNameW, _MAX_PATH, &lineNum, moduleNameW, _MAX_PATH);
        // We're just running - report it like the user setup (or the default way)
        // If we don't recognize this type, it defaults to an error
        if(fnW) 
        {
            if (fnW(crttype, srcNameW, lineNum, moduleNameW,
               L"Run-Time Check Failure #%d - %s", errnum, msgW) == 1)
                __debugbreak();
        }
        else
        {
            char * srcName = "Unknown Filename";
            char srcNameB[3*(_MAX_PATH - 1) + 1]; // allow space for UTF8 encoded characters
                                                  // A wchar_t converted to UTF-8 is at most 3 bytes.
            if (WideCharToMultiByte(CP_UTF8, 0, srcNameW, -1, srcNameB, _countof(srcNameB), NULL, NULL) != 0)
            {
                srcName = srcNameB; 
            }

            char * moduleName="Unknown Module Name";
            char moduleNameB[3*(_MAX_PATH - 1) + 1];
            if (WideCharToMultiByte(CP_UTF8, 0, moduleNameW, -1, moduleNameB , _countof(moduleNameB), NULL, NULL) !=0)
            {
                moduleName = moduleNameB;
            }
            
            if (fn(crttype, srcName, lineNum, moduleName,
               "Run-Time Check Failure #%d - %s", errnum, msg) == 1)
                __debugbreak();
        }
    }
}

void __cdecl
_RTC_Failure(void *retaddr, int errnum)
{
    int crttype;
    const char *msg;

    if (errnum < _RTC_ILLEGAL && errnum >= 0) {
        crttype = _RTC_ErrorLevels[errnum];
        msg = _RTC_ErrorMessages[errnum];
    } else {
        crttype = _CRT_ERROR;
        msg = _RTC_ErrorMessages[_RTC_ILLEGAL];
        errnum = _RTC_ILLEGAL;
    }

    // If we're running inside a debugger, raise an exception

    if (crttype != _RTC_ERRTYPE_IGNORE)
    {
        failwithmessage(retaddr, crttype, errnum, msg);
    }
}

static
char *IntToString(int i)
{
    static char buf[15];
    bool neg = i < 0;
    int pos = 14;
    buf[14] = 0;
    do {
        buf[--pos] = i % 10 + '0';
        i /= 10;
    } while (i);
    if (neg)
        buf[--pos] = '-';
    return &buf[pos];
}

#ifdef _RTC_ADVMEM
    // _RTC_MemFailure  will have to be changed if we ever compile this
    // It calls the old _RTC_GetSrcLine(ANSI) which no longer exists 
void __cdecl
_RTC_MemFailure(void *retaddr, int errnum, const void *assign)
{
    char *srcName = (char*)_calloca(sizeof(char), 513);
    int lineNum;
    char moduleName[_MAX_PATH];
    int crttype = _RTC_ErrorLevels[errnum];
    if (crttype == _RTC_ERRTYPE_IGNORE)
        return;

    if (srcName == NULL)
        return;

    _RTC_GetSrcLine(((LPBYTE)assign)-MD_CALL_SIZE, srcName, 512, &lineNum, moduleName);
    if (!lineNum)
        _RTC_Failure(retaddr, errnum);
    else
    {
        char *msg = (char*)_malloca(strlen(_RTC_ErrorMessages[errnum]) +
                                    strlen(srcName) + strlen(moduleName) +
                                    150);
        if (msg != NULL)
        {
            strcpy(msg, _RTC_ErrorMessages[errnum]);
            strcat(msg, "Invalid pointer was assigned at\n\rFile:\t");
            strcat(msg, srcName);
            strcat(msg, "\n\rLine:\t");
            strcat(msg, IntToString(lineNum));
            strcat(msg, "\n\rModule:\t");
            strcat(msg, moduleName);
            failwithmessage(retaddr, crttype, errnum, msg);
            _freea(msg);
        }
    }
    _freea(srcName);
}

#endif

static const char stack_premsg[]="Stack around the variable '";
static const char stack_postmsg[]="' was corrupted.";

void __cdecl
_RTC_StackFailure(void *retaddr, const char *varname)
{
    int crttype = _RTC_ErrorLevels[_RTC_CORRUPT_STACK];
    if (crttype != _RTC_ERRTYPE_IGNORE)
    {
        char msgB[1024];
        char *msg = NULL;
        if(varname[0] == '\0' ||
           (strlen(varname)+sizeof(stack_premsg)+sizeof(stack_postmsg))>sizeof(msgB))
        {
            msg="Stack corrupted near unknown variable";
        }
        else
        {
            msg=msgB;
#ifndef _WIN32_WCE            
            strcpy(msg, stack_premsg);
            strcat(msg, varname);
            strcat(msg, stack_postmsg);
#else            
            strcpy_s(msg, _countof(msgB), stack_premsg);
            strcat_s(msg, _countof(msgB), varname);
            strcat_s(msg, _countof(msgB), stack_postmsg);
#endif            
        }
        failwithmessage(retaddr, crttype, _RTC_CORRUPT_STACK, msg);
    }
}

#define MAXPRINT 16

#ifndef _CORESYS
typedef int (*WSPRINTF_FP)(LPTSTR lpOut, LPCTSTR lpFmt, ...);
WSPRINTF_FP wsprintffp = NULL;
#endif

/* Return data as an ASCII string in printbuff and as a Hex dump in valbuff
 * size of printbuff must be at lest MAXPRINT+1; of valbuff, 3*MAXPRINT+1
 */
static void __cdecl _getMemBlockDataString(
        char * printbuff,
        char * valbuff,
        char * data,
        size_t datasize
        )
{
        size_t i;
        unsigned char ch;

        for (i = 0; i < min(datasize, MAXPRINT); i++)
        {
            ch = data[i];
#ifndef _CORESYS
            wsprintffp((char *)&valbuff[i*3], "%.2X ", ch);
#else
            sprintf_s((char *)&valbuff[i*3], ((3*MAXPRINT+1) - i*3), "%.2X ", ch);
#endif
            printbuff[i] = ch;
        }
        printbuff[i] = '\0';
        valbuff[i*3] = '\0';
}

/* Report Alloca Failure.  If pn != NULL, reports Address, size, allocation number,
 * and data of faulty allocation */
void __cdecl
_RTC_AllocaFailure(void *retaddr, _RTC_ALLOCA_NODE * pn, int num)
{
    int crttype = _RTC_ErrorLevels[_RTC_CORRUPTED_ALLOCA];

    if (crttype != _RTC_ERRTYPE_IGNORE)
    {
#ifndef _CORESYS
        HMODULE hInst = LoadLibraryExW(L"user32.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
#if !defined(_M_ARM)
        if (!hInst && GetLastError() == ERROR_INVALID_PARAMETER)
        {
            // LOAD_LIBRARY_SEARCH_SYSTEM32 is not supported on this platfrom,
            // try one more time using default options
            hInst = LoadLibraryW(L"user32.dll");
        }
#endif
        if ( hInst != NULL )
        {
            wsprintffp = (WSPRINTF_FP)GetProcAddress(hInst, "wsprintfA");
        }
        if ( hInst == NULL || pn == NULL || wsprintffp == NULL )
#else
        if ( pn == NULL )
#endif
        {
            failwithmessage(retaddr, crttype, _RTC_CORRUPTED_ALLOCA,
                            "Stack area around _alloca memory reserved by this function is corrupted\n");
        }
        else
        {        
            char printbuff[MAXPRINT+1];
            char valbuff[3*MAXPRINT+1];
#define A "Stack area around _alloca memory reserved by this function is corrupted"
#define B "\nAddress: 0x"
#define C "\nSize: "
#define D "\nAllocation number within this function: "
#define E "\nData: <"
#define F "> "
#define G "\n"

            // max decimal-printed size of 32 bit ints is 10; of 64 bit ints is 20
            const int INTSTRSIZE = (sizeof(size_t)/4)*10; 
            const int SIZE = sizeof(A)+sizeof(B)+2*sizeof(void*)+sizeof(C)+
                INTSTRSIZE+sizeof(D)+10+sizeof(E)+ 
                sizeof(printbuff)+sizeof(F)+sizeof(valbuff)+sizeof(G)+1;
            char msg[SIZE];

#ifdef _WIN64
#ifndef _CORESYS
            wsprintffp(msg, "%s%s%p%s%I64d%s%d%s",
#else
            sprintf_s(msg, _countof(msg), "%s%s%p%s%I64d%s%d%s",
#endif
                      A, B, (char *)(pn+1), C,
                      pn->allocaSize - sizeof(_RTC_ALLOCA_NODE) - sizeof(DWORD),
                      D, num, E);
#else
#ifndef _CORESYS
            wsprintffp(msg, "%s%s%p%s%ld%s%d%s",
#else
            sprintf_s(msg, _countof(msg), "%s%s%p%s%ld%s%d%s",
#endif
                      A, B, (char *)(pn+1), C,
                      pn->allocaSize - sizeof(_RTC_ALLOCA_NODE) - sizeof(DWORD),
                      D, num, E);
#endif
            _getMemBlockDataString(printbuff, valbuff, (char *)(pn+1),
                                   pn->allocaSize-sizeof(_RTC_ALLOCA_NODE)-sizeof(DWORD));
#ifndef _CORESYS
            wsprintffp(&msg[lstrlen(msg)], "%s%s%s%s", printbuff, F, valbuff, G);
#else
            sprintf_s(&msg[strlen(msg)], (_countof(msg) - strlen(msg)), "%s%s%s%s", printbuff, F, valbuff, G);
#endif
            failwithmessage(retaddr, crttype, _RTC_CORRUPTED_ALLOCA, msg);
        }
    }
}

static const char uninit_premsg[]="The variable '";
static const char uninit_postmsg[]="' is being used without being initialized.";

void __cdecl
_RTC_UninitUse(const char *varname)
{
    int crttype = _RTC_ErrorLevels[_RTC_UNINIT_LOCAL_USE];
    if (crttype != _RTC_ERRTYPE_IGNORE)
    {
        char msgB[1024];
        char *msg = NULL;
        if(!varname || (strlen(varname)+sizeof(uninit_premsg)+sizeof(uninit_postmsg))>sizeof(msgB))
        {
            msg="A variable is being used without being initialized.";
        }
        else
        {
            msg=msgB;
#ifndef _WIN32_WCE
            strcpy(msg, uninit_premsg);
            strcat(msg, varname);
            strcat(msg, uninit_postmsg);
#else            
            strcpy_s(msg, _countof(msgB), uninit_premsg);
            strcat_s(msg, _countof(msgB), varname);
            strcat_s(msg, _countof(msgB), uninit_postmsg);
#endif            
        }

        failwithmessage(_ReturnAddress(), crttype, _RTC_UNINIT_LOCAL_USE, msg);
    }
}

#pragma warning(pop)
