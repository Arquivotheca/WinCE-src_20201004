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

int HCI_EstablishDeviceContext
(
void					*pUserContext,		/* IN */
unsigned int			uiControl,			/* IN */
BD_ADDR					*pba,				/* IN */
unsigned int			class_of_device,	/* IN */
unsigned char			link_type,			/* IN */
HCI_EVENT_INDICATION	*pInd,				/* IN */
HCI_CALLBACKS			*pCall,				/* IN */
HCI_INTERFACE			*pInt,				/* OUT */
int						*pcDataHeaders,		/* OUT */
int						*pcDataTrailers,	/* OUT */
HANDLE					*phDeviceContext	/* OUT */
) {
    return ERROR_CALL_NOT_IMPLEMENTED;
}

int HCI_CloseDeviceContext
(
HANDLE					hDeviceContext		/* IN */
) {
    return ERROR_CALL_NOT_IMPLEMENTED;
}

int hci_InitializeOnce (void) {
    return ERROR_SUCCESS;
}

int hci_CreateDriverInstance (void) {
    return ERROR_SUCCESS;
}

int hci_CloseDriverInstance (void) {
    return ERROR_SUCCESS;
}

int hci_UninitializeOnce (void) {
    return ERROR_SUCCESS;
}


int hci_ProcessConsoleCommand (WCHAR *pString) {
    return ERROR_CALL_NOT_IMPLEMENTED;
}

