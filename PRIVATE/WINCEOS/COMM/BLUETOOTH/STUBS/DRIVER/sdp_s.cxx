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
