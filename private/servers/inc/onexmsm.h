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
#ifndef __ONEX_MSM_H_
#define __ONEX_MSM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <wincrypt.h>
#include <onex.h>
#include <l2cmn.h>

/*
    The port status defines whether or not 1X wants to allow traffic to leak 
    from and to the port. 
    If the status is ONEX_PORT_STATUS_AUTHORIZED then allow leaking traffic.
    If the status is ONEX_PORT_STATUS_UNAUTHORIZED do not allow leaking traffic
    */
typedef enum _ONEX_PORT_STATUS
{
    OneXPortAuthorized,
    OneXPortUnauthorized,
    OneXPortInvalid
} ONEX_PORT_STATUS, *PONEX_PORT_STATUS;

/* 
    These flags are set in the result when 1X restarts authentication due to a
    reason for which MSM should reset any timers
    */
#define ONEX_RESULT_FLAG_USER_CHANGED   0x00000001
#define ONEX_RESULT_FLAG_QUARANTINE_REAUTH  0x00000002

typedef struct _ONEX_RESULT
{
    ONEX_STATUS onexStatus;
    ONEX_PORT_STATUS portStatus;

    DWORD dwOneXResultFlags;

    /*
        The following is non-NULL only when the authStatus is 
        OneXAuthSuccess or OneXAuthFailure
        */
    ONEX_VARIABLE_BLOB authParams;
    ONEX_VARIABLE_BLOB eapError;
 
    /*
        IMPORTANT - Both the following keys should be crypt protected
        The key pointers are non-NULL only if the Size field is non-zero.
        
        These can be NULL even if the authentication was successful. This
        would be the case when we didn't receive any keys from the Eap dll
        or we failed to parse the keys we got. MSMs should take appropriate
        actions for this.
        */
    ONEX_VARIABLE_BLOB EAPSendKey;
    ONEX_VARIABLE_BLOB EAPRecvKey;
} ONEX_RESULT, *PONEX_RESULT;

typedef struct _ONEX_RUNTIME_STATE
{
    DWORD fUserToken:1;
    DWORD fPLAPCredentials:1;
    DWORD fSessionId;
    DWORD fEAPCredentials:1;
    
    HANDLE hUserToken;
    
    DWORD dwPlapCredentialsSize;
    PBYTE pbPLAPCredentials;

    DWORD dwSessionId;

    DWORD dwNumEAPCredentialsFields;
    PBYTE pbEAPCredentials;
} ONEX_RUNTIME_STATE, *PONEX_RUNTIME_STATE;

/*
    MSM CALLBACKS - MSM exposes these callbacks to be used by 1X to inform 1X
    of any interesting events and to pass back the result of the authentication
    and the keys that were negotiated
    
    Memory model for these callbacks - MSM should make a copy of the data being 
    passed in the following callbacks. 1X makes no guarantees about the validity 
    of the memory corresponding to the data after the call is complete. To 
    deallocate the memory for any 'out' parameters in these callbacks, 1X should 
    use the PMSM_FREE_MEMORY callback. 
    */

/*
    Update MSM when there is an update to the result of the 1X authentication

    If the authStatus field in the result data is ONEX_AUTH_STATUS_SUCCESS,
    dwAuthParamsSize, dwEAPSendKeySize and dwEAPReceiveKeySize are all non-zero
    and pvAuthParams, pvEAPSendKey and pvEAPReceiveKey are valid
    
    If the authStatus field in the result data is ONEX_AUTH_STATUS_FAILURE,
    dwAuthParamsSizeis non-zero. dwEAPSendKeySize and dwEAPReceiveKeySize are zero
    and pvAuthParams is valid while pvEAPSendKey and pvEAPReceiveKey are NULL
    
    If the authStatus field in the result data is ONEX_AUTH_STATUS_NOT_STARTED
    or ONEX_AUTH_STATUS_IN_PROGRESS, dwAuthParamsSize, dwEAPSendKeySize 
    and dwEAPReceiveKeySize are all zero and and pvAuthParams, pvEAPSendKey and 
    pvEAPReceiveKey are all NULL
    */
typedef DWORD (* PMSM_UPDATE_ONEX_RESULT) (
                                          /*[in]*/ HANDLE hMSM,
                                          /*[in]*/ HANDLE hMSMPortCookie,
                                          /*[in]*/ DWORD dwSize,
                                          /*[in]*/ PONEX_RESULT pOneXResult
                                          );

/*
    Callback exposed by MSM to allow 1X to send packets down to the driver
    */
typedef DWORD (* PMSM_SEND_ONEX_PACKET) (
                                        /*[in]*/ HANDLE hMSM,
                                        /*[in]*/ HANDLE hMSMPortCookie, 
                                        /*[in]*/ DWORD dwDataLen,
                                        /*[in]*/ PBYTE pbData
                                        );

