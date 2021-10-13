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

int RFCOMM_EstablishDeviceContext
(
void					*pUserContext,		/* IN */
unsigned char			channel,			/* IN */
RFCOMM_EVENT_INDICATION	*pInd,				/* IN */
RFCOMM_CALLBACKS		*pCall,				/* IN */
RFCOMM_INTERFACE		*pInt,				/* OUT */
int						*pcDataHeaders,		/* OUT */
int						*pcDataTrailers,	/* OUT */
HANDLE					*phDeviceContext	/* OUT */
) {
    return ERROR_CALL_NOT_IMPLEMENTED;
}

int RFCOMM_CloseDeviceContext
(
HANDLE					hDeviceContext		/* IN */
) {
    return ERROR_CALL_NOT_IMPLEMENTED;
}

int rfcomm_InitializeOnce (void) {
    return ERROR_SUCCESS;
}

int rfcomm_CreateDriverInstance (void) {
    return ERROR_SUCCESS;
}

int rfcomm_CloseDriverInstance (void) {
    return ERROR_SUCCESS;
}

int rfcomm_UninitializeOnce (void) {
    return ERROR_SUCCESS;
}


int rfcomm_ProcessConsoleCommand (WCHAR *pString) {
    return ERROR_CALL_NOT_IMPLEMENTED;
}

