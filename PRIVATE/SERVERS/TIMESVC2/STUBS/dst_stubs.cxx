//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*--
Module Name: dst_stubs.cxx
Abstract: stub functions for DST layer
--*/

#include <windows.h>
#include <service.h>

#include "../inc/timesvc.h"

int   InitializeDST (HINSTANCE hInst) {
    return ERROR_SUCCESS;
}

void DestroyDST (void) {
}

int   StartDST (void) {
    return ERROR_SUCCESS;
}

int   StopDST (void) {
    return ERROR_SUCCESS;
}

int   RefreshDST (void) {
    return ERROR_SUCCESS;
}

DWORD GetStateDST (void) {
    return SERVICE_STATE_UNKNOWN;
}

int   ServiceControlDST (PBYTE pBufIn, DWORD dwLenIn, PBYTE pBufOut, DWORD dwLenOut, PDWORD pdwActualOut, int *pfProcessed) {
    *pfProcessed = FALSE;
    return ERROR_SUCCESS;
}

void SetDaylightOrStandardTimeDST(SYSTEMTIME *pNewSystemTime) {
    return;
}

