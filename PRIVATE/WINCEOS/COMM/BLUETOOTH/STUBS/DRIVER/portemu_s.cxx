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

int portemu_InitializeOnce (void) {
    return ERROR_SUCCESS;
}

int portemu_CreateDriverInstance (void) {
    return ERROR_SUCCESS;
}

int portemu_CloseDriverInstance (void) {
    return ERROR_SUCCESS;
}

int portemu_UninitializeOnce (void) {
    return ERROR_SUCCESS;
}


int portemu_ProcessConsoleCommand (WCHAR *pString) {
    return ERROR_CALL_NOT_IMPLEMENTED;
}

