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
#ifndef __ONEX_H_
#define __ONEX_H_

#include <specstrings.h>
#include <dot1x.h>
#include <eaptypes.h>

#if __midl
#define V1_ENUM [v1_enum]
#else
#define V1_ENUM
#endif

typedef struct _ONEX_VARIABLE_BLOB 
{
    DWORD dwSize; 
    DWORD dwOffset;
} ONEX_VARIABLE_BLOB, *PONEX_VARIABLE_BLOB;

/*
    The media type that onex can work with. 1X is more or less media independent 
    and uses this information only to tweak some profile parameters (e.g. timers) 
    and for profile validation.
    */
typedef V1_ENUM enum _ONEX_MEDIA_TYPE
{
    OneXMediaWireless,
    OneXMediaWired,
    OneXMediaInvalid
} ONEX_MEDIA_TYPE, *PONEX_MEDIA_TYPE;

/*
    The auth mode defines how 1X will calculate the identity that it should use
    for authentication
    */
typedef V1_ENUM enum _ONEX_AUTH_MODE 
{
    OneXAuthModeMachineOrUser,
    OneXAuthModeMachineOnly,
    OneXAuthModeUserOnly,
    OneXAuthModeGuest,
    OneXAuthModeInvalid
} ONEX_AUTH_MODE, *PONEX_AUTH_MODE;

/*
    OneX flags that can be present in the profile
    */
#define ONEX_FLAG_CLEAR_USER_DATA                0x00000002
#define ONEX_FLAG_PLAP_ENABLED                   0x00000004
#define ONEX_FLAG_TIMELY_ENABLED                 0x00000008
#define ONEX_FLAG_DISCOVERY_PROFILE              0x00000010   

/*
    The supplicant mode defines the behavior of the 1X module with respect to
    EAPOL starts
    */
typedef V1_ENUM enum _ONEX_SUPPLICANT_MODE
{
    OneXSupplicantModeInhibitTransmission,
    OneXSupplicantModeLearn,
    OneXSupplicantModeCompliant,
    OneXSupplicantModeInvalid
} ONEX_SUPPLICANT_MODE, *PONEX_SUPPLICANT_MODE;

/*
    The EAP types
    */
typedef DWORD ONEX_EAP_TYPE;

#define ONEX_PROFILE_VERSION_LONGHORN	1
#define ONEX_DEFAULT_NETWORK_AUTH_TIMEOUT   10
#define ONEX_DEFAULT_NETWORK_AUTH_ALLOW_UI  1
#define ONEX_DEFAULT_NETWORK_AUTH_USER_VLAN 0 
#define ONEX_DEFAULT_NETWORK_AUTH_WITH_UI_TIMEOUT   60

/*
    The 1X connection profile. Not all fields are mandatory. 
    */
typedef __struct_bcount(dwTotalLen) struct _ONEX_CONNECTION_PROFILE 
{
    DWORD dwVersion;
    DWORD dwTotalLen;

    DWORD fOneXSupplicantFlags:1;
    DWORD fsupplicantMode:1;
    DWORD fauthMode:1;
    DWORD fHeldPeriod:1;
    DWORD fAuthPeriod:1;
    DWORD fStartPeriod:1;
    DWORD fMaxStart:1;
    DWORD fMaxAuthFailures:1;
    DWORD fNetworkAuthTimeout:1;
    DWORD fAllowLogonDialogs:1;
    DWORD fNetworkAuthWithUITimeout:1;
    DWORD fUserBasedVLan:1;

    DWORD dwOneXSupplicantFlags;                        

    ONEX_SUPPLICANT_MODE supplicantMode;
    ONEX_AUTH_MODE authMode; 

    DWORD dwHeldPeriod;                 
    DWORD dwAuthPeriod; 
    DWORD dwStartPeriod;
    DWORD dwMaxStart;       
    DWORD dwMaxAuthFailures;    
    DWORD dwNetworkAuthTimeout;
    DWORD dwNetworkAuthWithUITimeout;
    BOOL  bAllowLogonDialogs;
    BOOL  bUserBasedVLan;

    ONEX_VARIABLE_BLOB EAPConnectionProfile;
} ONEX_CONNECTION_PROFILE, *PONEX_CONNECTION_PROFILE;

