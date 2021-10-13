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
//------------------------------------------------------------------------------
// 
//      Bluetooth Stack
// 
// 
// Module Name:
// 
//      bthstack.cxx
// 
// Abstract:
// 
//      This file implements Bluetooth Stack Integration
// 
// 
//------------------------------------------------------------------------------
#include <windows.h>
#include <svsutil.hxx>
#include <bt_debug.h>

#include <bt_buffer.h>
#include <bt_ddi.h>
#include <bt_avctp.h>

int avrcp_InitializeOnce(void);


//
// Note: TDI initialization happens inside TDI layer.
//
int bth_InitializeOnce (void) {
    svsutil_Initialize ();
    DebugInitialize ();

    IFDBG(DebugOut (DEBUG_SHELL_INIT, L"Global initialization for BTh stack\n"));

    btutil_InitializeOnce ();

    hci_InitializeOnce ();
    l2cap_InitializeOnce ();

    sdp_InitializeOnce ();
    bthns_InitializeOnce();

    rfcomm_InitializeOnce ();
    portemu_InitializeOnce ();
    pan_InitializeOnce ();
    avdtp_InitializeOnce ();
    avctp_InitializeOnce ();
    avrcp_InitializeOnce();

    return ERROR_SUCCESS;
}

int bth_CreateDriverInstance (void) {
    IFDBG(DebugOut (DEBUG_SHELL_INIT, L"Creating device instance\n"));
    int iError = btutil_CreateDriverInstance ();
    if (iError)
        return iError;

    iError = hci_CreateDriverInstance ();
    if (iError)
        return iError;

    iError = l2cap_CreateDriverInstance ();
    if (iError)
        return iError;

    iError = sdp_CreateDriverInstance ();
    if (iError)
        return iError;

    iError = rfcomm_CreateDriverInstance ();
    if (iError)
        return iError;

    iError = portemu_CreateDriverInstance ();
    if (iError)
        return iError;

    iError = avdtp_CreateDriverInstance ();
    if (iError)
        return iError;

    iError = avctp_CreateDriverInstance ();
    if (iError)
        return iError;

    return ERROR_SUCCESS;
}

int bth_CloseDriverInstance (void) {
    IFDBG(DebugOut (DEBUG_SHELL_INIT, L"Closing device instance\n"));

    avdtp_CloseDriverInstance ();
    pan_CloseDriverInstance ();

    portemu_CloseDriverInstance ();
    rfcomm_CloseDriverInstance ();

    sdp_CloseDriverInstance ();

    l2cap_CloseDriverInstance ();
    hci_CloseDriverInstance ();

    btutil_CloseDriverInstance ();

    return ERROR_SUCCESS;
}

int bth_UninitializeOnce (void) {
    IFDBG(DebugOut (DEBUG_SHELL_INIT, L"Global Uninitialization for BTh stack\n"));

    avdtp_UninitializeOnce ();
    pan_UninitializeOnce ();
    portemu_UninitializeOnce ();
    rfcomm_UninitializeOnce ();

    bthns_UninitializeOnce();
    sdp_UninitializeOnce ();

    l2cap_UninitializeOnce ();
    hci_UninitializeOnce ();

    btutil_UninitializeOnce ();

    DebugUninitialize ();

    svsutil_DeInitialize ();

    return ERROR_SUCCESS;
}
