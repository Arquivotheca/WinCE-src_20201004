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

int SDP_EstablishDeviceContext
(
void					*pUserContext,		/* IN */
SDP_EVENT_INDICATION    *pInd,              /* IN */
SDP_CALLBACKS           *pCall,             /* IN */
SDP_INTERFACE           *pInt,              /* OUT */
HANDLE					*phDeviceContext	/* OUT */
) {
    return ERROR_CALL_NOT_IMPLEMENTED;
}


int SDP_CloseDeviceContext
(
HANDLE					hDeviceContext		/* IN */
) {
    return ERROR_CALL_NOT_IMPLEMENTED;
}


int sdp_InitializeOnce (void) {
    return ERROR_SUCCESS;
}

int sdp_CreateDriverInstance (void) {
    return ERROR_SUCCESS;
}

int sdp_CloseDriverInstance (void) {
    return ERROR_SUCCESS;
}

int sdp_UninitializeOnce (void) {
    return ERROR_SUCCESS;
}

int sdp_ProcessConsoleCommand (WCHAR *pString) {
    return ERROR_CALL_NOT_IMPLEMENTED;
}

void sdp_LoadWSANameSpaceProvider(void) {
	;
}

void sdp_UnLoadWSANameSpaceProvider(void) {
	;
}