typedef struct _EAP_CONNECTION_PROFILE
{
    DWORD dwEapFlags;                           // M
    EAP_METHOD_TYPE eapType;                    // M

    DWORD fEapConnectionData:1;                 // M
    ONEX_VARIABLE_BLOB EapConnectionData;       // O maybe NULL
} EAP_CONNECTION_PROFILE, *PEAP_CONNECTION_PROFILE;

typedef __struct_bcount(dwTotalLen) struct _ONEX_USER_PROFILE
{
    DWORD dwVersion;
    DWORD dwTotalLen;

    ONEX_VARIABLE_BLOB EapUserProfile;
} ONEX_USER_PROFILE, *PONEX_USER_PROFILE;

/*
    The EAP user profile. Not all fields are mandatory. 
    */
typedef struct _EAP_USER_PROFILE
{
    DWORD fEapUserData:1;                       // M

    ONEX_VARIABLE_BLOB EapUserData;             // O - default NULL
} EAP_USER_PROFILE, *PEAP_USER_PROFILE;

/*
    The set of parameters that define the authentication context for 1X
    */
typedef struct _ONEX_AUTH_PARAMS
{
    BOOL fUpdatePending;
    ONEX_VARIABLE_BLOB oneXConnProfile;    
    ONEX_AUTH_IDENTITY authIdentity;
    DWORD dwQuarantineState;
    
    DWORD fSessionId:1;
    DWORD fhUserToken:1;
    DWORD fOnexUserProfile:1;
    DWORD fIdentity:1;
    DWORD fUserName:1;
    DWORD fDomain:1;

    DWORD dwSessionId;

    HANDLE hUserToken;
    
    ONEX_VARIABLE_BLOB OneXUserProfile;
    ONEX_VARIABLE_BLOB Identity;
    ONEX_VARIABLE_BLOB UserName;
    ONEX_VARIABLE_BLOB Domain;
} ONEX_AUTH_PARAMS, *PONEX_AUTH_PARAMS;

typedef struct _ONEX_STATISTICS
{
    /*
        ASSERT(dwNumAuthAttempts + dwNumAuthenticatorNotFound == dwNumAuthSuccess + dwNumAuthFailures)
        
        ASSERT(dwNumValidEapPacketsReceived = (dwNumEapNotifications + dwNumReqIdsReceived + 
                                                dwNumReqestsReceived + dwNumEapSuccessPacketsReceived +
                                                dwNumEapFailurePacketsReceived);   
        */
    DWORD dwNumAuthAttempts;    // number of times we have engaged with the authenticator. Equal to number of transitions to Authenticating state
    DWORD dwNumAuthenticatorNotFound; // number of times we were unable to find an authenticator on the remote end
    DWORD dwNumAuthSuccess;     // number of successful auth attempts. Equal to number of transitions to Authenticated state
    DWORD dwNumAuthFailures;    // number of authentication failure. Equal to number of transitions to Held state
    DWORD dwNumAuthTimeouts; // number of times the auth timed out (e.g. waiting for a packet from the authenticator)

    DWORD dwNumValidEapolFramesReceived; // Number of EAPOL frames that have been received and are valid (not malformed and of expected eapol type). 
    DWORD dwNumInvalidEapolFramesReceived; // Number of EAPOL frames that have been received and are malformed (parsing error). 
    DWORD dwNumUnsupportedEapolFramesReceived; // Number of EAPOL frames that have been received and have an unsupported eapol type (e.g. Eapol start at the supplicant end). 

    DWORD dwNumEapolStartTransmitted;

    DWORD dwNumTransmitErrors;  // number of times our call to MSMSendOneXPacket failed

    DWORD dwAverageTimeNeededToPerformAuth;
} ONEX_STATISTICS, *PONEX_STATISTICS;

/*
    The following list enumerates the reason for the 1X authentication process 
    getting restarted
    */
typedef V1_ENUM enum _ONEX_AUTH_RESTART_REASON
{
    OneXRestartReasonPeerInitiated,
    OneXRestartReasonMsmInitiated,
    OneXRestartReasonOneXHeldStateTimeout,
    OneXRestartReasonOneXAuthTimeout,
    OneXRestartReasonOneXConfigurationChanged,
    OneXRestartReasonOneXUserChanged,
    OneXRestartReasonQuarantineStateChanged,
    OneXRestartReasonAltCredsTrial,
    OneXRestartReasonInvalid
} ONEX_AUTH_RESTART_REASON, *PONEX_AUTH_RESTART_REASON;