typedef DWORD (* PMSM_PORT_DESTROY_COMPLETION) (
                                        /*[in]*/ HANDLE hMSM,
                                        /*[in]*/ HANDLE hMSMPortCookie
                                        );
		
/*
    Callback exposed by MSM to allow 1X to send a UI request to the 1X UI module
    */
typedef DWORD (*PMSM_UI_REQUEST) (
                                 /*[in]*/ HANDLE hMSM,
                                 /*[in]*/ HANDLE hMSMPortCookie, 
                                 /*[in]*/ DWORD dwSessionId,
                                 /*[in]*/ DWORD dwSize,
                                 /*[in]*/ PVOID pvOneXUIRequestData
                                 );

/*
    Callback exposed by MSM to allow 1X to send event notifications
    */
typedef DWORD (* PMSM_EVENT_NOTIFICATION) (
                                          /*[in]*/ HANDLE hMSM,
                                          /*[in]*/ HANDLE hMSMPortCookie, 
                                          /*[in]*/ PL2_NOTIFICATION_DATA pNotif
                                          );
/*
    MSM exposes this callback to allow 1X to update its connection profile. 
    1X calls this function whenever the 1X profile changes during the course 
    of an authentication process.
    */
typedef DWORD (* PMSM_SET_ONEX_CONNECTION_PROFILE) (
                                                   /*[in]*/ HANDLE hMSM,
                                                   /*[in]*/ HANDLE hMSMPortCookie,
                                                   /*[in]*/ DWORD dwOneXProfileLen,
                                                   /*[in]*/ PVOID pvOneXProfile
                                                   );

/*
    MSM exposes this callback to allow 1X to update its user profile. 1X calls 
    this function whenever the 1X user profile changes during the course of an 
    authentication process (e.g. on a user change).
    */
typedef DWORD (* PMSM_SET_ONEX_USER_PROFILE) (
                                             /*[in]*/ HANDLE hMSM,
                                             /*[in]*/ HANDLE hMSMPortCookie,
                                             /*[in]*/ HANDLE hUserToken,
                                             /*[in]*/ DWORD dwOneXUserProfileLen,
                                             /*[in]*/ PVOID pvOneXUserProfile
                                             );

/*
    This callback allows 1X to query the profile it previously saved. 1X will
    call PMSM_FREE_MEMORY to free the memory for the 'out' parameters
    */
typedef DWORD (* PMSM_QUERY_ONEX_CONNECTION_PROFILE) (
                                                     /*[in]*/ HANDLE hMSM,
                                                     /*[in]*/ HANDLE hMSMPortCookie,
                                                     /*[out]*/ DWORD *pdwOneXProfileLen,
                                                     /*[out]*/ PVOID *ppvOneXProfile
                                                     );

/*
    This callback allows 1X to query the user profile it previously saved. 
    1X will call PMSM_FREE_MEMORY to free the memory for the 'out' parameters
    */
typedef DWORD (* PMSM_QUERY_ONEX_USER_PROFILE) (
                                               /*[in]*/ HANDLE hMSM,
                                               /*[in]*/ HANDLE hMSMPortCookie,
                                               /*[in]*/ HANDLE hUserToken,
                                               /*[out]*/ DWORD *pdwOneXUserProfileLen,
                                               /*[out]*/ PVOID *ppvOneXUserProfile
                                               );
/*
    MSM provides this callback for 1X to use when it wants to free any memory
    buffer that was allocated by MSM (e.g. the user data in a call to 
    PMSM_QUERY_ONEX_USER_DATA
    */
typedef DWORD (* PMSM_FREE_MEMORY) (
                                   /*[in]*/ PVOID pvBuffer
                                   );

/*
    MSM exposes this callback to allow 1X to update its auth params.
    */
typedef DWORD (* PMSM_UPDATE_AUTH_PARAMS) (
    /*[in]*/ HANDLE hMSM,
    /*[in]*/ HANDLE hMSMPortCookie,
    /*[in]*/ DWORD dwAuthParamsSize,
    /*[in]*/ PVOID pvAuthParams
    );

/*
    The set of callbacks that MSM needs to provide. The callbacks marked as
    Optional can be passed in as NULL by the MSM
    */
