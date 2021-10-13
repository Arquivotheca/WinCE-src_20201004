//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