typedef struct _ONEX_EAP_ERROR
{
	DWORD dwWinError;
	EAP_METHOD_TYPE type;
	DWORD dwReasonCode;

	GUID   rootCauseGuid;
	GUID   repairGuid;
	GUID   helpLinkGuid;

	DWORD fRootCauseString:1;
	DWORD fRepairString:1;
	
	ONEX_VARIABLE_BLOB RootCauseString;
	ONEX_VARIABLE_BLOB RepairString;
} ONEX_EAP_ERROR, *PONEX_EAP_ERROR;

typedef struct _ONEX_STATUS
{
    ONEX_AUTH_STATUS authStatus;
    /*
        Any errors that happened during the authentication are indicated as 
        error and reason codes below
        */
    DWORD dwReason;
    DWORD dwError;    
} ONEX_STATUS, *PONEX_STATUS;

typedef V1_ENUM enum _ONEX_EAP_METHOD_BACKEND_SUPPORT
{
    OneXEapMethodBackendSupportUnknown,
    OneXEapMethodBackendSupported,
    OneXEapMethodBackendUnsupported
}ONEX_EAP_METHOD_BACKEND_SUPPORT;

typedef struct _ONEX_RESULT_UPDATE_DATA
{
    ONEX_STATUS oneXStatus;
    ONEX_EAP_METHOD_BACKEND_SUPPORT BackendSupport;
    BOOL fBackendEngaged;

    DWORD fOneXAuthParams:1;
    DWORD fEapError:1;
    
    ONEX_VARIABLE_BLOB authParams;
    ONEX_VARIABLE_BLOB eapError;
} ONEX_RESULT_UPDATE_DATA, *PONEX_RESULT_UPDATE_DATA;

typedef ONEX_RESULT_UPDATE_DATA ONEX_STATE, *PONEX_STATE;

typedef struct _ONEX_USER_INFO
{
    ONEX_AUTH_IDENTITY authIdentity;

    DWORD fUserName:1;
    DWORD fDomainName:1;

    ONEX_VARIABLE_BLOB UserName;
    ONEX_VARIABLE_BLOB DomainName;
} ONEX_USER_INFO, *PONEX_USER_INFO;

/*
    Events for which 1X sends notifications to MSM

    On getting an event notification the callee should switch on the dwEvent.
    
    If dwEvent =  OneXNotificationTypeResultUpdate, pvEventData points to an
    ONEX_RESULT_UPDATE_DATA structure
    
    If dwEvent = OneXNotificationTypeOneXUserIdentified, pvEventData points to
    a ONEX_USER_INFO structure
    
    If dwEvent = OneXNotificationTypeGotOneXIdentityString, pvEventData points to
    the identity (LPWSTR) being used for the 1X authentication
    
    If dwEvent = OneXNotificationTypeFallenBackOnGuest pvEventData is NULL
    
    if dwEvent = OneXNotificationTypeAuthRestarted, pvEventData points
    to the restart reason    
    */
typedef V1_ENUM enum _ONEX_NOTIFICATION_TYPE
{
    OneXPublicNotificationBase = 0,
    OneXNotificationTypeResultUpdate,
    OneXNotificationTypeAuthRestarted,
    OneXNotificationTypeEventInvalid,
    OneXNumNotifications = OneXNotificationTypeEventInvalid
} ONEX_NOTIFICATION_TYPE, *PONEX_NOTIFICATION_TYPE;

typedef V1_ENUM enum _ONEX_UI_MESSAGE_TYPE
{
    OneXMessageTypeInteractiveUI,
    OneXMessageTypeNotificationMissingCert,
    OneXMessageTypeNotificationNoSmartCardReader,
    OneXMessageTypeNotificationAuthenticationFailure,
    OneXMessageTypeNotificationCredChangeResult,
    OneXMessageTypeInvalid
} ONEX_UI_MESSAGE_TYPE, ONEX_UI_REQUEST_TYPE, ONEX_UI_RESPONSE_TYPE;

/*
    The 1X UI request structure
    */
