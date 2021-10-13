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
#ifndef __ONEX_INI_H_
#define __ONEX_INI_H_

#include <l2cmnpriv.h>

#if __midl
#define V1_ENUM [v1_enum]
#else
#define V1_ENUM
#endif

#define PORT_FLAG_TEST_CONTEXT					   0x00000001	

/*
    Define states for Supplicant PAE state machine
    */
typedef V1_ENUM enum _STATE_SUPPLICANT_PAE
{
    StateSpaeNone,
    StateSpaeAuthNotStarted = 1,
    StateSpaeLogoff,
    StateSpaeConfigChanged,
    StateSpaeStartAuth,
    StateSpaeInitialize,
    StateSpaeConnecting,
    StateSpaeAuthenticating,
    StateSpaeAuthenticated,
    StateSpaeHeld,
    StateSpaeUserUnavialable,
    StateSpaeFailure,
    StateSpaeMax
} STATE_SUPPLICANT_PAE;


/*
    Define states for Supplicant Backend state machine
    */
typedef V1_ENUM enum _STATE_SUPPLICANT_BACKEND
{
    StateSBackendNone,
    StateSBackendNotStarted = 1,
    StateSBackendUserChanged,
    StateSBackendEapConfigChanged,
    StateSBackendDeactivated,
    StateSBackendIdle,
    StateSBackendUIFailure,
    StateSBackendReceive,
    StateSBackendRequest,
    StateSBackendRequestUI,
    StateSBackendSuccess,
    StateSBackendTimeout,
    StateSBackendFail,
    StateSBackendMax
} STATE_SUPPLICANT_BACKEND;

typedef struct _ONEX_STATE_TRANSITION_DATA
{
    DWORD SPaeOldState;
    DWORD SPaeNewState;

    DWORD SBackendOldState;
    DWORD SBackendNewState;
} ONEX_STATE_TRANSITION_DATA, *PONEX_STATE_TRANSITION_DATA;

/*
    defined in eaptls.h
    */
typedef struct _EAP_TLV
{
    // The highest two bits are flags
    // M - 0 non-mandatory AVP; 1 mandatory AVP.
    // R - reserved, set to zero
    // The rest 14-bit field denotes the attribute type
    BYTE Type[2];
    // The length of the value field in octets
    BYTE Length[2];
    // The first byte of the value of the attribute
    BYTE Value[1];
} EAP_TLV, *PEAP_TLV;

/*
    The TLV data used in communications with EAP and for configuration
    */
typedef struct _ONEX_EAP_TLV_DATA
{
    DWORD dwNumTLVs;
    EAP_TLV EAPTLVs[1];
} ONEX_EAP_TLV_DATA, *PONEX_EAP_TLV_DATA;

typedef V1_ENUM enum _ONEX_DIAG_HINT
{
    OneXDiagHintUserNotResponsive,
    OneXDiagHintUserCancellation,
    OneXDiagHintBackendNotResponsive,
    OneXDiagHintAPTimeouts,
    OneXDiagHintInvalid
} ONEX_DIAG_HINT;

typedef V1_ENUM enum _ONEX_DIAG_HINT_STATE
{
	OneXDiagHintBegin,
	OneXDiagHintCancelled,
	OneXDiagHintStateInvalid
} ONEX_DIAG_HINT_STATE;

typedef struct _ONEX_DIAG_HINT_DATA
{
	ONEX_DIAG_HINT_STATE state;
	ONEX_DIAG_HINT hint;
} ONEX_DIAG_HINT_DATA, *PONEX_DIAG_HINT_DATA;

/*
    Private events for which 1X sends notifications to MSM

    On getting an event notification the callee should switch on the dwEvent.
    
    If dwEvent =  OneXNotificationTypeStateTransition, pvEventData points to an
    ONEX_STATE_TRANSITION_DATA structure
    
    If dwEvent = OneXNotificationTypeNetworkInfo, pvEventData is of type PBYTE
    
    If dwEvent = OneXNotificationTypeEapTLV, pvEventData points to a 
    ONEX_EAP_TLV_DATA structure

    If dwEvent = OneXPrivateNotificationTypeEapAttributes, pvEventData points
    to a EapAttributes structure
    */
typedef V1_ENUM enum _ONEX_PRIVATE_NOTIFICATION_TYPE
{
    OneXPrivateNotificationBase = L2_NOTIFICATION_CODE_PRIVATE_BEGIN,
    OneXPrivateNotificationTypeStateTransition,
    OneXPrivateNotificationTypeNetworkInfo,
    OneXPrivateNotificationTypeEapAttributes,
    OneXPrivateNotificationTypeDiagnosticsHint,
    OneXPrivateNotificationTypeOneXNetwork,
    OneXPrivateNotificationTypeInvalid,
    OneXNumPrivateNotifications = OneXPrivateNotificationTypeInvalid
} ONEX_PRIVATE_NOTIFICATION_TYPE, *PONEX_PRIVATE_NOTIFICATION_TYPE;

#endif //__ONEX_INI_H_

