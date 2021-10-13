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
#include <windows.h>
#include <svsutil.hxx>
#include <bt_buffer.h>
#include <bt_ddi.h>

int tdiR_InitializeOnce (void) {
    return ERROR_SUCCESS;
}

int tdiR_CreateDriverInstance (void) {
    return ERROR_SUCCESS;
}

int tdiR_CloseDriverInstance (void) {
    return ERROR_SUCCESS;
}

int tdiR_UninitializeOnce (void) {
    return ERROR_SUCCESS;
}


int tdiR_ProcessConsoleCommand (WCHAR *pString) {
    return ERROR_CALL_NOT_IMPLEMENTED;
}