typedef struct _MSM_CALLBACKS
{
    PMSM_UPDATE_ONEX_RESULT pMSMUpdateOneXResult;                       // Mandatory
    PMSM_SEND_ONEX_PACKET pMSMSendOneXPacket;                           // Mandatory
    PMSM_PORT_DESTROY_COMPLETION pMSMOneXPortDestroyCompletion;			// Mandatory
    PMSM_SET_ONEX_CONNECTION_PROFILE pMSMSetOneXConnectionProfile;      // Optional
    PMSM_SET_ONEX_USER_PROFILE pMSMSetOneXUserProfile;                  // Optional
    PMSM_QUERY_ONEX_CONNECTION_PROFILE pMSMQueryOneXConnectionProfile;  // Optional
    PMSM_QUERY_ONEX_USER_PROFILE pMSMQueryOneXUserProfile;              // Optional
    PMSM_FREE_MEMORY pMSMFreeMemory;                                    // Optional
    PMSM_UI_REQUEST pMSMUIRequest;                                      // Optional
    PMSM_EVENT_NOTIFICATION pMSMEventNotification;                      // Optional
    PMSM_UPDATE_AUTH_PARAMS pMSMUpdateAuthParams;                       // Optional
} MSM_CALLBACKS, *PMSM_CALLBACKS;

#define MAX_DESCRIPTION_LENGTH 1024
/*
    The MSM info structure that 1X maintains
    */
typedef struct _ONEX_MSM_INFO
{
    HANDLE hMSM;
    ONEX_MEDIA_TYPE oneXMediaType;
    GUID interfaceGuid;
    WCHAR wszFriendlyName[MAX_DESCRIPTION_LENGTH + 1];
    DWORD dwMaxFrameLength;
    MSM_CALLBACKS MSMCallbacks;
} ONEX_MSM_INFO, *PONEX_MSM_INFO;


/*
    APIs exposed by the 1X module. MSM uses these APIs to drive the 1X state
    machine. These APIs are also the mechanism through which upper layers (e.g.
    diagnostics) can talk to 1X (e.g. to query 1X state)
    
    Memory model for these APIs - 1X makes a copy of the data being passed 
    in the following APIs. To deallocate the memory for any 'out' parameters in 
    these APIs, MSM should use the OneXFreeMemory function unless specified
    otherwise. 
    */
#define ONEX_API_VERSION_LONGHORN	1

DWORD 
OneXInitialize (PVOID pvReserved);

DWORD 
OneXDeInitialize (PVOID pvReserved);

DWORD
OneXCreateDefaultProfile (
                         /*[in]*/ DWORD dwVersion,
                         /*[in]*/ ONEX_MEDIA_TYPE mediaType,
                         /*[out]*/ DWORD *pdwSize,
                         /*[out]*/ PVOID *ppvDefaultProfile,
                         PVOID pvReserved
                         );

DWORD
OneXCreateSupplicantPort (
                         /*[in]*/ DWORD dwVersion,
                         /*[in]*/ ONEX_MSM_INFO MSMInfo,
                         /*[in]*/ DWORD dwProfileSize,
                         /*[in]*/ PVOID pvOneXProfile,
                         /*[out]*/ HANDLE *phOneXPort,
                         PVOID pvReserved
                         );

DWORD
OneXDestroySupplicantPort (
                          /*[in]*/ DWORD dwVersion,
                          /*[in]*/ HANDLE hOneXPort,
                          PVOID pvReserved
                          );

DWORD
OneXStartAuthentication (
                        /*[in]*/ DWORD dwVersion,
                        /*[in]*/ HANDLE hMSMPortCookie,
                        /*[in]*/ HANDLE hOneXPort,
                        PVOID pvReserved
                        );

DWORD
OneXStopAuthentication (
                       /*[in]*/ DWORD dwVersion,
                       /*[in]*/ HANDLE hOneXPort,
                       PVOID pvReserved
                       );

DWORD
OneXUpdatePortProfile (
                      /*[in]*/ DWORD dwVersion,
                      /*[in]*/ HANDLE hOneXPort,
                      /*[in]*/ DWORD dwProfileSize,
                      /*[in]*/ PVOID pvOneXProfile,
                      /*[in]*/ HANDLE hMSMPortCookie,
                      PVOID pvReserved
                      );

DWORD
OneXQueryState (
               /*[in]*/ DWORD dwVersion,
               /*[in]*/ HANDLE hOneXPort,
               /*[in]*/ DWORD *pdwSize,
               /*[out]*/ PONEX_STATE *ppOneXState,
               PVOID pvReserved
               );

DWORD
OneXQueryStatistics (
                    /*[in]*/ DWORD dwVersion,
                    /*[in]*/ HANDLE hOneXPort,
                    /*[out]*/ DWORD *pdwSize,
                    /*[out]*/ PVOID *ppvOneXStats,
                    PVOID pvReserved
                    );