typedef struct _ONEX_UI_REQUEST
{
    DWORD dwTotalLen;

    ONEX_UI_REQUEST_TYPE oneXRequestType;
    ONEX_AUTH_IDENTITY oneXAuthIdentity;

    DWORD fUserName:1;
    DWORD fDomainName:1;
    DWORD fEapUIContextData:1;
    DWORD fPerformMSPeapHack:1;

    DWORD dwCredChangeErrorCode;

    ONEX_VARIABLE_BLOB UserName;
    ONEX_VARIABLE_BLOB DomainName;
    ONEX_VARIABLE_BLOB EapUIContextData;
} ONEX_UI_REQUEST, *PONEX_UI_REQUEST;

/*
    The 1X UI response structure
    */
typedef struct _ONEX_UI_RESPONSE
{
    DWORD dwTotalLen;
    /*
        BUGBUG - TODO 
        Do we want anything else to be returned if the Eaphost API to show
        interactive UI fails? Eaphost returns back EapError strcuture - is
        there anything else in that structure that is useful to us
        */
    DWORD dwErrorCode;
    DWORD dwReasonCode;

    ONEX_UI_RESPONSE_TYPE oneXResponseType;

    DWORD fEapDataFromInteractiveUI:1;

    ONEX_VARIABLE_BLOB EapDataFromInteractiveUI;
} ONEX_UI_RESPONSE, *PONEX_UI_RESPONSE;


typedef enum _ONEX_UI_DISPLAY_CONTEXT
{
    onexUIDisplayContextDefault,
    onexUIDisplayContextPreLogon,
    onexUIDisplayContextXWizard,
} ONEX_UI_DISPLAY_CONTEXT, *PONEX_UI_DISPLAY_CONTEXT;


typedef struct _ONEX_UI_DISPLAY_PARAMS
{
    ONEX_UI_DISPLAY_CONTEXT onexUIDisplayContext;
    LPVOID pvPlapUIParams; // type is: PPLAP_UI_PARAMS
} ONEX_UI_DISPLAY_PARAMS, *PONEX_UI_DISPLAY_PARAMS;

#ifdef __cplusplus
extern "C"
{
#endif
    /*
    This function returns the user friendly text to be shown on the balloon
    in the user's session
    if pwszText the function returns the size of text required.
    if the size of text returned is 0, 1X is informing the caller that it is 
    not interested in showing any UI for this specific request
    if pwszTitleSize, the function returns the size of the title. 
    */
DWORD
OneXGetUserFriendlyText(
    PONEX_UI_REQUEST pOneXUIRequest, 
    DWORD dwSessionId,
    __inout DWORD *pdwTextSize, 
    __out_bcount_opt(*pdwTextSize) WCHAR *pwszText,
    __inout_opt DWORD *pdwTitleSize, 
    __out_bcount_opt(*pdwTitleSize) WCHAR *pwszTitle

    );

/*
    This function is called asking 1X to show the appropriate UI and get the response
    from the user. The caller should later call OneXFreeMemory to free up the memory
    associated with the UI response
    */
DWORD
OneXShowUI(
    HWND hwndParent, 
    DWORD dwSessionId,
    PONEX_UI_REQUEST pOneXUIRequest, 
    PONEX_UI_RESPONSE *ppOneXUIResponse
    );

#ifndef UNDER_CE
DWORD
OneXShowUIFromEAPCreds(
    HWND hwndParent, 
    DWORD dwCurrentSessionId,
    PONEX_UI_DISPLAY_PARAMS pOneXUIDiaplayParams,
    PONEX_UI_REQUEST pOneXUIRequest, 
    PONEX_UI_RESPONSE *ppOneXUIResponse
    );
#endif

DWORD
OneXFreeMemory (
               /*[in]*/ PVOID pBuffer
               );

DWORD 
OneXReasonCodeToString(
    __in DWORD dwReasonCode,
    __in DWORD dwBufferSize,
    __in_ecount(dwBufferSize) PWCHAR pStringBuffer,
    __reserved PVOID pReserved
);

DWORD 
OneXRestartReasonCodeToString(
    __in DWORD dwReasonCode,
    __in DWORD dwBufferSize,
    __in_ecount(dwBufferSize) PWCHAR pStringBuffer,
    __reserved PVOID pReserved
);


#ifdef __cplusplus
}
#endif

#endif //__ONEX_H_

