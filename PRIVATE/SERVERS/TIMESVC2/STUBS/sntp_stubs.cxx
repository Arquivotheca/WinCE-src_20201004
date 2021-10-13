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
/*--
Module Name: sntp_stubs.cxx
Abstract: stub functions for SNTP layer
--*/

#include <windows.h>
#include <service.h>

#include "../inc/timesvc.h"

int   InitializeSNTP (HINSTANCE hInst) {
    return ERROR_SUCCESS;
}

void DestroySNTP (void) {
}


int   StartSNTP (void) {
    return ERROR_SUCCESS;
}

int   StopSNTP (void) {
    return ERROR_SUCCESS;
}

int   RefreshSNTP (void) {
    return ERROR_SUCCESS;
}

DWORD GetStateSNTP (void) {
    return SERVICE_STATE_UNKNOWN;
}

int   ServiceControlSNTP (PBYTE pBufIn, DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut, PDWORD pdwActualOut, int *pfProcessed) {
    *pfProcessed = FALSE;
    return ERROR_SUCCESS;
}

