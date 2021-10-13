//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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

