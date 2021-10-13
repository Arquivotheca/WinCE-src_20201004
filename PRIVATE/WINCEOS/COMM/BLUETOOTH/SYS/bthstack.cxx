//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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


int bth_InitializeOnce (void) {
	IFDBG(DebugOut (DEBUG_SHELL_INIT, L"Global initialization for BTh stack\n"));

	svsutil_Initialize ();
	DebugInitialize ();

	btutil_InitializeOnce ();

	hci_InitializeOnce ();
	l2cap_InitializeOnce ();

	sdp_InitializeOnce ();
	bthns_InitializeOnce();

	rfcomm_InitializeOnce ();
	portemu_InitializeOnce ();
	tdiR_InitializeOnce ();

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

	iError = tdiR_CreateDriverInstance ();
	if (iError)
		return iError;


	return ERROR_SUCCESS;
}

int bth_CloseDriverInstance (void) {
	IFDBG(DebugOut (DEBUG_SHELL_INIT, L"Closing device instance\n"));

	tdiR_CloseDriverInstance ();

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

	tdiR_UninitializeOnce ();
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