DWORD 
OneXQueryPendingUIRequest(
                         /*[in]*/ DWORD dwVersion,
                         /*[in]*/ HANDLE hOneXPort,
                         /*[out]*/ DWORD *pdwSessionId,
                         /*[out]*/ DWORD *pdwSize,
                         /*[out]*/ PVOID *ppvOneXUIRequestData,
                         PVOID pvReserved
                         );

DWORD
OneXUIResponse(
              /*[in]*/ DWORD dwVersion,
              /*[in]*/ HANDLE hOneXPort,
              /*[in]*/ DWORD dwSessionId,
              /*[in]*/ DWORD dwOriginalRequestSize,
              /*[in]*/ PVOID pvOriginalUIRequest,
              /*[in]*/ DWORD dwUIResponseSize,
              /*[in]*/ PVOID pvUIResponse,
              PVOID pvReserved
              );

DWORD
OneXAddTLV (
           /*[in]*/ DWORD dwVersion,
           /*[in]*/ HANDLE hOneXPort,
           /*[in]*/ DWORD dwSize,
           /*[in]*/ PVOID pvTLVData,
           PVOID pvReserved
           );

DWORD
OneXIndicatePacket (
                   /*[in]*/ DWORD dwVersion,
                   /*[in]*/ HANDLE hOneXPort,
                   /*[in]*/ UINT16 dwPacketLength,
                   /*[in]*/ PBYTE pbPacket,
                   PVOID pvReserved
                   );

DWORD
OneXIndicateSessionChange (
                          /*[in]*/ DWORD dwVersion,
                          /*[in]*/ DWORD dwEventType,
                          /*[in]*/ PVOID pvEventData,
                          PVOID pvReserved
                          );

#define INTERNALAPI 

DWORD
INTERNALAPI OneXQueryAuthParams (
                             /*[in]*/ DWORD dwVersion,
                             /*[in]*/ HANDLE hOneXPort,
                             /*[out]*/ DWORD *pdwSize,
                             /*[out]*/ PVOID *ppvContext,
                             PVOID pvReserved
                             );

DWORD
INTERNALAPI OneXSetAuthParams (
                           /*[in]*/ DWORD dwVersion,
                           /*[in]*/ HANDLE hOneXPort,
                           /*[in]*/ DWORD dwSize,
                           /*[in]*/ PVOID pvContext,
                           PVOID pvReserved
                           );

DWORD
INTERNALAPI OneXForceAuthenticatedState (
                                        /*[in]*/ DWORD dwVersion,
                                        /*[in]*/ HANDLE hMSMPortCookie,
                                        /*[in]*/ HANDLE hOneXPort,
                                        PVOID pvReserved
                                        );

DWORD
INTERNALAPI OneXCompareAuthParams (
                                  /*[in]*/ DWORD dwVersion,
                                  /*[in]*/ DWORD dwSize1,
                                  /*[in]*/ PVOID pvAuthParamData1,
                                  /*[in]*/ DWORD dwSize2,
                                  /*[in]*/ PVOID pvAuthParamData2,
                                  /*[out]*/ BOOL *pfDifferent,
                                  PVOID pvReserved
                                  );

DWORD
OneXFreeAuthParams (
        /*[in]*/ PVOID pvAuthParams
        );

DWORD
OneXCopyAuthParams(
    DWORD dwAuthParamsSize, 
    PVOID pvAuthParams, 
    PVOID *ppvAuthParamsCopy
    );

DWORD
OneXCreateDiscoveryProfiles(
    /*[in]*/ DWORD dwVersion,
    /*[in]*/ ONEX_MEDIA_TYPE mediaType,
    /*[out]*/ DWORD *pdwNumProfiles,
    /*[out]*/ DWORD **ppdwSizes,
    /*[out]*/ PVOID **pppvDiscoveryProfiles,
    PVOID pvReserved
    );

DWORD
OneXUpdateProfilePostDiscovery(
    ONEX_MEDIA_TYPE mediaType,
    DWORD dwOneXDiscoveryProfileLen,
    PVOID pvOneXDiscoveryProfile,
    DWORD *pdwUpdatedOneXProfileLen,
    PVOID *ppvUpdatedOneXProfile
    );

DWORD
OneXAddEapAttributes (
    /*[in]*/ DWORD dwVersion,
    /*[in]*/ HANDLE hOneXPort,
    /*[in]*/ EapAttributes *pEapAttributes,
    PVOID pvReserved
    );

DWORD
OneXSetRuntimeState (
    /*[in]*/ DWORD dwVersion,
    /*[in]*/ HANDLE hOneXPort,
    /*[in]*/ PVOID pvOneXState,
    PVOID pvReserved
    );

#ifdef __cplusplus
}
#endif

#endif //__ONEX_MSM_H_

