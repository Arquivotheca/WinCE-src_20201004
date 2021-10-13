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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
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

