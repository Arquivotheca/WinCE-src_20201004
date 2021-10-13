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
////////////////////////////////////////////////////////////////////////////////
//
//  TUXTEST TUX DLL
//  Copyright (c) Microsoft Corporation
//
//  Module:
//          Header for all files in the project.
//
//  Revision History:
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __MAIN_H__
#define __MAIN_H__

////////////////////////////////////////////////////////////////////////////////
// Included files

#include <windows.h>
#include <stdlib.h>
#include <tchar.h>
#include <tux.h>
#include <kato.h>
#include <pwrtestclass.h>
#include <pwrdrvr.h>
#include <pwrhelp.h>
#include <clparse.h>

////////////////////////////////////////////////////////////////////////////////
// Declarations

TESTPROCAPI GetTestResult(void);

void PWRMSG(DWORD verbosity ,LPCWSTR szFormat,...);

extern BOOL g_bAllowSuspend;


// For use with SetSystemPowerState() incorrect parameters testing
typedef struct SET_SYS_PWR_STATE_PARAM {
    LPCWSTR pszStateName;
    DWORD dwStateFlag;
    DWORD dwOptionFlag;
    TCHAR szComment[200];
}setSysPwrStateParam;


// For use with SetPowerRequirement() incorrect parameters testing
typedef struct SET_PWR_REQ_PARAM {
    PVOID pvDevName;
    CEDEVICE_POWER_STATE devState;
    ULONG ulDevFlag;
    PVOID pvStateName;
    ULONG ulStateFlag;
    TCHAR szComment[200];
}setPwrReqParam;


// For use with SetPowerRequirement() tests that get their own valid device name
typedef struct SET_PWR_REQ_PARAM_NO_DEV {
    CEDEVICE_POWER_STATE devState;
    ULONG ulDevFlag;
    PVOID pvStateName;
    ULONG ulStateFlag;
}setPwrReqParam_noDev;


// For use with SetDevicePower() incorrect parameters testing
typedef struct SET_DEV_PWR_PARAM {
    PVOID pvDevName;
    DWORD dwDevFlag;
    CEDEVICE_POWER_STATE devState;
    TCHAR szComment[200];
}setDevPwrParam;

#endif // __MAIN_H__
