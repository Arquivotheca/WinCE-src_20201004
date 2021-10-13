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
#define BUILDING_FS_STUBS
#include <windows.h>


extern "C"
BOOL xxx_CheckPassword(LPWSTR lpszPassword) {
    return CheckPassword_Trap(lpszPassword);
}


// Internal filesys exports

extern "C"
BOOL xxx_SetPassword(LPWSTR lpszOldpassword, LPWSTR lspzNewPassword) {
    return SetPassword_Trap(lpszOldpassword, lspzNewPassword);
}

extern "C"
BOOL xxx_GetPasswordActive(void) {
    return (GetPasswordStatus_Trap() & PASSWORD_STATUS_ACTIVE);
}

extern "C"
BOOL xxx_SetPasswordActive(BOOL bActive, LPWSTR lpszPassword) {
    DWORD dwStatus;
    
    // Avoid changing status flags other than the active one
    dwStatus = GetPasswordStatus_Trap();
    if (bActive) {
        dwStatus |= PASSWORD_STATUS_ACTIVE;
    } else {
        dwStatus &= ~PASSWORD_STATUS_ACTIVE;
    }
    
    return SetPasswordStatus_Trap(dwStatus, lpszPassword);
}

extern "C"
BOOL xxx_SetPasswordStatus(DWORD dwStatus, LPWSTR lpszPassword) {
    return SetPasswordStatus_Trap(dwStatus, lpszPassword);
}

extern "C"
DWORD xxx_GetPasswordStatus(void) {
    return GetPasswordStatus_Trap();
}

