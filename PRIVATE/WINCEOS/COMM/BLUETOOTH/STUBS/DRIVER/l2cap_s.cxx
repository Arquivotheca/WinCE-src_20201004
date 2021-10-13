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

int L2CAP_EstablishDeviceContext
(
void					*pUserContext,		/* IN */
unsigned short			psm,				/* IN */
L2CAP_EVENT_INDICATION	*pInd,				/* IN */
L2CAP_CALLBACKS			*pCall,				/* IN */
L2CAP_INTERFACE			*pInt,				/* OUT */
int						*pcDataHeaders,		/* OUT */
int						*pcDataTrailers,	/* OUT */
HANDLE					*phDeviceContext	/* OUT */
) {
    return ERROR_CALL_NOT_IMPLEMENTED;
}

int L2CAP_CloseDeviceContext
(
HANDLE					hDeviceContext		/* IN */
) {
    return ERROR_CALL_NOT_IMPLEMENTED;
}

int l2cap_InitializeOnce (void)  {
    return ERROR_SUCCESS;
}

int l2cap_CreateDriverInstance (void) {
    return ERROR_SUCCESS;
}

int l2cap_CloseDriverInstance (void) {
    return ERROR_SUCCESS;
}

int l2cap_UninitializeOnce (void) {
    return ERROR_SUCCESS;
}


int l2cap_ProcessConsoleCommand (WCHAR *pString) {
    return ERROR_CALL_NOT_IMPLEMENTED;
}

