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

#pragma once

#include <windows.h>
#include <types.h>
#include <dbgapi.h>



//Turn Debug on... turn debug off - compile times yes... but still better
//than nothing. 
#define SNS_DBG 1

static const DWORD WARN_ONLY = 0;
static const DWORD FAIL_EXIT = 1;



void LogHelper(BOOL bDbgOn, LPCTSTR szPrepend, LPCTSTR szFormat, ...);
void LogHelperEx(BOOL bDbgOn, LPCTSTR szPrepend, LPCTSTR szLabel, LPCTSTR szFormat, ...);



//------------------------------------------------------------------------------
// LOG
// Just for consistency
#define LOG(szFormat, ...)\
{\
    LogHelperEx(SNS_DBG, (_T(">SNS:")), _T(__FUNCTION__),_T(szFormat), __VA_ARGS__);\
}

//------------------------------------------------------------------------------
// LOG_ERROR
// 
#define LOG_ERROR(szFormat, ...)\
{\
    DWORD dwError = GetLastError();\
    RETAILMSG(SNS_DBG, (_T("\n\n")));\
    RETAILMSG(SNS_DBG, (_T("!SNS: -------------------- TEST EXECUTION ERROR ------------------------")));\
    RETAILMSG(SNS_DBG, (_T("!SNS: File: (%s)" ), _T(__FILE__)));\
    RETAILMSG(SNS_DBG, (_T("!SNS: Function: (%s) @ Line: (%d)"),_T(__FUNCTION__), __LINE__ ));\
    RETAILMSG(SNS_DBG, (_T("!SNS: Last Error: (%d)" ), dwError));\
    LogHelper(SNS_DBG, (_T("!SNS:")), _T(szFormat), __VA_ARGS__);\
    RETAILMSG(SNS_DBG, (_T("!SNS: ----------------------- EARLY EXIT -------------------------------")));\
    RETAILMSG(SNS_DBG, (_T("\n\n")));\
    bResult = FALSE;\
    goto DONE;\
}

//------------------------------------------------------------------------------
// LOG_WARN
// 
#define LOG_WARN(szFormat, ...)\
{\
    DWORD dwError = GetLastError();\
    RETAILMSG(SNS_DBG, (_T("\n\n")));\
    RETAILMSG(SNS_DBG, (_T("*SNS: ------------------- TEST RUNTIME WARNING  ------------------------")));\
    RETAILMSG(SNS_DBG, (_T("*SNS: File: (%s)" ), _T(__FILE__)));\
    RETAILMSG(SNS_DBG, (_T("*SNS: Function: (%s) @ Line: (%d)"),_T(__FUNCTION__), __LINE__ ));\
    RETAILMSG(SNS_DBG, (_T("*SNS: Last Error: (%d)" ), dwError));\
    LogHelper(SNS_DBG, (_T("*SNS:")), _T(szFormat), __VA_ARGS__);\
    RETAILMSG(SNS_DBG, (_T("*SNS: ----------------------- TEST RESUMING ----------------------------")));\
    RETAILMSG(SNS_DBG, (_T("\n\n")));\
}

//------------------------------------------------------------------------------
// LOG_START
// Indicates function entrance and starts tick count tracking
#define LOG_START()\
    DWORD LOG_dwStartTick = GetTickCount();\
    RETAILMSG(SNS_DBG, (_T("+SNS:%s"), _T(__FUNCTION__) ));

//------------------------------------------------------------------------------
// LOG_END
// Inidicates exit from function and displays time since LOG_START
#define LOG_END()\
    DEBUGMSG(SNS_DBG, (_T("-SNS:%s [%d ms]"), _T(__FUNCTION__), (GetTickCount()-LOG_dwStartTick) ))

//------------------------------------------------------------------------------
// Originally this Macro would generate "warning C4127: conditional expression 
// is constant" warnings for every invocation since the early exit parameter is
// a constant. Could not use #praga to disable this warning since it would have
// to live inside the macro (which won't compile) or around every invocation
// of the macro. Could replace macro with function to use the #pragma but would
// then not be able to use the embedded GOTO call to early exit the test. 
// So... added UOP (useless operation) to bypass the warning.
inline BOOL UOP( BOOL returnme){return returnme;}
#define VERIFY_STEP( szStepStr, bStepRes, bEarlyExit )\
    {\
        DWORD dwError = GetLastError();\
        RETAILMSG(1,(_T("?SNS: %s...(%s)"),_T(szStepStr),((bStepRes)?_T("Passed"):_T("Failed"))));\
        if(bStepRes == FALSE)\
        {\
            bResult = FALSE;\
            RETAILMSG(SNS_DBG, (_T("\n\n")));\
            RETAILMSG(SNS_DBG, (_T("!SNS: ------------------- TEST EXECUTION ERROR -----------------------")));\
            RETAILMSG(SNS_DBG, (_T("!SNS: File: (%s)" ), _T(__FILE__)));\
            RETAILMSG(SNS_DBG, (_T("!SNS: Function: (%s) @ Line: (%d)"),_T(__FUNCTION__), __LINE__ ));\
            RETAILMSG(SNS_DBG, (_T("!SNS: Last Error: (%d)" ), dwError));\
            if( UOP(bEarlyExit) == FAIL_EXIT){\
                RETAILMSG(SNS_DBG, (_T("!SNS: ------------------------  EARLY EXIT -----------------------------\n\n")));\
                goto DONE;\
            }else{\
                RETAILMSG(SNS_DBG, (_T("!SNS: ----------------------- TEST RESUMING ----------------------------")));\
            }\
        }\
    }

