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
#ifndef _PLAP_CREDS_H
#define _PLAP_CREDS_H

typedef enum _PLAP_INPUT_FIELD_TYPE {
    PlapInputUsername,
    PlapInputPassword,
    PlapInputNetworkUsername,
    PlapInputNetworkPassword,
    PlapInputPin, 
    PlapInputPSK, 
    PlapInputToken, 
    PlapInputEdit
} PLAP_INPUT_FIELD_TYPE, *PPLAP_INPUT_FIELD_TYPE;

typedef enum _PLAP_CREDENTIAL_CHANGED_STATUS {
    PlapCredentialChangedStatusUnknown,
    PlapCredentialChangedStatusSuccess,
    PlapCredentialChangedStatusFailure
} PLAP_CREDENTIAL_CHANGED_STATUS, *PPLAP_CREDENTIAL_CHANGED_STATUS;


#define PLAP_INPUT_FIELD_PROPS_DEFAULT             0
#define PLAP_INPUT_FIELD_PROPS_NON_DISPLAYABLE         1 // like password 
#define MAX_PLAP_INPUT_FIELD_NAME_LENGTH		256
#define MAX_PLAP_INPUT_FIELD_VALUE_LENGTH		1024


#define PLAP_FIELD_EXTENDED_PROP_OLD_CREDS      0x1
#define PLAP_FIELD_EXTENDED_PROP_NEW_CREDS      0x2
#define PLAP_FIELD_EXTENDED_PROP_CONFIRM_CREDS  0x4

typedef struct _PLAP_INPUT_FIELD_DATA {
     PLAP_INPUT_FIELD_TYPE Type;
     // these are the same as the EAP properties defined in eaptypes.w
     DWORD dwFlagProps;
     // these are additional field specific information that onexui comes up with
     DWORD dwExtendedProps;
     WCHAR wszLabel[MAX_PLAP_INPUT_FIELD_NAME_LENGTH];
     WCHAR wszData[MAX_PLAP_INPUT_FIELD_VALUE_LENGTH];
} PLAP_INPUT_FIELD_DATA, *PPLAP_INPUT_FIELD_DATA;

typedef struct _PLAP_UI_CREDS
{
    BOOL fCredsAvailable;
    PLAP_INPUT_FIELD_TYPE plapCredentialType;
    WCHAR wszCredential[MAX_PLAP_INPUT_FIELD_VALUE_LENGTH];
} PLAP_UI_CREDS, *PPLAP_UI_CREDS;

typedef struct _PLAP_UI_PARAMS
{
    PLAP_UI_CREDS plapOldCreds;
    PLAP_UI_CREDS plapNewCreds;
    PLAP_CREDENTIAL_CHANGED_STATUS plapCredentialChangedStatus;
} PLAP_UI_PARAMS, *PPLAP_UI_PARAMS;


#endif  // _PLAP_CREDS_H
